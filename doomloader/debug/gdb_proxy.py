#!/usr/bin/env python3
# Copyright (c) 2026 Larry Pyeatt.  All rights reserved.
"""GDB protocol proxy that fixes hw_server's broken packet responses.

hw_server has several GDB remote protocol bugs:
  1. Responds to vMustReplyEmpty with vCont support string (should be empty)
  2. QStartNoAckMode response causes "conflicting enabled" errors
  3. Pipelined requests cause response mis-association

This proxy:
  - Immediately acks GDB packets (prevents retransmissions)
  - Serializes requests to hw_server (one at a time)
  - Intercepts broken packets and replies directly
  - Absorbs hw_server acks (proxy handles ack protocol on each side)
  - Preempts old sessions when a new GDB client connects
  - Uses TCP keepalive to detect dead connections quickly

Usage:
    ./gdb_proxy.py [-v] [listen_port [upstream_port]]

    Default: listen on 3334, forward to hw_server on 3004
    Connect GDB/Ozone to localhost:3334 instead of 3004.
"""

import socket
import select
import sys
import time
import collections

flags = [a for a in sys.argv[1:] if a.startswith("-")]
args = [a for a in sys.argv[1:] if not a.startswith("-")]
LISTEN_PORT = int(args[0]) if len(args) > 0 else 3334
UPSTREAM_PORT = int(args[1]) if len(args) > 1 else 3004
UPSTREAM_HOST = "localhost"
VERBOSE = "-v" in flags

# Timeout for hw_server response (seconds).  If we send a packet and
# get no response within this time, assume hw_server is stuck.
RESPONSE_TIMEOUT = 30.0

# Packets to intercept from GDB and reply with empty (unsupported).
INTERCEPT_PACKETS = [
    b"vMustReplyEmpty",
]

def log(msg):
    print(f"[proxy] {msg}", flush=True)

def log_data(direction, data):
    if VERBOSE:
        display = data.decode("ascii", errors="replace")
        if len(display) > 300:
            display = display[:300] + "..."
        log(f"{direction} ({len(data)}b): {display}")

def enable_keepalive(sock):
    """Enable TCP keepalive with aggressive timers to detect dead peers."""
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPIDLE, 10)
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPINTVL, 5)
    sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_KEEPCNT, 3)

def split_gdb_tokens(data):
    """Split raw bytes into individual GDB protocol tokens.

    Returns (tokens, remaining) where:
    - tokens: list of (token_bytes, is_packet) tuples.
      is_packet is True for $...#xx packets or Ctrl-C, False for +/- acks.
    - remaining: incomplete data at the end (prepend to next recv).
    """
    tokens = []
    i = 0
    while i < len(data):
        if data[i:i+1] in (b'+', b'-'):
            tokens.append((data[i:i+1], False))
            i += 1
        elif data[i:i+1] == b'$':
            hash_pos = data.find(b'#', i + 1)
            if hash_pos == -1 or hash_pos + 2 >= len(data):
                # Incomplete packet — return as remaining for next recv
                return tokens, data[i:]
            end = hash_pos + 3
            tokens.append((data[i:end], True))
            i = end
        elif data[i:i+1] == b'\x03':
            tokens.append((data[i:i+1], True))
            i += 1
        else:
            i += 1
    return tokens, b''

MAX_M_DATA = 0xC00   # 3072 bytes per $M chunk (~6KB packet, matches system GDB)

def make_m_packet(addr, data):
    """Build a single $Maddr,length:HEXDATA#cs packet."""
    length = len(data)
    m_body = f'M{addr:x},{length:x}:'.encode('ascii') + data.hex().encode('ascii')
    checksum = sum(m_body) & 0xFF
    return b'$' + m_body + b'#' + f'{checksum:02x}'.encode('ascii')

def x_to_m_chunks(x_packet):
    """Convert $X (binary write) to one or more $M (hex write) packets.

    hw_server doesn't support $X and rejects large $M packets, so we
    split into chunks of MAX_M_DATA bytes each.

    $X format: $Xaddr,length:ESCAPED_BINARY#cs
    Binary escaping: } (0x7d) is escape char, next byte XOR 0x20.
    Returns a list of $M packet bytes.
    """
    # Strip $ prefix and #xx checksum suffix
    body = x_packet[1:-3]
    colon_pos = body.find(b':')
    header = body[:colon_pos]   # b'Xaddr,length'
    binary_data = body[colon_pos + 1:]

    # Parse base address
    addr_str = header[1:]       # strip the 'X'
    base_addr = int(addr_str.split(b',')[0], 16)

    # Unescape binary data
    raw = bytearray()
    i = 0
    while i < len(binary_data):
        if binary_data[i] == 0x7d and i + 1 < len(binary_data):
            raw.append(binary_data[i + 1] ^ 0x20)
            i += 2
        else:
            raw.append(binary_data[i])
            i += 1

    # Split into chunks
    chunks = []
    offset = 0
    while offset < len(raw):
        chunk = raw[offset:offset + MAX_M_DATA]
        chunks.append(make_m_packet(base_addr + offset, chunk))
        offset += len(chunk)
    if not chunks:
        # Empty write (probe packet)
        chunks.append(make_m_packet(base_addr, bytearray()))
    return chunks

def should_intercept(packet_data):
    for pattern in INTERCEPT_PACKETS:
        if pattern in packet_data:
            return pattern
    return None

# Sentinel returned by proxy_connection when a new client preempts.
PREEMPTED = object()

def proxy_connection(client_sock, server_sock):
    """Handle one GDB client connection.

    Returns a (new_client_sock, new_addr) tuple if a new client
    connected and preempted this session, or None on normal exit.
    """
    upstream = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    upstream.connect((UPSTREAM_HOST, UPSTREAM_PORT))
    log(f"Connected to hw_server at {UPSTREAM_HOST}:{UPSTREAM_PORT}")

    enable_keepalive(client_sock)
    enable_keepalive(upstream)

    # Queue of GDB packets to send to hw_server one at a time
    pending_packets = collections.deque()
    waiting_for_response = False
    response_sent_time = 0.0
    gdb_noack = False
    # Remaining chunks from a split $X -> $M conversion.
    # Intermediate chunk responses are absorbed; only the last
    # chunk's response is forwarded to GDB.
    pending_chunks = []
    # hw_server needs QStartNoAckMode to set up the GDB debug session.
    # The proxy replies to GDB immediately but also forwards to hw_server.
    # After hw_server processes QStartNoAckMode, we inject a sequence of
    # init packets: Hg0 (set thread), then '?' (halt reason).
    # hw_server returns $W00 for '?' without Hg first, but $T05 after Hg.
    upstream_noack = False
    absorb_noack_response = False
    # Init packet injection queue (sent one at a time, responses absorbed)
    inject_queue = []
    absorb_injected = False

    # Receive buffers for incomplete packets split across TCP segments.
    # Data is accumulated here until a complete $...#xx packet arrives.
    gdb_buf = b''
    upstream_buf = b''

    client_sock.setblocking(False)
    upstream.setblocking(False)

    new_client_info = None

    def send_next():
        """Send next queued packet to hw_server if not already waiting."""
        nonlocal waiting_for_response, absorb_noack_response
        nonlocal inject_queue, absorb_injected, response_sent_time
        if waiting_for_response:
            return
        # Send next injected init packet (responses are absorbed)
        if inject_queue:
            pkt = inject_queue.pop(0)
            absorb_injected = True
            log(f"Injecting init packet")
            log_data("FWD->", pkt)
            upstream.sendall(pkt)
            waiting_for_response = True
            response_sent_time = time.monotonic()
            return
        # Send next chunk from a split $X first
        if pending_chunks:
            pkt = pending_chunks.pop(0)
            log_data("FWD->", pkt)
            upstream.sendall(pkt)
            waiting_for_response = True
            response_sent_time = time.monotonic()
            return
        if pending_packets:
            pkt = pending_packets.popleft()
            # Translate $X (binary write) to $M (hex write) chunks --
            # hw_server doesn't support $X and rejects large $M.
            if pkt[:2] == b'$X':
                chunks = x_to_m_chunks(pkt)
                log(f"Split $X into {len(chunks)} $M chunks")
                pkt = chunks.pop(0)
                pending_chunks.extend(chunks)
            log_data("FWD->", pkt)
            upstream.sendall(pkt)
            waiting_for_response = True
            response_sent_time = time.monotonic()

    try:
        while True:
            # Include the server socket so we can detect new connections
            watch = [client_sock, upstream, server_sock]
            readable, _, _ = select.select(watch, [], [], 1.0)

            # Check for response timeout
            if waiting_for_response:
                elapsed = time.monotonic() - response_sent_time
                if elapsed > RESPONSE_TIMEOUT:
                    log(f"hw_server response timeout ({elapsed:.0f}s), "
                        f"closing session")
                    return new_client_info

            for s in readable:
                if s is server_sock:
                    # New GDB client is connecting — preempt this session
                    new_sock, new_addr = server_sock.accept()
                    log(f"New client {new_addr} preempting current session")
                    new_client_info = (new_sock, new_addr)
                    return new_client_info

                data = s.recv(65536)
                if not data:
                    log("Connection closed")
                    return new_client_info

                if s is client_sock:
                    log_data("GDB->", data)
                    gdb_buf += data
                    tokens, gdb_buf = split_gdb_tokens(gdb_buf)
                    for token, is_packet in tokens:
                        if not is_packet:
                            # GDB ack (+/-) — absorb, don't forward
                            # (proxy handles acks independently on each side)
                            pass
                        elif token == b'\x03':
                            # Ctrl-C (interrupt) — forward immediately.
                            # This is an async signal, not a request-response
                            # packet, so it must NOT go through the serialization
                            # queue (hw_server won't send a response for it;
                            # it triggers an async stop reply instead).
                            log("Forwarding interrupt (Ctrl-C)")
                            upstream.sendall(b'\x03')
                        elif b"QStartNoAckMode" in token:
                            # Reply OK so GDB switches to noack mode.
                            # Also forward to hw_server so it sets up the
                            # debug session (absorb its response since we
                            # already replied to GDB).
                            log("QStartNoAckMode -> reply to GDB, "
                                "forward to hw_server")
                            client_sock.sendall(b"+$OK#9a")
                            gdb_noack = True
                            absorb_noack_response = True
                            pending_packets.append(token)
                        else:
                            pattern = should_intercept(token)
                            if pattern is not None:
                                ack = b"" if gdb_noack else b"+"
                                log(f"Intercepted {pattern.decode()}")
                                client_sock.sendall(ack + b"$#00")
                            else:
                                # Ack the packet immediately to prevent
                                # GDB retransmission (skip if noack mode)
                                if not gdb_noack:
                                    client_sock.sendall(b"+")
                                # Queue for serialized forwarding
                                pending_packets.append(token)

                elif s is upstream:
                    log_data("HWS->", data)
                    upstream_buf += data
                    tokens, upstream_buf = split_gdb_tokens(upstream_buf)
                    for token, is_packet in tokens:
                        if not is_packet:
                            # hw_server ack — absorb (we already acked GDB)
                            pass
                        else:
                            waiting_for_response = False
                            # Ack the response packet to hw_server
                            # (required in ack mode; skip after noack)
                            if not upstream_noack:
                                upstream.sendall(b"+")
                            if absorb_noack_response:
                                absorb_noack_response = False
                                upstream_noack = True
                                # Queue init packets: Hg0 then ? (halt reason)
                                # hw_server needs Hg before ? returns $T05.
                                inject_queue.extend([
                                    b'$Hg0#df',
                                    b'$?#3f',
                                ])
                                log("Absorbed QStartNoAckMode response, "
                                    "queuing init packets")
                            elif absorb_injected:
                                absorb_injected = False
                                log(f"Absorbed injected response: {token}")
                            elif pending_chunks:
                                # Intermediate chunk response — absorb.
                                # If error, abort remaining chunks and
                                # forward the error to GDB.
                                if token[:2] == b'$E':
                                    log(f"Chunk error, aborting "
                                        f"{len(pending_chunks)} remaining")
                                    pending_chunks.clear()
                                    client_sock.sendall(token)
                            else:
                                # Final (or only) response — forward to GDB.
                                client_sock.sendall(token)

            # Try to send next queued packet
            send_next()

    finally:
        upstream.close()

def main():
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(("0.0.0.0", LISTEN_PORT))
    server.listen(1)
    log(f"GDB proxy listening on port {LISTEN_PORT}")
    log(f"Forwarding to hw_server at {UPSTREAM_HOST}:{UPSTREAM_PORT}")
    log(f"Connect GDB/Ozone to localhost:{LISTEN_PORT}")
    print(flush=True)

    pending_client = None

    while True:
        if pending_client is not None:
            client_sock, addr = pending_client
            pending_client = None
        else:
            client_sock, addr = server.accept()

        log(f"Client connected from {addr}")
        try:
            result = proxy_connection(client_sock, server)
            if result is not None:
                # A new client preempted — queue it for the next iteration
                pending_client = result
        except (ConnectionRefusedError, ConnectionResetError,
                BrokenPipeError, OSError) as e:
            log(f"Connection error: {e}")
        finally:
            client_sock.close()
            log(f"Client disconnected")

if __name__ == "__main__":
    main()

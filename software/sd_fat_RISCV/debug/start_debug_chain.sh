#!/bin/bash
# Copyright (c) 2026 Larry Pyeatt.  All rights reserved.
#
# start_debug_chain.sh -- Start the MicroBlaze V debug chain for VS Code.
#
# Kills any existing debug chain processes, then starts hw_server,
# xsdb (BSCAN/Hart init), and gdb_proxy.py fresh.
#
# VS Code calls this as a preLaunchTask.  The script prints
# "DEBUG_CHAIN_READY" when GDB clients can connect to port 3334.
#
# Prerequisites:
#   1. FPGA must be programmed with the MicroBlaze V bitstream.
#   2. Nexys A7 connected via USB.

VIVADO_ROOT="/opt/Xilinx/2025.1/Vivado"
HW_SERVER="${VIVADO_ROOT}/bin/hw_server"
XSDB="${VIVADO_ROOT}/bin/xsdb"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
INIT_TCL="${SCRIPT_DIR}/hw_server_init.tcl"
PROXY_PY="${SCRIPT_DIR}/gdb_proxy.py"

HW_SERVER_TCF_PORT=3121
HW_SERVER_GDB_PORT=3004
PROXY_PORT=3334

echo "=== MicroBlaze V Debug Chain ==="

# --- Kill any existing debug chain processes ---
echo "[0/3] Cleaning up old processes..."
pkill -f "gdb_proxy.py" 2>/dev/null
pkill -f "hw_server_init.tcl" 2>/dev/null
pkill -x xsdb 2>/dev/null
pkill -x hw_server 2>/dev/null
sleep 0.3

# --- Step 1: hw_server ---
echo "[1/3] Starting hw_server..."
nohup "$HW_SERVER" -s TCP::${HW_SERVER_TCF_PORT} -p 3000 >/dev/null 2>&1 &
disown
for i in $(seq 1 30); do
    if ss -tlnp 2>/dev/null | grep -q ":${HW_SERVER_TCF_PORT} "; then
        echo "      hw_server ready."
        break
    fi
    sleep 0.3
done
if ! ss -tlnp 2>/dev/null | grep -q ":${HW_SERVER_TCF_PORT} "; then
    echo "hw_server did not start within 10 seconds."
    exit 1
fi

# --- Step 2: xsdb init (BSCAN discovery + Hart halt) ---
echo "[2/3] Initializing BSCAN target and halting Hart..."
XSDB_LOG=$(mktemp /tmp/xsdb_init.XXXXXX)
nohup "$XSDB" "$INIT_TCL" >"$XSDB_LOG" 2>&1 &
XSDB_PID=$!
disown
# Poll for the "Hart halted" message instead of blind sleep.
for i in $(seq 1 30); do
    if ! kill -0 $XSDB_PID 2>/dev/null; then
        echo "xsdb init failed. Is the FPGA programmed?"
        cat "$XSDB_LOG"
        rm -f "$XSDB_LOG"
        exit 1
    fi
    if grep -q "Hart halted" "$XSDB_LOG" 2>/dev/null; then
        echo "      Hart halted, xsdb keeping session alive (PID $XSDB_PID)."
        rm -f "$XSDB_LOG"
        break
    fi
    sleep 0.5
done
if [ -f "$XSDB_LOG" ]; then
    echo "xsdb init timed out (15s). Output:"
    cat "$XSDB_LOG"
    rm -f "$XSDB_LOG"
    exit 1
fi

# --- Step 3: GDB proxy ---
echo "[3/3] Starting GDB proxy on port $PROXY_PORT..."
nohup python3 "$PROXY_PY" $PROXY_PORT $HW_SERVER_GDB_PORT >/dev/null 2>&1 &
disown
for i in $(seq 1 15); do
    if ss -tlnp 2>/dev/null | grep -q ":${PROXY_PORT} "; then
        echo "      GDB proxy ready."
        break
    fi
    sleep 0.3
done
if ! ss -tlnp 2>/dev/null | grep -q ":${PROXY_PORT} "; then
    echo "GDB proxy did not start."
    exit 1
fi

# Brief probe: connect and disconnect to force the proxy to do its
# upstream init sequence (QStartNoAckMode + Hg0 + ?).  This primes
# hw_server's GDB target context so the real GDB session doesn't
# get a stale $W00 (exited) response on the first connection.
echo "[4/4] Priming GDB session..."
(echo -ne '$QStartNoAckMode#b0'; sleep 1) | nc -q 0 -w 2 localhost $PROXY_PORT >/dev/null 2>&1 || true
sleep 0.5

echo ""
echo "DEBUG_CHAIN_READY"
echo "Connect GDB/VS Code to localhost:$PROXY_PORT"

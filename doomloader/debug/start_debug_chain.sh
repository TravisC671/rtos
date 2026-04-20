#!/bin/bash
# Copyright (c) 2026 Larry Pyeatt.  All rights reserved.
#
# start_debug_chain.sh -- Start the MicroBlaze V debug chain for VS Code.
#
# Starts hw_server, xsdb (BSCAN/Hart init), and gdb_proxy.py if not
# already running.  Idempotent: safe to call multiple times.
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

port_listening() {
    ss -tlnp 2>/dev/null | grep -q ":${1} "
}

echo "=== MicroBlaze V Debug Chain ==="

# --- Step 1: hw_server ---
if port_listening $HW_SERVER_TCF_PORT; then
    echo "[1/3] hw_server already running on port $HW_SERVER_TCF_PORT."
else
    echo "[1/3] Starting hw_server..."
    "$HW_SERVER" -s TCP::${HW_SERVER_TCF_PORT} -p 3000 >/dev/null 2>&1 &
    for i in $(seq 1 10); do
        if port_listening $HW_SERVER_TCF_PORT; then
            echo "      hw_server ready."
            break
        fi
        sleep 1
    done
    if ! port_listening $HW_SERVER_TCF_PORT; then
        echo "ERROR: hw_server did not start within 10 seconds."
        exit 1
    fi
fi

# --- Step 2: xsdb init (BSCAN discovery + Hart halt) ---
if pgrep -f "hw_server_init.tcl" > /dev/null 2>&1; then
    echo "[2/3] xsdb init already running."
else
    echo "[2/3] Initializing BSCAN target and halting Hart..."
    "$XSDB" "$INIT_TCL" >/dev/null 2>&1 &
    XSDB_PID=$!
    # Wait for the init to complete (BSCAN discovery + Hart halt).
    sleep 8
    if ! kill -0 $XSDB_PID 2>/dev/null; then
        echo "ERROR: xsdb init failed. Is the FPGA programmed?"
        exit 1
    fi
    echo "      Hart halted, xsdb keeping session alive (PID $XSDB_PID)."
fi

# --- Step 3: GDB proxy ---
if port_listening $PROXY_PORT; then
    echo "[3/3] GDB proxy already running on port $PROXY_PORT."
else
    echo "[3/3] Starting GDB proxy on port $PROXY_PORT..."
    python3 "$PROXY_PY" $PROXY_PORT $HW_SERVER_GDB_PORT >/dev/null 2>&1 &
    for i in $(seq 1 5); do
        if port_listening $PROXY_PORT; then
            echo "      GDB proxy ready."
            break
        fi
        sleep 1
    done
    if ! port_listening $PROXY_PORT; then
        echo "ERROR: GDB proxy did not start."
        exit 1
    fi
fi

echo ""
echo "DEBUG_CHAIN_READY"
echo "Connect GDB/VS Code to localhost:$PROXY_PORT"

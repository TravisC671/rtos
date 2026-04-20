#!/bin/bash
# Copyright (c) 2026 Larry Pyeatt.  All rights reserved.
#
# stop_debug_chain.sh -- Stop the MicroBlaze V debug chain.
#
# Kills gdb_proxy.py, xsdb (hw_server_init.tcl), and hw_server.

echo "Stopping debug chain..."

if pgrep -f "gdb_proxy.py" > /dev/null 2>&1; then
    pkill -f "gdb_proxy.py"
    echo "  Stopped gdb_proxy.py"
fi

if pgrep -f "hw_server_init.tcl" > /dev/null 2>&1; then
    pkill -f "hw_server_init.tcl"
    echo "  Stopped xsdb init"
fi

if pgrep -x xsdb > /dev/null 2>&1; then
    pkill -x xsdb
    echo "  Stopped xsdb"
fi

if pgrep -x hw_server > /dev/null 2>&1; then
    pkill -x hw_server
    echo "  Stopped hw_server"
fi

sleep 1
echo "Debug chain stopped."

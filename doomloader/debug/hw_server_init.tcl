# Copyright (c) 2026 Larry Pyeatt.  All rights reserved.
#
# hw_server_init.tcl -- Initialize the BSCAN target and halt the Hart
# so the RISC-V core is ready for GDB clients.
#
# Must be run via xsdb/xsct AFTER hw_server is already running.
# This script does NOT exit — it keeps the xsdb session connected
# so the debug session state is maintained.  Kill it with Ctrl-C
# or by killing the process.
#
# Usage:
#   /opt/Xilinx/2025.1/Vivado/bin/xsdb hw_server_init.tcl
#
# Target hierarchy (Nexys A7 with MicroBlaze V):
#   1: xc7a100t (FPGA)
#   2: BSCAN JTAG at USER2
#   3: RISC-V at USER2 (appears after stop on target 2)
#   4: Hart #0
#
# Without this initialization, hw_server returns $W00 for halt-reason
# queries and $E01 for all memory writes — GDB/Ozone cannot debug.

puts "Connecting to hw_server on localhost:3121..."
connect -host localhost -port 3121

# Wait for JTAG scan to complete
after 2000

puts ""
puts "=== Targets before init ==="
set tgt_list [targets]
puts $tgt_list

# Stop the BSCAN target to trigger RISC-V core discovery
puts ""
puts "Stopping BSCAN target to discover RISC-V core..."
targets -set -filter {name =~ "*BSCAN*" || name =~ "*bscan*"}
stop

# Give hw_server time to discover the RISC-V core
after 3000

puts ""
puts "=== Targets after BSCAN stop ==="
set tgt_list [targets]
puts $tgt_list

# Now halt the Hart so GDB sees a stopped target (not $W00)
puts ""
puts "Halting the RISC-V Hart..."
if {[catch {targets -set -filter {name =~ "*Hart*" || name =~ "*hart*"}} err]} {
    # Try selecting by RISC-V name if no Hart target
    if {[catch {targets -set -filter {name =~ "*RISC-V*" || name =~ "*riscv*"}} err2]} {
        puts "ERROR: Could not find Hart or RISC-V target."
        puts "Targets available:"
        targets
        exit 1
    }
}
if {[catch {stop} err]} {
    puts "Hart already stopped (OK): $err"
}

after 1000

puts ""
puts "=== Final target state ==="
set tgt_list [targets]
puts $tgt_list

puts ""
puts "RISC-V Hart halted. GDB clients can connect to localhost:3004"
puts "Keeping xsdb session alive (Ctrl-C to stop)..."
puts ""

# Keep the xsdb session alive so the debug state persists.
# If xsdb disconnects, hw_server may release the target.
while {1} {
    after 10000
}

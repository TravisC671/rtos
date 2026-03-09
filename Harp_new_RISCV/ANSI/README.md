PDCurses for ANSI Terminals
===========================

This is a port of PDCurses for ANSI-compatible terminals.  It is
designed to allow existing curses programs to be re-compiled with
PDCurses. It is implemented in two layers.

  1. The top layer translates the PDC... functions into
     ANSI control sequences.

  2. The bottom layer (the driver) provides communications with the terminal.

There are currently two drivers to choose from.

  1. The Linux driver provides support for the Linux console and
     various terminal programs such as Konsole.  Communication is
     done using stdin and stdout, so it is possible to redirect input
     and output to use a serial port or other device. It should work
     on other versions of Unix as well. The Linux driver directory
     contains more information.

  2. The FreeRTOS driver provides support for an ANSI terminal
     connected to an embedded system using a 16550A UART and running
     FreeRTOS. The FreeRTOS driver directory contains more information.

Building
--------


- Run "make" in the ANSI directory. This should build libcurses.a. 



ANSI port was provided by Larry Pyeatt <larry.pyeatt@sdsmt.edu>  


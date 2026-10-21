# tmac_ds

Reads and configures the TMAC counter over SPI, in three versions that speak
the same protocol: MicroPython (tmac_ds.py), Arduino IDE (src/) and C with the
Pico SDK (c/). Each one is installed and used independently of the others.

## Installation

MicroPython: with mpremote connected to the board, mpremote mip install
github:catuta0/tmac_ds. Or copy tmac_ds.py by hand.

Arduino IDE: clone into the libraries folder, which on macOS is
~/Documents/Arduino/libraries, or download the zip and use Sketch, Include
Library, Add .ZIP Library. The example turns up under File, Examples, tmac_ds,
tmac_basic.

C with the Pico SDK: there is no package manager, installation is CMake. Put
the repository in external/tmac_ds of your project, add
add_subdirectory(external/tmac_ds/c tmac_ds) to CMakeLists.txt and tmac_ds to
target_link_libraries. The add_subdirectory already brings the include
directories along, so including tmac_ds.h is enough.


## API

The three versions have the same six functions, named by each language's
convention. In order, MicroPython, Arduino and C:

- TmacDS(...), begin, tmac_init and tmac_init_bb. Opens the bus. In C the _bb
  one does the SPI in software instead of in hardware, for when the free pins
  cannot form an SPI, and that choice then governs every other call.
- config, config, tmac_config. Sends one word with all three fields.
- reset, reset, tmac_reset. Zeroes everything. Takes the configuration to
  leave in place, because the word carries the three fields together.
- read_raw, readRaw, tmac_read_raw. The 18 bytes as they arrive.
- read_counters, readCounters, tmac_read_counters. The same frame, decoded.
- start_aquisition, startAquisition, tmac_start_aquisition. Bring-up only, see
  the next section.

The constants for the out field and for the reset are called MAJORITY,
ADJACENCY, NO_RESET and RESET, prefixed with TMAC_ in C and in Arduino.


## Three things to know before putting it in an existing program

A readout is 18 bytes and blocks for about 150 us at 1 MHz; a configuration is
2 bytes. Never call any of this from inside an interrupt handler: in a program
that also services a fast IRQ you would miss events. The right place is the
main loop or a command interpreter. Being interrupted in the middle of a
transaction is harmless, because CS stays low and the FPGA only advances on the
SCK transitions.

The driver keeps no state. The word carries channel, level and out together,
and the readout frame does not give them back, so the FPGA cannot be asked
which configuration it is in: every call to config states all three fields, and
remembering them between calls is the caller's job.

start_aquisition is for bring-up and testing only. It never returns, and it
writes CSV text, to a file in MicroPython and to the serial port or stdout in
the other two. In a program that sends binary over the same channel, the text
corrupts the stream. For anything real, use config and read_counters and write
your own loop.

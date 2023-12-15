# GSIM: A Graph-based RTL Simulator

## Prerequisites

Install [Verilator](https://verilator.org/guide/latest/install.html), [GMP](https://gmplib.org/).

## Usage

Generate high-firrtl from scala

    $ mill -i __.test.runMain $(TOP_NAME) -td $(BUILD_DIR) -X high

Compile high-firttl to C++

    $ make compile MODE=0

Run the C++ code for simulation

    $ make difftest MODE=0

+ `MODE=0` - run GSIM (default)
+ `MODE=1` - run verilator
+ `MODE=2` - run GSIM and verilator concurrently for difftest

See [C++ harness example](https://github.com/jaypiper/simulator/blob/master/emu/emu-NutShell.cpp) to know how it interacts with the emitted C++ code.

# GSIM: A Graph-based RTL Simulator

## Prerequisites

Install [Verilator](https://verilator.org/guide/latest/install.html), [GMP](https://gmplib.org/).

## Usage

Compile low-firttl to C++

    $ make compile MODE=0

Run the C++ code for simulation

    $ make difftest MODE=0

+ `MODE=0` - run GSIM (default)
+ `MODE=1` - run verilator
+ `MODE=2` - run GSIM and verilator concurrently for difftest 

# GSIM: A Fast RTL Simulator for Large-Scale Designs

GSIM accepts chirrtl, and compiles it to C++

## Prerequisites

+ [GMP](https://gmplib.org/), [clang 17+](https://clang.llvm.org/).
+ [libc++ 17+](https://libcxx.llvm.org/) (Debian only)

## Quike Start

+ GSIM provides 4 RISC-V cores ready for simulation: [ysyx3](https://ysyx.oscc.cc/), [Rocket](https://github.com/chipsalliance/rocket-chip), [BOOM](https://github.com/riscv-boom/riscv-boom), [XiangShan](https://github.com/OpenXiangShan/XiangShan).

+ To try GSIM, using
    ```
    $ make init
    $ make run dutName=core
    ```
+ Set core to `ysyx3`, `rocket`, `small-boom`, `large-boom`, `minimal-xiangshan` or `default-xiangshan`

## Usage

+ Run `make build-gsim` to build GSIM
+ Run `build/gsim/gsim $(chirrtl-file)` to compile chirrtl to C++
+ Refer to `build/gsim/gsim --help` for more information
+ See [C++ harness example](https://github.com/jaypiper/simulator/blob/master/emu/emu.cpp) to know how it interacts with the emitted C++ code.

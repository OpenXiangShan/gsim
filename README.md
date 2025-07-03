# GSIM: A Fast RTL Simulator for Large-Scale Designs

GSIM accepts chirrtl, and compiles it to C++

## Prerequisites

+ Install [GMP](https://gmplib.org/), [clang 19(+)](https://clang.llvm.org/).

## Quick Start

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

## FST Waveform Generation

GSIM supports FST waveform generation for debugging and analysis. You can control this feature via Makefile:

+ **Enable FST waveform**: Add `-DFST_WAVE` to `FST_CFLAGS` in Makefile
  ```
  FST_CFLAGS = -DFST_WAVE -I$(abspath include/libfst)
  ```

+ **Disable FST waveform**: Remove `-DFST_WAVE` from `FST_CFLAGS`
  ```
  FST_CFLAGS = -I$(abspath include/libfst)
  ```

When enabled, the generated C++ model will produce `.fst` waveform files that can be viewed with tools like GTKWave.

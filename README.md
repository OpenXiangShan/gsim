# GSIM: A Fast RTL Simulator for Large-Scale Designs

GSIM accepts chirrtl, and compiles it to C++

## Prerequisites

+ Install [GMP](https://gmplib.org/), [clang 19(+)](https://clang.llvm.org/).

## Quike Start

+ GSIM provides 4 RISC-V cores ready for simulation: [ysyx3](https://ysyx.oscc.cc/), [Rocket](https://github.com/chipsalliance/rocket-chip), [BOOM](https://github.com/riscv-boom/riscv-boom), [XiangShan](https://github.com/OpenXiangShan/XiangShan).

+ To try GSIM, using
    ```
    $ make init
    $ make run dutName=core
    $ make diff dutName=core # add V_WAVE=1 to generate waveform from verilated model
    ```
+ Set core to `ysyx3`, `rocket`, `small-boom`, `large-boom`, `minimal-xiangshan` or `default-xiangshan`

## Usage

+ Run `make build-gsim` to build GSIM
+ Run `build/gsim/gsim $(chirrtl-file)` to compile chirrtl to C++
+ Refer to `build/gsim/gsim --help` for more information
+ See [C++ harness example](https://github.com/jaypiper/simulator/blob/master/emu/emu.cpp) to know how it interacts with the emitted C++ code.


## Papers and Presentations
### GSIM: Accelerating RTL Simulation for Large-Scale Design
Lu Chen, Dingyi Zhao, Zihao Yu, Ninghui Sun, Yungang Bao

Design Automation Conference (DAC), 2025

[Paper PDF](https://github.com/jaypiper/simulator/blob/master/docs/dac-gsim.pdf) | [Slides](https://github.com/jaypiper/simulator/blob/master/docs/2025-6-26-dac.pdf) | [Chinese Slides](https://github.com/jaypiper/simulator/blob/master/docs/2025-4-9.pdf)

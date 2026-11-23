FIRRTL version 3.3.0
circuit FstWidthTest :
  module FstWidthTest :
    input clock : Clock
    input arr_1bit : UInt<1>[99]
    input arr_2bit : UInt<2>[99]
    input arr_8bit : UInt<8>[99]
    input arr_16bit : UInt<16>[99]
    input arr_128x1bit : UInt<1>[128]

    input arr_2d : UInt<16>[99][10]

    output out_1bit : UInt<1>[99]
    output out_2bit : UInt<2>[99]
    output out_8bit : UInt<8>[99]
    output out_16bit : UInt<16>[99]
    output out_128x1bit : UInt<1>[128]

    output out_2d : UInt<16>[99][10]

    connect out_1bit, arr_1bit
    connect out_2bit, arr_2bit
    connect out_8bit, arr_8bit
    connect out_16bit, arr_16bit
    connect out_128x1bit, arr_128x1bit

    connect out_2d, arr_2d

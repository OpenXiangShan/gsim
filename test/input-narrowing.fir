FIRRTL version 3.3.0
circuit InputNarrowing :
  module InputNarrowing :
    input data9 : UInt<9>
    input data64 : UInt<64>
    input data128 : UInt<128>
    input signed9 : SInt<9>
    output narrow3 : UInt<3>
    output narrow33 : UInt<33>
    output narrow65 : UInt<65>
    output equal3 : UInt<1>
    output equal33 : UInt<1>
    output equal65 : UInt<1>
    output signed_narrow : SInt<3>
    output negative : UInt<1>
    output signed_wide : SInt<64>

    wire value3 : UInt<3>
    wire value33 : UInt<33>
    wire value65 : UInt<65>
    wire signed_value : SInt<3>
    connect value3, data9
    connect value33, data64
    connect value65, data128
    connect signed_value, signed9
    connect narrow3, value3
    connect narrow33, value33
    connect narrow65, value65
    connect equal3, eq(value3, UInt<3>(7))
    connect equal33, eq(value33, UInt<33>(7))
    connect equal65, eq(value65, UInt<65>(7))
    connect signed_narrow, signed_value
    connect negative, lt(signed_value, SInt<3>(0))
    connect signed_wide, signed_value

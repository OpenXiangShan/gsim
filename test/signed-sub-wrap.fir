FIRRTL version 3.3.0
circuit SignedSubWrap :
  module SignedSubWrap :
    input a9 : SInt<9>
    input b9 : SInt<9>
    input a32 : SInt<32>
    input b32 : SInt<32>
    input a64 : SInt<64>
    input b64 : SInt<64>
    input a128 : SInt<128>
    input b128 : SInt<128>
    output result9 : SInt<9>
    output result32 : SInt<32>
    output result64 : SInt<64>
    output result128 : SInt<128>
    output less9 : UInt<1>
    output less32 : UInt<1>
    output less64 : UInt<1>
    output less128 : UInt<1>
    output decrement_less : UInt<1>
    output wide_difference : SInt<65>
    output shifted_difference : SInt<64>

    wire narrow9 : SInt<9>
    wire narrow32 : SInt<32>
    wire narrow64 : SInt<64>
    wire narrow128 : SInt<128>
    connect narrow9, sub(a9, b9)
    connect narrow32, sub(a32, b32)
    connect narrow64, sub(a64, b64)
    connect narrow128, sub(a128, b128)
    connect result9, narrow9
    connect result32, narrow32
    connect result64, narrow64
    connect result128, narrow128
    connect less9, lt(narrow9, a9)
    connect less32, lt(narrow32, a32)
    connect less64, lt(narrow64, a64)
    connect less128, lt(narrow128, a128)
    wire decrement : SInt<64>
    connect decrement, sub(a64, SInt<64>(1))
    connect decrement_less, lt(decrement, a64)
    connect wide_difference, sub(a64, b64)
    connect shifted_difference, shr(sub(a64, b64), 1)

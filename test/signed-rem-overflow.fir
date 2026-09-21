FIRRTL version 3.3.0
circuit SignedRemOverflow :
  module SignedRemOverflow :
    input a32 : SInt<32>
    input b32 : SInt<32>
    input a64 : SInt<64>
    input b64 : SInt<64>
    input a128 : SInt<128>
    input b128 : SInt<128>
    output result32 : SInt<32>
    output result64 : SInt<64>
    output result128 : SInt<128>
    output constant_divisor : SInt<64>
    output narrow : SInt<3>
    output unsigned_result : UInt<64>

    connect result32, rem(a32, b32)
    connect result64, rem(a64, b64)
    connect result128, rem(a128, b128)
    connect constant_divisor, rem(a64, SInt<64>(-1))
    connect narrow, rem(a64, b64)
    connect unsigned_result, rem(asUInt(a64), asUInt(b64))

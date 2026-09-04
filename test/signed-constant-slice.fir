FIRRTL version 3.3.0
circuit SignedConstantSlice :
  module SignedConstantSlice :
    output shifted : SInt<7>
    output selected : UInt<3>
    output padded : SInt<12>
    output padded_high : UInt<4>
    output padded_shifted : SInt<4>
    output pad_identity : SInt<8>

    connect shifted, shr(SInt<8>(-5), 1)
    connect selected, bits(SInt<8>(-5), 3, 1)
    connect padded, pad(SInt<8>(-5), 12)
    connect padded_high, bits(pad(SInt<8>(-5), 12), 11, 8)
    connect padded_shifted, shr(pad(SInt<8>(-5), 12), 8)
    connect pad_identity, pad(SInt<8>(-5), 4)

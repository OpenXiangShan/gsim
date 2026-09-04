FIRRTL version 3.3.0
circuit SignedNodeShr :
  module SignedNodeShr :
    output shifted : SInt<7>

    wire negative : SInt<8>
    connect negative, SInt<8>(-5)
    connect shifted, shr(negative, 1)

FIRRTL version 3.3.0
circuit DshrOvershift :
  module DshrOvershift :
    input unsigned_in : UInt<128>
    input signed_in : SInt<128>
    input shift : UInt<8>
    output unsigned_out : UInt<128>
    output signed_out : SInt<128>

    connect unsigned_out, dshr(unsigned_in, shift)
    connect signed_out, dshr(signed_in, shift)

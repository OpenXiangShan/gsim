FIRRTL version 3.3.0
circuit SignedRuntimeSlice :
  module SignedRuntimeSlice :
    input value : UInt<5>
    output result : UInt<6>

    node signed_value = asSInt(value)
    node low = bits(signed_value, 4, 0)
    connect result, cat(UInt<1>(0), low)

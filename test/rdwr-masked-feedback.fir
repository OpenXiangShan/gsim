FIRRTL version 3.3.0
circuit RdwrMaskedFeedback :
  module RdwrMaskedFeedback :
    input clock : Clock
    input en : UInt<1>
    input write : UInt<1>
    input initialize : UInt<1>
    input addr : UInt<2>
    input din0 : UInt<8>
    input din1 : UInt<8>
    input mask0 : UInt<1>
    input mask1 : UInt<1>
    output rdata0 : UInt<8>
    output rdata1 : UInt<8>

    smem mem : UInt<8>[2][4]
    node data0 = mux(initialize, din0, add(rdata0, UInt<8>(1)))
    node data1 = mux(initialize, din1, xor(rdata1, UInt<8>(0hA5)))
    when en :
      rdwr mport p = mem[addr], clock
      when write :
        when mask0 :
          connect p[0], data0
        when mask1 :
          connect p[1], data1
    connect rdata0, p[0]
    connect rdata1, p[1]

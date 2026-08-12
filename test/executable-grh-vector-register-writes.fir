FIRRTL version 3.3.0
circuit VectorRegWrites :
  module VectorRegWrites :
    input clock : Clock
    input reset : UInt<1>
    input areset : AsyncReset
    input c0 : UInt<1>
    input c1 : UInt<1>
    input en : UInt<1>
    input addr : UInt<2>
    input d0 : UInt<8>
    input d1 : UInt<8>
    output out0 : UInt<8>
    output out1 : UInt<8>
    output out2 : UInt<8>
    output out3 : UInt<8>
    output sout : UInt<8>
    output rout : UInt<8>
    output raout0 : UInt<8>
    output raout2 : UInt<8>
    output wout : UInt<8>


    reg v : UInt<8>[4], clock
    when c0 :
      connect v[1], d0
    else :
      when c1 :
        connect v[1], d1
    when en :
      connect v[2], d1
      connect v[addr], d0

    connect out0, v[0]
    connect out1, v[1]
    connect out2, v[2]
    connect out3, v[3]


    reg s : UInt<8>, clock
    when c0 :
      connect s, d0
    connect sout, s


    wire rinit : UInt<8>[4]
    connect rinit[0], UInt<8>(0)
    connect rinit[1], UInt<8>(17)
    connect rinit[2], UInt<8>(34)
    connect rinit[3], UInt<8>(51)
    regreset r : UInt<8>[4], clock, reset, rinit
    when en :
      connect r[addr], d1
    connect rout, r[addr]

    wire rainit : UInt<8>[4]
    connect rainit[0], UInt<8>(3)
    connect rainit[1], UInt<8>(20)
    connect rainit[2], UInt<8>(37)
    connect rainit[3], UInt<8>(54)
    regreset ra : UInt<8>[4], clock, areset, rainit
    when en :
      connect ra[addr], d1
    connect raout0, ra[0]
    connect raout2, ra[2]

    wire w : UInt<8>[4]
    connect w[0], add(d0, d1)
    connect w[1], and(d0, d1)
    connect w[2], or(d0, d1)
    connect w[3], xor(d0, d1)
    connect wout, w[addr]

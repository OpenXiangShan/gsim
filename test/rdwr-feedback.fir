FIRRTL version 3.3.0
circuit RdwrFeedback :
  module RdwrFeedback :
    input clock : Clock
    input gate : UInt<1>
    input initialize : UInt<1>
    input en : UInt<1>
    input write : UInt<1>
    input addr : UInt<2>
    input din : UInt<8>
    output plain : UInt<8>
    output old_data : UInt<8>
    output one : UInt<8>
    output gated : UInt<8>

    wire gated_clock : Clock
    connect gated_clock, asClock(and(asUInt(clock), or(gate, UInt<1>(0))))

    smem mem_plain : UInt<8>[4] undefined
    node data_plain = mux(initialize, din, add(plain, UInt<8>(1)))
    when en :
      rdwr mport port_plain = mem_plain[addr], clock
      when write :
        connect port_plain, data_plain
    connect plain, port_plain

    smem mem_old : UInt<8>[4] old
    node data_old = mux(initialize, din, add(old_data, UInt<8>(1)))
    when en :
      rdwr mport port_old = mem_old[addr], clock
      when write :
        connect port_old, data_old
    connect old_data, port_old

    smem mem_one : UInt<8>[1] undefined
    node data_one = mux(initialize, din, add(one, UInt<8>(1)))
    when en :
      rdwr mport port_one = mem_one[UInt<1>(0)], clock
      when write :
        connect port_one, data_one
    connect one, port_one

    smem mem_gated : UInt<8>[4] undefined
    node data_gated = mux(initialize, din, add(gated, UInt<8>(1)))
    when en :
      rdwr mport port_gated = mem_gated[addr], gated_clock
      when write :
        connect port_gated, data_gated
    connect gated, port_gated

FIRRTL version 3.3.0
circuit RdwrPipeline :
  module RdwrPipeline :
    input clock : Clock
    input reset : UInt<1>
    input initialize : UInt<1>
    input init_addr : UInt<2>
    input init_data : UInt<8>
    input s0_valid : UInt<1>
    input s0_addr : UInt<2>
    output s1_valid : UInt<1>
    output s1_fire : UInt<1>
    output s1_rdata : UInt<8>
    output s1_wdata : UInt<8>

    regreset pending : UInt<1>, clock, reset, UInt<1>(0)
    reg address : UInt<2>, clock
    node writeback = and(pending, not(s0_valid))
    node read_request = and(s0_valid, not(initialize))
    node fire = and(writeback, not(initialize))
    node port_addr = mux(initialize, init_addr, mux(s0_valid, s0_addr, address))
    node port_en = or(initialize, or(read_request, fire))
    node port_write = or(initialize, fire)

    connect s1_valid, pending
    connect s1_fire, fire
    connect s1_wdata, add(s1_rdata, UInt<8>(1))
    when read_request :
      connect pending, UInt<1>(1)
      connect address, s0_addr
    else :
      when fire :
        connect pending, UInt<1>(0)

    smem memory : UInt<8>[4]
    when port_en :
      rdwr mport p = memory[port_addr], clock
      when port_write :
        connect p, mux(initialize, init_data, s1_wdata)
    connect s1_rdata, p

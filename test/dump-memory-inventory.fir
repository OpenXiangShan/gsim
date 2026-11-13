FIRRTL version 6.0.0
circuit DumpMemoryInventory :
  extmodule Echo :
    input clock : Clock
    input secondaryClock : Clock
    input data : UInt<8>
    output result : UInt<8>
    defname = Echo
  module DumpMemoryInventory :
    input clock : Clock
    input addr : UInt<2>
    input data : UInt<8>
    output result : UInt<8>
    output echo : UInt<8>
    cmem mem : UInt<8>[4]
    smem delayed : UInt<8>[4]
    write mport w = mem[addr], clock
    connect w, data
    read mport r = mem[addr], clock
    write mport sw = delayed[addr], clock
    connect sw, data
    read mport sr = delayed[addr], clock
    inst ext of Echo
    connect ext.clock, clock
    connect ext.secondaryClock, clock
    connect ext.data, sr
    connect result, r
    connect echo, ext.result

FIRRTL version 6.0.0
circuit DumpSourceAggregates :
  module Child :
    input a : UInt<8>
    output z : UInt<8>
    connect z, a
  module DumpSourceAggregates :
    input clock : Clock
    input addr : UInt<2>
    input bus : {a : UInt<8>, flip b : UInt<8>, nested : {c : UInt<8>, flip d : UInt<8>}}
    output result : UInt<8>
    output memoryResult : UInt<8>
    output singletonResult : UInt<4>
    reg state : {x : UInt<8>[2], nested : {a : UInt<8>, b : UInt<8>}}, clock
    wire bundle : {a : UInt<8>[2], a_0 : UInt<8>}
    smem delayed : UInt<8>[4]
    cmem records : {data : UInt<8>, nested : {tag : UInt<4>}}[4]
    cmem singleton : {data : UInt<8>, nested : {tag : UInt<4>}}[1]
    write mport sw = singleton[UInt<1>(0)], clock
    connect sw.data, bus.a
    connect sw.nested.tag, bits(bus.a, 3, 0)
    read mport sr = singleton[UInt<1>(0)], clock
    node alias = sr
    node nestedAlias = alias.nested
    connect singletonResult, nestedAlias.tag
    write mport writer = delayed[addr], clock
    connect writer, bus.a
    read mport reader = delayed[addr], clock
    connect memoryResult, reader
    inst child of Child
    connect child.a, bus.a
    connect bus.b, bus.a
    connect bus.nested.d, bus.nested.c
    when eq(bus.a, UInt<8>(0)) :
      connect bus.b, bus.nested.c
    connect bundle.a[0], bus.a
    connect bundle.a[1], bus.nested.c
    connect bundle.a_0, bus.a
    connect state.x, bundle.a
    connect state.nested.a, bus.a
    connect state.nested.b, bus.nested.c
    connect result, xor(xor(state.x[0], bundle.a_0), child.z)

FIRRTL version 6.0.0
circuit DumpSemanticLineage :
  module Child :
    input clock : Clock
    input values : {data : UInt<8>[2]}[1]
    output result : UInt<8>
    reg state : UInt<8>[1][2], clock
    connect state[0][0], values[0].data[0]
    connect state[1][0], values[0].data[1]
    when eq(values[0].data[0], UInt<8>(0)) :
      connect state[0][0], values[0].data[1]
    connect result, xor(state[0][0], state[1][0])
  module DumpSemanticLineage :
    input clock : Clock
    input a : UInt<8>
    output result : UInt<8>
    wire bundle : {data : UInt<8>[2], data_0 : UInt<8>}
    connect bundle.data[0], a
    connect bundle.data[1], UInt<8>(7)
    connect bundle.data_0, UInt<8>(3)
    inst child of Child
    connect child.clock, clock
    connect child.values[0].data, bundle.data
    connect result, xor(child.result, bundle.data_0)
    printf(clock, UInt<1>(1), "%d", a) : trace
    assert(clock, UInt<1>(1), UInt<1>(1), "ok") : check
    stop(clock, UInt<1>(0), 0) : done

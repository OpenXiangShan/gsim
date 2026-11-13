FIRRTL version 6.0.0
circuit NodeEvolution :
  module NodeEvolution :
    input clock : Clock
    input data : UInt<8>
    input index : UInt<2>
    input bus : UInt<8>[4]
    output result : UInt<8>
    output memoryResult : UInt<8>
    wire cycle : UInt<8>[2]
    connect cycle[0], data
    connect cycle[1], xor(cycle[0], UInt<8>(1))
    wire dynamic : UInt<8>[4]
    connect dynamic[0], data
    connect dynamic[1], UInt<8>(1)
    connect dynamic[2], UInt<8>(2)
    connect dynamic[3], UInt<8>(3)
    wire copied : UInt<8>[4]
    connect copied, bus
    reg state : UInt<8>[2], clock
    connect state[0], data
    connect state[1], cycle[1]
    reg dynamicWrite : UInt<8>[4], clock
    connect dynamicWrite[index], data
    node selected = dynamic[index]
    connect result, xor(xor(xor(state[0], selected), copied[2]), dynamicWrite[1])
    wire addresses : UInt<2>[2]
    connect addresses[0], index
    connect addresses[1], UInt<2>(0)
    cmem records : {a : UInt<8>, b : UInt<8>}[4]
    read mport reader = records[addresses[0]], clock
    connect memoryResult, xor(reader.a, reader.b)

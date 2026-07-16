FIRRTL version 3.3.0
circuit EmptyMemoryWriter :
  module EmptyMemoryWriter :
    input clock : Clock
    input addr : UInt<1>
    output out : UInt<8>

    cmem mem : UInt<8>[2]
    when UInt<1>(0h0) :
      infer mport write = mem[addr], clock
      connect write, UInt<8>(0h5a)
    infer mport read = mem[addr], clock
    connect out, read

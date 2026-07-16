FIRRTL version 3.3.0
circuit SynchronousMemoryAddress :
  module SynchronousMemoryAddress :
    input clock : Clock
    input enable : UInt<1>
    input write : UInt<1>
    input addr : UInt<2>
    input data : UInt<8>
    output out : UInt<8>

    smem mem : UInt<8>[4]
    when enable :
      rdwr mport port = mem[addr], clock
      when write :
        connect port, data
    connect out, port

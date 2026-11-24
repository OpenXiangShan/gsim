FIRRTL version 3.3.0
circuit FstCustomTimestamp :
  module FstCustomTimestamp :
    input clock : Clock
    input reset : UInt<1>
    input last_dump : UInt<64>
    input didDump : UInt<1>
    output out : UInt<64>

    regreset acc : UInt<64>, clock, reset, UInt<64>(0)
    node next_acc = mux(didDump, bits(add(acc, last_dump), 63, 0), acc)
    connect acc, next_acc
    connect out, next_acc

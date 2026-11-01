FIRRTL version 6.0.0
circuit AsyncResetHold :
  public module AsyncResetHold :
    input clock : Clock
    input reset : AsyncReset
    input en : UInt<1>
    input data : UInt<8>
    output zero : UInt<8>
    output nonzero : UInt<8>

    regreset r0 : UInt<8>, clock, reset, UInt<8>(0)
    regreset r1 : UInt<8>, clock, reset, UInt<8>(165)
    when en :
      connect r0, data
      connect r1, data
    connect zero, r0
    connect nonzero, r1

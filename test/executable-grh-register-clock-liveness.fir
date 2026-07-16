FIRRTL version 3.3.0
circuit RegisterClock :
  module Child :
    input clock : Clock
    input in : UInt<1>
    output out : UInt<1>
    reg state : UInt<1>, clock
    connect state, in
    connect out, state

  module RegisterClock :
    input clock : Clock
    input in : UInt<1>
    output out : UInt<1>
    reg generated : UInt<1>, clock
    node generated_next = not(generated)
    connect generated, generated_next
    node generated_clock = asClock(generated)
    inst child of Child
    connect child.clock, generated_clock
    connect child.in, in
    connect out, child.out

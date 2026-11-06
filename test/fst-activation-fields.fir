FIRRTL version 3.3.0
circuit FstActivationFields :
  module FstActivationFields :
    input clock : Clock
    input reset : UInt<1>
    input a : UInt<64>
    input b : UInt<64>
    output first : UInt<64>
    output second : UInt<64>

    regreset state : UInt<64>, clock, reset, UInt<64>(0)
    node shared = xor(a, state)
    node cross = bits(add(shared, b), 63, 0)
    connect state, cross
    connect first, or(shared, b)
    connect second, and(cross, a)

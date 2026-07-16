FIRRTL version 3.3.0
circuit SplitRegClock :
  module SplitRegClock :
    input clock : Clock
    input in : UInt<256>
    output lo : UInt<128>
    output hi : UInt<128>

    wire clock_alias : Clock
    connect clock_alias, clock
    reg value : UInt<256>, clock_alias
    connect value, in
    connect lo, bits(value, 127, 0)
    connect hi, bits(value, 255, 128)

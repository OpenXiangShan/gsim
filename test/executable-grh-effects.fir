FIRRTL version 3.3.0
circuit ExecutableGrhEffects :
  module ExecutableGrhEffects :
    input clock : Clock
    input enable : UInt<1>
    input predicate : UInt<1>
    input wide : UInt<128>
    output passthrough : UInt<128>

    connect passthrough, wide
    printf(clock, UInt<1>(0h0), "dead=%d\n", wide) : dead_print
    printf(clock, enable, "wide=%d\n", wide) : wide_print
    assert(clock, predicate, enable, "fatal d=%d x=%x\n") : formatted_assert

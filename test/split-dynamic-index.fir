FIRRTL version 3.3.0
circuit SplitDynamicIndex :
  module SplitDynamicIndex :
    input index : UInt<2>
    input data : UInt<8>
    output result : UInt<8>
    wire values : UInt<8>[3]
    connect values[0], data
    connect values[1], values[0]
    connect values[2], values[1]
    connect values[index], UInt<8>(0h99)
    connect result, values[2]

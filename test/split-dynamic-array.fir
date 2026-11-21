FIRRTL version 3.3.0
circuit SplitDynamicArray :
  module SplitDynamicArray :
    input row : UInt<2>
    input column : UInt<2>
    input row_enable : UInt<1>
    input cell_enable : UInt<1>
    input fixed_enable : UInt<1>
    input data : UInt<8>
    output element_0_0 : UInt<8>
    output element_0_1 : UInt<8>
    output element_1_0 : UInt<8>
    output element_1_1 : UInt<8>
    wire matrix : UInt<8>[2][2]
    connect matrix[0][0], data
    connect matrix[0][1], matrix[0][0]
    connect matrix[1][0], matrix[0][1]
    connect matrix[1][1], matrix[1][0]
    wire replacement : UInt<8>[2]
    connect replacement[0], UInt<8>(0h21)
    connect replacement[1], UInt<8>(0h32)
    when row_enable :
      connect matrix[row], replacement
    when cell_enable :
      connect matrix[row][column], UInt<8>(0h55)
    when fixed_enable :
      connect matrix[1][column], UInt<8>(0h77)
    connect element_0_0, matrix[0][0]
    connect element_0_1, matrix[0][1]
    connect element_1_0, matrix[1][0]
    connect element_1_1, matrix[1][1]

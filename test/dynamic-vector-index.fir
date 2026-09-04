FIRRTL version 3.3.0
circuit DynamicVectorIndex :
  module DynamicVectorIndex :
    input read_index : UInt<4>
    input write_index : UInt<4>
    input write_enable : UInt<1>
    output selected : UInt<8>
    output element_0 : UInt<8>
    output element_1 : UInt<8>
    output element_2 : UInt<8>
    output copied_0 : UInt<8>
    output copied_1 : UInt<8>
    output matrix_0_0 : UInt<8>
    output matrix_0_1 : UInt<8>
    output matrix_1_0 : UInt<8>
    output matrix_1_1 : UInt<8>

    wire values : UInt<8>[3]
    connect values[0], UInt<8>(0h11)
    connect values[1], UInt<8>(0h22)
    connect values[2], UInt<8>(0h33)
    when write_enable :
      connect values[write_index], UInt<8>(0h99)

    wire matrix : UInt<8>[2][2]
    connect matrix[0][0], UInt<8>(0h40)
    connect matrix[0][1], UInt<8>(0h41)
    connect matrix[1][0], UInt<8>(0h50)
    connect matrix[1][1], UInt<8>(0h51)
    wire row : UInt<8>[2]
    connect row[0], UInt<8>(0h61)
    connect row[1], UInt<8>(0h62)
    when write_enable :
      connect matrix[write_index], row
    wire copied : UInt<8>[2]
    connect copied, matrix[read_index]

    connect selected, values[read_index]
    connect element_0, values[0]
    connect element_1, values[1]
    connect element_2, values[2]
    connect copied_0, copied[0]
    connect copied_1, copied[1]
    connect matrix_0_0, matrix[0][0]
    connect matrix_0_1, matrix[0][1]
    connect matrix_1_0, matrix[1][0]
    connect matrix_1_1, matrix[1][1]

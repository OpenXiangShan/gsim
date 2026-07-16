FIRRTL version 3.3.0
circuit NodeFinalAssignElision :
  module NodeFinalAssignElision :
    input a : UInt<8>
    input b : UInt<8>
    output sum : UInt<9>
    output widened : UInt<9>
    output constant : UInt<8>

    node add_result = add(a, b)
    connect sum, add_result
    connect widened, a
    connect constant, UInt<8>(0h5a)

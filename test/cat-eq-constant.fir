FIRRTL version 3.3.0
circuit CatEqConstant :
  module CatEqConstant :
    input lhs : UInt<4>
    input rhs : UInt<4>
    output out : UInt<1>
    output inverted : UInt<1>

    wire matched : UInt<1>
    connect matched, eq(cat(lhs, rhs), UInt<8>(0hab))
    connect out, matched
    connect inverted, not(matched)

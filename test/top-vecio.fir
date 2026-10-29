FIRRTL version 3.3.0
circuit TopVecIO :
  module TopVecIO :
    input a : UInt<8>[2]
    output y : UInt<8>[2]
    connect y[0], a[0]
    connect y[1], a[1]

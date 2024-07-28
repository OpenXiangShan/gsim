FIRRTL version 3.3.0
circuit GCD :
  module GCD : @[src/main/GCD.scala 10:7]
    input clock : Clock @[src/main/GCD.scala 10:7]
    input reset : UInt<1> @[src/main/GCD.scala 10:7]
    output io : { flip value1 : UInt<16>, flip value2 : UInt<16>, flip loadingValues : UInt<1>, outputGCD : UInt<16>, outputValid : UInt<1>} @[src/main/GCD.scala 11:14]

    reg x : UInt, clock @[src/main/GCD.scala 19:14]
    reg y : UInt, clock @[src/main/GCD.scala 20:14]
    node _T = gt(x, y) @[src/main/GCD.scala 22:10]
    when _T : @[src/main/GCD.scala 22:15]
      node _x_T = sub(x, y) @[src/main/GCD.scala 22:24]
      node _x_T_1 = tail(_x_T, 1) @[src/main/GCD.scala 22:24]
      connect x, _x_T_1 @[src/main/GCD.scala 22:19]
    else :
      node _y_T = sub(y, x) @[src/main/GCD.scala 22:49]
      node _y_T_1 = tail(_y_T, 1) @[src/main/GCD.scala 22:49]
      connect y, _y_T_1 @[src/main/GCD.scala 22:44]
    when io.loadingValues : @[src/main/GCD.scala 24:26]
      connect x, io.value1 @[src/main/GCD.scala 25:7]
      connect y, io.value2 @[src/main/GCD.scala 26:7]
    connect io.outputGCD, x @[src/main/GCD.scala 29:18]
    node _io_outputValid_T = eq(y, UInt<1>(0h0)) @[src/main/GCD.scala 30:23]
    connect io.outputValid, _io_outputValid_T @[src/main/GCD.scala 30:18]

FIRRTL version 6.0.0
circuit DumpExpressionInventory :
  module DumpExpressionInventory :
    input clock : Clock
    input reset : UInt<1>
    input enable : UInt<1>
    input addr : UInt<2>
    input data : UInt<8>
    output result : UInt<8>
    output leftResult : UInt<8>
    output rightResult : UInt<8>
    output invalidResult : UInt<8>
    wire left : UInt<8>
    wire right : UInt<8>
    wire invalidValue : UInt<8>
    regreset state : UInt<8>, clock, reset, UInt<8>(7)
    smem delayed : UInt<8>[4]
    write mport writer = delayed[addr], clock
    connect writer, data
    when enable :
      connect left, data
    else :
      connect right, data
    invalidate invalidValue
    when enable :
      read mport reader = delayed[addr], clock
      connect state, reader
    connect result, state
    connect leftResult, left
    connect rightResult, right
    connect invalidResult, invalidValue

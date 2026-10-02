FIRRTL version 3.3.0
circuit MtSparseRegisterReset :
  module MtSparseRegisterReset :
    input clock : Clock
    input reset : UInt<1>
    input io_write_a : UInt<1>
    input io_write_b : UInt<1>
    input io_index : UInt<2>
    input io_data_a : UInt<8>
    input io_data_b : UInt<8>
    output io_value : UInt<8>

    wire init : UInt<8>[4]
    connect init[0], UInt<8>(0h11)
    connect init[1], UInt<8>(0h22)
    connect init[2], UInt<8>(0h33)
    connect init[3], UInt<8>(0h44)
    regreset entries : UInt<8>[4], clock, reset, init
    when io_write_a :
      connect entries[io_index], io_data_a
    when io_write_b :
      connect entries[io_index], io_data_b
    connect io_value, entries[io_index]

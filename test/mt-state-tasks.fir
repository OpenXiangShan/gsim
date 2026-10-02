FIRRTL version 3.3.0
circuit MtStateTasks :
  module MtStateTasks :
    input clock : Clock
    input reset : UInt<1>
    input async_reset : AsyncReset
    input reset_request : UInt<1>
    input enable : UInt<1>
    input a : UInt<8>
    input b : UInt<8>
    output value0 : UInt<8>
    output value1 : UInt<8>
    output value_async : UInt<8>
    output value_reg_reset : UInt<8>
    output value_late_reset : UInt<8>
    output mix : UInt<8>
    output reset_value : UInt<1>

    regreset q0 : UInt<8>, clock, reset, UInt<8>(0h12)
    regreset q1 : UInt<8>, clock, reset, UInt<8>(0h34)
    regreset qa : UInt<8>, clock, async_reset, UInt<8>(0h56)
    regreset qr : UInt<1>, clock, reset, UInt<1>(0)
    regreset qb : UInt<8>, clock, qr, UInt<8>(0h78)
    node late_reset = asAsyncReset(xor(reset_request, enable))
    regreset ql : UInt<8>, clock, late_reset, UInt<8>(0h9a)
    node n0 = bits(add(q1, a), 7, 0)
    node n1 = xor(q0, b)
    node na = bits(add(qa, q0), 7, 0)
    node nb = bits(add(qb, b), 7, 0)
    when enable :
      connect q0, n0
    connect q1, n1
    connect qa, na
    connect qb, nb
    connect qr, reset_request
    connect ql, bits(add(ql, a), 7, 0)
    connect value0, q0
    connect value1, q1
    connect value_async, qa
    connect value_reg_reset, qb
    connect value_late_reset, ql
    connect reset_value, qr
    connect mix, xor(xor(q0, q1), xor(qa, qb))

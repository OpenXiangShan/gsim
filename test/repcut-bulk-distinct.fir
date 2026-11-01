FIRRTL version 3.3.0
circuit RcBulkCommit :
  module RcBulkCommit :
    input clock : Clock
    input inc : UInt<64>
    output sum : UInt<64>
    output tap1 : UInt<64>
    output tap2 : UInt<64>
    output tap3 : UInt<64>
    reg r0 : UInt<64>, clock
    reg r1 : UInt<64>, clock
    reg r2 : UInt<64>, clock
    reg r3 : UInt<64>, clock
    reg r4 : UInt<64>, clock
    reg r5 : UInt<64>, clock
    reg r6 : UInt<64>, clock
    reg r7 : UInt<64>, clock
    reg r8 : UInt<64>, clock
    reg r9 : UInt<64>, clock
    reg r10 : UInt<64>, clock
    reg r11 : UInt<64>, clock
    reg r12 : UInt<64>, clock
    reg r13 : UInt<64>, clock
    reg r14 : UInt<64>, clock
    reg r15 : UInt<64>, clock
    connect r0, inc
    connect r1, inc
    connect r2, inc
    connect r3, inc
    connect r4, inc
    connect r5, inc
    connect r6, inc
    connect r7, inc
    connect r8, inc
    connect r9, inc
    connect r10, inc
    connect r11, inc
    connect r12, inc
    connect r13, inc
    connect r14, inc
    connect r15, inc
    node s0 = add(r0, r1)
    node s1 = add(s0, r2)
    node s2 = add(s1, r3)
    node s3 = add(s2, r4)
    node s4 = add(s3, r5)
    node s5 = add(s4, r6)
    node s6 = add(s5, r7)
    node s7 = add(s6, r8)
    node s8 = add(s7, r9)
    node s9 = add(s8, r10)
    node s10 = add(s9, r11)
    node s11 = add(s10, r12)
    node s12 = add(s11, r13)
    node s13 = add(s12, r14)
    connect sum, add(s13, r15)
    connect tap1, xor(r1, UInt<64>(0h55))
    connect tap2, xor(r7, UInt<64>(0h33))
    connect tap3, xor(r11, UInt<64>(0h77))

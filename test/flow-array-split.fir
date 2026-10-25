FIRRTL version 3.3.0
circuit FlowArraySplit :
  module FlowArraySplit :
    input clock : Clock
    input idx : UInt<2>
    input count : UInt<1>
    input bankIn : UInt<8>
    input b00 : UInt<8>
    input b01 : UInt<8>
    input b10 : UInt<8>
    input b11 : UInt<8>
    output glued : UInt<8>
    output lane0 : UInt<8>
    output lane1 : UInt<8>
    output spill : UInt<8>
    output ob0 : UInt<8>
    output ob1 : UInt<8>
    output og0 : UInt<8>
    output og1 : UInt<8>
    output os0 : UInt<8>
    output os1 : UInt<8>
    output obulk0 : UInt<8>
    output obulk1 : UInt<8>
    output pack0 : UInt<8>
    output pack1 : UInt<8>
    output hit0 : UInt<8>
    output hit1 : UInt<8>

    reg gbits : UInt<8>[4], clock
    connect gbits[0], UInt<8>(0h1)
    connect gbits[1], UInt<8>(0h2)
    connect gbits[2], UInt<8>(0h3)
    connect gbits[3], UInt<8>(0h4)
    connect glued, gbits[idx]

    wire table : UInt<8>[4]
    wire seq0 : UInt<8>[2]
    wire seq1 : UInt<8>[2]
    reg r0 : UInt<8>, clock
    reg r1 : UInt<8>, clock
    reg spillReg : UInt<8>, clock

    connect table[0], UInt<8>(0h1)
    connect table[1], UInt<8>(0h2)
    connect table[2], UInt<8>(0h3)
    connect table[3], UInt<8>(0h4)
    node t0 = table[0]
    node t1 = table[1]
    node t2 = table[2]
    node t3 = table[3]
    connect seq0[0], t0
    connect seq0[1], t2
    connect seq1[0], t1
    connect seq1[1], t3
    connect r0, seq0[count]
    connect r1, seq1[count]
    connect spillReg, table[idx]
    connect lane0, seq0[count]
    connect lane1, seq1[count]
    connect spill, table[idx]

    reg bank : UInt<8>[2], clock
    reg bank0 : UInt<8>, clock
    reg bank1 : UInt<8>, clock
    connect bank[0], bankIn
    connect bank[1], bankIn
    connect bank0, bank[0]
    connect bank1, bank[1]
    connect ob0, bank[0]
    connect ob1, bank[1]

    wire grid : UInt<8>[2][2]
    reg grow0 : UInt<8>, clock
    reg grow1 : UInt<8>, clock
    connect grid[0][0], UInt<8>(0h1)
    connect grid[0][1], UInt<8>(0h2)
    connect grid[1][0], UInt<8>(0h3)
    connect grid[1][1], UInt<8>(0h4)
    connect grow0, grid[0][count]
    connect grow1, grid[1][count]
    connect og0, grid[0][count]
    connect og1, grid[1][count]

    reg slot : UInt<8>[2], clock
    reg s0 : UInt<8>, clock
    reg s1 : UInt<8>, clock
    connect slot[count], bankIn
    connect s0, slot[0]
    connect s1, slot[1]
    connect os0, slot[0]
    connect os1, slot[1]

    wire bulkLane : UInt<8>[2]
    wire bulkSrc : UInt<8>[2]
    wire bulkDst : UInt<8>[2]
    reg br0 : UInt<8>, clock
    reg br1 : UInt<8>, clock
    connect bulkLane[0], UInt<8>(0h5)
    connect bulkLane[1], UInt<8>(0h6)
    connect bulkSrc[0], UInt<8>(0h5)
    connect bulkSrc[1], UInt<8>(0h6)
    connect bulkSrc[idx], bulkLane[idx]
    connect bulkDst, bulkSrc
    connect br0, bulkDst[0]
    connect br1, bulkDst[1]
    connect obulk0, br0
    connect obulk1, br1

    wire bundles : UInt<8>[2][2]
    wire picked : UInt<8>[2]
    reg pl0 : UInt<8>, clock
    reg pl1 : UInt<8>, clock
    connect bundles[0][0], b00
    connect bundles[0][1], b01
    connect bundles[1][0], b10
    connect bundles[1][1], b11
    connect picked, bundles[count]
    connect pl0, picked[0]
    connect pl1, picked[1]
    connect pack0, pl0
    connect pack1, pl1

    reg acc0 : UInt<8>, clock
    reg acc1 : UInt<8>, clock
    node acc0Next = tail(add(acc0, picked[0]), 1)
    node acc1Next = tail(add(acc1, picked[1]), 1)
    connect acc0, acc0Next
    connect acc1, acc1Next
    connect hit0, acc0
    connect hit1, acc1

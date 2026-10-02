FIRRTL version 3.3.0
circuit MtMemoryCommit :
  module MtMemoryCommit :
    input clock : Clock
    input reset : UInt<1>
    input wen : UInt<1>
    input waddr : UInt<2>
    input raddr : UInt<2>
    input wdata : UInt<8>
    input wmask : UInt<2>
    input rw_en : UInt<1>
    input rw_wmode : UInt<1>
    input rw_addr : UInt<2>
    input rw_data : UInt<8>
    output comb_out : UInt<8>
    output sync_out_0 : UInt<8>
    output sync_out_1 : UInt<8>
    output rw_out : UInt<8>
    output block_out_0 : UInt<8>
    output block_out_1 : UInt<8>
    output multi_out : UInt<8>

    cmem comb_mem : UInt<8> [4]
    read mport comb_r = comb_mem[raddr], clock
    when wen :
      write mport comb_w = comb_mem[waddr], clock
      connect comb_w, wdata
    connect comb_out, comb_r

    smem sync_mem : UInt<8>[2] [4] new
    read mport sync_r = sync_mem[raddr], clock
    when wen :
      write mport sync_w = sync_mem[waddr], clock
      node lane_0 = add(wdata, UInt<1>(0h1))
      node lane_1 = add(wdata, UInt<2>(0h2))
      when bits(wmask, 0, 0) :
        connect sync_w[0], lane_0
      when bits(wmask, 1, 1) :
        connect sync_w[1], lane_1
    connect sync_out_0, sync_r[0]
    connect sync_out_1, sync_r[1]

    cmem block_mem : UInt<8>[2] [4]
    read mport block_r = block_mem[raddr], clock
    wire block_data : UInt<8>[2]
    connect block_data[0], wdata
    node block_data_1 = xor(wdata, UInt<8>(0h5a))
    connect block_data[1], block_data_1
    when wen :
      write mport block_w = block_mem[waddr], clock
      connect block_w, block_data
    connect block_out_0, block_r[0]
    connect block_out_1, block_r[1]

    cmem multi_mem : UInt<8> [4]
    read mport multi_r = multi_mem[raddr], clock
    when wen :
      write mport multi_w0 = multi_mem[waddr], clock
      connect multi_w0, wdata
    node multi_w1_en_0 = not(wen)
    node multi_w1_en = and(rw_en, multi_w1_en_0)
    when multi_w1_en :
      write mport multi_w1 = multi_mem[waddr], clock
      connect multi_w1, rw_data
    connect multi_out, multi_r

    smem rw_mem : UInt<8> [4] new
    when rw_en :
      rdwr mport rw = rw_mem[rw_addr], clock
      when rw_wmode :
        connect rw, rw_data
    connect rw_out, rw

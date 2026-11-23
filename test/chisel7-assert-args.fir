FIRRTL version 7.0.0
circuit Chisel7AssertArgs :
  layer Verification, bind, "verification" :
    layer Assert, bind, "verification/assert" :

  public module Chisel7AssertArgs :
    input clock : Clock
    input enFmt : UInt<1>
    input enWhen : UInt<1>
    input enLegacy : UInt<1>
    input enNoFmt : UInt<1>
    input pred : UInt<1>
    input u8 : UInt<8>
    input u4 : UInt<4>
    input u64 : UInt<64>
    input ch : UInt<8>
    output out : UInt<8>

    connect out, u8
    intrinsic(circt_chisel_ifelsefatal<format = "FMT u8=%d u64=0x%x bin=%b ch=%c c64=%d esc=100%%", label = "chisel3_builtin">, clock, pred, enFmt, u8, u64, u4, ch, UInt<64>(0h7))
    when enWhen :
      layerblock Verification :
        layerblock Assert :
          intrinsic(circt_chisel_ifelsefatal<format = "WHEN u4=%b u8=%d", label = "chisel3_builtin">, clock, pred, UInt<1>(0h1), u4, u8)
    intrinsic(circt_chisel_ifelsefatal, clock, pred, enNoFmt)
    assert(clock, pred, enLegacy, "LEGACY_TOP") : legacy_top
    when enLegacy :
      assert(clock, pred, UInt<1>(0h1), "LEGACY_WHEN") : legacy_when
    intrinsic(circt_chisel_ifelsefatal<format = "NEVER_FIRE %d", label = "chisel3_builtin">, clock, UInt<1>(0h0), neq(u8, u8), u8)

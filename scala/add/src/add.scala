import chisel3._
import chisel3.util._

class _add extends Module {
  val io = IO(new Bundle {
    val a = Input(UInt(32.W))
    val b = Input(UInt(32.W))
    val result = Output(UInt(32.W))
  })
  val __sub = Module(new _sub)
  __sub.io.a := io.a
  __sub.io.b := io.b
  io.result := io.a + io.b + __sub.io.result
}

class _sub extends Module {
  val io = IO(new Bundle {
    val a = Input(UInt(32.W))
    val b = Input(UInt(32.W))
    val result = Output(UInt(32.W))
  })
  io.result := io.a - io.b
}

import chisel3._
import chisel3.util._

class add extends Module {
  val io = IO(new Bundle {
    val a = Input(UInt(32.W))
    val b = Input(UInt(32.W))
    val result = Output(UInt(32.W))
  })
  io.result := io.a + io.b
}
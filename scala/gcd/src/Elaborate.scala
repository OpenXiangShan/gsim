import chisel3._

object Elaborate extends App {
  def top = new GCD()
  val useMFC = true // use MLIR-based firrtl compiler
  val generator = Seq(chisel3.stage.ChiselGeneratorAnnotation(() => top))
  // if (useMFC) {
  //   (new circt.stage.ChiselStage).execute(args, generator :+ CIRCTTargetAnnotation(CIRCTTarget.Verilog))
  // } else {
    (new chisel3.stage.ChiselStage).execute(args, generator)
  // }
}

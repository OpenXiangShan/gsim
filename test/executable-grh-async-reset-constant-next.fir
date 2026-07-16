FIRRTL version 3.3.0
circuit AsyncResetConstantNext :
  module AsyncResetConstantNext :
    input clock : Clock
    input reset : AsyncReset
    input dynamicNext : UInt<1>
    output constantValue : UInt<1>
    output controlValue : UInt<1>

    regreset constantReg : UInt<1>, clock, reset, UInt<1>(0h0)
    regreset controlReg : UInt<1>, clock, reset, UInt<1>(0h0)
    connect constantReg, UInt<1>(0h1)
    connect controlReg, dynamicNext
    connect constantValue, constantReg
    connect controlValue, controlReg

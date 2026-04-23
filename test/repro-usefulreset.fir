FIRRTL version 3.3.0
circuit ReproUsefulReset :
  module ReproUsefulReset :
    input clock : Clock
    input reset : UInt<1>
    input io_usefulResetStart : UInt<1>
    input io_gate : UInt<1>
    output io_inFlight : UInt<1>
    output io_granted : UInt<1>
    output io_setIdx : UInt<9>

    regreset usefulResetSetIdx : UInt<9>, clock, reset, UInt<9>(0h0)
    regreset usefulResetGrantedMask : UInt<1>, clock, reset, UInt<1>(0h0)
    regreset usefulResetInFlightMask : UInt<1>, clock, reset, UInt<1>(0h0)

    node usefulResetInFlight = usefulResetInFlightMask
    node _T = eq(usefulResetInFlight, UInt<1>(0h0))
    node _T_1 = and(io_usefulResetStart, _T)
    when _T_1 :
      connect usefulResetInFlightMask, UInt<1>(0h0)
      connect usefulResetSetIdx, UInt<1>(0h0)

    node _usefulResetGranted_T = and(io_gate, UInt<1>(0h1))
    node _usefulResetGranted_T_1 = and(usefulResetInFlightMask, _usefulResetGranted_T)
    node usefulResetGranted = _usefulResetGranted_T_1
    connect usefulResetGrantedMask, usefulResetGranted

    when usefulResetInFlight :
      when usefulResetGrantedMask :
        node _T_42 = eq(usefulResetSetIdx, UInt<9>(0h1ff))
        when _T_42 :
          connect usefulResetInFlightMask, UInt<1>(0h0)
        else :
          node _usefulResetSetIdx_0_T = add(usefulResetSetIdx, UInt<1>(0h1))
          node _usefulResetSetIdx_0_T_1 = tail(_usefulResetSetIdx_0_T, 1)
          connect usefulResetSetIdx, _usefulResetSetIdx_0_T_1

    connect io_inFlight, usefulResetInFlightMask
    connect io_granted, usefulResetGrantedMask
    connect io_setIdx, usefulResetSetIdx

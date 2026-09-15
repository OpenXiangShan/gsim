FIRRTL version 3.3.0
circuit ArrayNarrowing :
  module ArrayNarrowing :
    input clock : Clock
    input reset : UInt<1>
    input initialize : UInt<1>
    input index : UInt<1>
    input column : UInt<2>
    input unsigned_0 : UInt<9>
    input unsigned_1 : UInt<9>
    input unsigned_2 : UInt<9>
    input signed_0 : SInt<9>
    input signed_1 : SInt<9>
    input signed_2 : SInt<9>
    output selected : UInt<2>
    output signed_dependency : SInt<9>
    output signed_dependency_raw : SInt<3>
    output unsigned_source : UInt<9>
    output unsigned_narrow : UInt<3>
    output unsigned_wide : UInt<9>
    output signed_source : SInt<9>
    output signed_narrow : SInt<3>
    output signed_wide : SInt<9>

    wire zeros : UInt<2>[3][2]
    connect zeros[0][0], UInt<2>(0)
    connect zeros[0][1], UInt<2>(0)
    connect zeros[0][2], UInt<2>(0)
    connect zeros[1][0], UInt<2>(0)
    connect zeros[1][1], UInt<2>(0)
    connect zeros[1][2], UInt<2>(0)
    regreset dependency : UInt<2>[3][2], clock, reset, zeros

    when initialize :
      connect dependency[0][0], UInt<2>(1)
      connect dependency[0][1], UInt<2>(1)
      connect dependency[0][2], UInt<2>(1)
      connect dependency[1][0], UInt<2>(1)
      connect dependency[1][1], UInt<2>(1)
      connect dependency[1][2], UInt<2>(1)
    else :
      when orr(dependency[0][0]) :
        wire shifted0 : UInt<3>[3]
        connect shifted0[0], shl(dependency[0][0], 1)
        connect shifted0[1], shl(dependency[0][1], 1)
        connect shifted0[2], shl(dependency[0][2], 1)
        connect dependency[0], shifted0
      when orr(dependency[1][0]) :
        wire shifted1 : UInt<3>[3]
        connect shifted1[0], shl(dependency[1][0], 1)
        connect shifted1[1], shl(dependency[1][1], 1)
        connect shifted1[2], shl(dependency[1][2], 1)
        connect dependency[1], shifted1

    connect selected, dependency[index][0]

    wire signed_zeros : SInt<3>[3][2]
    connect signed_zeros[0][0], SInt<3>(0)
    connect signed_zeros[0][1], SInt<3>(0)
    connect signed_zeros[0][2], SInt<3>(0)
    connect signed_zeros[1][0], SInt<3>(0)
    connect signed_zeros[1][1], SInt<3>(0)
    connect signed_zeros[1][2], SInt<3>(0)
    regreset signed_dependency_reg : SInt<3>[3][2], clock, reset, signed_zeros

    when initialize :
      connect signed_dependency_reg[0][0], SInt<3>(-3)
      connect signed_dependency_reg[0][1], SInt<3>(-3)
      connect signed_dependency_reg[0][2], SInt<3>(-3)
      connect signed_dependency_reg[1][0], SInt<3>(-3)
      connect signed_dependency_reg[1][1], SInt<3>(-3)
      connect signed_dependency_reg[1][2], SInt<3>(-3)
    else :
      when orr(signed_dependency_reg[0][0]) :
        wire signed_shifted0 : SInt<4>[3]
        connect signed_shifted0[0], shl(signed_dependency_reg[0][0], 1)
        connect signed_shifted0[1], shl(signed_dependency_reg[0][1], 1)
        connect signed_shifted0[2], shl(signed_dependency_reg[0][2], 1)
        connect signed_dependency_reg[0], signed_shifted0
      when orr(signed_dependency_reg[1][0]) :
        wire signed_shifted1 : SInt<4>[3]
        connect signed_shifted1[0], shl(signed_dependency_reg[1][0], 1)
        connect signed_shifted1[1], shl(signed_dependency_reg[1][1], 1)
        connect signed_shifted1[2], shl(signed_dependency_reg[1][2], 1)
        connect signed_dependency_reg[1], signed_shifted1

    wire signed_extended : SInt<9>[3][2]
    connect signed_extended, signed_dependency_reg
    connect signed_dependency, signed_extended[index][0]
    connect signed_dependency_raw, signed_dependency_reg[index][0]

    wire unsigned_sources : UInt<9>[3]
    connect unsigned_sources[0], unsigned_0
    connect unsigned_sources[1], unsigned_1
    connect unsigned_sources[2], unsigned_2
    wire unsigned_narrowed : UInt<3>[3]
    connect unsigned_narrowed, unsigned_sources
    wire unsigned_widened : UInt<9>[3]
    connect unsigned_widened, unsigned_narrowed
    connect unsigned_source, unsigned_sources[column]
    connect unsigned_narrow, unsigned_narrowed[column]
    connect unsigned_wide, unsigned_widened[column]

    wire signed_sources : SInt<9>[3]
    connect signed_sources[0], signed_0
    connect signed_sources[1], signed_1
    connect signed_sources[2], signed_2
    wire signed_narrowed : SInt<3>[3]
    connect signed_narrowed, signed_sources
    wire signed_widened : SInt<9>[3]
    connect signed_widened, signed_narrowed
    connect signed_source, signed_sources[column]
    connect signed_narrow, signed_narrowed[column]
    connect signed_wide, signed_widened[column]

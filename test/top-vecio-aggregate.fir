FIRRTL version 3.3.0
circuit TopVecAggregate :
  module TopVecAggregate :
    input a : { u : UInt<9>[3], s : SInt<9>[3], wide : UInt<65>}[2]
    input i0 : UInt<8>[2]
    input val : UInt<8>[2]
    input singleton : UInt<8>[1]
    input narrow : UInt<9>[3]
    input select : UInt<1>
    output y : { u : UInt<9>[3], s : SInt<16>[3], wide : UInt<65>}[2]
    output i1 : UInt<8>[2]
    output val_out : UInt<8>[2]
    output singleton_out : UInt<8>[1]
    output low : UInt<3>[3]
    output constant : UInt<8>[3]
    output varying : UInt<8>[3]
    output uniform : UInt<8>[3]
    output scalar : UInt<1>

    connect y, a
    connect i1, i0
    connect val_out, val
    connect singleton_out, singleton
    connect scalar, select
    connect constant[0], UInt<8>(0h12)
    connect constant[1], UInt<8>(0h34)
    connect constant[2], UInt<8>(0h56)
    connect uniform[0], UInt<8>(0h5A)
    connect uniform[1], UInt<8>(0h5A)
    connect uniform[2], UInt<8>(0h5A)
    connect varying[0], UInt<8>(0h23)
    connect varying[1], i0[1]
    connect varying[2], UInt<8>(0h67)
    connect low[0], bits(narrow[0], 2, 0)
    connect low[1], bits(narrow[1], 2, 0)
    connect low[2], bits(narrow[2], 2, 0)

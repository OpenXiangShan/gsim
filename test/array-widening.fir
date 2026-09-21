FIRRTL version 3.3.0
circuit ArrayWidening :
  module ArrayWidening :
    input index : UInt<1>
    input a : UInt<3>
    input b : UInt<3>
    input sa : SInt<3>
    input sb : SInt<3>
    output u5 : UInt<5>
    output u9 : UInt<9>
    output u64 : UInt<64>
    output u128 : UInt<128>
    output s5 : SInt<5>
    output s9 : SInt<9>
    output s64 : SInt<64>
    output s128 : SInt<128>
    output mixed : SInt<64>

    wire source : UInt<3>[2]
    connect source[0], a
    connect source[1], b
    wire signed_source : SInt<3>[2]
    connect signed_source[0], sa
    connect signed_source[1], sb
    wire mixed_source : SInt<3>[2]
    connect mixed_source[0], sa
    connect mixed_source[1], SInt<3>(-2)

    wire unsigned5 : UInt<5>[2]
    wire unsigned9 : UInt<9>[2]
    wire unsigned64 : UInt<64>[2]
    wire unsigned128 : UInt<128>[2]
    connect unsigned5, source
    connect unsigned9, source
    connect unsigned64, source
    connect unsigned128, source
    wire signed5 : SInt<5>[2]
    wire signed9 : SInt<9>[2]
    wire signed64 : SInt<64>[2]
    wire signed128 : SInt<128>[2]
    connect signed5, signed_source
    connect signed9, signed_source
    connect signed64, signed_source
    connect signed128, signed_source
    wire mixed64 : SInt<64>[2]
    connect mixed64, mixed_source

    connect u5, unsigned5[index]
    connect u9, unsigned9[index]
    connect u64, unsigned64[index]
    connect u128, unsigned128[index]
    connect s5, signed5[index]
    connect s9, signed9[index]
    connect s64, signed64[index]
    connect s128, signed128[index]
    connect mixed, mixed64[index]

FIRRTL version 7.0.0
circuit ArrayAliasIndex :
  module ArrayAliasIndex :
    input clock : Clock
    input allocValid : UInt<1>
    input allocIdx : UInt<1>
    input eventValid : UInt<1>
    input eventIdx : UInt<1>
    input eventPosition : UInt<5>
    output valid_0 : UInt<1>
    output valid_1 : UInt<1>
    output bits_0 : UInt<5>
    output bits_1 : UInt<5>
    node indexAlias = eventIdx
    node indexAlias2 = indexAlias
    reg entries : { valid : UInt<1>, bits : UInt<5>}[2], clock
    wire nextEntries : { valid : UInt<1>, bits : UInt<5>}[2]
    connect nextEntries, entries
    when allocValid :
      connect nextEntries[allocIdx].valid, UInt<1>(0)
    wire recoveredEntries : { valid : UInt<1>, bits : UInt<5>}[2]
    connect recoveredEntries, nextEntries
    wire observedEntries : { valid : UInt<1>, bits : UInt<5>}[2]
    connect observedEntries, recoveredEntries
    node notRecorded = eq(recoveredEntries[indexAlias2].valid, UInt<1>(0))
    node earlier = lt(eventPosition, recoveredEntries[indexAlias2].bits)
    node update = or(notRecorded, earlier)
    when eventValid :
      when update :
        connect observedEntries[indexAlias2].valid, UInt<1>(1)
        connect observedEntries[indexAlias2].bits, eventPosition
    connect entries, observedEntries
    connect valid_0, entries[0].valid
    connect valid_1, entries[1].valid
    connect bits_0, entries[0].bits
    connect bits_1, entries[1].bits

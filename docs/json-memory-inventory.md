# Memory and external-node JSON inventory

Graph JSON keeps executable graph nodes in `nodes` and physical memory storage
in a separate `memories` array. `memoryInventoryComplete: true` means every entry
of the current graph's memory-storage vector was exported. This does not add
storage objects to ordinary node or edge statistics. Older dumps without the
flag do not establish that a missing physical memory is absent from the graph.

Each memory has the ordinary node identity fields (`id`, `name`, `type`,
`status`, `width`, `sign`, `dimension`, `lineno`, `super`) and:

- `depth`: the number of addressable memory entries.
- `readLatency`, `writeLatency`: source memory read/write latency in cycles.
- `readUnderWrite`: the source read-under-write policy.
- `portIds`: numeric IDs of memory-port objects associated with this storage.

`width` is the width of one scalar data element. `dimension` describes any
remaining data-vector axes; `depth` is separate. For example, a scalar
`cmem mem : UInt<8>[4]` exports width 8, dimension `[]`, depth 4, read latency 0,
write latency 1. A scalar `smem` has read latency 1. Depth-one memories whose
data is also scalar can be converted to register objects and then appear in
`nodes`, without a storage record. Depth-one memories with nontrivial data
dimensions remain in `memories`.

Ordinary `NODE_EXT` records include `externalDefname`, `memberIds`, and
`clockId` (`-1` if absent). `boundaryPortIds` separately records every original
external boundary port, including all clock inputs, before graph dependency
classification. Secondary clocks can be ordinary nodes outside `memberIds`;
the complete source boundary must use `boundaryPortIds`. Port objects with a
parent include `parentId`.
These numeric links distinguish instances even when their external module
names repeat. A port ID later replaced by array splitting must be resolved
through the explicit split-array ID map; names are not unique identifiers.

Use the `TopoSort` checkpoint for the live graph immediately after
`splitArray`; the immediate `SplitArray` checkpoint can still serialize stale
pre-sort ordinary nodes. `InferAllWidth` supplies inferred widths under the
same physical IDs, but may also rewrite expressions.

`make test-memory-inventory` checks memory shape/latency and external member
links with a small FIRRTL fixture.

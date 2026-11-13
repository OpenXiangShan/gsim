# Faithful expression sidecars

`--dump-expressions=when-condition,memory-read-address` writes a separate
`<graph>_<stage>_Expressions.jsonl` file at every selected dump stage. It does
not change the existing graph JSON. `--dump-expression-ids=PATH` adds physical
seed IDs from a file with one nonnegative integer per line. These selectors
can be combined; omitting a source kind does not select the whole graph.

The first line is a `record: "header"` with schema `gsim.expressions.v1`, graph,
stage, `selectedSeedIds`, requested missing IDs, source kinds, closure mode,
verification-root count, and maximum node count. The selected IDs name the
actual physical nodes in that stage. The final line is `record: "footer"`.
Its `complete: true` means the file was fully written. `closureComplete` is
false if the node cap, a missing explicitly requested seed, or an unresolved
source owner prevented completion; `truncationReasons` explains each cause.
The default `--dump-expression-max-nodes=2000000` is a hard record cap.

Each `record: "node"` occupies one complete JSONL line and includes its physical
ID, name, type, status, width, sign, dimensions, source location and generated
origin, reset classifications, clock ID, register pair IDs, parent ID, and
ordered member IDs. Null pointers remain JSON `null`. `inventory` is `nodes`,
`memories`, or `dependency-only` for an actual referenced object outside those
current inventories. `splitMemberIds` comes directly from the splitter's
original-object map. It allows an original memory-port owner to reach its
current split members without inferring a relation from names. Physical memory
records also carry depth, read/write latency, and read-under-write behavior.

Every node exports all six original tree slots in `trees`:

- `assignTree` is the ordered vector, including null entries.
- `valTree`, `resetTree`, `resetCond`, `resetVal`, and `memTree` are each an
  actual tree or `null`.

`valTree` can still alias a tree already imported into `assignTree`; consumers
must not apply it as an additional assignment. For each nonnull tree, `root`
and `lvalue` are IDs in that tree's `nodes` array, or `null`. Each ENode has a
local `id`, original `nativeId`, text `op`, numeric `opcode`, width, sign,
clock/reset classifications, used bits, physical `nodeId` and `memoryNodeId`,
ordered integer `values`, raw `strVal`, and ordered `children`. An absent child
is a `null` entry, preserving its index. Unknown widths such as `-1`, invalid
operations, and unknown numeric opcodes are retained without normalization.
ENode sharing and cycles are represented by local IDs rather than recursion.

For example, the two partial WHEN expressions differ:

```json
{"op":"OP_WHEN","children":[0,1,null]}
{"op":"OP_WHEN","children":[0,null,1]}
```

For both `OP_WHEN` and `OP_MUX`, child zero is the condition, one is the true
branch, and two is the false branch. A WHEN hole describes an absent assignment
in that branch; the dumper does not replace it with a value. Reset enums retain
the C++ numeric values: `UNCERTAIN=0`, `ASYRESET=1`, `UINTRESET=2`, `ZERO_RESET=3`.
`asReset` is `EMPTY=0`, `NODE_ASYNC_RESET=1`, `NODE_UINT_RESET=2`,
`NODE_ALL_RESET=3`.

The exporter also scans actual root-expression reads and lvalue-index reads
in all current nodes and memories for references to selected seeds. Each
direct consumer becomes a `verificationRoot`; the lvalue's destination leaf
alone is not a value read. Separate `record: "seed-use"` lines identify
`seedId`, `consumerId`, `slot`, `treeIndex` (`-1` for scalar slots), `role`
(`root` or `lvalue-index`), and `nativeExpressionId`. These are physical ENode
references, not graph-adjacency or name-based guesses. Register verification
roots also include their actual paired endpoint.

`--dump-expression-closure=combinational` (default) follows expression
references, clock, pair, parent, member, original owner, and split-member links.
It emits full records but stops traversal at non-root register sources,
memories, inputs, external boundaries (`NODE_EXT`, `NODE_EXT_IN`, `NODE_EXT_OUT`),
and special effect nodes. These records have `closureBoundary: true`; references
beyond such boundaries can be absent. A root register source retains its reset,
clock, pair, and address-update dependencies. `none` emits verification roots
only. `transitive` follows all structural dependencies and does not assert that
external effects or state are pure functions.

Init is after frontend `clockOptimize`, so this sidecar describes that actual
stage and cannot recover a clock expression already rewritten there. Earlier
stages can have uninferred ENode widths even when the physical node type is
known. The sidecar is a faithful structural inventory; equivalence and supported
operator semantics require a separate checker.

`source-cuts` uses the same complete verification roots, then also stops at
ordinary non-generated `NODE_OTHERS` dependencies. Non-root input, state,
memory, external, and ordinary source candidates have
`boundaryReason: "source-cut-candidate"`. Other boundary reasons distinguish
input, state, memory, external, and effect traversal stops. Generated WHEN
dependencies continue to expand. A source candidate is not proven equivalent:
the consumer must independently establish its source identity and actual GRH
value, or report it unverified. To avoid dumping unused expressions at these
cuts, non-root boundary records have `treeInventoryComplete: false` and
`trees: null`. This describes omitted inventory, not an empty expression.
All full records have `treeInventoryComplete: true` and the six real slots.
Register roots, their paired endpoints, and memory-port owner records retain
their full expression slots.

`make test-expression-inventory` checks native partial WHENs, memory address
registers, reset and clock slots, exact numeric references, direct consumer
evidence, and three stage snapshots. A synthetic graph additionally checks null
trees/roots/child slots, unknown opcode payloads, and explicit cap truncation.

# Source objects and register links in graph JSON

Graph JSON exports three independent object inventories: executable graph
`nodes`, physical `memories`, and original source `sourceAggregates`.
`sourceAggregateInventoryComplete: true` means every `allAggr` entry, memory
aggregate in `memoryMap`, and their nested aggregate objects at the end
of AST-to-graph lowering was copied. Conflicting names for distinct objects fail
the capture. This metadata does not create executable
nodes or graph edges.

Each source aggregate has:

- `id`: a one-based ID in a namespace independent of physical node IDs. IDs
  follow the lexicographic order of original source map keys.
- `name`: a copy of the original `allAggr` key or the registered memory/nested
  aggregate object's name, before array splitting can rename physical nodes
  to resolve name collisions.
- `memberIds`: physical node IDs of all leaf members after frontend replacement,
  in original member order. A leaf can itself have vector dimensions.
- `originalMemberIds`: the corresponding physical IDs before frontend
  replacement. Each differing entry must have an explicit `sourceReplacements`
  record; equal entries retain the original physical identity.
- `memberFlips`: one direction flag per `memberIds` entry, from the aggregate's
  original leaf membership view.
- `nestedAggregateIds`: IDs for the non-leaf aggregate objects in the original
  `AggrParentNode::parent` collection. Despite the C++ field name, these are
  nested members, and can include multiple descendant levels.

For example, `{a: UInt<8>, nested: {flip b: UInt<8>}}` has two source aggregate
objects. The outer object's leaf list contains `a` and `nested.b`; its nested
list contains the inner aggregate ID. `memberFlips` is `[false, true]`.
CHIRRTL memory ports copy nested links through the original memory aggregate
object pointers, using the matching aggregate objects created for that port.
The links are not inferred from string prefixes.

If a leaf is later split,
use the explicit split-array map to reach its live member IDs. Neither a
renamed leaf nor a split leaf changes the frozen source aggregate record.

`sourceReplacementInventoryComplete: true` marks the complete set of frontend
depth-one-memory replacements in `sourceReplacements`. Each record has
`originalId`, `replacementId`, `reason` (`depth-one-memory-to-register`), and
the original memory's `originalName`, `originalWidth`, `originalSign`,
`originalDimension`, and `originalDepth` (one). The record is created when
`removeDummyDim` creates the replacement `NODE_REG_SRC`. IDs are one-to-one;
consumers must reject duplicate endpoints, replacement chains, or cycles and
validate the replacement against the actual physical register record.

For example, a depth-one bundle memory with physical leaf ID `10` can become
register-source ID `20`. Its aggregate exports `originalMemberIds: [10]` and
`memberIds: [20]`; the explicit replacement row records `10 -> 20` with the
old memory's type information. A later array split must then follow the split
map from ID `20` rather than from the removed memory ID `10`.

`sourceScopeInventoryComplete: true` marks the complete original module traversal
in `sourceScopes`. Each record contains `path`, `module`, `line`, `instanceLine`,
and `kind` (`module` or `external`). The top has an empty path and instance line
`-1`. Other records come from actual instance visits, including external modules;
paths are copied before graph transformations and sorted lexicographically.
`line` is the parser's existing `module->lineno` value, which records the module
end line rather than its header line. `instanceLine` records the instance
statement line. Consumers must not compare `line` to a source module header.
They identify source scopes, not runtime graph nodes.

Only `NODE_REG_SRC` and `NODE_REG_DST` export `regNextId`, `regSourceId`, and
`regDestinationId`. For a bound pair `(S,D)`, the source record has `(D,S,D)`
and the destination record has `(S,S,D)`. Missing links use `-1`; consumers must
validate both endpoint types and reciprocal links before accepting a pair.
Array-split register members publish the live pair created by the splitter.

Generated node records carry `sourceKind`, `sourceLine`, and `sourceOwnerId`
when the frontend knows an explicit origin. `when-condition` records the FIRRTL
when line and owner `-1`; `memory-read-address` records the memory-port line and
the original port's physical ID. Node duplication and array-member construction
copy this metadata. These fields establish origin only; assignment/reset
expression equivalence needs a separate check.

`make test-source-aggregate-inventory` checks nested bundles and memory ports,
depth-one aggregate-memory replacements, flips, source-name collisions, split
register links, module traversal, and generated origins at Init, TopoSort,
and InferAllWidth.

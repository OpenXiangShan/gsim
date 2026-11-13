# Native source semantic lineage

AST lowering records source identity on each `Node` before singleton removal,
clock optimization, and `splitArray`. Ordinary graph JSON declares
`semanticLineageVersion: 1`. Each ordinary node, memory, and expression-sidecar
node exports its `semantic` object directly at the current checkpoint:

- `path`: source identifier tokens, starting with instance names and ending with
  the declaration and bundle fields. The top module name is omitted.
- `scopeDepth`: the number of instance tokens in `path`.
- `role`: `value`, `register-current`, `register-next`, `predicate`,
  `memory-storage`, `effect`, or `memory-read-address`. A generated memory-address register also exports
  `endpoint: "current"` or `"next"`; its path names the actual mport.
- `shape`: original leaf vector extents, in outermost-first order, before
  singleton dimensions are removed.
- `shapeFieldDepths`: one entry per vector axis. Depth zero inserts the vector
  after the declaration; depth one inserts it after the first field, and so on.
- `indices`: one entry per original vector axis. `-1` leaves the axis open;
  nonnegative entries select an actual source member. Removed singleton axes
  select zero. Scalar nodes have empty shape, depth, and index lists.
- `line`: the parser source line, using the explicit generated-source line when
  available. Declaration identity does not depend on this line.
- `originRole: "memory-storage"` and `storageDepth`: original memory storage
  identity and depth, retained when a depth-one memory becomes a physical
  register. Its physical role remains `register-current` or `register-next`.
  Storage depth is separate from the data leaf's vector `shape`.
- `statementKind`: `printf`, `assert`, or `stop` for an `effect`.

For `values: {data: UInt<8>[2]}[1]` inside instance `child`, a physical member
selected from `values[0].data[1]` exports:

```json
{"path":["child","values","data"],"scopeDepth":1,"role":"value",
 "shape":[1,2],"shapeFieldDepths":[0,1],"indices":[0,1]}
```

A split register's current and next endpoints have the same source path and
member projection, with distinct roles. A FIRRTL `when` on absolute source line
10 inside `child` has path `["child", "when@10"]` and role `predicate`; its
physical `WHEN_COND_10` name is not used as a source declaration.
An effect on source line 25 has path `["effect@25"]`, independent of an optional
source label or a generated physical name. It anchors the effect operation,
rather than a data-value endpoint.

Paths come from native AST identifier tokens and aggregate membership, including
rebasing aggregate node aliases and CHIRRTL mports. They are never reconstructed
by splitting a physical name at configurable separators. `Node::dup` and
`arrayMemberNode` preserve them, so name collisions such as bundle field
`data_0` versus member `data[0]` remain distinguishable after physical renaming.
The split node writes its member indices using the retained original-axis map,
including singleton axes already removed from the executable representation.

`sourceAggregates` also export `semanticPath` and `semanticScopeDepth`, and
`sourceScopes` export `semanticPath`, in their existing independent inventories.
These paths distinguish aggregate and scope objects from executable leaf nodes.
The empty top scope has path `[]`.

This is a source correspondence contract. Matching metadata does not certify
expression equivalence, state update/event behavior, or four-state semantics.
In particular, clock optimization keeps the source identity even when it changes
an executable node's status or removes its active reset expression.

`make test-semantic-lineage` verifies structured paths, interleaved bundle/vector
geometry, removed singleton axes, split register endpoints, generated predicate
identity, and physical name collisions at Init, TopoSort, and InferAllWidth.
The source aggregate, memory, and expression inventory tests additionally cover
mport/address origins, depth-one-memory replacement, aggregate aliases, and
ordinary/sidecar consistency.

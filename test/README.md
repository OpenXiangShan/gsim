# Test Inputs

- Any `*.fir` file in this directory is auto-discovered by `make fir-tests` and by the GitHub CI `fir-regression` job.
- repro-usefulreset.fir: Minimized FIR reproducer for GSIM issue #106, used to guard against ConstantAnalysis hangs and OOM regressions.
- dump-memory-inventory.fir: Memory storage and external-boundary JSON inventory; validate with `make test-memory-inventory`.
- `make test-source-aggregate-inventory`: check original aggregate memberships, nested aggregate IDs, flips, scope traversal records, register-pair IDs, and generated origins across array splitting and source-name collisions.

## Native semantic lineage

`make test-semantic-lineage` checks source paths and member geometry carried
through singleton removal, renaming, and array splitting; see
[`../docs/semantic-lineage.md`](../docs/semantic-lineage.md).

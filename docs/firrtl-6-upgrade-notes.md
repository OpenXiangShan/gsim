## FIRRTL 6.0.0 Syntax Support Notes

### rocket-test failure on FIRRTL 6 verification layers
- `make rocket-test -j` now trips the FIRRTL parser: `Error at line 15563: syntax error, unexpected ID, expecting <- or '[' or '.' (unexpected token: 'Verification').` Running the command without piping shows `build/gsim/gsim` exiting with status 1 even though `make` reports success.
- The offending construct is the FIRRTL 6 verification layer block at `playground/Rocket-SimTop.fir:15563`, e.g. `layerblock Verification` → `layerblock Assert` → `intrinsic(circt_chisel_ifelsefatal...)` emitted from `Broadcast.scala`.
- Our grammar in `parser/syntax.y` only recognises the older `layer` declaration form and lacks a `layerblock` token, so the new keyword is parsed as an identifier and rejected before reaching the `intrinsic` body.
- Because the Makefile target pipes the parser output through `tee` without `set -o pipefail`, the non-zero exit code is masked and no C++ model is emitted under `build/rocket` (only `build/rocket/gsim.log` appears).
- Action: add lexer+grammar support for `layerblock … :` (mapping it to the existing layer handling or otherwise lowering it) so FIRRTL 6 verification layers parse; alternatively preprocess the FIRRTL to strip/flatten these layer blocks before invoking `gsim`.

1. If `layerblock` is not recognized, we can instead make it a no-op and continue parsing the inner contents (equivalent to `layerblock` being always enabled).
2. If an `intrinsic` is not recognized, we can check whether it is `ifelsefatal`; if so, handle it the same way as the original `assert`.
3. Multiple-operand `cat` is not recognized: for example, `node _hits_T_8 = cat(_hits_T_7, _hits_T_6, _hits_T_5, _hits_T_4, _hits_T_3, _hits_T_2, _hits_T_1, _hits_T)` will raise an error. It appears that in FIRRTL 6.7.0 `.fir` files, all `cat` operations use exactly two operands.

### Completed compatibility changes (rocket-test passing)
- Lexer and parser: added the `layerblock` keyword; added support for the `cat(...)` form and `cat` with an arbitrary number of operands; added a parse branch for `intrinsic(circt_chisel_ifelsefatal...)` that directly lowers it to a normal `assert` (using the `format` text as the message). `layerblock` is treated as a statement block that always expands unconditionally and no longer blocks its inner contents.
- AST2Graph: parses `layerblock` as a list of child statements; `when` branches are also handled recursively. Provides default names for generated `assert`/`stop` statements to avoid crashes due to duplicate signals.
- Code generation: hardened the generation logic for signed addition to avoid triggering assertions when sign metadata is missing.
- Makefile: for the `rocket-test` target, added the missing `mkdir -p` and `set -o pipefail` so that parse/generation failures propagate outward correctly.

### 当前状态
- `make rocket-test -j1` 在 FIRRTL 6.7 输入上已可正常生成 `build/rocket/*.cpp`，日志见 `build/rocket/gsim.log`。

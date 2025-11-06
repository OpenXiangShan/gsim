## FIRRTL 6.0.0 Syntax Support Notes

Date: 2025-07-04

### Summary
- Added lexer tokens and grammar productions for `layer` declarations so nested `layer` hierarchies in FIRRTL 6 circuits are parsed (currently ignored semantically, but the syntax no longer fails).
- Introduced a dedicated `cat` production to accept the FIRRTL 6 variadic concatenation syntax `cat(a, b, c, ...)` by folding it into the existing binary `cat` node representation.

### Validation
Run the following to confirm the parser handles the new constructs:
```sh
make build-gsim
build/gsim/gsim playground/ysyx_210153.fir
```
The second command should complete without syntax errors and emit the generated C++ for the test circuit.

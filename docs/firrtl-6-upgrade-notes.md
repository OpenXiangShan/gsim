## FIRRTL 6.0.0 Syntax Support Notes

Date: 2025-07-04

### Summary
- Added lexer tokens and grammar productions for `layer` declarations so nested `layer` hierarchies in FIRRTL 6 circuits are parsed (currently ignored semantically, but the syntax no longer fails).
- Introduced a dedicated `cat` production to accept the FIRRTL 6 variadic concatenation syntax `cat(a, b, c, ...)` by folding it into the existing binary `cat` node representation.
- Enhanced task sharding in `parseFIR` to recognise qualified headers such as `public module`, `public extmodule`, etc., preventing mis-splitting of large FIRRTL 6 files that use visibility modifiers. This keeps the multi-threaded parser stable for Chisel 7 output.
- Fixed `clockOptimize` to tolerate memories whose clock member remains an input node after inference, eliminating cmem `infer mport` segmentation faults seen on `playground/Foo.fir`, `playground/test.fir`, and similar traces.

### Validation
Run the following to confirm the parser handles the new constructs:
```sh
make build-gsim
build/gsim/gsim playground/ysyx_210153.fir

# cmem infer-port regression set
build/gsim/gsim playground/Foo.fir
build/gsim/gsim playground/test.fir
```
The second command should complete without syntax errors and emit the generated C++ for the test circuit.

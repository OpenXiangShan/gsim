# Test Inputs

- Any `*.fir` file in this directory is auto-discovered by `make fir-tests` and by the GitHub CI `fir-regression` job.
- A same-name `*.cpp` file is compiled with the generated model and run under
  AddressSanitizer and UndefinedBehaviorSanitizer.
- dshr-overshift.fir: Checks FIRRTL-defined dynamic right shifts at and beyond
  the operand width for both unsigned and signed values.
- cat-eq-constant.fir: Checks constant slicing in the
  `cat(lhs, rhs) == constant` optimization.
- repro-usefulreset.fir: Minimized FIR reproducer for GSIM issue #106, used to guard against ConstantAnalysis hangs and OOM regressions.
- signed-node-shr.fir: Checks constant propagation of a signed right shift
  whose operand is a node reference.

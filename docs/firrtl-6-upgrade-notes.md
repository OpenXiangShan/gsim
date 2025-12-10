## FIRRTL 6.0.0 Syntax Support Notes

### rocket-test failure on FIRRTL 6 verification layers
- `make rocket-test -j` now trips the FIRRTL parser: `Error at line 15563: syntax error, unexpected ID, expecting <- or '[' or '.' (unexpected token: 'Verification').` Running the command without piping shows `build/gsim/gsim` exiting with status 1 even though `make` reports success.
- The offending construct is the FIRRTL 6 verification layer block at `playground/Rocket-SimTop.fir:15563`, e.g. `layerblock Verification` → `layerblock Assert` → `intrinsic(circt_chisel_ifelsefatal...)` emitted from `Broadcast.scala`.
- Our grammar in `parser/syntax.y` only recognises the older `layer` declaration form and lacks a `layerblock` token, so the new keyword is parsed as an identifier and rejected before reaching the `intrinsic` body.
- Because the Makefile target pipes the parser output through `tee` without `set -o pipefail`, the non-zero exit code is masked and no C++ model is emitted under `build/rocket` (only `build/rocket/gsim.log` appears).
- Action: add lexer+grammar support for `layerblock … :` (mapping it to the existing layer handling or otherwise lowering it) so FIRRTL 6 verification layers parse; alternatively preprocess the FIRRTL to strip/flatten these layer blocks before invoking `gsim`.

1. 不识别layerblock，可以改成默认什么都不做，继续往内部看（相当于layerblock永远enable）
2. 不识别intrinsic，可以改成判断一下是不是ifelsefatal，是的话就和原来的assert一样
3. 不识别cat多个operand，比如node _hits_T_8 = cat(_hits_T_7, _hits_T_6, _hits_T_5, _hits_T_4, _hits_T_3, _hits_T_2, _hits_T_1, _hits_T)会报错。看起来6.7.0的fir里面cat都只有两个操作数

### 已完成的兼容改动（rocket-test 已通过）
- 词法与语法：新增 `layerblock` 关键字；支持 `cat(...)` 形式和任意多操作数的 `cat`；为 `intrinsic(circt_chisel_ifelsefatal...)` 建立解析分支，直接下沉为普通 `assert`（使用 format 文本作为消息）。`layerblock` 被视为无条件展开的 statement 块，不再挡住内部内容。
- AST2Graph：`layerblock` 解析为子语句列表，`when` 分支也能递归处理。为生成的 assert/stop 提供默认名，避免重复信号导致的崩溃。
- 代码生成：加固带符号加法的生成逻辑，防止缺失 sign 元数据时触发断言。
- Makefile：`rocket-test` 目标补齐 `mkdir -p` 与 `set -o pipefail`，确保解析/生成失败会向外冒泡。

### 当前状态
- `make rocket-test -j1` 在 FIRRTL 6.7 输入上已可正常生成 `build/rocket/*.cpp`，日志见 `build/rocket/gsim.log`。
<END_PATCH>***

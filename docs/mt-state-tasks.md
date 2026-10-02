# 独立的周期初状态 MTask

MT 模式将任务分为两个阶段。普通计算任务不再包含已经在周期初提交的
`NODE_REG_SRC`；每个 worker 有一个 `MtStateTask`，其
`kind == MtTaskKind::StateUpdate`、`owner == worker`。没有状态更新的 worker
也保留空状态任务，并参加状态屏障。

```text
上一周期普通任务计算 reg_dst / pending writes
                    ↓
W0 的状态任务   W1 的状态任务   …   WN 的状态任务
                    ↓
            全 worker state barrier
                    ↓
W0 普通任务链   W1 普通任务链   …   WN 普通任务链
                    ↓
周期完成屏障 / 必要时的 async reset 与组合 replay
```

## 成员与所有权

- `MtTaskPlan::tasks_`、`taskByNode_`：只描述计算阶段。周期初提交的
  `reg_src` 在候选组初始化、合并之前就被排除，因此不影响计算阶段的合并、
  运算成本、通信节点数及 token DAG。`reg_dst` 仍保留在计算任务中。
- `MtTaskPlan::stateRegisters_`：待分配的周期初寄存器源。
- `MtWorkerPlan::stateTasks_[worker]`：每 worker 一个固定 owner 的状态任务，
  继承普通 `MtTask` 的成员、类型、owner、指令与成本字段。
- `MtWorkerPlan::stateTaskByNode_`：状态任务成员到 worker/状态任务编号的映射。
  同一状态成员只属于一个状态任务，并且不能同时属于计算任务。
- `emissionNodes_` 仍包括寄存器源：存储声明、初始化、接口、diff signature
  以及寄存器驱动 reset 的周期初快照仍需要这些节点。不能因为不在计算任务
  中就把它们从完整 Node 图或 emission 节点表删除。

普通与状态任务使用独立编号空间：`compute:N` 与 `state:W`。状态任务不参加
HEFT/list 的插入调度，也不进入普通 worker dispatch 表、token 压缩、lookahead
或异步 reset 后的组合 replay。它们在任务模型中通过“先状态任务、全体屏障、
后计算任务”的阶段关系同步，不展开成每对任务间的 token 边。

状态归属仍采用现有策略：在普通任务 owner 确定后，结合寄存器更新字节数和
`reg_dst` 生产者的 owner 分配状态工作。寄存器的提交 owner 与其消费者
所在 worker 可以不同；state barrier 保证消费者读取的是已提交的当前值。

## 哪些操作进入状态任务

状态任务保留现有执行顺序和代码生成路径：

1. 常量 next-state 的物化（若存在）。
2. 上一周期记录的稀疏寄存器写入。
3. 同步 reset（覆盖稀疏写入，保持 reset 优先级）。
4. 普通寄存器块 `memcpy(src, dst)`。
5. 上一周期记录的 memory pending write 提交。

成员包括：普通和稀疏寄存器的 `reg_src`、由该 worker 提交的 memory 存储节点，
以及实际进入此阶段的同步 reset 节点。同步 reset 的内部节点可能与普通
`reg_dst` 同名，但 Node 身份不同，不应仅按名字判定重复归属。

memory writer/readwriter 在普通任务中负责计算本周期的地址、数据和使能，
不能整体移到周期初。状态任务持有其 pending-write producer 引用，并提交
上一周期记录的值；引用不是成员。稀疏寄存器的合成 writer 也遵循同样规则。

异步 reset 的少见修复路径仍在周期末执行：检查 reset、应用 reset、串行
replay 普通组合任务。不能把这次 replay 误当作再执行一次状态任务。

边界识别复用 `mtIsCycleStartRegisterUpdate()`：节点必须有效、类型为
`NODE_REG_SRC`、已拆分 src/dst，并有有效或常量的 destination。没有满足
这个条件的节点不能未经语义分析就当成“所有 worker 都已可读”的状态源。

## 生成物与检查

不需要新增命令行选项，`--mt-mode=on` 即使用新成员划分。运行时函数仍叫
`mtUpdateStateW<worker>()`，保留既有状态屏障与分析工具的函数匹配方式。

每次生成额外输出：

- `mt-task-phases.csv`：每个状态/普通任务的阶段、编号、worker、函数名、
  成员数、`reg_src` 数、指令数、静态成本、状态字节数和屏障关系。状态任务
  数恰好等于生成时的 worker 数。成本为相对静态分数，不是测得的时间；
  特别是批量 memcpy 的耗时不能只用该指令分数衡量，应结合字节数。
- `mt-state-task-members.csv`：状态成员及 pending-write producer 引用。
  `role=member` 才是状态成员，`node_id` 用于区分同名内部节点。

日志中的 `mtasks=` 仍指普通计算任务数量，`state-mtasks=` 单独报告状态任务数。
新的划分可能改变普通任务边界、ID、owner 和 token 数，旧版 task ID 不能
直接用于新生成物。

重新生成并编译仿真器后即可使用新的状态任务划分。

## 回归验证

`python3 test/test_mt_state_tasks.py` 包含任务成员归属检查，以及
HEFT/list、1/4 worker、lookahead 0/16 下的逐周期状态参考模型。
`test/mt-state-tasks.cpp` 覆盖寄存器交叉读取、enable、同步/异步 reset，
以及寄存器驱动的 reset 快照、组合逻辑产生的迟到异步 reset/replay。
该回归同时检查 memory commit 和稀疏寄存器数组提交与复位语义。

本次验证记录（2026-10-01）：

- 7 项生成/运行回归、2 项时间线工具回归通过；6 个小设计与修改前生成器
  在 lookahead 0/16 下运行签名一致。状态任务及 memory commit 用例的
  ASan/UBSan 检查通过。
- 使用用户的 `../XiangShan/build/rtl/SimTop.fir`、32 worker、target-tasks=8000、
  lookahead=0、关闭复制优化完成完整生成。153,554 个寄存器源全部归入
  32 个状态任务，5,620 个普通计算任务中的 `reg_src` 成员数为 0。
- 状态成员另有 36,274 个同步 reset 节点、2,404 个 memory；引用
  2,444 个 memory writer 和 20 个 sparse writer。
- 与原 `build/xiangshan-heft` 生成物相比，普通任务数 5,681 → 5,620，
  token 组/等待表项 8,347 → 7,860。此次未测量香山仿真速度，不能将
  token 减少直接等同于性能提升。

# GSIM 多线程 C++ Emitter 设计与使用

本文介绍 MT 后端三个阶段的设计、生成流程、运行时协议、正确性约束、测试方法和性能调优方式。阅读前建议先了解 GSIM 的基本流水线：CHIRRTL 经 parser 和 `AST2Graph` 转为节点图，完成图优化、`graphCoarsen`、`generateStmtTree` 和 `instsGenerator` 后，依次经过 MTask 划分、worker 调度，最后由 `cppEmitter-mt.cpp` 生成可编译的 C++ 模型。

本文讨论的是当前仓库中的 MT-level dispatch 实现，而不是早期实验性线程池或旧版 lookahead 实现。核心结论可以先概括为：

```text
超节点图 -> 依赖拓扑序 -> 静态 MTask -> 固定 worker chain
                                      |
                           跨 worker wait/store token
                                      |
                         每周期 dense 全量计算
```

任务归属在生成阶段确定，但每个周期仍要遍历 dispatch 表、检查 token、执行函数、发布 token，并等待其他 worker 完成。因此这是“静态分配 + 运行时同步”，不是零调度开销的完全静态展开。

## 1. 目标与非目标

多线程 emitter 的目标是把完成优化的超节点图静态划分给固定数量的 worker，并在每个仿真周期并行计算整个电路。

它采用 dense 执行语义：

- 每个有效超节点在每个周期都执行；
- 不根据节点值是否变化决定是否运行；
- 不进行动态任务窃取；
- 不在 worker 内乱序执行任务；
- 线程数、任务划分和同步表在生成模型时确定。

因此，它更接近 Verilator 的“全量计算”策略，而不是原单线程 emitter 的 ESSENT 风格活跃度执行。MT 模型不生成 `activeFlags` 或活跃度执行路径。

## 2. 源码边界

关键文件如下：

| 文件 | 职责 |
| --- | --- |
| `src/mtTaskPartition.cpp` | 从 SuperNode 依赖图建立、拓扑排序并合并 MTask |
| `include/mtTaskPartition.h` | MTask 结构和 partition builder 接口 |
| `src/mtTaskSchedule.cpp` | worker owner 分配、token 表和同步/异步 reset 规划 |
| `include/mtTaskSchedule.h` | worker、token、reset 计划结构和 builder 接口 |
| `src/cppEmitter-mt.cpp` | 消费 task/worker 计划，生成 MT C++ 类、dispatch 表和线程池 |
| `include/cppEmitter-mt.h` | `CppEmitterMt` 输出接口及三个阶段之间的组合边界 |
| `src/cppEmitter.cpp` | 原单线程 emitter；MT 实现不修改它 |
| `include/Node.h` | `Node`、`SuperNode`、`InstInfo` |
| `include/graph.h` | `sortedSuper`、`allReset` 和 emitter 所需私有输出接口 |
| `src/main.cpp` | `--mt-mode` 命令行开关 |
| `Makefile` | 构建同时包含 single 和 MT emitter 的统一生成器 |

`Makefile` 只生成一份编译器：

- `make build-gsim` 生成 `build/gsim/gsim`，同时链接 `cppEmitter.cpp` 和
  `cppEmitter-mt.cpp`；
- 默认或 `--mt-mode=off` 调用 `graph::cppEmitter()`；
- `--mt-mode=on` 调用 `graph::cppEmitterMt()`。

`cppEmitter-mt.cpp` 保留了一份稳定 lowering 的独立副本，而不是调用单线程 emitter。
两种 emitter 可以分别演进；MT 使用带 `Mt` 后缀的 `graph` 输出接口，因此统一链接时不会与单线程实现产生重复符号。

## 3. 从电路图到 MTask

### 3.1 输入数据

MT planner 不向 `Node` 或 `SuperNode` 增加并行专用字段，而是读取已有图信息：

- `graph::sortedSuper`：完成图划分后的规范计算顺序；
- `SuperNode::cppId`：对需要生成代码的超节点连续编号；
- `SuperNode::prev/next`：直接数据依赖；
- `SuperNode::depPrev/depNext`：非直接但必须保持的顺序依赖；
- `Node::nextActiveId`：节点值变化时需要激活的后继超节点；dense planner 用其中的正向关系补充同周期顺序；
- `SuperNode::member`：超节点包含的 RTL 节点；
- `SuperNode::insts`：计算该超节点的 `InstInfo` 指令流；
- `graph::allReset`：同步和异步复位超节点。

整体转换关系如下：

```text
CHIRRTL
   |
parser -> AST2Graph -> 图优化 -> graphPartition
                                      |
                              graph::sortedSuper
                                      |
                           连续合并为 MTask
                                      |
                      静态分配到 worker chain
                                      |
                         生成 wait/store token
                                      |
                       C++ 固定线程池运行时
```

### 3.2 规范顺序

`cppId` 按 `sortedSuper` 顺序分配，但 dense planner 不把编号当作计算顺序。它从
`next/depNext` 构建数据依赖图，再用 Kahn 算法得到依赖拓扑序。`prev/depPrev` 是严格对称的反向索引，不重复扫描。

随后加入 `nextActiveId` 中拓扑序向前的边。向后的激活表示下一次活跃度扫描的影响，不能作为当前周期依赖，否则寄存器反馈会制造伪环。`nextNeedActivate` 是 active executor 为排除 always-active 目标维护的过滤集合，dense planner 不需要它。所有最终边都必须在依赖拓扑序中向前。

assert/printf 只使用图中已有的真实数据依赖。不能把所有较早 `cppId` 连接到观察节点；这会把观察节点变成全局 barrier，正确性依据不充分且会严重损失并行度。

### 3.3 SuperNode 合并为 MTask

MTask 是多线程运行时的最小 dispatch 单位。一个 MTask 可以包含多个 SuperNode；这些 SuperNode 被写入同一个 `mtTaskN()` 函数并严格顺序执行，因此它们之间不需要线程同步。当前实现采用“拓扑序上的连续、按代价贪心分组”算法，代码位于 `CppEmitterMt::prepare()`。

#### 代价估计

每个 SuperNode 的静态成本定义为：

```text
superCost = max(1, insts.size() + member.size())
```

`insts.size()` 近似生成的计算语句数量，`member.size()` 近似局部声明和节点维护成本。该成本只用于生成阶段的启发式规划，不是运行时间测量结果。

planner 首先累加全部 SuperNode 的成本，再根据期望 MTask 数计算目标成本：

```text
totalCost = sum(superCost)
targetCost = max(1, ceil(totalCost / maxTasks))
```

其中 `maxTasks` 来自 `GSIM_MT_DENSE_VCONTRACT_MAXMT`。

#### 连续贪心分组

planner 按 `topologicalCppIds_` 顺序扫描 SuperNode，维护当前 MTask 的 `cppIds` 和累计 `cost`：

```text
for super in topological order:
    if current 非空且 current.cost + super.cost > targetCost:
        输出 current
        新建空 MTask

    current.cppIds.push(super.cppId)
    current.cost += super.cost

输出最后一个 current
```

因此只会合并依赖拓扑序上连续的 SuperNode。单个 SuperNode 若已经超过 `targetCost`，不会被拆分，而是独自构成一个超出目标成本的 MTask。

例如：

```text
SuperNode cost:  2  1  3  1  2
targetCost:      3

MTask0 = [SN0, SN1]  cost=3
MTask1 = [SN2]       cost=3
MTask2 = [SN3, SN4]  cost=3
```

#### 收缩依赖图

分组完成后，planner 建立 `taskByCppId[superCppId] = taskId`，将每条 SuperNode 依赖边 `A -> B` 映射为 MTask 依赖边：

```text
taskByCppId[A] -> taskByCppId[B]
```

若 A 和 B 位于同一个 MTask，边被删除，因为函数内部顺序已经保证依赖；若位于不同 MTask，则将边加入 `predecessors/successors`，并通过 `std::set` 去重。由于每个 MTask 都是拓扑序上的连续区间，所有跨 MTask 边都满足 `fromTask < toTask`，代码使用断言检查这一条件。

#### 生成代码

每个 MTask 最终展开成一个函数：

```cpp
void STop::mtTaskN() {
  // 按 cppIds 顺序生成第一个 SuperNode
  // 按 cppIds 顺序生成第二个 SuperNode
  // ...
}
```

之后的 list scheduler 把整个 MTask 作为不可拆分单位分配给一个 worker。worker 归属不会影响前面的合并过程。

#### Task 数据结构

`CppEmitterMt::Task` 保存：

| 字段 | 含义 |
| --- | --- |
| `cppIds` | 当前 MTask 包含的连续依赖拓扑序片段 |
| `predecessors/successors` | 收缩后的 MTask 图前驱和后继 |
| `owner` | list scheduler 分配的固定 worker 编号 |
| `cost` | 所含 SuperNode 的累计估算成本 |
| `waits/stores` | 跨 worker token 槽 |
| `waitBegin/.../storeEnd` | 生成扁平静态数组时使用的区间 |

`maxTasks` 是软目标而不是硬上限。不可拆分的高成本 SuperNode 和连续贪心分组产生的碎片都可能令实际 MTask 数超过目标，此时生成器只输出警告。增大 MTask 可以减少函数调用、dispatch 和 token 开销，但会降低可用并行度并加重负载不均；减小 MTask 则提供更多并行机会，同时增加调度与同步成本。当前算法没有直接考虑跨 worker 边、cache locality 或 profile 数据，这些因素由后续静态调度部分处理或留作进一步优化。

### 3.4 Worker 分配

MTask 使用依赖感知的静态 list scheduling 分配到 worker。算法只在生成 C++ 模型时执行一次，结果写入每个 worker 的固定 dispatch 数组；仿真运行时不会重新划分，也没有任务迁移或窃取。

```text
MTask DAG
   |
   +-> 计算关键路径优先级
   +-> 初始化无未调度前驱的 ready 集合
   +-> 枚举 readyTask x worker
   +-> 选择预计最早开始的组合
   +-> 更新时间并释放新的 ready task
   +-> 按调度顺序重新编号
   `-> 生成 workerTasks_[worker]
```

#### 调度状态

planner 维护以下生成期状态：

| 状态 | 含义 |
| --- | --- |
| `remainingPredecessors[task]` | 任务尚未被调度的前驱数 |
| `priority[task]` | 从任务到 DAG 末端的最长估算成本 |
| `completion[task]` | 任务的预计完成时间 |
| `scheduledOwner[task]` | 已选择的 worker |
| `workerAvailable[worker]` | worker 的预计空闲时间 |
| `scheduleOrder` | 调度器依次选出的 MTask |

`completion` 和 `workerAvailable` 是静态调度使用的虚拟时间，不是实际仿真时间。

#### 关键路径优先级

MTask 初始编号已经是拓扑序，因此可以反向计算：

```text
priority[task] = task.cost + max(priority[successor])
```

无后继任务的 `priority` 就是自身成本。该值表示任务所在下游路径的重要程度，但只在两个候选组合预计开始时间相同时用于打破平局，不是第一选择条件。

#### Ready 集合

只有 `remainingPredecessors == 0` 的任务进入 `readyTasks`。选择并提交一个任务后，planner 递减其每个后继的剩余前驱数；某个后继降到零时才进入 ready 集合。因此被评估的任务一定已经知道所有前驱的 owner 和预计完成时间。

#### 枚举 Task/Worker 组合

每轮不是先选任务再选线程，而是评估笛卡尔积 `readyTasks x workers`。任务在候选 worker 上的预计开始时间先取该 worker 的空闲时间，再受所有前驱约束：

```text
start(task, worker) = max(
    workerAvailable[worker],
    completion[pred0] + crossWorkerPenalty(pred0, worker),
    completion[pred1] + crossWorkerPenalty(pred1, worker),
    ...)
```

前驱和候选 worker 相同时，惩罚为零；不同时，当前启发式增加前驱成本的 30%：

```text
crossWorkerPenalty = predecessor.cost * 30 / 100
```

它近似 atomic token、cache line 传递和等待成本，并非 profile 得到的实测值。同一 worker 上的依赖不增加该惩罚，因为固定 worker chain 会顺序执行。

所有组合按以下顺序选择：

1. `start` 更小；
2. `start` 相同时，任务的 `priority` 更大；
3. `priority` 仍相同时，原 `taskId` 更小。

若 task 和上述条件完全相同，worker 遍历顺序会自然保留编号更小的 worker。当前实现比较的是预计开始时间而不是预计结束时间：同一个 task 在不同 worker 上成本相同，所以选择 owner 时两者等价；不同 ready task 之间的自身成本只通过关键路径优先级间接影响选择。

#### 提交分配

确定 `bestTask` 和 `bestWorker` 后更新：

```text
scheduledOwner[bestTask] = bestWorker
completion[bestTask] = bestStart + max(1, bestTask.cost)
workerAvailable[bestWorker] = completion[bestTask]
scheduleOrder.push_back(bestTask)
```

然后从 ready 集合移除该任务并更新后继。循环结束时，`scheduleOrder` 必须覆盖全部 MTask，否则说明 MTask DAG 有环或调度状态不一致。

#### 重新编号与固定 Worker Chain

`scheduleOrder` 仍然是合法拓扑序，因为每次只选择 ready task。planner 按该顺序重新编号 MTask，同时重写全部 `predecessors/successors`，并断言每条边满足 `taskId < successor`。

最后按新 task ID 从小到大扫描：

```text
workerTasks_[task.owner].push_back(taskId)
```

例如四个 worker 的结果可能为：

```text
worker 0: T0 -> T4 -> T7
worker 1: T1 -> T3 -> T8
worker 2: T2 -> T5
worker 3: T6 -> T9
```

生成模型将每条 chain 固化成 `mtDispatchW0...Wn`。同一 worker 内按数组顺序执行；跨 worker 前驱由后续章节介绍的 wait/store token 保序。

#### 可分配的任务范围

所有 MTask 都在 worker `0..N-1` 中使用同一算法选择 owner，包括：

- 普通 wire 和寄存器计算；
- `SUPER_EXTMOD` 和 external node；
- memory、reader、writer、readwriter；
- printf、assert 等 `NODE_SPECIAL`。

注意：来自不同 worker 的 printf 文本可能交错，多个同时失败的 assertion 报告顺序也不保证稳定。

### 3.5 生成结果中有哪些多线程数据结构

生成的 `SimTop.h` 和 `SimTop*.cpp` 中，多线程实现主要由以下结构组成：

| 生成结构 | 作用 |
| --- | --- |
| `kMtWorkerCount` | 生成时固定的 worker 数；dense 运行时必须匹配 |
| `MtDispatch` | 一个 worker chain 中的静态任务描述，包括函数指针和 wait/store 区间 |
| `mtDispatchW0...Wn` | 每个 worker 的固定 MTask 顺序 |
| `mtTaskN()` | 一个或多个超节点拼接后的计算函数 |
| `MtReadyToken` | 跨 worker 依赖的奇偶 token，内部是 atomic byte |
| `mtWaitSlots` | 所有 task 的等待 token 槽位扁平数组 |
| `mtStoreSlots` | 所有 task 的发布 token 槽位扁平数组 |
| `mtDoneFlags` | worker 完成本周期后的 parity 标志 |
| `mtGeneration` | 周期 generation；低位 parity 用于复用 token |
| `mtThreads` | 后台 worker 线程对象；worker 0 使用调用线程 |

`Task::waits/stores` 是生成器内部的规划数据，生成后被压缩成 `MtDispatch` 的四个区间字段；运行时不会为每个 task 动态分配 vector。

## 4. 静态调度的性质

该算法优先缩短任务的预计开始时间，同时用关键路径处理平局，并通过跨 worker 惩罚倾向于把相关任务放在同一 worker。最终得到的 owner 和 worker chain 固定、可重复，运行时只解释静态 dispatch 表。

同一 worker 必须严格按 chain 顺序执行，不能在队头等待时跳到后续任务。即使两个任务之间没有显式普通边，它们仍可能通过 external helper、memory side effect 或指令组织包含隐含顺序。

本实现没有以下机制：

- work stealing；
- 运行时负载迁移；
- worker 内候选扫描；
- 动态活跃 task 队列；
- 根据历史 profile 重新分配。

当前成本模型也不感知 NUMA、共享 cache、CPU 拓扑或输入相关的真实任务耗时。30% 跨 worker 惩罚是经验参数，因此生成的分配是启发式结果，不保证全局最优。这些能力只有在补齐依赖证明和性能数据后才适合加入。

## 5. 跨 Worker 同步

### 5.1 Token 压缩

若 consumer 的多个前驱位于同一个 producer worker，只需等待其中在 producer chain 上最晚的前驱。因为 producer 严格顺序执行，最晚前驱完成意味着其之前的相关 task 均已完成。

planner 为每个“producer owner -> consumer owner -> consumer task”组合建立一个 `TokenGroup`：

- consumer 在执行前 acquire-load 对应 token；
- producer 的最晚前驱执行后 release-store token；
- 同一 owner pair 的 token 放在同一个 bank；
- bank 起止按 64 个槽补齐，减少不同 owner pair 之间的 false sharing。

生成模型中，`mtWaitSlots` 和 `mtStoreSlots` 是扁平只读数组，`MtDispatch` 只保存区间，避免每周期构造容器。

### 5.2 奇偶代际

`mtGeneration` 每周期递增，token 只保存 `generation & 1`：

```text
cycle k:
  main release: generation = k
  worker waits for a new generation
  task waits until token == (k & 1)
  producer release-stores (k & 1)
  worker release-stores done parity
  main acquire-waits all done parity
```

这避免了每周期清空所有 token。上一周期 token 的 parity 与当前周期不同，因此不会被误认为当前依赖已完成。

所有跨 worker 数据可见性都通过 token 的 release/acquire 建立。worker 完成标记 `mtDoneFlags` 同样使用 release/acquire。

奇偶复用成立还有一个必要条件：主线程必须等所有 worker 完成本周期后，才能发布下一 generation。当前 `stepMt()` 的周期末 barrier 保证任意 worker 都不会漏掉一个 generation，因此两位交替不会产生 ABA 歧义。

### 5.3 调度不变量

修改 planner 或 runtime 时必须保持：

1. MTask 中的 `cppIds` 保持依赖拓扑序；
2. 每条 worker chain 严格按 MTask ID 递增执行；
3. 每条跨 worker 同周期边都由 wait/store token 覆盖；
4. 一个 consumer 等待 producer worker 上最晚相关前驱后，producer 的更早前驱必然已经完成；
5. external、memory 和 special task 的共享状态访问必须由图边覆盖，或由 helper 自身保证线程安全；
6. 下一周期不能在当前周期所有 worker 完成前开始；
7. dense runtime 的 worker 数必须等于生成宽度。

破坏第 2 或第 5 条会重现早期 XiangShan 乱序问题；破坏第 3、4 或第 6 条会形成数据竞争、旧值读取或死锁。

## 6. 固定线程池

模型构造顺序为：

1. `mtInit()` 读取运行模式；
2. 原模型 `init()` 初始化信号；
3. dense 模式调用 `mtStart()`；
4. 创建 worker 1 到 `N-1`，调用线程作为 worker 0；
5. 等待所有后台 worker 更新 `mtReadyWorkers`。

每个后台线程在 `mtWorkerLoop()` 中等待 `mtGeneration` 变化，然后执行自己的固定 dispatch 数组。模型析构时设置 `mtStop`、递增 generation 唤醒线程并 join。

线程是在主线程执行 CPU pin 之前创建的。否则后台线程会继承主线程已经缩窄到单 CPU 的 affinity。

### 6.1 CPU affinity

设置 `GSIM_MT_CPU_AFFINITY=auto` 后，每个 worker 使用 `sched_getaffinity` 获取当前允许的 CPU 集合，并绑定到其中第 `worker` 个 CPU。可以先用 `taskset -c <cpu-list>` 限定允许集合，再让 `auto` 在集合内逐线程绑定。

不要用仓库默认 `make run` 测量 MT 性能。该目标使用 `taskset 0x1`，会在模型构造前把整个进程限制到一个 CPU，所有 worker 都会继承该限制。

## 7. 每周期执行流程

`stepMt()` 的顺序如下：

1. `resetAllMt()` 处理同步复位；
2. 保存寄存器 reset 源值；
3. 发布新 generation，worker 0 和后台 worker 执行各自的固定 chain；
4. 等待所有后台 worker 完成；
5. 增加 `cycles`。

MT emitter 只生成 dense 多线程执行器：

```text
dense:
  resetAllMt()
  generation++
  worker 0 执行自己的 chain
  worker 1..N-1 等待 token 后执行自己的 chain
  主线程等待所有 done parity
  ++cycles
```

`GSIM_MT_EXECUTOR` 不是 `dense`，或运行时线程数与生成时不同，模型会直接报错退出。单线程 active reference 必须由原 emitter 独立生成。

## 8. Reset 与局部变量

### Reset

大型设计的 reset 指令可能形成非常大的 C++ 函数。默认情况下，`GSIM_EMIT_RESET_CHUNK=4096` 会在控制嵌套深度为 0 的位置拆分 reset body，生成 `subResetMtN_cM()`。不会在 if/else 内部切分，因此保持控制结构完整。

若原 `InstInfo` 流整体已被同一个 reset 条件包裹，emitter 会识别并去除这一层，再由生成的 reset 函数统一添加条件，避免重复嵌套。

设置 `GSIM_EMIT_RESET_CHUNK=0` 可关闭切分；有效的显式 chunk 大小不得小于 256。

### 局部变量

task 内局部中间量使用 `T value{};` 值初始化。部分 external/memory 参数只在 enable 为真时才有语义，但未初始化参数在 C++ 层仍可能触发未定义行为。值初始化保证禁用路径不会把栈垃圾传给 helper；已在所有路径赋值的局部量通常会被优化器消除多余清零。

## 9. 配置接口

### 生成阶段

| 配置 | 默认值 | 说明 |
| --- | --- | --- |
| `--mt-mode=on` | `off` | 启用 MT 代码生成 |
| `GSIM_THREADS` | `1` | 烘焙进生成模型的 worker 数 |
| `GSIM_MT_DENSE_VCONTRACT_MAXMT` | `1600` | 期望 MTask 数，软限制 |
| `GSIM_EMIT_RESET_CHUNK` | `4096` | reset 函数目标语句数；`0` 禁用 |
| `--supernode-max-size=N` | 项目默认值 | 上游图划分粒度，会影响 task 数和并行度 |
| `--cpp-max-size-KB=N` | 项目默认值 | 生成 C++ 文件切分大小 |

### 运行阶段

| 配置 | 值 | 说明 |
| --- | --- | --- |
| `GSIM_MT_EXECUTOR` | `dense` | 启动固定线程池 |
| `GSIM_THREADS` | 正整数 | dense 模式必须与生成时完全一致 |
| `GSIM_MT_CPU_AFFINITY` | `auto` | 按允许 CPU 集合绑定 worker |

只有上表变量会影响当前实现。旧实验版本中的
`GSIM_MT_DENSE_EXECUTOR_CODEGEN`、`GSIM_MT_DENSE_LOOKAHEAD`、
`GSIM_MT_DENSE_XTHREAD_DEPS_ONLY`、`GSIM_MT_DENSE_OWNER_READY_FLAGS`、
`GSIM_MT_DENSE_UNPIN_SPECIAL`、`GSIM_MT_WORKER_POOL_FLAG_JOIN` 等开关不再读取。
`GSIM_MT_DENSE_LOOKAHEAD` 是有意删除的：worker 内乱序与隐含顺序不兼容。
`GSIM_MT_DENSE_VCONTRACT_MAXMT` 仅为兼容旧脚本保留了原名称。

## 10. 构建与运行

### 10.1 通用模型

```bash
make -j"$(nproc)" build-gsim

GSIM_THREADS=4 \
GSIM_MT_DENSE_VCONTRACT_MAXMT=256 \
  build/gsim/gsim \
  --mt-mode=on \
  --dir out/model \
  design.fir
```

编译生成模型时必须使用 `-pthread`：

```bash
clang++ --std=c++20 -O3 -pthread -Iout/model \
  out/model/Top*.cpp harness.cpp -o out/model/emu
```

运行：

```bash
GSIM_THREADS=4 \
GSIM_MT_EXECUTOR=dense \
GSIM_MT_CPU_AFFINITY=auto \
  out/model/emu
```

如果运行时线程数与生成宽度不一致，模型会直接 abort，而不是以错误的同步表继续运行。

### 10.2 XiangShan 32 线程

从 GSIM 仓库运行：

```bash
make -j"$(nproc)" build-gsim
mkdir -p build/xiangshan-perf/gsim-compile/model

GSIM_THREADS=32 \
GSIM_MT_DENSE_VCONTRACT_MAXMT=2400 \
  build/gsim/gsim \
  --supernode-max-size=30 \
  --cpp-max-size-KB=8192 \
  --sep-mod=__DOT__ \
  --sep-aggr=__DOT__ \
  --mt-mode=on \
  --dir build/xiangshan-perf/gsim-compile/model \
  ../XiangShan/build/rtl/SimTop.fir
```

使用 XiangShan difftest 工程编译 harness：

```bash
test -d build/xiangshan-perf/generated-src || \
  cp -a ../XiangShan/build/generated-src build/xiangshan-perf/

make -C ../XiangShan/difftest gsim-build-emu \
  BUILD_DIR=$(realpath build/xiangshan-perf) \
  RTL_DIR=$(realpath ../XiangShan/build/rtl) \
  NOOP_HOME=$(realpath ../XiangShan) \
  WITH_CHISELDB=0 WITH_CONSTANTIN=0 EMU_THREADS=32 \
  EMU_OPTIMIZE="-O3 -march=znver4  -fno-slp-vectorize -march=native -DCPU_XIANGSHAN" \
  -j"$(nproc)"
```

固定 50,000 周期进行性能测量：

```bash
NEMU_HOME=$(realpath ../XiangShan/ready-to-run) \
GSIM_THREADS=32 \
GSIM_MT_EXECUTOR=dense \
GSIM_MT_CPU_AFFINITY=auto \
  /usr/bin/time -f 'elapsed=%e user=%U system=%S cpu=%P maxrss=%MKB' \
  taskset -c 0-31 \
  build/xiangshan-perf/gsim-compile/emu \
  -i ../XiangShan/ready-to-run/coremark-2-iteration.bin \
  -b 0 -e 0 -C 50000 --no-diff
```

先用 50,000 周期确认运行稳定，再增加到 500,000 周期。比较配置时应保持 FIRRTL、编译参数、CPU 集合、工作负载和周期数不变。
`taskset -c 0-31` 只是示例；若机器的允许 CPU 编号不同，应替换为 `taskset -pc $$` 显示集合中的 32 个 CPU。

## 11. 正确性测试

### 11.1 输出签名

`test/mt-dense-smoke.fir` 和 `test/mt-dense-smoke.cpp` 构成小模型签名测试。测试流程分别用原单线程 emitter 和 MT emitter 生成模型，在 ASan/UBSan 下运行 4,000 周期并比较最终签名。它检查 lowering 和基本运行行为是否一致。该流程不作为专用 Makefile target，避免把调试编排逻辑混入主构建文件。

### 11.2 单线程 active 与多线程 dense 逐信号 Difftest

`test/mt-dense-diff.cpp`、`scripts/genReference.sh` 和 `scripts/genSigDiff.py` 组成小模型逐信号测试流程：

1. 用原单线程 emitter 生成 active reference；
2. 用 MT emitter 对同一 FIRRTL 独立生成 dense DUT；
3. 重命名 reference 类，生成逐信号 checker；
4. 在 ASan/UBSan 下逐周期比较 4,000 周期。

reference 不修改原单线程 active 功能，也不使用 MT emitter 内部的替代串行模式。发生差异时，checker 会扫描完当前周期；`emu` 随后打印该周期全部匹配信号的 DUT/reference 值并退出。65 位以上信号使用 `_BitInt(N)` 掩码比较，并按 64 位分块打印。

### 11.3 Ready-to-run 设计

Ready-to-run 测试同样应独立生成单线程 active reference 和多线程 dense DUT，并在每个周期比较所有可映射信号。大型用例的生成、编译和运行编排放在外部调试脚本中，不加入主 Makefile。`GSIM_DIFF_MAX_CYCLES` 只在 difftest 构建中生效。

当前实现已验证：

| 设计 | 配置 | 结果 |
| --- | --- | --- |
| MT smoke | 4 worker，ASan/UBSan | 4,000 周期通过 |
| ysyx3 | 4 worker，1,594 个信号 | 10,000 周期通过 |
| Rocket | 4 worker，10,188 个信号 | 10,000 周期通过 |
| XiangShan SimTop | 32 worker，861,255 个信号 | 用于回归和性能测试；完整 active/dense 正确性应以当前生成版本重新执行逐周期比较 |

## 12. 已修复的 XiangShan 调度问题

### 12.1 Dense 依赖拓扑

早期精简版直接按 `cppId` 计算所有超节点。活跃度扫描允许向后激活留到下一次 `step()`，但 dense 全量执行若仍按该编号顺序，会把新状态和旧组合逻辑混在同一周期，最终触发 `MemRegCache` 等断言。

当前实现先对纯数据依赖做 Kahn 排序，再只加入该排序中向前的激活边。这个修改已通过独立单线程 active/MT 模型的香山逐信号比较，并且不依赖给 assertion/printf 添加全局前序边。

### 12.2 Worker 内 lookahead

早期 worker 在队头等待跨线程依赖时，会扫描后续 task 并提前执行“看起来 ready”的任务。XiangShan 在第一个周期出现 ROB 信号
`_io_enq_canAccept_T_5` 差异。关闭 lookahead 后，单线程 active/MT 逐信号对比恢复一致。

当前 runtime 始终严格顺序执行 worker chain。不要在没有补齐和证明全部隐含依赖前重新加入乱序。

### 12.3 未初始化 helper 参数

部分 memory helper 参数在 enable 为假时没有 RTL 语义，但旧生成代码仍可能把未初始化的 C++ 局部变量作为实参传递。不同模型栈布局会产生不同垃圾值，也构成 C++ 未定义行为。MT emitter 现在对 task 局部变量进行值初始化。

## 13. 性能调优

建议按以下顺序调优：

1. 先用小模型或目标设计的 active/dense difftest 建立正确性基线；
2. 固定 workload 和周期数，记录原单线程 active 模型时间；
3. 分别为 2、4、8、16、32 worker 重新生成模型；
4. 调整 `GSIM_MT_DENSE_VCONTRACT_MAXMT`，观察 task 数、token 数和各 worker 的负载；
5. 使用固定 CPU 集合和 `GSIM_MT_CPU_AFFINITY=auto`；
6. 关闭逐信号 difftest，用 `-O3 -march=native` 测量；
7. 至少重复三次，比较中位数。

生成器会打印：

```text
[cppEmitter-mt] workers=32 supernodes=... mtasks=... edges=... tokens=...
```

指标含义：

- `mtasks` 太少：并行度不足，长 task 容易造成尾部不均衡；
- `mtasks` 太多：函数调用、dispatch 和同步开销增加；
- `tokens` 高：跨 worker acquire/release 和 cache traffic 增加；
- 超节点过大：单 task 难以均衡；
- 超节点过小：图和生成代码体积增加。

性能测试只运行 dense DUT，不链接逐信号 checker：

```bash
GSIM_THREADS=32 GSIM_MT_EXECUTOR=dense ./emu workload.bin
```

dense 必须设置生成时的 `GSIM_THREADS`。若要比较单线程性能，应另行生成原 active 模型；两种执行语义不同，结果用于端到端性能比较，不用于隔离调度开销。

## 14. 故障排查

### 启动时 abort

若出现：

```text
this model requires GSIM_MT_EXECUTOR=dense ...
```

确认运行时设置了 `GSIM_MT_EXECUTOR=dense`，并且 `GSIM_THREADS` 与生成时一致。还要确认运行的是新生成的模型，而不是旧 `gsim-mt/build` 中的二进制。

### 多线程反而更慢

检查：

- 是否误用 `make run` 或 `taskset 0x1`；
- CPU affinity 集合是否至少包含 worker 数量的 CPU；
- worker chain 的成本是否明显失衡；
- MTask 是否远多于期望值；
- 是否仍启用了逐信号 checker、ASan/UBSan 或 `-O0`；
- 是否只运行了很少周期，导致线程创建和初始化占主导。

### 仿真挂起

可能原因包括：

- 外部 helper 阻塞其 owner worker；
- 使用了与模型不匹配的线程数或旧运行时；
- 新图优化产生了未被 generation-time forward-edge 断言捕获的同步问题；
- CPU 严重 oversubscribe，spin wait 看起来像停滞。

先运行小模型 active/dense 逐信号测试，再缩短目标模型周期数并检查最后一个完成的 worker/task。

### 出现信号差异

使用未修改的原单线程 active 模型作为 reference：

1. 对同一 FIRRTL 分别运行 single emitter 和 MT emitter；
2. DUT 设置 `GSIM_MT_EXECUTOR=dense`；
3. 每周期比较导出信号；
4. 从第一个差异信号定位其 `mtTaskN`；
5. 检查 producer/consumer owner 和对应 wait/store token。

## 15. 当前限制

- 线程数在生成时固定，修改线程数必须重新生成模型；
- 当前是 dense executor，不利用节点活跃度；
- 调度成本是静态近似值，没有 profile feedback；
- external、memory、printf 和 assert 都可分配到任意 worker；helper 的并发安全由集成方保证；
- worker 使用忙等同步，会占满所分配 CPU；
- CPU affinity 使用 Linux `sched_*affinity`；
- spin pause 当前生成 x86 `pause` 指令，移植到非 x86 平台前需要增加对应实现；
- 不支持并发调用同一个模型对象的 `step()`；
- correctness 依赖跨 worker 的同周期数据边完整，同时依赖每条 worker chain 保持 `cppId` 顺序；
- 目前正确性测试覆盖到 XiangShan 10,000 周期，尚不等于完整 CoreMark 跑完；
- 尚未建立正式的线程扩展性和 NUMA 性能基准。

任何新的调度优化都应先扩充 active/dense 逐信号 difftest，再进行性能测量。

## 16. 已知的编译器优化风险

### 16.1 现象

早期包含 `serial-dense` 诊断模式的 XiangShan 模型曾出现以下现象。当前 emitter 已删除该模式，以下内容仅记录编译器问题的历史定位结果：

- active reference 在第 4266 周期得到 `s2_instrCompactInfo.instrEndOffset$NEXT_5 = 6`；
- serial-dense 在相同周期得到 `7`；
- 在生成函数中加入一个仅用于打印 `instrBoundary___T_3` 的 `printf` 后，结果变为 `6`；
- GDB 在无打印机器码的实际 store 指令处观察到：`boundary_3_T_1=1`、`offset6=6`、`offset7=7`，但最终写入寄存器为 `7`。

这说明错误不是 `$NEXT_5` 在 task 结束后被覆盖，也不是 serial-dense 的线程调度问题。`printf` 改变了函数的 observable use 和调用边界，使 Clang 的 SLP 分组及 AVX-512 byte-shuffle codegen 发生变化。

### 16.2 复现实验

对同一份 `SimTop135.cpp`、同一份输入和同一套编译参数，结果如下：

| 编译选项 | 第 4266 周期 `$NEXT_5` |
| --- | ---: |
| `-O3 -march=znver4` | `7` |
| 加 `T_3 printf` | `6` |
| `-fno-slp-vectorize` | `6` |
| `-fno-vectorize` | `7` |
| `-fno-strict-aliasing` | `7` |
| `-mno-avx512vbmi -mno-avx512vbmi2` | `6` |
| `-march=znver3` | `6` |
| UBSan/bounds | 无运行时错误，结果 `6` |

当前最小规避方式是对生成模型的编译增加：

```bash
EMU_OPTIMIZE="-O3 -march=znver4 -fno-slp-vectorize -DCPU_XIANGSHAN"
```

或者避免 AVX-512 VBMI 字节重排：

```bash
EMU_OPTIMIZE="-O3 -march=znver4 -mno-avx512vbmi -mno-avx512vbmi2 -DCPU_XIANGSHAN"
```

该规避只针对已确认的 Clang/AVX-512 codegen 风险，不应被误解为修复了 GSIM 图或 MTask 依赖。LLVM 也有公开的 SLP shuffle 相关问题记录，例如 [LLVM issue #127220](https://github.com/llvm/llvm-project/issues/127220)；这不是本案例的等价 reproducer，只说明该优化链本身需要谨慎验证。长期方案是将 `mtTask3682()` 的相关表达式缩减成编译器 reproducer，并升级或修复 LLVM；当前回归以独立生成的单线程 active reference 和 dense MT DUT 为准。

### 16.3 推荐的定位顺序

遇到“加入打印后结果改变”时按以下顺序排查：

1. 用独立生成的单线程 active reference 和 dense MT DUT 找到第一个差异周期；
2. 先比较完整成员信号，再比较目标 MTask 内的局部变量；
3. 用 GDB 在原始目标 store 指令处读取写入寄存器，而不是在函数中添加打印；
4. 对同一源文件分别尝试 `-fno-slp-vectorize`、`-mno-avx512vbmi*` 和 UBSan/bounds；
5. 检查目标 task 的生成代码和机器码后，再检查跨 worker token 和 worker chain；
6. 任何修复都要用相同 FIRRTL、输入和编译参数重跑 active/dense difftest。

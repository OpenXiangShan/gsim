# GSIM RepCut + active 设计

本文描述当前 GSIM 中 `--mt-mode=repcut` 的实现。这里的 “RepCut + active”
有两个同时成立的含义：RepCut 在生成阶段复制少量纯组合节点，把电路划成固定的
独立分区；运行阶段每个分区仍使用单线程 emitter 的 active flag，只计算本周期被
激活的超节点。它不是旧论文中每周期完整重算的 activity-oblivious RepCut，也不是
`--mt-mode=on` 使用的 dense MTask/token 后端。

当前实现的核心目标是把昂贵的周期内数据依赖变成分区之间的周期边界同步，同时保留
active 执行对稀疏变化的跳过能力。状态、memory 和副作用仍只有一个语义上的 owner；
复制只作用于允许安全重算的组合数据锥。

## 1. 设计摘要

生成流程可以概括为：

```text
FIRRTL / CHIRRTL
      |
      v
Parser -> AST2Graph -> 通用图优化
                           |
                           v
                    RepCut resource analysis
                           |
                           v
                sink groups + intersection hypergraph
                           |
                           v
                KaHyPar / local km1 partitioning
                           |
                           v
       regsrc/regdst state affinity + pure-node replication
                           |
                           v
                 cross-partition edge pruning
                           |
                           v
        each partition: SuperNode -> StmtTree -> InstInfo
                           |
                           v
                   cppEmitter active functions
                           |
                           v
                 fixed workers + two phase barriers
```

运行时的一个模型周期为：

```text
周期初 reset 输入刷新 / 同步 reset
              |
      发布 epoch 给后台 worker
              |
各 worker: memory flush + register commit
              |
          barrier 0
              |
worker 0: 异步 reset 条件刷新和级联复位
              |
          barrier 1
              |
各 worker: 合并远端 active pending 位图
              |
各 worker: 执行自己的 active evalPart 函数
              |
主线程等待所有 worker 完成
```

如果设计没有动态异步 reset，第二个 barrier 和对应的 serial reset 阶段可以省略。
XiangShan 当前生成物包含异步 reset 路径，因此有两个 barrier。

## 2. 入口和源码边界

`src/main.cpp` 将 `--mt-mode=repcut` 映射为 `RepCutMode=true`、`MtMode=false`：

- `--mt-mode=off`：普通单线程 active 路径；
- `--mt-mode=on`：`cppEmitterMt` 的 dense MTask/token 路径；
- `--mt-mode=repcut`：`repCutSplit`、`repCutLower`，然后复用 `cppEmitter` 的 active
  代码生成路径。

关键源码职责如下：

| 文件 | 职责 |
| --- | --- |
| [src/main.cpp](../src/main.cpp) | 解析 `--mt-mode`，选择 RepCut 流程 |
| [src/repCut.cpp](../src/repCut.cpp) | 资源识别、交集超图、划分、复制、状态归属、边裁剪 |
| [src/graphPartition.cpp](../src/graphPartition.cpp) | 对每个分区独立 lowering，并重新合并生成结果 |
| [src/cppEmitter.cpp](../src/cppEmitter.cpp) | active flag、分区函数、commit、memory、reset 和线程运行时 |
| [include/Node.h](../include/Node.h) | `Node::rcPart`、`rcCommitOnly`、`rcResetSource` 等 RepCut 状态 |
| [test/test_repcut_active.py](../test/test_repcut_active.py) | 小模型状态传播、memory、reset、复制和 barrier 回归 |

生成器不修改通用 FIRRTL 优化的语义。RepCut 在这些优化完成后运行；当前主流程中的
位置见 [src/main.cpp:533](../src/main.cpp:533)。

## 3. Node 图和寄存器周期边界

AST2Graph 将电路变成 `Node` 依赖图。普通组合节点通过 `prev/next` 表示同周期数据
依赖。寄存器被拆成：

- `NODE_REG_SRC`：周期开始可读的当前寄存器值；
- `NODE_REG_DST`：本周期计算出的下一状态；
- reset 节点：在同步或异步 reset 路径上覆盖状态。

寄存器的反馈只在下一周期生效。RepCut 因此不能把 `regsrc` 当作普通可复制组合节点。
`Node` 中的相关字段为：

```cpp
int rcPart = -1;       // 分区 owner
bool rcCommitOnly;     // 周期初 state/commit 阶段执行
bool rcResetSource;    // serial reset/clock cone
```

`Node::getDst()` 对 `NODE_REG_SRC` 返回绑定的 `regNext`，`getSrc()` 对
`NODE_REG_DST` 返回对应的 source。RepCut 使用这个绑定关系保持状态归属。

## 4. RepCut 划分前的资源分类

### 4.1 source、pure 和可复制节点

RepCut 首先将常量、寄存器源、顶层输入、memory 和 reset source 视为锥遍历的边界
source。它们不作为普通组合工作向上展开。

可复制节点必须满足以下条件：

- 是有效节点；
- 不是 source；
- 不是 register destination；
- 不是 memory writer、readwriter 或 write-first reader；
- 不是 extmodule、特殊副作用节点或 reset/clock cone；
- 表达式不包含 memory read/write、DPI/extmodule、printf、assert 或 exit。

`repCut.cpp` 中的 `replicable()` 和 `pure()` 实现这一检查。复制的原则是“重复计算
纯数据比等待跨分区同步便宜”，不能借复制绕过 memory、DPI、printf、assert、reset
或 ordering 依赖。

### 4.2 reset 和 clock cone

RepCut 会从所有 reset 节点和 clock 表达式向上遍历，把相关纯节点标记为
`rcResetSource`。这些节点由 serial reset phase 刷新，不能由普通 worker 独立重算。

异步 reset 超节点在 RepCut lowering 前转换为普通有效超节点；真正的异步 reset
副作用在运行时由 worker 0 按 `allReset` 的依赖顺序调用。这样 reset 条件先后关系
仍由生成器控制，而不是被分区切断。

如果一个 reset body 写入的寄存器改变了另一个 async reset 的条件，RepCut 会在图上
建立 reset 之间的顺序，使用 Kahn 拓扑排序得到 reset cascade order。存在循环时生成
阶段直接报错。

### 4.3 memory、extmodule 和副作用资源

memory writer/readwriter 被视为不可随意拆开的资源。对普通 read-old memory：

- writer 仍属于自己的分区；
- reader 可以位于其他分区；
- reader 读取稳定的周期前 memory snapshot；
- writer 的本周期写入先进入该 writer partition 的 log；
- commit/memory flush 后才写回共享 memory；
- flush 时唤醒全部 reader：同 partition 直接写本地 `activeFlags`，其他 partition 记入远端 pending。

如果一个 memory 的 writer 和 reader 跨分区且不能安全保持旧值语义，生成阶段会把它
加入 snapshot memory 集合。write-first memory、readwriter 以及含副作用的端口不
通过复制规避其顺序约束。

## 5. Sink group 和交集超图

### 5.1 sink 选择

RepCut 从有效图中选择 sink。输出、外部观察节点、不可继续展开的节点以及没有后继的
节点可以成为 sink；source 节点不作为普通 sink cone 的内部工作。

每个 sink 向 `prev` 方向遍历得到祖先 cone。属于同一组 cone 的非 source 节点会形成
cluster。一个组合 cluster 如果被多个 sink group 使用，就成为交集超图中的 hyperedge。

```text
sink group = hypergraph vertex
shared non-sink cluster = hyperedge
hyperedge 被切开 = 该纯组合 cluster 需要复制
```

### 5.2 顶点和超边权重

普通 cluster 的权重使用 `mtStructuralNodeCost(node)` 累加。它是基于 FIRRTL 节点、
表达式类型和位宽的结构成本，不是实测 CPU 时间。

如果一个 cluster 只连接一个 sink group，它的成本全部加入该顶点；如果连接多个
sink group，成本按端点数均摊到各顶点，同时作为 hyperedge weight 保留。划分器目标
是同时平衡顶点权并降低：

```text
km1 = Σ (number_of_blocks(hyperedge) - 1) × hyperedge_weight
```

如果本机有 `mtkahypar`，生成器将写出 `repcut-kahypar/intersection.hgr` 并调用
KaHyPar；否则使用源码中的局部 `partitionKm1()` fallback。fallback 采用按权重降序
和当前最轻 block 的贪心分配，再做局部移动。

### 5.3 state-aware 顶点权重

`regsrc` 必须留在对应 `regdst` 的 worker 中，因为当前语义只允许在 commit 阶段
产生跨 worker active 更新。它不能在划分完成后被任意迁移。

因此当前实现把与每个 `regdst` 配对的 state cost 提前加入其 sink group：

```text
partition_weight(group)
    = structural_weight(group)
    + GSIM_REPCUT_STATE_WEIGHT × state_cost(group)
```

当前静态估算为：

```text
state_cost(regsrc) = 16 + 4 × max(1, ceil(width / 64))
```

固定项近似 active guard、旧值比较和状态赋值；word 项近似宽状态写回。这个模型不
假设可以移动 `regsrc`，只改变包含该 `regdst` 的 sink group 被分配到哪个 worker。

环境变量：

```sh
GSIM_REPCUT_STATE_WEIGHT=1    # 默认启用
GSIM_REPCUT_STATE_WEIGHT=0    # 仅使用旧 structural weight
```

允许范围是 0 到 1000。生成器在 `repcut-partitions.csv` 中输出：

| 字段 | 含义 |
| --- | --- |
| `structural_cost` | 该 worker 的组合节点和复制节点结构成本 |
| `state_cost` | 跟随该 worker 的寄存器提交静态成本 |
| `combined_cost` | `structural_cost + state_weight × state_cost` |
| `regdst_count` | 该 worker 的寄存器数量 |
| `regdst_bits` | 该 worker 的寄存器位宽总和 |

state-aware cost 的局限是它还不知道具体 workload 的寄存器活跃率。一个宽寄存器
可能几乎不变，而窄寄存器组可能每周期大量变化；因此后续应把 commit 超节点的实际
执行次数、值变化次数和远端 active 写次数用于 profile-guided 校准。

这项修改已经在当前 RepCut + active workload 上观察到性能提升。提升的原因与模型
一致：原划分只平衡组合求值，容易把 `regsrc` 提交工作集中到少数 worker；加入 state
cost 后，sink group 的选择会同时考虑其配对的状态提交负载，从而降低提交阶段的
worker tail 和 barrier 等待。当前文档只记录提升方向，不固化单一加速数字；具体数值
应随 workload、线程数、CPU affinity、编译器和 `GSIM_REPCUT_STATE_WEIGHT` 一起报告。

## 6. 分区、复制和状态归属

### 6.1 初始 sink 分区

KaHyPar 或 fallback 给每个 sink group 一个 block。sink 的 `rcPart` 被设置为该
block，形成 `roots[part]`。

随后从每个 partition 的 root 向上遍历祖先。每个节点得到一个 partition bit mask：

```text
mask[node] = 哪些 partition 的 sink cone 使用该节点
```

### 6.2 纯组合节点复制

如果节点被多个 partition 使用：

- 原节点保留在最低编号 partition；
- 对其他使用者 partition 创建 `$RC$<part>` 副本；
- 副本复制 assign tree、宽度和 parent 信息；
- 副本拥有自己的 `rcPart` 和 SuperNode；
- 后续表达式引用按 partition 重写。

不可复制的节点必须只属于一个 partition。违反此条件会触发 assertion，而不是静默
生成一个带数据竞争的模型。

### 6.3 regsrc/regdst state affinity

状态归属在普通 cone 分区和组合复制完成后确定：

```cpp
for (Node* reg : regsrc) {
  reg->rcCommitOnly = true;
  Node* dst = reg->getDst();
  reg->rcPart = dst && dst->rcPart >= 0 ? dst->rcPart : 0;
}
```

这保证：

1. `regsrc` 不被复制；
2. `regsrc` 与它的 `regdst` 使用同一 partition owner；
3. commit 阶段对寄存器当前值的更新仍由固定 worker 执行；
4. 任何远端 active 影响都通过 commit pending 协议发布。

这也是 state-aware partition weight 必须在分区前估算的原因：分区后不能再独立移动
寄存器源来修正负载。

### 6.4 跨分区边裁剪

复制和分区完成后，`repCutPruneEdges()` 删除跨 partition 的普通 `prev/next` 和
对应 dependency edge。被删除的计算边必须满足：

- source、顶层 input、constant 或 reset source 可以作为周期边界来源；或
- snapshot memory 的 read-old ordering edge 已由 memory flush/wake 机制替代。

如果发现普通计算边没有合法的边界来源，生成阶段报错：

```text
RepCut has an unsynchronized compute edge ...
```

被剪掉的边会记录在 `boundaryNext`，后续 active propagation 使用它生成目标 partition
的 activation。

## 7. 每个 partition 的 lowering

`graph::repCutLower()` 对每个 partition 单独执行：

1. 取出该 partition 的 SuperNode 列表；
2. 给 SuperNode stamped partition owner；
3. `reconnectSuper()` 重建 partition 内连接；
4. `partitionSorted(false)` 在 partition 内做普通超节点合并；
5. `replicationOpt()` 只在该 partition 内做 active 相关的复制优化；
6. `generateStmtTree()` 合并 assign tree 和控制流；
7. `instsGenerator()` 生成 InstInfo；
8. 再次检查和裁剪跨 partition 边；
9. 把所有 partition 的 lowering 结果合并回 `sortedSuper`。

RepCut lowering 使用单线程 active emitter 的 StmtTree/InstInfo 表示，因此每个分区
内部仍保持 active flag、值变化传播和局部超节点顺序。RepCut 不把这些分区重新转换
为 dense MTask，也不建立 MT 后端的 wait/store token DAG。

## 8. Active flag 生成

### 8.1 分区独立 flag

Emitter 为每个 partition 生成独立的：

```cpp
alignas(64) uint64_t activeFlags<part>[...];
```

每个 partition 的 SuperNode 使用本地 `cppId`。因此 `activeFlags3[5]` 的 bit 只
能解释为 partition 3 的本地 ID，不能直接拿另一个 partition 的 cppId 使用。

### 8.2 activation 分类

生成阶段的 `repCutActivate()` 为每个节点建立三类传播：

- 同 partition 的后继：直接写本地 `activeFlags`；
- 跨 partition 的后继：记录到 `rcForeign[node]`，生成 commit pending 写入；
- memory writer 的 reader：无论是否同一 partition，都由 snapshot memory flush 在数组写回时置位。同 partition 直接写本地 `activeFlags`；其他 partition 仍写入 pending。

`regdst` 还会显式激活绑定的 `regsrc` SuperNode。这样下一周期 state commit 不会因
regdst 的值变化而被跳过。

只有节点值真的变化时，`activateNext()` 才设置后继 active flag；无条件传播使用
`activateUncondNext()`。异步 reset 使用更强的路径，可能调用 `activateAll()`，因为
reset 会覆盖大量状态并改变激活条件。

### 8.3 远端 activation 的 private pending

worker 在 commit 或 partition-local evaluation 中不能直接改另一个 worker 正在读取
的 active flag。生成器为跨 partition 目标创建：

```cpp
RcCommitDirty rcCommitDirty[threads];
uint64_t rcCommitPend<source>_<dest>[...];
```

源 worker 只写自己的 pending 位图和 dirty mask。commit barrier 之后，目标 worker
执行 `rcMergeCommitFlags(part)`：

1. 检查每个 source 的 dirty mask；
2. 将对应 pending words OR 到目标 `activeFlags`；
3. 清空 pending buffer。

这种协议把跨线程写入限制在 commit/state phase 产生，并避免 evaluation phase 中
不同 worker 同时修改共享 active cache line。

## 9. 生成的函数组织

### 9.1 ID 分配和函数分类

`assignRepCutIds()` 为每个 partition 使用独立的本地 ID 空间，并把 SuperNode 分成：

- `rcResetSupers`：reset/clock cone，serial refresh 使用；
- `rcCommitSupers[part]`：所有 `rcCommitOnly` SuperNode；
- `rcEvalSupers[part]`：普通 active 计算 SuperNode；
- extmodule/always-active SuperNode：按分区保存，不能被错误跳过。

每一类函数按生成 C++ 文件大小切 chunk：

```text
evalPart<part>_<chunk>()
commitPart<part>_<chunk>()
```

每个 SuperNode 函数体仍由 active guard 包围：检查本 partition 的本地 active bit，
清除该 bit，执行 InstInfo，再根据新旧值传播后继。

### 9.2 commitPart

当前 emitter 使用一个统一的 `rcCommitPart(int part)` 入口。它先清零
`rcCommitDirty[part].mask`，再调用 `rcFlushMem(part)`，最后通过 `switch(part)`
调用该 worker 的 `commitPart<part>_<chunk>()` 函数。

真正的寄存器提交 body 和 chunks 仍按 worker 独立生成；统一入口只是负责公共的
dirty-mask/memory 顺序和运行时路由。这个选择减少了 wrapper 和函数布局数量，但保留
了一次每周期的 dispatch。后续若再次研究 per-worker wrapper，必须同时测量代码体积、
instruction cache 和 branch 行为，不能只看 switch 是否被移除。

寄存器提交函数不是简单的整段 memcpy。每个 active commit SuperNode 可能执行：

- 读取 `$NEXT` 值；
- 保存旧当前值；
- 写入当前寄存器值；
- 比较新旧值；
- 更新本地 activeFlags；
- 写入远端 `rcCommitPend` 和 dirty mask。

因此提交负载同时取决于 state 数量、宽度、当前 active 稀疏性以及跨 partition fanout。

### 9.3 evalPart

`rcRunPart(part)` 只执行该 partition 的 `evalPart` chunks。函数内不再进行动态任务
窃取，也不根据其他 worker 的实时状态重新排序。active flag 是唯一的计算跳过机制。

同一 partition 内的 SuperNode 仍按生成时顺序执行。跨 partition 数据在当前周期内
不会通过普通共享值边直接传递；需要等到下一次合法状态/activation 边界。

## 10. Memory snapshot 和 flush

对被识别为跨 partition snapshot memory 的每个 writer partition，emitter 生成：

```cpp
std::vector<RcStore$mem> rcLog$mem$part;
void rcWrite$mem$part(uint64_t elem, T data);
T rcRead$mem$part(uint64_t elem);
```

`rcWrite` 对同一 element 做最后一次写覆盖，并在没有已有记录时追加 log。`rcRead`
从本 partition 的 log 反向查找本周期写，再回退到共享 memory，保持该 memory helper
需要的 forwarding 语义。

`rcFlushMem(part)` 在 commit 阶段只处理传入 `part` 对应 writer partition 的 log，
再将其写回共享 memory。只有 log 非空时才唤醒 reader，避免空 writer 每周期唤醒所有
reader。同 partition reader 在这里直接置本地 `activeFlags`；其他 partition 仍写入
pending。switch 的每个 case 访问不同的 worker log；函数入口是共享的，实际 memory
工作仍按 owner partition 分离。

memory 的所有 writer 仍只在自己的 owner partition flush；析构时也会 flush 各 partition
日志，避免模型销毁时丢失 deferred store。

## 11. Reset 语义

### 11.1 初始化和同步 reset

`step()` 首先刷新 reset 输入并调用 `resetAll()`。初始化时所有 partition active flag
会被置为 active，确保首次计算完整传播。

同步 reset 仍沿用原 emitter 的 reset helper；状态提交的 reset 优先级由生成的
`commitPart`/reset assignment 顺序保证。

### 11.2 异步 reset

在多 worker 模式下，async reset 不让每个 worker 各自 replay。`rcCycle()` 的顺序是：

```text
rcCommitPart(part)
barrier(phase=0)
worker 0:
  rcRefreshResetInputs()
  rcResetActiveN = previous_async || reset_condition
  subResetN()
barrier(phase=1)
rcMergeCommitFlags(part)
rcRunPart(part)
```

`rcRefreshResetInputs()` 执行 reset source cone 的 active evaluation，确保后续
`subResetN()` 看到本周期最新 reset 条件。多个 async reset 按图分析得到的 cascade
order 调用，后一个 reset 可以读取前一个 reset 刚写入的状态。

reset 阶段不能被普通 active propagation 取代，因为 reset 可能同时覆盖大量寄存器、
改变下游条件，并触发跨 partition 的整片激活。

## 12. Runtime 线程协议

### 12.1 worker 生命周期

模型第一次 `step()` 时：

1. 检查运行时 `GSIM_THREADS` 是否等于生成时线程数；
2. 创建 worker 1 到 worker N-1 的 pthread；
3. worker 0 由调用 `step()` 的主线程直接执行；
4. 如果设置 `GSIM_MT_CPU_AFFINITY`，按 allowed CPU 集合将 part 映射到 CPU；
5. 主线程递增 `rcEpoch` 发布新周期。

后台 worker 等待 epoch 变化。看到新 epoch 后执行 `rcCycle(part)`，完成后把对应
`rcJoin[part]` 写为该 epoch。主线程执行自己的 `rcCycle(0)`，再等待所有后台
`rcJoin` 等于当前 epoch。

### 12.2 barrier parity

barrier 使用 `rcEpoch & 1` 作为 parity，避免每周期清空全部 token：

```cpp
RcPhaseFlag rcArrive[2][threads];
RcPhaseFlag rcRelease[2];
```

worker 1..N-1 先写自己的 arrive flag，再等待 release；worker 0 轮询所有 arrive，
然后写 release。phase 0 用于提交结束，phase 1 用于 async reset 结束。

每个 `RcPhaseFlag` 和 `RcJoin` 都按 cache line 对齐，减少不同 worker 写入同一 cache
line 的机会。当前等待采用 acquire/release 原子操作加 `pause` 自旋。

### 12.3 线程销毁

析构函数把 `rcEpoch` 写为全 1，后台 worker 看到该值后退出，主线程随后 join 所有
pthread。snapshot memory log 在析构前 flush。模型拷贝构造和赋值被删除，避免复制
pthread、epoch 和 active 状态。

## 13. 正确性不变量

修改 RepCut、emitter 或 runtime 时必须保持以下不变量：

1. `regsrc` 与绑定的 `regdst` 属于同一 partition owner；
2. 纯组合节点可以复制，但 memory、register state、extmodule 和副作用不能被非法复制；
3. 每条被裁剪的跨 partition 计算边都有 source/input/reset boundary 或 snapshot memory 语义；
4. commit 阶段结束前，远端 active pending 不得直接并入目标 activeFlags；
5. barrier 0 结束后，所有 worker 读取的寄存器状态一致；
6. async reset 在 barrier 1 结束后才允许普通 evaluation；
7. 每个 worker 只执行自己的 `commitPart` 和 `evalPart`；
8. evaluation 不依赖另一个 worker 尚未完成的同周期普通计算；
9. 主线程发布下一 epoch 前必须观察所有 worker 当前 epoch 完成；
10. 生成时线程数、运行时 `GSIM_THREADS` 和 runtime 数组大小一致；
11. 同一 memory 的 deferred log 只能由其 writer owner flush；
12. async reset cascade 的顺序必须是无环拓扑序。

这些不变量比减少 barrier 数量或增加复制节点数量更优先。若一个优化无法证明满足
上述条件，应保留原边或让生成阶段失败。

## 14. 配置和生成产物

典型生成命令：

```sh
GSIM_THREADS=32 \
GSIM_REPCUT_STATE_WEIGHT=1 \
build/gsim/gsim \
  --supernode-max-size=30 \
  --cpp-max-size-KB=8192 \
  --sep-mod=__DOT__ \
  --sep-aggr=__DOT__ \
  --mt-mode=repcut \
  --dir build/xiangshan-repcut/gsim-compile/model \
  ready-to-run/new-xiangshan-default.fir
```

运行时示例：

```sh
GSIM_THREADS=32 \
GSIM_MT_CPU_AFFINITY=auto \
taskset -c 64-95 \
build/xiangshan-repcut/gsim-compile/emu \
  -i ../XiangShan/ready-to-run/coremark-2-iteration.bin \
  -b 0 -e 0 --no-diff
```

主要产物：

| 产物 | 说明 |
| --- | --- |
| `repcut-partitions.csv` | 每个 worker 的 sink、组合成本、state cost、复制节点和寄存器统计 |
| `repcut-kahypar/intersection.hgr` | KaHyPar 输入超图（如果使用外部 solver） |
| `SimTop.h` | 分区 activeFlags、pending buffers、barrier 和线程结构声明 |
| `SimTop*.cpp` | `evalPart`、`commitPart`、memory flush、reset、worker runtime |
| `rcCommitPend<source>_<dest>` | 跨分区 active pending 位图 |
| `rcLog$memory$part` | snapshot memory writer log |

`--mt-target-tasks` 是 dense MTask 后端的目标；当前 RepCut 的 worker 数由
`GSIM_THREADS` 决定，RepCut 不把电路拆成动态 MTask/token DAG。

## 15. 性能模型和已知瓶颈

RepCut + active 的周期时间近似为：

```text
Tcycle ≈ max_part(Tmemory_flush + Tcommit)
       + barrier0
       + Tasync_reset
       + barrier1
       + max_part(Tmerge_active + Teval)
       + join / harness overhead
```

这里的 `max_part` 很关键。不同 worker 的工作可以重叠，但最慢 worker 决定下一阶段
何时开始。

当前 XiangShan 测量显示：

- memory flush 平均很小；
- 状态提交主要集中在少数拥有大量活跃寄存器的 worker；
- active evaluation 的拖尾 worker 与提交拖尾 worker 不完全相同；
- worker 0 的静态 structural cost 最大，不代表它的动态 active 求值最大；
- active merge 可能因为远端 pending 位图容量和 dirty source 数量在少数 worker 上变重；
- barrier/join 等待时间包含其他 worker 的有效工作，不能直接当作纯同步开销。

这解释了为什么仅平衡复制前组合节点成本不能保证运行时平衡。加入 state-aware weight
后当前 workload 已经得到性能改善，但它只解决静态 state 规模的一部分；动态活跃率和
传播扇出仍需要 profile-guided cost。

建议的 profile 字段包括：

```text
commit super 被检查次数
commit super 实际执行次数
寄存器值变化次数
本地 active 写次数
远端 dirty mask 写次数
每个 source->dest pending buffer 实际 OR word 数
eval super 实际执行次数
```

## 16. 验证策略

### 16.1 小模型功能回归

`test/test_repcut_active.py` 覆盖：

- 1/2/4 worker 下的跨分区寄存器状态传播；
- 多实例模型生命周期和线程析构；
- memory writer/reader 资源不能被错误合并；
- packed bit array；
- sparse register reset；
- synchronous/async reset；
- async reset cascade；
- 复制组合节点在输入不变时保持 inactive；
- 32 worker 下 back-to-back barrier parity。

测试使用 ASan/UBSan 编译生成模型，并比较多个 worker 配置的输出一致性。

### 16.2 生成阶段检查

生成器应检查：

- `GSIM_THREADS` 为 1 到 64 的整数；
- KaHyPar 输出 block 编号合法；
- 不可复制资源没有跨 partition；
- state cost unassigned 数量可见；
- 所有生成 SuperNode 的 `rcPart` 合法；
- `repCutPruneEdges()` 没有未同步的普通计算边；
- reset cascade 无环；
- `regsrc/regdst` owner 一致。

### 16.3 XiangShan 性能测量

性能对照需要固定：

- 同一 FIRRTL/生成参数；
- 同一编译器和优化选项；
- 同一 `GSIM_THREADS` 和 CPU affinity；
- 同一 workload、seed、`--no-diff` 设置；
- 至少三次完整 workload 运行；
- `HIT GOOD TRAP`、guest cycles、instruction count、CRC 和 PC 一致。

不要只比较静态 `structural_cost`、复制节点数量或 token 数。应同时报告：

- Host time 和 guest cycles/s；
- 每 worker commit/eval 阶段均值、P95、P99；
- barrier 和 join 等待；
- worker 尾部完成分布；
- commit code size、active flag bytes、pending bitmap bytes；
- 必要时的 instruction/data cache 和 branch 事件。

本地性能测量资料强调，完整 workload、稳定 CPU 绑定、重复运行和 A/B 产物隔离是
生成代码仿真器性能结论的前提；详见 [wiki-benchmark-and-profiling](../../mtwiki/wiki/wiki-benchmark-and-profiling.md)
和 [wiki-ab-isolation-discipline](../../mtwiki/wiki/wiki-ab-isolation-discipline.md)。

### 16.4 性能演进记录：加入 state cost 后

测量日期：2026-10-18。

本节的 32-worker 数字对应已启用 state cost、并使用当前通用 commit/memory dispatch
入口的 XiangShan 生成物。曾经尝试过把 commit 和 memory flush 拆成每 worker wrapper；
该版本使完整 XiangShan Host time 从约 27.076 s 降到约 28.398 s，约慢 4.88%，
因此已经回退。小模型功能回归通过，但大模型的代码体积和 instruction-cache 代价超过
了去掉部分路由的收益。随后已重新生成、编译完整 XiangShan 模型，并确认生成物恢复
为 `rcCommitPart(int)`/`rcFlushMem(int)`；一次完整 workload 回退验证得到
`Host time spent: 27215ms`，且 `HIT GOOD TRAP`、guest cycles、instruction count
和 cycle count 均一致。当前性能数字不应解释为 wrapper 拆分的收益。

这次版本对比使用相同的 XiangShan CoreMark workload、`GSIM_THREADS=32`、
`GSIM_MT_CPU_AFFINITY=auto` 和 CPU 64–95。两版都得到相同的
`instrCnt=663758`、`cycleCnt=290601`、`Guest cycle spent=290605` 和 `HIT GOOD TRAP`。

旧版是只按组合 structural cost 划分的生成物；新版本在 sink group 权重中加入
`regsrc` state cost。旧版结果来自 [phase-analysis-current-2a30xud4](../build/xiangshan-repcut/phase-analysis-current-2a30xud4)，
新版本结果来自 [phase-analysis-state-current](../build/xiangshan-repcut/phase-analysis-state-current)。

完整 workload 的 Host time 对比：

| 版本 | 有效对照运行 | 平均 Host time | 变化 |
| --- | ---: | ---: | ---: |
| structural-only | 3 | 28.957 s | 基线 |
| state-aware | 2 | 27.076 s | **-6.50%** |

当前运行次数少于旧版，且运行跨越不同会话；这个数字用于记录当前优化的方向和量级，
不是最终统计结论。插桩 trace 的两次平均为 26.997 s，与 state-aware 对照的 27.076 s
相差约 0.29%，说明阶段计时探针没有造成明显的额外扰动。

稳态模型周期（模型周期 >=20,000，16,920 个对齐样本）的结果为：

| 指标 | structural-only | state-aware | 变化 |
| --- | ---: | ---: | ---: |
| mean cycle span | 100.506 μs | 94.312 μs | -6.16% |
| median | 101.995 μs | 99.110 μs | -2.83% |
| P95 | 149.090 μs | 133.760 μs | -10.28% |
| P99 | 158.149 μs | 142.014 μs | -10.20% |
| sampled maximum | 236.940 μs | 168.090 μs | -29.06% |

这里的周期 span 是主线程从相邻 `step()` 边界测得的墙钟时间。它包含 worker 计算、
barrier、join、harness 和调度抖动；不能把所有下降都归因于某一条代码路径。

主线程 worker 0 的阶段分解如下。它是 worker 0 自己的时间，不是所有 worker 的最大值：

| 阶段 | structural-only | state-aware |
| --- | ---: | ---: |
| register commit | 0.991 μs | 16.559 μs |
| state barrier wait | 27.350 μs | 3.426 μs |
| merge active | 0.474 μs | 4.571 μs |
| active evaluate | 4.864 μs | 25.077 μs |
| join wait | 64.782 μs | 42.603 μs |
| cycle span | 100.506 μs | 94.312 μs |

state-aware 版本把更多提交和计算工作放到了 worker 0，但显著减少了它在 state barrier
和周期末 join 上等待其他 worker 的时间。判断负载是否改善必须看所有 worker 的完成尾部，
不能只看 worker 0 的局部阶段。

新版本的 32-worker 阶段表如下。`B0` 是提交后状态屏障等待，`merge` 是远端 active
pending 合并；“最后提交/求值”表示该 worker 在所有稳态采样周期中最后完成对应阶段的比例。

| W | CPU | commit μs | B0 μs | merge μs | eval μs | last commit | last eval |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 0 | 64 | 16.56 | 3.43 | 4.57 | 25.08 | 11.35% | 0.02% |
| 1 | 65 | 0.96 | 19.21 | 0.44 | 4.76 | 0.01% | 0.05% |
| 2 | 66 | 2.62 | 17.54 | 1.23 | 6.09 | 0.00% | 0.00% |
| 3 | 67 | 5.44 | 14.72 | 1.92 | 23.17 | 0.00% | 0.05% |
| 4 | 68 | 12.34 | 7.85 | 3.22 | 7.95 | 0.00% | 0.00% |
| 5 | 69 | 11.12 | 9.06 | 3.50 | 46.08 | 0.00% | 22.45% |
| 6 | 70 | 16.30 | 3.88 | 3.48 | 42.64 | 14.10% | 1.58% |
| 7 | 71 | 14.02 | 6.16 | 4.15 | 52.94 | 1.06% | 15.21% |
| 8 | 72 | 1.23 | 18.70 | 1.25 | 1.66 | 0.00% | 0.01% |
| 9 | 73 | 7.46 | 12.54 | 6.08 | 6.28 | 0.00% | 0.00% |
| 10 | 74 | 4.76 | 15.24 | 6.30 | 9.62 | 0.01% | 0.01% |
| 11 | 75 | 0.90 | 19.09 | 2.90 | 21.65 | 0.01% | 13.52% |
| 12 | 76 | 3.41 | 16.63 | 1.92 | 12.32 | 0.01% | 0.00% |
| 13 | 77 | 3.73 | 16.32 | 2.25 | 8.30 | 0.00% | 0.00% |
| 14 | 78 | 5.95 | 14.10 | 3.68 | 3.39 | 0.00% | 0.00% |
| 15 | 79 | 1.98 | 18.05 | 2.19 | 1.76 | 0.00% | 0.00% |
| 16 | 80 | 2.68 | 17.37 | 0.44 | 7.21 | 0.01% | 0.00% |
| 17 | 81 | 11.68 | 8.37 | 4.24 | 49.86 | 0.00% | 14.47% |
| 18 | 82 | 18.79 | 1.27 | 3.87 | 26.18 | 73.37% | 0.98% |
| 19 | 83 | 13.08 | 6.97 | 1.19 | 17.88 | 0.06% | 0.09% |
| 20 | 84 | 1.98 | 18.10 | 0.84 | 34.09 | 0.00% | 0.46% |
| 21 | 85 | 1.04 | 18.96 | 2.19 | 57.49 | 0.00% | 28.44% |
| 22 | 86 | 6.30 | 13.78 | 3.62 | 38.11 | 0.01% | 2.67% |
| 23 | 87 | 9.42 | 10.66 | 6.46 | 15.54 | 0.00% | 0.01% |
| 24 | 88 | 0.15 | 19.80 | 0.50 | 10.58 | 0.00% | 0.00% |
| 25 | 89 | 2.67 | 17.31 | 1.47 | 12.64 | 0.00% | 0.00% |
| 26 | 90 | 0.46 | 19.55 | 0.48 | 0.81 | 0.00% | 0.00% |
| 27 | 91 | 0.85 | 19.12 | 3.60 | 6.57 | 0.00% | 0.00% |
| 28 | 92 | 0.27 | 19.73 | 0.59 | 1.53 | 0.00% | 0.00% |
| 29 | 93 | 1.49 | 18.54 | 0.61 | 6.38 | 0.01% | 0.00% |
| 30 | 94 | 1.21 | 18.85 | 0.62 | 1.67 | 0.00% | 0.00% |
| 31 | 95 | 11.38 | 8.65 | 5.45 | 10.99 | 0.00% | 0.00% |

这组数据显示当前优化已经改变了瓶颈形状：

1. 旧版提交拖尾主要是 worker 15、5、27；新版本变为 worker 18、6、0，最后提交比例分别为 73.37%、14.10%、11.35%。
2. 新版提交不再由旧的三个 worker 集中承担，但 worker 18 仍然是明显的 state commit tail，需要继续分析其动态活跃率和提交代码大小。
3. 求值拖尾变为 worker 21、5、7、17，最后求值比例合计 80.57%；state-aware 划分改善了 commit barrier，却把 eval tail 重新分布到了新的 sink groups。
4. 新版 `combined_cost` 最大/平均约 2.61；旧版 structural cost 最大/平均约 2.99。新版本的 `state_cost` 最大/平均约 1.95，说明静态寄存器数量已经比旧版更均衡，但复制后的 structural cost 仍会制造新的大分区。
5. 新版测得的 useful worker time 占线程墙钟约 28.22%，旧版约 24.57%；这是阶段计时分类，不等于硬件利用率，但说明等待占比下降、有效工作占比上升。

完整逐周期数据和生成快照保存在：

- [state-aware summary.json](../build/xiangshan-repcut/phase-analysis-state-current/summary.json)
- [state-aware worker-summary.csv](../build/xiangshan-repcut/phase-analysis-state-current/worker-summary.csv)
- [state-aware worker-phase-statistics.csv](../build/xiangshan-repcut/phase-analysis-state-current/worker-phase-statistics.csv)
- [state-aware trace-0 worker phases](../build/xiangshan-repcut/phase-analysis-state-current/trace-0/worker-phases.csv.gz)
- [state-aware trace-1 worker phases](../build/xiangshan-repcut/phase-analysis-state-current/trace-1/worker-phases.csv.gz)
- [state-aware partition report](../build/xiangshan-repcut/gsim-compile/model/repcut-partitions.csv)

后续性能比较必须继续沿用这套口径：完整 workload、相同 CPU 集合、至少三次正式对照，
同时记录周期分布和每个 worker 的阶段尾部。当前结果已经证明 state cost 值得保留，
下一步重点从“是否加入 state cost”转为“如何继续平衡 state commit、active merge 和新的 eval tail”。

### 16.5 当前版本的线程内阶段占比

下面的百分比以每个 worker 自己的 `cycle_span` 为分母，因此每行约为 100%。
`epoch/空等` 是上一个 `rcCycle` 结束到当前 `rcCycle` 开始之间的时间，包含等待
下一 epoch、主线程调用边界和调度间隔；它不是单独的 barrier 时间。`reset` 合并了
async reset 和 B1 等待，memory 和 async reset 的占比都很小。

| 线程 | epoch/空等 | memory | commit | B0 | reset | merge | eval |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 0 | 46.2% | 0.1% | 17.6% | 3.6% | 1.1% | 4.9% | 26.6% |
| 1 | 71.9% | 0.0% | 1.0% | 20.4% | 1.1% | 0.5% | 5.1% |
| 2 | 69.6% | 0.0% | 2.8% | 18.6% | 1.1% | 1.3% | 6.5% |
| 3 | 50.9% | 0.1% | 5.8% | 15.6% | 1.0% | 2.0% | 24.6% |
| 4 | 65.5% | 0.0% | 13.1% | 8.3% | 1.1% | 3.4% | 8.4% |
| 5 | 25.0% | 0.1% | 11.8% | 9.6% | 1.0% | 3.7% | 48.8% |
| 6 | 28.5% | 0.0% | 17.3% | 4.1% | 1.1% | 3.7% | 45.2% |
| 7 | 16.9% | 0.1% | 14.9% | 6.5% | 1.1% | 4.4% | 56.2% |
| 8 | 74.6% | 0.2% | 1.3% | 19.8% | 1.1% | 1.3% | 1.8% |
| 9 | 64.5% | 0.1% | 7.9% | 13.3% | 1.1% | 6.4% | 6.7% |
| 10 | 60.7% | 0.1% | 5.0% | 16.2% | 1.1% | 6.7% | 10.2% |
| 11 | 51.6% | 0.1% | 1.0% | 20.2% | 1.0% | 3.1% | 23.0% |
| 12 | 62.5% | 0.1% | 3.6% | 17.6% | 1.1% | 2.0% | 13.1% |
| 13 | 66.5% | 0.1% | 3.9% | 17.3% | 1.0% | 2.4% | 8.8% |
| 14 | 70.1% | 0.1% | 6.3% | 14.9% | 1.1% | 3.9% | 3.6% |
| 15 | 73.5% | 0.1% | 2.1% | 19.1% | 1.1% | 2.3% | 1.9% |
| 16 | 69.5% | 0.1% | 2.8% | 18.4% | 1.1% | 0.5% | 7.6% |
| 17 | 20.2% | 0.1% | 12.4% | 8.9% | 1.1% | 4.5% | 52.9% |
| 18 | 45.6% | 0.1% | 20.0% | 1.3% | 1.1% | 4.1% | 27.8% |
| 19 | 57.3% | 0.1% | 13.9% | 7.4% | 1.0% | 1.3% | 19.0% |
| 20 | 40.4% | 0.1% | 2.1% | 19.2% | 1.1% | 0.9% | 36.2% |
| 21 | 14.2% | 0.1% | 1.1% | 20.1% | 1.0% | 2.3% | 61.0% |
| 22 | 33.2% | 0.1% | 6.7% | 14.6% | 1.1% | 3.8% | 40.4% |
| 23 | 54.2% | 0.0% | 10.0% | 11.3% | 1.1% | 6.9% | 16.5% |
| 24 | 65.9% | 0.1% | 0.2% | 21.0% | 1.1% | 0.5% | 11.2% |
| 25 | 62.6% | 0.1% | 2.8% | 18.4% | 1.1% | 1.6% | 13.4% |
| 26 | 76.2% | 0.1% | 0.5% | 20.7% | 1.1% | 0.5% | 0.9% |
| 27 | 66.9% | 0.1% | 0.9% | 20.3% | 1.0% | 3.8% | 7.0% |
| 28 | 75.3% | 0.1% | 0.3% | 20.9% | 1.1% | 0.6% | 1.6% |
| 29 | 70.3% | 0.1% | 1.6% | 19.6% | 1.0% | 0.6% | 6.8% |
| 30 | 75.2% | 0.1% | 1.3% | 20.0% | 1.1% | 0.7% | 1.8% |
| 31 | 60.2% | 0.1% | 12.1% | 9.2% | 1.1% | 5.8% | 11.6% |

这张表说明当前主要剩余问题不是 memory 或 reset，而是两类负载的相互错位：
worker 18 的 commit 占比最高，worker 21/7/17 的 eval 占比最高；大量其他 worker
则主要在等待 epoch 或 B0。后续调优应同时观察这两类尾部，不能只优化某一个阶段。

## 17. 当前限制和后续方向

当前实现有以下已知限制：

1. state cost 是静态启发式，不含 workload-specific active rate；当前 workload 已观察到性能提升，不能据此推断所有 workload 都会提升；
2. RepCut 分区在生成阶段固定，运行时没有动态迁移或 work stealing；
3. `commitPart` 仍可能扫描大量 inactive guard，代码大小与真实活跃工作不完全一致；
4. active pending bitmap 的容量不等于每周期实际 OR 字节数；
5. 复制可能让某些 partition 的 eval tail 变长，必须观察实际 worker makespan；
6. 当前 runtime 使用 pause spin，过小设计可能被 barrier 及 cache coherence 成本主导；
7. reset、memory、DPI 和 printf/assert 仍是并行化边界，不能为了平衡而删除顺序边；
8. 不同 workload 可能激活完全不同的寄存器和组合 cone，单一 profile 不一定泛化。

可按以下顺序推进：

1. 在 commit/eval emitter 中增加轻量计数，获得 profile-guided state 和 active cost；
2. 用 profile 修正 `state_cost`，并比较不同 `GSIM_REPCUT_STATE_WEIGHT`；
3. 让划分器同时约束 `combined_cost` 和复制后的最大 worker tail；
4. 优化 owner 内的 commit active scan 和 pending bitmap merge；
5. 研究只在可证明的 enable cone 上做 active/dense 混合跳过；
6. 只有在完整正确性和 barrier 语义仍成立时，才减少同步或扩大复制范围。

## 18. 与论文 RepCut 和 GSIM dense MT 的区别

论文 RepCut 的经典执行模型是 activity-oblivious：复制组合逻辑后，每个线程每周期
完整执行自己的分区，并在周期阶段之间同步。当前 GSIM 实现保留了其交集超图和复制
思想，但运行时使用 active flags，因此可以跳过未变化的 SuperNode，同时引入 active
传播、远端 pending 和 state commit 逻辑。

与 `--mt-mode=on` 的 dense MTask 后端相比：

| 特征 | RepCut + active | dense MTask |
| --- | --- | --- |
| 基本划分 | sink cone / intersection hypergraph | Node 收缩为 MTask DAG |
| 组合复制 | 跨 sink group 的纯组合节点 | 仅在 RHEFT 候选中局部复制 |
| 每周期执行 | active SuperNode 才执行 | 每个有效 MTask 都执行 |
| 跨 worker 同步 | 周期 barrier + pending active | task wait/store token |
| 状态 owner | `regsrc` 跟随 `regdst` partition | 独立 state task owner |
| memory | snapshot log + commit flush | pending write + state barrier |
| 动态调度 | 无迁移、无 stealing | 固定 worker chain |
| 主要风险 | active tail、commit imbalance、bitmap merge | dense 重算、token 和 barrier 开销 |

两条路径共享通用 Node 图和 C++ 编译环境，但不能把一个路径的成本数字直接当作另
一个路径的性能上界。

## 19. 混合 active cost 的重新划分方案（设计）

本节是下一步方案，目前尚未进入 `src/repCut.cpp`。当前实现已经加入静态
`regsrc/regdst` state cost；本方案进一步把 active workload 纳入 sink group 和
hyperedge 的成本，同时保留 structural cost 作为安全下界。

### 19.1 为什么不能直接把未激活节点成本设为零

`evalPart<part>()` 会按固定顺序检查每个 SuperNode 的 active flag。即使主体不执行，
仍然有 guard、分支预测和 flag load 成本。因此一个 SuperNode 的运行成本应拆成：

```text
runtime_cost(super)
  = guard_scan_cost(super)
  + active_probability(super) × body_cost(super)
  + value_change_probability(super) × propagation_cost(super)
```

如果把 inactive SuperNode 完全计为零，划分器会把大量低频逻辑集中到少数 worker，
最后由 guard 扫描、active merge 或下一阶段 barrier 暴露出来。

### 19.2 profile 字段

在可选 profile 模式下，每个 worker 使用线程本地计数器记录以下字段，避免原子操作
进入热路径：

| 字段 | 含义 |
| --- | --- |
| `guard_checks[super]` | 该 SuperNode 的 active guard 被检查次数 |
| `active_hits[super]` | active body 实际执行次数 |
| `value_changes[node]` | 节点值发生变化并触发传播的次数 |
| `remote_activations[node]` | 产生远端 pending activation 的次数 |
| `commit_hits[regsrc]` | register commit body 执行次数 |
| `commit_changes[regsrc]` | commit 后当前值发生变化的次数 |
| `memory_writes[port]` | memory writer 实际产生 log 的次数/字节数 |

统计窗口应跳过初始化和 reset warmup，只采集稳定 workload。多个 workload 可以按运行
次数或代表性权重合并，避免只针对一个程序过拟合。

对每个对象使用平滑概率：

```text
p_active(s) = (active_hits(s) + κ × p_prior)
              / (sampled_cycles + κ)

p_change(n) = (value_changes(n) + κ × q_prior)
              / (sampled_cycles + κ)
```

`κ` 用于避免短窗口把对象估计成绝对零成本。没有 profile 时使用 `p_active=1`、
`p_change=1`，即退化为当前 structural upper-bound 模型。

### 19.3 SuperNode 和 state 的混合成本

对一个可执行 SuperNode `s`，定义：

```text
L(s) = max(1, ceil(width_words / 64))
Cbody(s) = 当前 structural/lowered body cost
Cguard(s) = active flag load + branch 的固定成本
Cprop(s) = 本地/远端 activation 写入成本

Wbody(s) = (1 - λ) + λ × p_active(s)
Wchange(s) = (1 - λ) + λ × p_change(s)

Cactive(s) = Cguard(s)
            + Wbody(s) × Cbody(s)
            + Wchange(s) × Cprop(s)
```

`λ=0` 时完全回到静态结构模型；`λ=1` 时使用 profile 概率，但仍保留 guard 成本。
初始实验可以使用 `λ=0.7`，并把它作为生成器参数而不是硬编码常量。

寄存器提交沿用 `regsrc` 必须跟随 `regdst` 的语义约束，但把静态 state cost 改成：

```text
Cstate(regsrc) = Cscan(regsrc)
                 + Wbody(regsrc) × Ccompare_write(regsrc)
                 + Wchange(regsrc) × Cremote_activation(regsrc)
```

该成本附加到 `regdst` 所属 sink group，不能把 state source 独立迁移到其他 worker。

memory writer 使用：

```text
Cmemory(port) = Cguard(port)
              + p_write(port) ×
                (C_log_append + C_flush + C_reader_wake)
```

这样可以区分“每周期检查 writer enable”和“确实产生 deferred memory log”两种成本。

### 19.4 Sink vertex 和 hyperedge 的混合权重

对 sink group `g`，顶点权重为：

```text
V(g) = Σ Cactive(s in g)
     + Σ Cstate(regsrc paired with regdst in g)
     + Σ Cmemory(writer owned by g)
```

对共享组合 cluster `e`，hyperedge 权重为该 cluster 在额外 partition 中重算的成本：

```text
H(e) = Σ Cactive(s in e)
```

如果 hyperedge 被放入 `k` 个 partition，复制目标项仍为：

```text
(k - 1) × H(e)
```

如果一个共享节点在不同 sink 上的 active 概率差异很大，可以使用每个 pin 的 profile
概率估算：

```text
H(e) = Σ_s Cbody(s) × max(p_active(s, pin))
```

这样不会因为某个 consumer 很少激活，就错误地把复制到另一个高活跃 consumer 的成本
估成零。

### 19.5 划分和局部修复

第一阶段仍使用 KaHyPar/local km1：

```text
vertex weight = V(g)
hyperedge weight = H(e)
objective = balance(V) + km1(H)
```

由于 KaHyPar 只直接表达顶点平衡和 hyperedge cut，生成器在初始划分后增加一个受限
局部修复阶段：

1. 计算每个 partition 的 `V(g)` 总负载；
2. 对可移动 sink group 枚举少量候选目标 worker；
3. 重新计算最大 partition load、km1 和 state affinity 是否仍合法；
4. 按词典序比较：

   ```text
   (max_partition_load,
    total_hyperedge_replication,
    max_commit_load,
    stable_group_id)
   ```

5. 只接受能够改善该键的移动，直到没有合法改进或达到迭代上限。

不可复制资源、memory writer 资源和 `regsrc/regdst` 配对关系不能被局部修复破坏。

### 19.6 生成和 profile 迭代

建议采用两轮离线流程：

```text
第一次生成：structural + state 静态 cost
       ↓
运行代表性 workload，采集 active profile
       ↓
第二次生成：hybrid active/state cost + 局部修复
       ↓
逐周期正确性和性能 A/B
```

profile 必须绑定 FIRRTL 输入指纹、生成参数、线程数、CPU affinity 和 workload seed。
如果输入图或 worker 数变化，旧 profile 不应静默复用。

### 19.7 成功判据

这个方案不能只看 `structural_cost` 或复制节点数量。至少需要同时比较：

- 全局 Host time、mean/median/P95/P99 cycle span；
- commit barrier 的最后完成 worker；
- eval barrier/join 的最后完成 worker；
- 每 worker 的 `guard_checks`、`active_hits` 和 `value_changes`；
- state commit、active merge、memory flush 的阶段时间；
- 生成 binary `.text`、热点函数布局和 instruction-cache 事件；
- state/active 语义下的逐周期逐信号正确性。

如果 hybrid cost 降低了静态最大负载，却增加了代码体积、instruction-cache miss 或
复制后的 eval tail，则应判定为无效优化。当前 per-worker commit wrapper 实验已经
说明，减少一个小 dispatch 不足以抵消生成代码布局变差的代价。

## 20. 基于逐线程逐周期数据的优化优先级（2026-10-19）

本节使用 [phase-analysis-state-current/summary.json](../build/xiangshan-repcut/phase-analysis-state-current/summary.json)
和 [worker-summary.csv](../build/xiangshan-repcut/phase-analysis-state-current/worker-summary.csv)
中的 16,920 个稳态样本。该数据对应 state-aware 划分版本、32 worker、CPU 64–95、
CoreMark 完整 workload。

平均模型周期为 **94.312 μs**。主线程阶段为：

| 阶段 | 平均时间 |
| --- | ---: |
| register commit | 16.559 μs |
| state barrier | 3.426 μs |
| async reset | 0.362 μs |
| reset barrier | 0.661 μs |
| merge active | 4.571 μs |
| worker 0 evaluate | 25.077 μs |
| join wait | 42.603 μs |

`join wait` 主要是主线程等待其他 worker 的 eval tail，不是独立的计算阶段。逐 worker
的动态尾部是：

- commit：worker 18 在 73.37% 的采样周期最后完成，worker 6 为 14.10%，worker 0 为 11.35%；
- eval：worker 21 在 28.44% 的周期最后完成，worker 5 为 22.45%，worker 7 为 15.21%，worker 17 为 14.47%；
- worker 21、7、17、5 的平均 eval 分别为 57.485、52.942、49.864、46.078 μs；
- worker 26、8、15 等轻载线程有 70% 以上时间处于 epoch 等待或阶段等待。

因此当前周期的近似关键路径应写成：

```text
Tcycle ≈ max_p(commit_p)
       + B0
       + max_p(merge_p + eval_p)
       + B1
       + 主线程边界开销
```

这比把所有 worker 的时间相加更接近实际。当前 B0 和 B1 的平均值已经小于 eval tail；
直接更换 barrier 不是第一优先级。知识库也指出同步优化必须先区分负载不均衡、缓存一致性
和 barrier 本身，避免把最快线程的等待时间误认为 barrier 原语成本，见
[wiki-sync-overhead](../../mtwiki/wiki/wiki-sync-overhead.md) 和
[wiki-barrier-and-compiler](../../mtwiki/wiki/wiki-barrier-and-compiler.md)。

### 20.1 第一优先级：profile-aware 双阶段重新划分

当前 state-aware 划分已经降低了旧版 worker 15/5/27 的 commit 集中，但新的 eval tail
转移到了 worker 21/5/7/17。下一版划分不能再只最小化 `structural_cost + state_cost`，
而应直接优化两个阶段的最大负载：

```text
Ccommit(p) = static_state(p)
           + commit_guard_scan(p)
           + active_commit_body(p)
           + remote_pending_writes(p)

Ceval(p) = guard_scan(p)
         + active_body(p)
         + value_change_propagation(p)
         + active_merge(p)

objective = max_p Ccommit(p)
         + max_p Ceval(p)
         + replication_penalty
```

生成代码中增加线程本地计数器，不在热路径使用原子操作：

```text
guard_checks[super]
active_hits[super]
value_changes[node]
remote_activations[node]
commit_hits[regsrc]
commit_changes[regsrc]
merge_words[src][dst]
```

用代表性 workload 估计每个 SuperNode 的 active probability 和每个寄存器的 change
probability，再将这些概率加入 sink group 顶点权重和共享 cluster hyperedge 权重。`regsrc`
仍然必须跟随对应 `regdst`，不能为了平衡而迁移状态源。

KaHyPar 初始结果后增加受限局部修复：只移动不包含 anchored resource 的 sink group，
同时检查 `max(Ccommit)`、`max(Ceval)`、hyperedge replication 和 state affinity。这个
方向与知识库对关键路径和 makespan 的建议一致：拓扑图的最长路径决定并行上限，不能只
看割边或节点总数，见 [wiki-graph-algorithms](../../mtwiki/wiki/wiki-graph-algorithms.md)
和 [wiki-scheduling](../../mtwiki/wiki/wiki-scheduling.md)。

### 20.2 第二优先级：减少 active/commit 的无效 guard 扫描

当前 `evalPart` 和 `commitPart` 都按固定生成顺序检查 active flag。建议先做不改变图
语义的 word-level grouping：

```text
if (activeFlags[flag_word] != 0) {
    按原 cppId 顺序检查该 word 下的 SuperNode
}
```

这保留原有顺序和 activation 语义，只在整个 flag word 为空时跳过一组 guard。commit
和 eval 都分别做 A/B，避免同时改变两个阶段的代码布局。只有 profile 证明 guard scan
占主体时，才进一步尝试 partition-local active list；动态 list 需要去重、顺序和跨
partition pending 处理，复杂度更高。

### 20.3 第三优先级：稀疏 active merge

当前 `rcMergeCommitFlags` 在 dirty source 命中后，按目标 partition 的整个 flag word
范围执行 OR 和清零。建议在生成期为每个 `(src, dst)` 建立静态 touched-word 列表，
运行时只遍历这些 word；再用 profile 记录每周期真实触及的 word 数。

该优化不改变 barrier 或 active 语义，因为 commit 完成后 pending buffer 才被 consumer
读取。要同时观察：生成代码大小、pending word 数、merge 时间和 cache-line 访问；如果
列表展开使 `.text` 显著膨胀，应改成紧凑的 word-index 数组。

### 20.4 第四优先级：验证 barrier 原语和 L3 映射

CPU 64–95 位于同一 socket 的 4 个 L3 集群，每个集群 8 个 CPU。当前 barrier 是 worker
0 轮询 31 个 arrive flag、其他 worker 等一个 release flag。B0 平均 3.426 μs，B1 平均
0.661 μs，其中已经包含阶段不均衡，尚不能证明集中式 barrier 是主因。

先增加 barrier 的 arrival/release 时间戳，分离“最后一个 worker 迟到”和“最后一个到达
后仍等待 release”。只有后者明显时，才 A/B：

- 按 4 个 L3 集群做层级 barrier；
- 或使用对齐的 dissemination/MCS tree barrier；
- 保留 parity、phase 0/1 和 reset 顺序不变。

知识库推荐固定线程数的 RTL 仿真优先评估 dissemination 或 MCS tree，但也提醒 barrier
变体需要结合缓存拓扑实测，见 [wiki-barrier-and-compiler](../../mtwiki/wiki/wiki-barrier-and-compiler.md)。

### 20.5 第五优先级：代码布局和缓存验证

当前生成二进制的 `.text` 规模约 76 MB，属于 instruction-cache 需要重点测量的范围。
此前 per-worker commit/flush wrapper 实验虽然减少了部分 dispatch，却使完整 workload
约慢 4.88%，说明额外函数和代码布局成本超过了路由收益。后续应优先使用 PGO/LTO 或
worker-major 的函数布局实验，而不是继续增加 wrapper 数量。

每次 A/B 至少记录：

```text
instructions, cycles, branches, branch-misses
L1-icache-load-misses, LLC-load-misses
Host time, cycle span P95/P99
```

并用 `perf c2c` 检查 pending bitmap、barrier flag、join flag 和模型状态字段是否出现
false sharing。知识库对大型生成模型明确建议把热状态和冷元数据分开，并用 cache-line
对齐验证而不是假设布局正确，见 [wiki-cache-and-memory](../../mtwiki/wiki/wiki-cache-and-memory.md)。

### 20.6 不建议立即采用的方案

- 不建议直接把所有 active SuperNode 改成运行时 work stealing。当前跨 partition 依赖已
  被周期 barrier 和 pending activation 约束，细粒度 steal 会引入 CAS、间接调用、去重和
  cache 迁移；知识库建议仅对粗粒度 bundle、且在同一 NUMA/L3 内受限 stealing。
- 不建议直接做 N 周期 barrier。寄存器跨周期可见性、memory read-old/write commit、
  async reset、DPI/printf/assert 和外部观察都要求当前周期边界，不能把同步简单推迟。
- 不建议先优化 B1 或 async reset。当前 async reset 只有 0.362 μs，B1 只有 0.661 μs，
  远小于 eval tail。

### 20.7 建议的实验顺序

```text
1. 当前 state-aware 版本：增加 guard/active/change/merge-word 计数
2. 仅改 state-aware 权重：加入 profile active cost，重新划分
3. 仅改 commit word grouping
4. 仅改 eval word grouping
5. 仅改 sparse active merge
6. 若 barrier arrival/release 分解证明必要，再测试 L3-aware barrier
7. 最后做 PGO/LTO 和函数布局
```

每一步都使用相同 FIRRTL、`GSIM_THREADS=32`、CPU 64–95、完整 CoreMark workload，
至少三次正式运行，并检查 `HIT GOOD TRAP`、guest cycles、instruction count、CRC 和
PC。当前目录最近经历过中断的 XiangShan 生成尝试，新的性能 A/B 必须先清理并重新生成
完整 32-worker 模型；不能把部分生成物或 `GSIM_THREADS=1` 的中间结果作为基线。

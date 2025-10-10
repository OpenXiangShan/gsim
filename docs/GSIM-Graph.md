# GSIM 图结构概览

## Node
- `Node` 是电路图上的基本顶点，抽象了模块内外的所有信号、寄存器、内存端口等对象。
- 每个 `Node` 维护拓扑关系（`prev`/`next` 以及依赖 `depPrev`/`depNext`），并记录类型、位宽、符号位、复位信息、所属的 `SuperNode`、寄存器配对、时钟/复位来源等关键元数据。
- 图构建过程中的分析（如 AST 转图、寄存器拆分、内存推理、聚合类型展开）都会在 `Node` 上挂载额外信息，例如 `assignTree`、`valTree`、`resetTree` 以及聚合成员 `member` 和参数 `params`。

### Node 类型与必需成员
`NodeType` 当前共包含 16 种取值（`include/Node.h:15-30`），不同类型在构图阶段需要补齐的关键成员如下：

- **NODE_INVALID**
  - 仅作占位/错误检测使用；在有效图中出现会触发 `updatePrevNext` 的 `Panic`（`src/AST2Graph.cpp:1494-1496`），因此无需额外成员，但也不应真正实例化。

- **NODE_REG_SRC**
  - 必须与对应的 `NODE_REG_DST` 通过 `regNext` 互指（`include/Node.h:170-193`），绑定发生于寄存器创建时（`src/AST2Graph.cpp:632-641`）。
  - 需要记录驱动时钟 `clock`（`src/AST2Graph.cpp:641`）以及复位条件/复位值，分别写入 `resetCond` 与 `resetVal`，复位表达树随后在优化阶段复用（`src/AST2Graph.cpp:645-668`, `src/mergeNodes.cpp:240-265`）。
  - 所有寄存器更新逻辑最终压入 `assignTree`，配合 `updateDep` 建立依赖关系（`src/AST2Graph.cpp:1555-1569`, `src/Node.cpp:46-78`）。

- **NODE_REG_DST**
  - `regNext` 指回源寄存器，以便 `getSrc()` 等 API 取回成对节点（`include/Node.h:170-193`）。
  - 必须与源节点共享同一 `clock`，由寄存器定义阶段同步设置（`src/AST2Graph.cpp:632-641`）。
  - 承载下一拍值表达式：`whenConnect` 会把赋值树挂在 `valTree`/`assignTree` 上，并通过 `updateConnect` 建图（`src/AST2Graph.cpp:1123-1143`, `src/AST2Graph.cpp:1555-1569`, `src/Node.cpp:7-26`）。

- **NODE_SPECIAL**
  - 表示 `printf`/`assert`/`stop` 等特殊语句；构图时直接生成对应的 `valTree`（`src/AST2Graph.cpp:1176-1269`），随后进入 `assignTree` 供调度与代码生成使用（`src/AST2Graph.cpp:1555-1569`）。
  - 需要登记到 `graph::specialNodes` 以便后续专用处理（`src/AST2Graph.cpp:1204`, `src/AST2Graph.cpp:1237`, `src/AST2Graph.cpp:1267`）。

- **NODE_INP**
  - 由顶层或实例化端口生成，宽度/符号/维度信息通过 `updateInfo` 写入（`src/AST2Graph.cpp:247-251`），同时加入 `graph::input` 列表（`src/AST2Graph.cpp:265-269`）。
  - 作为纯源节点，`valTree` 必须保持为空或 `OP_INVALID`，`updatePrevNext` 会进行断言（`src/AST2Graph.cpp:1474-1478`）。

- **NODE_OUT**
  - 同样由端口流程创建并补齐类型信息（`src/AST2Graph.cpp:247-251`），并登记到 `graph::output`（`src/AST2Graph.cpp:265-269`）。
  - 输出驱动逻辑通过 `whenConnect` 生成 `valTree`，随后写入 `assignTree` 以参与拓扑关系构建（`src/AST2Graph.cpp:1123-1143`, `src/AST2Graph.cpp:1555-1569`, `src/Node.cpp:7-26`）。

- **NODE_MEMORY**
  - 表示内存本体，需要设置容量/读写延迟等元信息（`set_memory`，`include/Node.h:194-198`），并保存 `ruw` 等额外属性到 `extraInfo`（`src/AST2Graph.cpp:782-805`）。
  - 通过 `add_member` 维护所有端口节点，同时为端口设置 `parent` 反向引用（`src/AST2Graph.cpp:747-749`, `include/Node.h:199-207`）。

- **NODE_READER**
  - 由内存端口定义生成，`memTree` 需指向 `OP_READ_MEM` 根节点并携带所属内存指针（`src/AST2Graph.cpp:718-724`），`clock` 在端口解析时补齐（`src/AST2Graph.cpp:724`, `src/AST2Graph.cpp:733-748`）。
  - 端口加入所属内存的 `member` 列表，`parent` 因此被设置，用于在 `updateConnect` 中与写端口建立依赖（`src/AST2Graph.cpp:747-749`, `src/Node.cpp:27-41`）。
  - 读结果需回填到 `valTree` 并推入 `assignTree`，同步履行时序扩展（`src/AST2Graph.cpp:1555-1569`）。若内存读延迟为 1，还要插入地址寄存器（`src/AST2Graph.cpp:1559-1562`）。

- **NODE_WRITER**
  - 初始 `memTree` 保存写端口的地址信息，并在真正的连接语句里改写为 `OP_WRITE_MEM`，同时附加写数据（`src/AST2Graph.cpp:698-724`, `src/AST2Graph.cpp:1123-1137`）。
  - 需要与 `NODE_READER` 一样，通过 `member`/`parent` 维护所属内存关系，以支持端口间的前后序约束（`src/AST2Graph.cpp:747-749`, `src/Node.cpp:27-41`）。
  - 写请求的条件/数据表达式追加到 `assignTree` 供调度和代码生成使用（`src/AST2Graph.cpp:1123-1143`, `src/AST2Graph.cpp:1555-1569`）。

- **NODE_READWRITER**
  - 同一端口同时承担读写，初始 `memTree` 记录读操作，写路径在连接时附加并保持 `assignTree`（`src/AST2Graph.cpp:698-715`, `src/AST2Graph.cpp:1123-1143`）。
  - 端口需要完整的 `member`/`parent` 关联，且在需要时可能插入额外寄存器或条件树（`src/AST2Graph.cpp:747-749`, `src/AST2Graph.cpp:1559-1577`）。

- **NODE_INFER**
  - 代表待推断方向的内存端口，构建时使用 `OP_INFER_MEM` 占位（`src/AST2Graph.cpp:698-716`），并提前填充地址、时钟等信息。
  - 在 AST 收尾阶段被转换成 `NODE_READER`，同时把 `memTree` 调整为 `OP_READ_MEM` 并挪到 `assignTree`（`src/AST2Graph.cpp:1555-1566`）；因此构建阶段必须保证 `memTree`/`clock`/`parent` 已就绪。

- **NODE_OTHERS**
  - 通用的组合逻辑节点，用于普通连线、局部变量及条件节点等。必须补齐类型信息（`src/AST2Graph.cpp:597-606`, `src/AST2Graph.cpp:1299-1302`），并在连接时生成 `valTree`。
  - `assignTree` 在图构建末尾统一落地，随后由 `updateConnect` 推导图中依赖（`src/AST2Graph.cpp:1123-1143`, `src/AST2Graph.cpp:1555-1569`, `src/Node.cpp:7-26`）。

- **NODE_REG_RESET**
  - 复位通路上的虚拟寄存器，复用原寄存器的复位表达树，并通过 `regNext` 指回真实寄存器（`src/mergeNodes.cpp:240-249`）。
  - `assignTree` 需包含复位表达式，以便异步/同步复位超节点在生成代码时使用（`src/mergeNodes.cpp:240-265`）。

- **NODE_EXT_IN / NODE_EXT_OUT / NODE_EXT**
  - 外部模块主体 `NODE_EXT` 需记录实例化模块名到 `extraInfo`，并维护端口 `member` 与参数 `params`（`src/AST2Graph.cpp:546-568`）。
  - 每个外部端口节点在生成时补齐类型信息并加入 `member`，若端口是时钟则挂到 `extNode->clock`（`src/AST2Graph.cpp:550-557`）。输出端口还要建立 `valTree` 表达对外连接（`src/AST2Graph.cpp:579-584`）。
  - `member` 的顺序决定了 `OP_EXT_FUNC` 的参数排列，输入/输出节点分别作为调用参数与返回值占位（`src/AST2Graph.cpp:571-584`）。

> **提示**：内存与外设端口节点的 `member` 向量需要按 `ReaderMember` / `WriterMember` / `ReadWriterMember` 的索引约定存放字段（`include/Node.h:44-48`），以便 `get_port_clock()` 等接口直接定位时钟、数据、掩码成员（`include/Node.h:221-224`）。

### 邻接关系的构建：`updateConnect`
- 触发条件  
  - 除 `NODE_REG_SRC` 外，所有在 `assignTree` 中挂有表达式的节点都会在 `updatePrevNext` 阶段调用 `updateConnect`，从而补齐图上的前驱/后继（`src/AST2Graph.cpp:1474-1491`）。
- 表达式遍历  
  - 函数会把节点的每棵 `assignTree` 根结点及其 lvalue 子树入队，逐层遍历所有 `ENode`；一旦遇到叶子 `ENode` 携带实际 `Node` 指针，就把该前驱加入 `prev`，并把当前节点加入对方的 `next`（`src/Node.cpp:7-26`）。
- 内存端口互联  
  - 对 `NODE_READER` 节点，遍历所属内存的 `member` 列表，将同一内存的写端口作为下一跳，保证写操作在读之前调度（`src/Node.cpp:27-33`）。  
  - 对 `NODE_WRITER` 节点执行相反的连接，把读端口作为前驱，确保写之前先感知到所有读依赖（`src/Node.cpp:34-41`）。  
  - `NODE_READWRITER` 在前述遍历中同时得到两方向的连接，因为连接语句在写入时会将 `memTree` 和 `assignTree` 拓展成读/写双路径（`src/AST2Graph.cpp:1123-1143`）。
- 依赖结果  
  - `prev`/`next` 集合随后用于拓扑排序、激活路径分析和 `SuperNode` 连接；同时配合 `depPrev`/`depNext`（由 `updateDep` 等流程维护）共同描述节点间的显式与隐式依赖关系。

### 数据类型
- **整数/布尔**：`UInt` 与 `SInt` 在解析 FIRRTL 类型时通过 `TypeInfo::set_width` 与 `TypeInfo::set_sign` 落入 `TypeInfo`，随后 `Node::updateInfo` 将其写入节点的 `width`、`sign` 字段，单比特布尔量即 `width=1` 且 `sign=false`（`src/AST2Graph.cpp:241`, `src/Node.cpp:131`）。
- **时钟信号**：`Clock` 类型除设置 `width=1` 外，会把 `TypeInfo::clock` 标记为真，`updateInfo` 因此把 `isClock` 置为 `true` 并保留时钟属性，供后续时钟优化使用（`src/AST2Graph.cpp:300`, `src/Node.cpp:133`）。
- **复位类型**：同步/异步复位在解析时写入 `TypeInfo::reset`，节点的 `reset` 字段保存 `ResetType` 枚举（`UNCERTAIN`/`UINTRESET`/`ASYRESET`/`ZERO_RESET`），供复位合并与代码生成判断（`src/AST2Graph.cpp:302`, `include/common.h:119`, `src/Node.cpp:136`）。
- **数组（向量）**：`TypeInfo::addDim` 会把每一层向量长度记录在 `dimension`，`Node::updateInfo` 将其复制到节点，`isArray()`/`arrayEntryNum()` 等接口依赖该向量，`addArrayVal` 根据维度拆分逐元素赋值（`src/Node.cpp:167`, `include/Node.h:231`, `include/Node.h:303`, `src/Node.cpp:234`）。
- **聚合数组字段**：当数组出现在结构体字段中时，`TypeInfo::addDim` 会同步更新所有 `aggrMember`，确保每个叶子节点的 `dimension` 与父字段一致，防止聚合展开时丢失索引信息（`src/Node.cpp:167`）。

## ENode
- `ENode` 是表达式树上的节点，用来描述 `Node` 之间的运算关系及语句控制逻辑（例如算术、位运算、条件、访存、外设调用等）。
- 内部节点通过 `child` 形成树结构并携带 `opType` 指定操作类型，叶子节点可以关联实际的 `Node`（通过 `nodePtr`）、字面值或特定语句。
- `ENode` 负责在不同上下文下生成电路操作信息（如常量传播、指令生成、访存访问），因此还跟踪运算宽度、符号、时钟/复位属性及用于代码生成的唯一编号 `id`。

### ENode 概览与操作类型
- **构造来源**：AST 访问阶段针对每个表达式/语句创建 `ENode` 树并挂载到目标 `Node` 的 `valTree`/`assignTree`；常量传播、宽度推断、指令生成等 Pass 直接沿用该树进行分析（`src/AST2Graph.cpp:1115-1143`, `src/constantNode.cpp:922-996`, `src/inferWidth.cpp:170-330`）。
- **叶子与内部节点**：叶子 `ENode` 可以指向真实 `Node`（`new ENode(nodePtr)`），也可以保存字面量或特殊语句；内部节点通过 `child` 向量连接其操作数（`include/ExpTree.h:82-150`）。
- **操作枚举 `OPType`**（`include/ExpTree.h:11-75`）：
  - 条件/控制：`OP_MUX`、`OP_WHEN`、`OP_STMT_SEQ`、`OP_STMT_WHEN`、`OP_STMT_NODE`。
  - 算术与比较：`OP_ADD`、`OP_SUB`、`OP_MUL`、`OP_DIV`、`OP_REM`、`OP_LT`、`OP_LEQ`、`OP_GT`、`OP_GEQ`、`OP_EQ`、`OP_NEQ`。
  - 位运算与移位：`OP_AND`、`OP_OR`、`OP_XOR`、`OP_ANDR`、`OP_ORR`、`OP_XORR`、`OP_DSHL`、`OP_DSHR`、`OP_SHL`、`OP_SHR`、`OP_HEAD`、`OP_TAIL`、`OP_BITS`、`OP_BITS_NOSHIFT`、`OP_CAT`。
  - 类型/宽度变换：`OP_ASUINT`、`OP_ASSINT`、`OP_ASCLOCK`、`OP_ASASYNCRESET`、`OP_CVT`、`OP_PAD`、`OP_SEXT`、`OP_NEG`、`OP_NOT`。
  - 索引与数组：`OP_INDEX_INT`、`OP_INDEX`、`OP_GROUP`。
  - 内存/外设：`OP_READ_MEM`、`OP_WRITE_MEM`、`OP_INFER_MEM`、`OP_EXT_FUNC`。
  - 常量与特殊语句：`OP_INT`、`OP_PRINTF`、`OP_ASSERT`、`OP_EXIT`、`OP_RESET`、`OP_INVALID`、`OP_EMPTY`。
- **类型信息**：每个 `ENode` 记录结果的 `width`、`sign`、`isClock`、`reset`、`usedBit`，使宽度推断、常量折叠和代码生成在表达式层面保持类型一致（`include/ExpTree.h:82-147`）。

### valTree / assignTree 结构
- **基础形式**：两者都由 `ExpTree` 表示，包含一对 `(root, lvalue)`。`root` 指向右值表达式的根 `ENode`，`lvalue` 则是左值 `ENode`，记录目标信号及索引信息（`include/ExpTree.h:252-307`）。
- **valTree 角色**：构图阶段先把表达式临时挂在 `Node::valTree`，当连接/条件遍历完成后，再将其压入 `assignTree` 并清空 `valTree`；寄存器、输出、内存端口等节点均遵循这一流程（`src/AST2Graph.cpp:1100-1139`, `src/AST2Graph.cpp:1555-1569`）。
- **assignTree 角色**：`Node::assignTree` 汇总节点的所有可能赋值，每条记录代表一条更新语句，后续拓扑排序、常量传播、激活分析都遍历该集合（`include/Node.h:128-150`, `src/Node.cpp:7-43`）。
- **根节点示例**：常见为运算或控制 `ENode`（如 `OP_WHEN`、`OP_MUX`、`OP_ADD`、`OP_READ_MEM`），必要时嵌套 `OP_WHEN` 以表达条件赋值或复位路径（`src/AST2Graph.cpp:1123-1143`, `src/mergeNodes.cpp:240-265`）。
- **叶节点**：要么是 `ENode(nodePtr)` 指向具体 `Node`，要么是常量/特殊语句节点（`OP_INT`、`OP_PRINTF` 等），提供表达式的数据源或动作（`include/ExpTree.h:82-147`, `src/AST2Graph.cpp:1176-1269`）。
- **中间节点**：各类运算、类型变换、访存 `ENode` 通过 `child` 串联叶子与根，构成完整表达式；其子节点顺序即操作数顺序（`include/ExpTree.h:11-75`, `src/inferWidth.cpp:170-330`）。
- **数组/聚合左值**：若赋值目标带索引或字段，`lvalue` 自身是一棵子树，根为目标 `Node`，子节点为索引表达式，`splitArray`、`StmtTree` 等 pass 会解析这些索引以定位具体元素（`src/splitArray.cpp:20-256`, `src/StmtTree.cpp:154-208`）。

### OPType 逐项说明
通用约定：除特别说明外，`child[i]` 均为右值 `ENode`，需要在构图阶段递归补齐其型宽与符号；`nodePtr` 只在叶子节点上使用，`values`/`strVal` 用于保存操作所需的额外参数，`memoryNode` 仅对内存相关操作有效。

#### 条件与控制
- `OP_EMPTY`：占位节点，`child` 为空；用于构树时的临时标记，应在图生成结束前消解。
- `OP_MUX`：`child[0]` 为条件，`child[1]`/`child[2]` 为 true/false 分支，第三个子节点可为空代表缺省分支；根宽度需与分支一致。
- `OP_WHEN`：语义保持与 FIRRTL `when` 一致，子节点布局同 `OP_MUX`；常嵌套于 `assignTree` 表达条件赋值。
- `OP_STMT_SEQ`：用于 Statement Tree，`child` 数量可变，每个为顺序执行的语句节点。
- `OP_STMT_WHEN`：`child[0]` 条件，`child[1]` then 块，`child[2]` else 块（可为空），封装调度树中的条件区域。
- `OP_STMT_NODE`：Statement Tree 的叶子，`child` 为空，`nodePtr` 指向被调度的 `Node`。
- `OP_RESET`：2 个子节点，`child[0]` 为复位条件，`child[1]` 为复位值；用于寄存器复位路径。

#### 算术与比较（均为二元运算）
- `OP_ADD`、`OP_SUB`、`OP_MUL`、`OP_DIV`、`OP_REM`
- `OP_LT`、`OP_LEQ`、`OP_GT`、`OP_GEQ`、`OP_EQ`、`OP_NEQ`
上述运算都要求 2 个子节点（左、右操作数），根的宽度/符号由宽度推断写入；比较类输出 1 位无符号结果。

#### 位运算与移位
- `OP_AND`、`OP_OR`、`OP_XOR`：2 个子节点。
- 归约运算 `OP_ANDR`、`OP_ORR`、`OP_XORR`：1 个子节点，结果宽度为 1。
- 双目移位 `OP_DSHL`、`OP_DSHR`：2 个子节点，依次为被移值与移位量。
- 常量移位/截取：`OP_SHL`、`OP_SHR`、`OP_HEAD`、`OP_TAIL` 各只有 1 个子节点，移位/截取参数记录在 `values[0]`。
- 位切片：`OP_BITS`、`OP_BITS_NOSHIFT` 有 1 个子节点，`values[0]`/`values[1]` 分别表示高/低位。
- 拼接 `OP_CAT`：至少 2 个子节点，构图时按原操作数顺序加入。

#### 类型与宽度转换
- 一元转换：`OP_ASUINT`、`OP_ASSINT`、`OP_ASCLOCK`、`OP_ASASYNCRESET`、`OP_CVT`、`OP_SEXT`、`OP_NEG`、`OP_NOT` 均只有 1 个子节点；根节点的 clock/reset 属性根据转换目标设置。
- `OP_PAD`：1 个子节点，`values[0]` 保存目标宽度。

#### 索引与数组
- `OP_INDEX_INT`：无子节点，`values[0]` 为常量索引；常与 `lvalue` 搭配描述数组写入。
- `OP_INDEX`：1 个子节点，为动态索引表达式。
- `OP_GROUP`：子节点数量可变，用于把多路元素组合成数组/掩码向量。

#### 常量与特殊语句
- `OP_INT`：叶子节点，`strVal` 保存字面量字符串，`values` 可存解析值。
- `OP_INVALID`：叶子节点，表示未连接/未驱动的值。
- `OP_PRINTF`：`strVal` 为格式串，`child` 依次为条件和参数（数量可变）；通常嵌在 `OP_WHEN` 中。
- `OP_ASSERT`：2 个子节点，`child[0]` 为断言条件，`child[1]` 为使能；`strVal` 保存消息文本。
- `OP_EXIT`：1 个子节点（退出条件），`strVal` 保存退出码。

#### 内存与外设
- `OP_READ_MEM`：`memoryNode` 指向对应内存节点，`child[0]` 为地址表达式；结果宽度与内存数据口一致。
- `OP_WRITE_MEM`：基于读端模板复制生成；`child[0]` 仍为地址，其余子节点依次附加写数据、掩码等（至少包含写数据）。`memoryNode` 继承自源模板。
- `OP_INFER_MEM`：推断阶段的读端占位，与 `OP_READ_MEM` 相同，后续会被替换。
- `OP_EXT_FUNC`：外部函数/模块调用；`child` 依次为输入参数，若表示输出会把 `extNode` 作为第一个子节点；必要时在 `strVal` 或 `extraInfo` 中记录函数名。

#### 其他说明
- 可变子节点操作（如 `OP_GROUP`、`OP_STMT_SEQ`）应严格保持加入顺序，供后续 Pass 按索引解析。
- 带 `nodePtr` 的 `ENode`（例如 `new ENode(targetNode)`）都是叶子，`child` 为空，宽度/符号直接继承自目标 `Node`。

#include "cppEmitter-mt.h"

#include "common.h"
#include "mtTaskLowering.h"
#include "util.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace {

int positiveEnv(const char* name, int fallback) {
  const char* value = std::getenv(name);
  if (value == nullptr || value[0] == '\0') return fallback;
  char* end = nullptr;
  errno = 0;
  long parsed = std::strtol(value, &end, 10);
  return errno == 0 && end != value && *end == '\0' && parsed > 0 && parsed <= INT_MAX
             ? static_cast<int>(parsed)
             : fallback;
}

int resetChunkSize() {
  const char* value = std::getenv("GSIM_EMIT_RESET_CHUNK");
  if (value == nullptr || value[0] == '\0') return 4096;
  char* end = nullptr;
  long parsed = std::strtol(value, &end, 10);
  return end != value && *end == '\0' && parsed >= 256 && parsed <= INT_MAX ? static_cast<int>(parsed) : 0;
}
void appendValues(std::string& text, const std::vector<int>& values) {
  if (values.empty()) {
    text += "0";
    return;
  }
  for (size_t i = 0; i < values.size(); ++i) {
    if (i != 0) text += ',';
    text += std::to_string(values[i]);
  }
}
void collectReferencedNodes(ENode* root, std::set<Node*>& nodes) {
  if (root == nullptr) return;
  std::vector<ENode*> pending{root};
  while (!pending.empty()) {
    ENode* current = pending.back();
    pending.pop_back();
    if (current->nodePtr != nullptr) nodes.insert(current->nodePtr);
    for (ENode* child : current->child) {
      if (child != nullptr) pending.push_back(child);
    }
  }
}

std::string nodeDeclaration(Node* node) {
  std::string declaration = widthUType(node->width) + " " + node->name;
  for (int dimension : node->dimension) {
    declaration += "[" + std::to_string(upperPower2(dimension)) + "]";
  }
  return declaration;
}

}  // namespace

CppEmitterMt::CppEmitterMt(graph& graph)
    : graph_(graph),
      enabled_(globalConfig.MtMode),
      workerCount_(positiveEnv("GSIM_THREADS", 1)),
      targetTasks_(globalConfig.MtTargetTasks),
      resetChunk_(resetChunkSize()) {}

bool CppEmitterMt::enabled() const { return enabled_; }


void CppEmitterMt::prepare() {
  if (!enabled()) return;
  MtTaskPartitioner::build(graph_, static_cast<MtTaskPlan&>(*this), targetTasks_);
  MtTaskLowerer::generateStmtTrees(graph_, static_cast<MtTaskPlan&>(*this));
  MtWorkerBuilder::build(graph_, static_cast<MtTaskPlan&>(*this), static_cast<MtWorkerPlan&>(*this),
                         workerCount_, resetChunk_);
  buildRegisterStorageNames();
}

bool CppEmitterMt::isTaskLocal(Node* node) const {
  return localNodes_.find(node) != localNodes_.end();
}

bool CppEmitterMt::isWorkerLocal(Node* node) const {
  return workerLocalOwners_.find(node) != workerLocalOwners_.end();
}

int CppEmitterMt::workerLocalOwner(Node* node) const {
  auto owner = workerLocalOwners_.find(node);
  Assert(owner != workerLocalOwners_.end(), "missing worker-local owner for %s", node->name.c_str());
  return owner->second;
}

bool CppEmitterMt::isPackedRegister(Node* node) const {
  return packedRegisterNames_.find(node) != packedRegisterNames_.end();
}

std::string CppEmitterMt::packedRegisterName(Node* node) const {
  auto found = packedRegisterNames_.find(node);
  Assert(found != packedRegisterNames_.end(), "missing packed register name for %s",
         node->name.c_str());
  return found->second;
}

void CppEmitterMt::buildRegisterStorageNames() {
  packedRegisterNames_.clear();
  packedRegisterNamesByText_.clear();
  for (const StateUpdate& update : stateUpdates_) {
    const std::string source = "mtRegisterSrcW" + std::to_string(update.worker) + ".";
    const std::string destination = "mtRegisterDstW" + std::to_string(update.worker) + ".";
    for (Node* reg : update.registers) {
      Node* dst = reg->getDst();
      const std::string sourceName = source + reg->name;
      const std::string destinationName = destination + reg->name;
      Assert(packedRegisterNames_.emplace(reg, sourceName).second,
             "duplicate packed register source %s", reg->name.c_str());
      Assert(packedRegisterNames_.emplace(dst, destinationName).second,
             "duplicate packed register destination %s", dst->name.c_str());
      Assert(packedRegisterNamesByText_.emplace(reg->name, sourceName).second,
             "duplicate packed register source name %s", reg->name.c_str());
      Assert(packedRegisterNamesByText_.emplace(dst->name, destinationName).second,
             "duplicate packed register destination name %s", dst->name.c_str());
    }
  }
}

std::string CppEmitterMt::mapRegisterNames(const std::string& input) const {
  if (packedRegisterNamesByText_.empty()) return input;
  auto nameCharacter = [](unsigned char value) {
    return std::isalnum(value) || value == '_' || value == '$';
  };
  std::string output;
  output.reserve(input.size());
  bool quoted = false;
  char quote = 0;
  for (size_t index = 0; index < input.size();) {
    const char value = input[index];
    if (quoted) {
      output.push_back(value);
      ++index;
      if (value == '\\' && index < input.size()) {
        output.push_back(input[index++]);
      } else if (value == quote) {
        quoted = false;
      }
      continue;
    }
    if (value == '\'' || value == '"') {
      quoted = true;
      quote = value;
      output.push_back(value);
      ++index;
      continue;
    }
    if (!nameCharacter(static_cast<unsigned char>(value))) {
      output.push_back(value);
      ++index;
      continue;
    }
    size_t end = index + 1;
    while (end < input.size() &&
           nameCharacter(static_cast<unsigned char>(input[end]))) {
      ++end;
    }
    const std::string token = input.substr(index, end - index);
    auto replacement = packedRegisterNamesByText_.find(token);
    output += replacement == packedRegisterNamesByText_.end() ? token : replacement->second;
    index = end;
  }
  return output;
}

const std::vector<Node*>& CppEmitterMt::emissionNodes() const { return emissionNodes_; }

void CppEmitterMt::emitText(int indent, bool canStartFile, const std::string& text) {
  graph_.__emitSrcMt(indent, canStartFile, true, nullptr, "%s", text.c_str());
}

void CppEmitterMt::emitHeaderPreamble(FILE* header) const {
  if (!enabled()) return;
  fprintf(header,
          "#include <atomic>\n#include <cstdio>\n#include <thread>\n#include <sched.h>\n"
          "#include <string>\n#include <utility>\n\n");
}

void CppEmitterMt::emitClassMembers(FILE* header) const {
  if (!enabled()) return;
  fprintf(header, "static constexpr int kMtWorkerCount = %d;\n", workerCount_);
  fprintf(header, "struct MtReadyToken { std::atomic<uint8_t> value{0}; };\n");
  fprintf(header, "struct alignas(64) MtDoneFlag { std::atomic<uint8_t> parity{0}; };\n");
  fprintf(header, "struct MtDispatch { void (S%s::*fn)(); uint32_t waitBegin, waitEnd, storeBegin, storeEnd; };\n",
          graph_.name.c_str());
  fprintf(header, "alignas(64) MtReadyToken mtReadyTokens[%d];\n", readySlotCount_);
  fprintf(header, "MtDoneFlag mtDoneFlags[%d];\n", std::max(1, workerCount_ - 1));
  fprintf(header, "std::vector<MtDeferredEvent> mtDeferredEventsByWorker[%d];\n",
          workerCount_);
  fprintf(header, "MtDoneFlag mtStateDoneFlags[%d];\n", std::max(1, workerCount_ - 1));
  fprintf(header, "MtDoneFlag mtStateReady;\n");
  fprintf(header, "std::vector<std::thread> mtThreads;\n");
  fprintf(header, "alignas(64) std::atomic<uint64_t> mtGeneration{0};\n");
  fprintf(header, "alignas(64) std::atomic<int> mtReadyWorkers{0};\n");
  fprintf(header, "alignas(64) std::atomic<bool> mtStop{false};\n");
  fprintf(header, "bool mtEnabled = false;\n");
  for (const StateUpdate& update : stateUpdates_) {
    fprintf(header, "struct MtRegisterBlockW%d {\n", update.worker);
    if (update.registers.empty()) {
      fprintf(header, "  uint8_t unused;\n");
    } else {
      for (Node* reg : update.registers) {
        fprintf(header, "  %s; // width = %d, lineno = %d\n",
                nodeDeclaration(reg).c_str(), reg->width, reg->lineno);
      }
    }
    fprintf(header, "};\n");
    fprintf(header, "alignas(64) MtRegisterBlockW%d mtRegisterSrcW%d{};\n",
            update.worker, update.worker);
    fprintf(header, "alignas(64) MtRegisterBlockW%d mtRegisterDstW%d{};\n",
            update.worker, update.worker);
    for (Node* reg : update.registers) {
      if (!reg->isReset()) continue;
      Assert(!reg->isArray() && reg->width <= BASIC_WIDTH,
             "%s is treated as reset (isArray: %d width: %d)",
             reg->name.c_str(), reg->isArray(), reg->width);
      fprintf(header, "%s %s$RESET{};\n", widthUType(reg->width).c_str(),
              reg->name.c_str());
    }
    for (Node* writer : update.memoryWriters) {
      fprintf(header, "alignas(64) uint64_t %s{};\n",
              mtMemoryWriteAddressName(writer).c_str());
      fprintf(header, "%s %s", widthUType(writer->width).c_str(),
              mtMemoryWriteDataName(writer).c_str());
      for (int dimension : writer->dimension) {
        fprintf(header, "[%d]", upperPower2(dimension));
      }
      fprintf(header, "{};\n");
      fprintf(header, "uint8_t %s", mtMemoryWriteValidName(writer).c_str());
      for (int dimension : writer->dimension) {
        fprintf(header, "[%d]", upperPower2(dimension));
      }
      fprintf(header, "{};\n");
    }
  }
  for (int worker = 0; worker < workerCount_; ++worker) {
    fprintf(header, "struct MtWorkerStateW%d {\n", worker);
    for (const auto& entry : workerLocalOwners_) {
      if (entry.second != worker) continue;
      fprintf(header, "  %s;\n", nodeDeclaration(entry.first).c_str());
    }
    fprintf(header, "};\n");
    fprintf(header, "alignas(64) MtWorkerStateW%d mtWorkerStateW%d{};\n", worker, worker);
  }
  fprintf(header, "static const uint32_t mtWaitSlots[%zu];\n", std::max<size_t>(1, waitSlots_.size()));
  fprintf(header, "static const uint32_t mtStoreSlots[%zu];\n", std::max<size_t>(1, storeSlots_.size()));
  for (int worker = 0; worker < workerCount_; ++worker) {
    fprintf(header, "static const MtDispatch mtDispatchW%d[%zu];\n", worker,
            std::max<size_t>(1, workerTasks_[static_cast<size_t>(worker)].size()));
  }
  fprintf(header, "void mtInit();\nvoid mtStart();\nvoid mtStopWorkers();\n");
  fprintf(header, "void mtWorkerLoop(int worker);\nvoid mtRunWorker(int worker, uint8_t parity);\n");
  fprintf(header, "void mtPinWorker(int worker);\nvoid stepMt();\n");
  for (const StateUpdate& update : stateUpdates_) {
    fprintf(header, "void mtUpdateStateW%d();\n", update.worker);
    for (int chunk = 1; chunk <= update.chunkCount; ++chunk) {
      fprintf(header, "void mtUpdateStateW%d_c%d();\n", update.worker, chunk);
    }
  }
  for (const Reset& reset : resets_) {
    fprintf(header, "void subResetMt%d();\n", reset.id);
    for (int chunk = 1; chunk <= reset.chunkCount; ++chunk) {
      fprintf(header, "void subResetMt%d_c%d();\n", reset.id, chunk);
    }
  }
  fprintf(header, "bool mtAnyAsyncResetAsserted();\n");
  fprintf(header, "void mtApplyAsyncResets();\n");
  fprintf(header, "void mtReplayAfterAsyncReset();\n");
  for (size_t taskId = 0; taskId < tasks_.size(); ++taskId) fprintf(header, "void mtTask%zu();\n", taskId);
  fprintf(header, "~S%s();\n", graph_.name.c_str());
}

void CppEmitterMt::emitConstructorInit() {
  if (enabled()) emitText(1, false, "mtInit();\n");
}

void CppEmitterMt::emitConstructorStart() {
  if (enabled()) {
    emitText(1, false, "if (mtEnabled) mtStart();\n");
  }
}

void CppEmitterMt::emitStateInitialization() {
  if (!enabled()) return;
  for (const StateUpdate& update : stateUpdates_) {
    if (!update.registers.empty()) {
      const std::string source = "mtRegisterSrcW" + std::to_string(update.worker);
      const std::string destination = "mtRegisterDstW" + std::to_string(update.worker);
      emitText(1, false, "memset(&" + source + ", 0, sizeof(" + source + "));\n");
      emitText(1, false,
               "memset(&" + destination + ", 0, sizeof(" + destination + "));\n");
    }
    for (Node* writer : update.memoryWriters) {
      const std::string valid = mtMemoryWriteValidName(writer);
      emitText(1, false, "memset(&" + valid + ", 0, sizeof(" + valid + "));\n");
    }
  }
}

void CppEmitterMt::emitResetBodyFunction(const std::string& name, const std::string& condition,
                                         const std::vector<Reset::Instruction>& body,
                                         int chunkCount) {
  const std::string className = "S" + graph_.name;
  emitText(0, true, "void " + className + "::" + name + "() {\n");
  int indent = 1;
  if (!condition.empty()) {
    emitText(indent++, false, "if (unlikely(" + condition + ")) {\n");
  }
  int emitted = 0;
  int chunk = 0;
  int nesting = 0;
  for (const Reset::Instruction& instruction : body) {
    const SuperInfo type = static_cast<SuperInfo>(instruction.type);
    const bool statement = type == SUPER_INFO_IF || type == SUPER_INFO_ELSE ||
                           type == SUPER_INFO_STR;
    if (resetChunk_ > 0 && statement && emitted >= resetChunk_ && nesting == 0) {
      emitText(indent, false, name + "_c" + std::to_string(chunk + 1) + "();\n");
      if (chunk == 0 && !condition.empty()) emitText(--indent, false, "}\n");
      emitText(--indent, false, "}\n");
      emitText(0, true, "void " + className + "::" + name + "_c" +
                            std::to_string(++chunk) + "() {\n");
      indent = 1;
      emitted = 0;
    }
    switch (type) {
      case SUPER_INFO_IF:
        emitText(indent++, false, mapRegisterNames(instruction.text) + "\n");
        ++nesting;
        break;
      case SUPER_INFO_ELSE:
        emitText(indent - 1, false, mapRegisterNames(instruction.text) + "\n");
        break;
      case SUPER_INFO_DEDENT:
        emitText(--indent, false, mapRegisterNames(instruction.text) + "\n");
        --nesting;
        break;
      case SUPER_INFO_STR:
        emitText(indent, false, mapRegisterNames(instruction.text) + "\n");
        break;
      case SUPER_INFO_ASSIGN_BEG:
      case SUPER_INFO_ASSIGN_END: break;
    }
    emitted += statement;
  }
  Assert(nesting == 0, "unbalanced reset instructions while emitting %s", name.c_str());
  Assert(chunk == chunkCount, "reset chunk mismatch for %s: planned %d emitted %d",
         name.c_str(), chunkCount, chunk);
  if (chunk == 0 && !condition.empty()) emitText(--indent, false, "}\n");
  emitText(--indent, false, "}\n");
}

void CppEmitterMt::emitAsyncResetFunction(const Reset& reset) {
  emitResetBodyFunction("subResetMt" + std::to_string(reset.id), "",
                        reset.body, reset.chunkCount);
}

void CppEmitterMt::emitStateUpdateFunction(const StateUpdate& update) {
  emitResetBodyFunction("mtUpdateStateW" + std::to_string(update.worker), "",
                        update.body, update.chunkCount);
}

void CppEmitterMt::emitInstructions(const std::vector<InstInfo>& instructions, int indent) {
  const int initialIndent = indent;
  for (const InstInfo& inst : instructions) {
    switch (inst.infoType) {
      case SUPER_INFO_IF: emitText(indent++, false, mapRegisterNames(inst.inst) + "\n"); break;
      case SUPER_INFO_ELSE:
        emitText(indent - 1, false, mapRegisterNames(inst.inst) + "\n");
        break;
      case SUPER_INFO_DEDENT:
        emitText(--indent, false, mapRegisterNames(inst.inst) + "\n");
        break;
      case SUPER_INFO_STR: emitText(indent, false, mapRegisterNames(inst.inst) + "\n"); break;
      case SUPER_INFO_ASSIGN_BEG:
      case SUPER_INFO_ASSIGN_END: break;
    }
  }
  Assert(indent == initialIndent, "unbalanced MTask instruction tree");
}

void CppEmitterMt::emitTask(const Task& task, int indent) {
  Assert(task.insts != nullptr, "MTask has no lowered instruction stream");
  Assert(!task.members.empty(), "cannot emit an empty MTask");

  for (Node* node : task.members) {
    if (task.localNodes.count(node) != 0) {
      emitText(indent, false, nodeDeclaration(node) + "{};\n");
    }
  }

  std::set<Node*> workerLocals;
  for (Node* node : task.members) {
    if (isWorkerLocal(node) && workerLocalOwner(node) == task.owner) workerLocals.insert(node);
    for (ExpTree* tree : node->assignTree) {
      std::set<Node*> referenced;
      collectReferencedNodes(tree->getRoot(), referenced);
      collectReferencedNodes(tree->getlval(), referenced);
      for (Node* reference : referenced) {
        if (isWorkerLocal(reference) && workerLocalOwner(reference) == task.owner) {
          workerLocals.insert(reference);
        }
      }
    }
  }
  for (Node* node : workerLocals) {
    const std::string state = "mtWorkerStateW" + std::to_string(task.owner) + "." + node->name;
    if (node->dimension.empty()) {
      emitText(indent, false, widthUType(node->width) + "& " + node->name + " = " + state + ";\n");
    } else {
      std::string alias = widthUType(node->width) + " (&" + node->name + ")";
      for (int dimension : node->dimension) {
        alias += "[" + std::to_string(upperPower2(dimension)) + "]";
      }
      emitText(indent, false, alias + " = " + state + ";\n");
    }
  }

  emitInstructions(*task.insts, indent);
}

void CppEmitterMt::emitDefinitions() {
  if (!enabled()) return;
  const std::string className = "S" + graph_.name;

  for (const StateUpdate& update : stateUpdates_) {
    emitStateUpdateFunction(update);
  }

  for (const Reset& reset : resets_) {
    emitAsyncResetFunction(reset);
  }

  for (size_t taskId = 0; taskId < tasks_.size(); ++taskId) {
    emitText(0, true, "void " + className + "::mtTask" + std::to_string(taskId) + "() {\n");
    emitTask(tasks_[taskId], 1);
    emitText(0, false, "}\n");
  }

  emitText(0, true, "bool " + className + "::mtAnyAsyncResetAsserted() {\n");
  std::string anyReset = "false";
  for (const Reset& reset : resets_) {
    Node* resetNode = reset.super->resetNode;
    const std::string condition = isPackedRegister(resetNode)
                                      ? packedRegisterName(resetNode)
                                      : resetNode->name;
    anyReset += " || " + condition;
  }
  emitText(1, false, "return " + anyReset + ";\n");
  emitText(0, false, "}\n");

  emitText(0, true, "void " + className + "::mtApplyAsyncResets() {\n");
  for (const Reset& reset : resets_) {
    Node* resetNode = reset.super->resetNode;
    const std::string condition = isPackedRegister(resetNode)
                                      ? packedRegisterName(resetNode)
                                      : resetNode->name;
    emitText(1, false, "if (unlikely(" + condition + ")) {\n");
    emitText(2, false, "subResetMt" + std::to_string(reset.id) + "();\n");
    emitText(1, false, "}\n");
  }
  emitText(0, false, "}\n");

  emitText(0, true, "void " + className + "::mtReplayAfterAsyncReset() {\n");
  for (size_t taskId = 0; taskId < tasks_.size(); ++taskId) {
    if (tasks_[taskId].kind == MtTaskKind::ExtModule) continue;
    emitText(1, false, "mtTask" + std::to_string(taskId) + "();\n");
  }
  emitText(0, false, "}\n");

  auto emitArray = [&](const std::string& type, const std::string& name, const std::vector<int>& values) {
    std::string text = "const " + type + " " + className + "::" + name + "[" +
                       std::to_string(std::max<size_t>(1, values.size())) + "] = {";
    appendValues(text, values);
    text += "};\n";
    emitText(0, true, text);
  };
  emitArray("uint32_t", "mtWaitSlots", waitSlots_);
  emitArray("uint32_t", "mtStoreSlots", storeSlots_);

  for (int worker = 0; worker < workerCount_; ++worker) {
    const std::vector<int>& chain = workerTasks_[static_cast<size_t>(worker)];
    const size_t dispatchCount = chain.size();
    std::string text = "const " + className + "::MtDispatch " + className + "::mtDispatchW" +
                       std::to_string(worker) + "[" +
                       std::to_string(std::max<size_t>(1, dispatchCount)) + "] = {\n";
    if (dispatchCount == 0) {
      text += "  {nullptr, 0, 0, 0, 0}\n";
    } else {
      for (int taskId : chain) {
        const Task& task = tasks_[static_cast<size_t>(taskId)];
        text += "  {&" + className + "::mtTask" + std::to_string(taskId) + "," +
                std::to_string(task.waitBegin) + "," + std::to_string(task.waitEnd) + "," +
                std::to_string(task.storeBegin) + "," + std::to_string(task.storeEnd) + "},\n";
      }
    }
    text += "};\n";
    emitText(0, true, text);
  }

  std::string runtime;
  runtime += "void " + className + "::mtInit() {\n";
  runtime += "  const char* executor = getenv(\"GSIM_MT_EXECUTOR\");\n";
  runtime += "  if (executor == nullptr) return;\n";
  runtime += "  if (strcmp(executor, \"dense\") != 0) return;\n";
  runtime += "  const char* threads = getenv(\"GSIM_THREADS\");\n";
  runtime += "  char* end = nullptr; long count = threads ? strtol(threads, &end, 10) : 1;\n";
  runtime += "  if (!threads || end == threads || *end != '\\0' || count != kMtWorkerCount) {\n";
  runtime += "    fprintf(stderr, \"[gsim-mt] runtime GSIM_THREADS must equal generated width %d\\n\", kMtWorkerCount); abort();\n";
  runtime += "  }\n  mtEnabled = true;\n}\n";

  runtime += "void " + className + "::mtPinWorker(int worker) {\n";
  runtime += "  const char* affinity = getenv(\"GSIM_MT_CPU_AFFINITY\");\n";
  runtime += "  if (!affinity || strcmp(affinity, \"auto\") != 0) return;\n";
  runtime += "  cpu_set_t allowed; if (sched_getaffinity(0, sizeof(allowed), &allowed) != 0) return;\n";
  runtime +=
      "  int selected = -1, index = 0;\n"
      "  for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {\n"
      "    if (CPU_ISSET(cpu, &allowed) && index++ == worker) { selected = cpu; break; }\n"
      "  }\n"
      "  if (selected < 0) return;\n"
      "  cpu_set_t target; CPU_ZERO(&target); CPU_SET(selected, &target);\n"
      "  if (sched_setaffinity(0, sizeof(target), &target) != 0)\n"
      "    fprintf(stderr, \"[gsim-mt] warning: failed to pin worker %d\\n\", worker);\n"
      "}\n";

  runtime += "void " + className + "::mtStart() {\n";
  runtime +=
      "  mtStop.store(false, std::memory_order_relaxed);\n"
      "  mtReadyWorkers.store(0, std::memory_order_relaxed);\n";
  runtime += "  mtThreads.reserve(kMtWorkerCount > 1 ? kMtWorkerCount - 1 : 0);\n";
  runtime +=
      "  for (int worker = 1; worker < kMtWorkerCount; ++worker)\n"
      "    mtThreads.emplace_back([this, worker]() { mtWorkerLoop(worker); });\n";
  runtime += "  mtPinWorker(0);\n";
  runtime +=
      "  while (mtReadyWorkers.load(std::memory_order_acquire) != kMtWorkerCount - 1)\n"
      "    __asm__ __volatile__(\"pause\" ::: \"memory\");\n"
      "}\n";

  runtime += "void " + className + "::mtStopWorkers() {\n";
  runtime +=
      "  if (mtThreads.empty()) return;\n"
      "  mtStop.store(true, std::memory_order_release);\n"
      "  mtGeneration.fetch_add(1, std::memory_order_release);\n"
      "  for (std::thread& thread : mtThreads) if (thread.joinable()) thread.join();\n"
      "  mtThreads.clear();\n"
      "}\n";
  runtime += className + "::~" + className + "() { mtStopWorkers(); }\n";

  runtime += "void " + className + "::mtWorkerLoop(int worker) {\n";
  runtime +=
      "  mtPinWorker(worker);\n"
      "  uint64_t seen = mtGeneration.load(std::memory_order_acquire);\n"
      "  mtReadyWorkers.fetch_add(1, std::memory_order_release);\n"
      "  for (;;) {\n"
      "    uint64_t generation;\n"
      "    do {\n"
      "      generation = mtGeneration.load(std::memory_order_acquire);\n"
      "      if (mtStop.load(std::memory_order_acquire)) return;\n"
      "      __asm__ __volatile__(\"pause\" ::: \"memory\");\n"
      "    } while (generation == seen);\n"
      "    if (mtStop.load(std::memory_order_acquire)) return;\n"
      "    mtRunWorker(worker, static_cast<uint8_t>(generation & 1));\n"
      "    mtDeferredEventsByWorker[worker] = mtTakeDeferredEvents();\n"
      "    mtDoneFlags[worker - 1].parity.store(static_cast<uint8_t>(generation & 1),\n"
      "                                         std::memory_order_release);\n"
      "    seen = generation;\n"
      "  }\n"
      "}\n";

  runtime += "void " + className + "::mtRunWorker(int worker, uint8_t parity) {\n";
  runtime += "  switch (worker) {\n";
  for (int worker = 0; worker < workerCount_; ++worker) {
    runtime += "    case " + std::to_string(worker) + ": mtUpdateStateW" +
               std::to_string(worker) + "(); break;\n";
  }
  runtime += "    default: abort();\n  }\n";
  runtime +=
      "  if (worker == 0) {\n"
      "    for (int other = 1; other < kMtWorkerCount; ++other) {\n"
      "      while (mtStateDoneFlags[other - 1].parity.load(std::memory_order_acquire) != parity)\n"
      "        __asm__ __volatile__(\"pause\" ::: \"memory\");\n"
      "    }\n"
      "    mtStateReady.parity.store(parity, std::memory_order_release);\n"
      "  } else {\n"
      "    mtStateDoneFlags[worker - 1].parity.store(parity, std::memory_order_release);\n"
      "    while (mtStateReady.parity.load(std::memory_order_acquire) != parity)\n"
      "      __asm__ __volatile__(\"pause\" ::: \"memory\");\n"
      "  }\n";
  runtime += "  const MtDispatch* entries = nullptr; uint32_t count = 0;\n  switch (worker) {\n";
  for (int worker = 0; worker < workerCount_; ++worker) {
    runtime += "    case " + std::to_string(worker) + ": entries = mtDispatchW" + std::to_string(worker) +
               "; count = " +
               std::to_string(workerTasks_[static_cast<size_t>(worker)].size()) +
               "; break;\n";
  }
  runtime += "    default: abort();\n  }\n";
  // A worker's task order is part of correctness: sortedSuper contains ordering
  // constraints that are not all represented by graph edges.
  runtime += "  for (uint32_t position = 0; position < count; ++position) {\n";
  runtime += "    const MtDispatch& entry = entries[position];\n";
  runtime +=
      "    for (uint32_t i = entry.waitBegin; i < entry.waitEnd; ++i) {\n"
      "      while (mtReadyTokens[mtWaitSlots[i]].value.load(std::memory_order_acquire) != parity)\n"
      "        __asm__ __volatile__(\"pause\" ::: \"memory\");\n"
      "    }\n";
  runtime += "    (this->*entry.fn)();\n";
  runtime +=
      "    for (uint32_t i = entry.storeBegin; i < entry.storeEnd; ++i)\n"
      "      mtReadyTokens[mtStoreSlots[i]].value.store(parity, std::memory_order_release);\n"
      "  }\n"
      "}\n";

  runtime += "void " + className + "::stepMt() {\n";
  for (Node* node : emissionNodes_) {
    if (node->isReset() && node->type == NODE_REG_SRC) {
      const std::string value =
          isPackedRegister(node) ? packedRegisterName(node) : node->name;
      runtime += "  " + node->name + "$RESET = " + value + ";\n";
    }
  }
  runtime +=
      "  uint64_t generation = mtGeneration.fetch_add(1, std::memory_order_release) + 1;\n"
      "  uint8_t parity = static_cast<uint8_t>(generation & 1);\n";
  runtime += "  mtRunWorker(0, parity);\n";
  runtime +=
      "  mtDeferredEventsByWorker[0] = mtTakeDeferredEvents();\n";
  runtime +=
      "  for (int worker = 1; worker < kMtWorkerCount; ++worker) {\n"
      "    while (mtDoneFlags[worker - 1].parity.load(std::memory_order_acquire) != parity)\n"
      "      __asm__ __volatile__(\"pause\" ::: \"memory\");\n"
      "  }\n"
      "  if (mtAnyAsyncResetAsserted()) {\n"
      "    for (int worker = 0; worker < kMtWorkerCount; ++worker)\n"
      "      mtDeferredEventsByWorker[worker].clear();\n"
      "    mtApplyAsyncResets();\n"
      "    mtReplayAfterAsyncReset();\n"
      "    mtDeferredEventsByWorker[0] = mtTakeDeferredEvents();\n"
      "  }\n"
      "  bool assertionFailed = false;\n"
      "  for (int worker = 0; worker < kMtWorkerCount; ++worker)\n"
      "    assertionFailed |= mtFlushDeferredEvents(mtDeferredEventsByWorker[worker]);\n"
      "  if (assertionFailed) {\n"
      "    assert(!\"deferred RTL assertion failure\");\n"
      "    abort();\n"
      "  }\n";
  runtime += "  ++cycles;\n}\n";
  emitText(0, true, runtime);
}

void CppEmitterMt::emitStep() {
  Assert(enabled(), "cannot emit MT step while MT is disabled");
  const std::string className = "S" + graph_.name;
  emitText(0, true, "void " + className + "::step() {\n");
  emitText(1, false,
           "if (!mtEnabled) { fprintf(stderr, \"[gsim-mt] this model requires "
           "GSIM_MT_EXECUTOR=dense and matching GSIM_THREADS\\n\"); abort(); }\n");
  emitText(1, false, "stepMt();\n");
  emitText(0, false, "}\n");
}


// Dense-specific copy of the common C++ model lowering. It emits declarations,
// initialization and interfaces; all evaluation is emitted by CppEmitterMt.
#ifdef DIFFTEST_PER_SIG
FILE* sigFileMt = nullptr;
#endif

#define RESET_NAME(node) (node->name + "$RESET")
#define emitFuncDecl(indent, ...) __emitSrcMt(indent, true, true, NULL, __VA_ARGS__)
#define emitBodyLock(indent, ...) __emitSrcMt(indent, false, false, NULL, __VA_ARGS__)

static std::set<Node*> definedNode;

extern int maxConcatNum;
bool nameExist(std::string str);

static void inline includeLib(FILE* fp, std::string lib, bool isStd) {
  std::string format = isStd ? "#include <%s>\n" : "#include \"%s\"\n";
  fprintf(fp, format.c_str(), lib.c_str());
}

static void inline newLine(FILE* fp) {
  fprintf(fp, "\n");
}

FILE* graph::genHeaderStartMt() {
  FILE* header = std::fopen((globalConfig.OutputDir + "/" + name + ".h").c_str(), "w");

  fprintf(header, "#ifndef %s_H\n#define %s_H\n", name.c_str(), name.c_str());
  /* include all libs */
  includeLib(header, "iostream", true);
  includeLib(header, "vector", true);
  includeLib(header, "assert.h", true);
  includeLib(header, "stdlib.h", true);
  includeLib(header, "cstdint", true);
  includeLib(header, "ctime", true);
  includeLib(header, "iomanip", true);
  includeLib(header, "cstring", true);
  includeLib(header, "map", true);
  includeLib(header, "cstdarg", true);
  newLine(header);

  fprintf(header, "\n// User configuration\n");
  fprintf(header, "//#define ENABLE_LOG\n");
  fprintf(header, "//#define RANDOMIZE_INIT\n");

  fprintf(header, "\nstruct MtDeferredEvent { bool assertion; std::string text; };\n");
  fprintf(header, "inline thread_local std::vector<MtDeferredEvent> mtDeferredEvents;\n");
  fprintf(header, "void mtDeferAssert(const char *fmt, ...);\n");
  fprintf(header, "std::vector<MtDeferredEvent> mtTakeDeferredEvents();\n");
  fprintf(header, "bool mtFlushDeferredEvents(std::vector<MtDeferredEvent>& events);\n");
  fprintf(header, "#define gAssert(cond, ...) do {"
                     "if (!(cond)) mtDeferAssert(__VA_ARGS__);"
                   "} while (0)\n");
  fprintf(header, "#define gdiv(a, b) ((b) == 0 ? 0 : (a) / (b))\n");

  fprintf(header, "#ifndef __BITINT_MAXWIDTH__\n");
  fprintf(header, "#error  BITINT support is required\n");
  fprintf(header, "#endif\n\n");

  /* There is some bugs with _BitInt in clang 18 */
  fprintf(header, "#ifdef __clang__\n");
  fprintf(header, "#if __clang_major__ < 19\n");
  fprintf(header, "#error  Please compile with clang 19 or above\n");
  fprintf(header, "#endif\n");
  fprintf(header, "#endif // __clang__ \n\n");

  fprintf(header, "#define likely(x) __builtin_expect(!!(x), 1)\n");
  fprintf(header, "#define unlikely(x) __builtin_expect(!!(x), 0)\n");
  fprintf(header, "void gprintf(const char *fmt, ...);\n\n");

  for (int num = 2; num <= maxConcatNum; num ++) {
    std::string param;
    for (int i = num; i > 0; i --) param += format(i == num ? "_%d" : ", _%d", i);
    std::string value;
    std::string type = widthUType(num * 64);
    for (int i = num; i > 1; i --) {
      value += format(i == num ? "((%s)_%d << %d) " : "| ((%s)_%d << %d)", type.c_str(), i, (i-1) * 64);
    }
    value += format("| ((%s)_1)", type.c_str());
    fprintf(header, "#define UINT_CONCAT%d(%s) (%s)\n", num, param.c_str(), value.c_str());
  }
  for (std::string str : extDecl) fprintf(header, "%s\n", str.c_str());
  newLine(header);
  return header;
}

void graph::genInterfaceInputMt(Node* input) {
  emitFuncDecl(0, "void S%s::set_%s(%s val) {\n", name.c_str(), input->name.c_str(), widthUType(input->width).c_str());
  emitBodyLock(1, "%s = val;\n", input->name.c_str());
  emitBodyLock(0, "}\n");
}

void graph::genInterfaceOutputMt(Node* output) {
  emitFuncDecl(0, "%s S%s::get_%s() {\n"
               "  return %s;\n"
               "}\n",
               widthUType(output->width).c_str(), name.c_str(),
               output->name.c_str(), output->status == CONSTANT_NODE ? output->computeInfo->valStr.c_str() : output->name.c_str());
}

#if defined(DIFFTEST_PER_SIG) && defined(GSIM_DIFF)
void graph::genDiffSigMt(FILE* fp, Node* node, const std::string& emittedName) {
  std::map<std::string, std::string> allNames;
  const std::string diffNodeName = emittedName.empty() ? node->name : emittedName;
  const std::string originName = node->name;
  if (node->type == NODE_MEMORY){

  } else if (node->isArray()) {
    int num = node->arrayEntryNum();
    std::vector<std::string> suffix(num);
    int pairNum = 1;
    for (size_t i = 0; i < node->dimension.size(); i ++) {
      int suffixIdx = 0;
      for (int l = 0; l < pairNum; l ++) {
        for (int j = 0; j < node->dimension[i]; j ++) {
          int suffixNum = num / node->dimension[i];
          for (int k = 0; k < suffixNum; k ++) {
            suffix[suffixIdx] += "[" + std::to_string(j) + "]";
            suffixIdx ++;
          }
        }
      }
      num = num / node->dimension[i];
      pairNum *= node->dimension[i];
    }
    for (size_t i = 0; i < suffix.size(); i ++) {
      allNames[diffNodeName + suffix[i]] = originName + suffix[i];
    }
  } else {
    allNames[diffNodeName] = originName;
  }
  for (auto iter : allNames)
    fprintf(sigFileMt, "%d %d %s %s\n", node->sign, node->width,
            iter.first.c_str(), iter.second.c_str());
}
#endif

#if defined(DIFFTEST_PER_SIG) && defined(VERILATOR_DIFF)
void graph::genDiffSigMt(FILE* fp, Node* node, const std::string& emittedName) {
  std::string verilatorName = name + "__DOT__" + node->name;
  size_t pos;
  while ((pos = verilatorName.find("$$")) != std::string::npos) {
    verilatorName.replace(pos, 2, "_");
  }
  while ((pos = verilatorName.find("$")) != std::string::npos) {
    verilatorName.replace(pos, 1, "__DOT__");
  }
  std::map<std::string, std::string> allNames;
  const std::string diffNodeName = emittedName.empty() ? node->name : emittedName;
  std::string originName = node->name;
  if (node->type == NODE_MEMORY){

  } else if (node->isArray()) {
    int num = node->arrayEntryNum();
    std::vector<std::string> suffix(num);
    std::vector<std::string> verilatorSuffix(num);
    int pairNum = 1;
    for (size_t i = 0; i < node->dimension.size(); i ++) {
      int suffixIdx = 0;
      for (int l = 0; l < pairNum; l ++) {
        for (int j = 0; j < node->dimension[i]; j ++) {
          int suffixNum = num / node->dimension[i];
          for (int k = 0; k < suffixNum; k ++) {
            verilatorSuffix[suffixIdx] += "_" + std::to_string(j);
            suffix[suffixIdx] += "[" + std::to_string(j) + "]";
            suffixIdx ++;
          }
        }
      }
      num = num / node->dimension[i];
      pairNum *= node->dimension[i];
    }
    for (size_t i = 0; i < suffix.size(); i ++) {
      if (!nameExist(originName + verilatorSuffix[i])) {
        allNames[diffNodeName + suffix[i]] = verilatorName + verilatorSuffix[i];
      }
    }
  } else {
    allNames[diffNodeName] = verilatorName;
  }
  for (auto iter : allNames)
    fprintf(sigFileMt, "%d %d %s %s\n", node->sign, node->width, iter.first.c_str(), iter.second.c_str());
}
#endif

void graph::genNodeDefMt(FILE* fp, Node* node, bool taskLocal,
                         const std::string& emittedName) {
  if (node->type == NODE_SPECIAL || node->type == NODE_REG_RESET || (node->status != VALID_NODE)) return;
  if (node->type == NODE_REG_DST && !node->regSplit) return;
  if (node->type == NODE_WRITER) return;
  if (taskLocal) return;
#if defined(GSIM_DIFF) || defined(VERILATOR_DIFF)
  genDiffSigMt(fp, node, emittedName);
#endif
  if (!emittedName.empty()) return;
  if (definedNode.find(node) != definedNode.end()) return;
  definedNode.insert(node);
  fprintf(fp, "%s %s", widthUType(node->width).c_str(), node->name.c_str());
  if (node->type == NODE_MEMORY) fprintf(fp, "[%d]", upperPower2(node->depth));
  for (int dim : node->dimension) fprintf(fp, "[%d]", upperPower2(dim));
  fprintf(fp, "; // width = %d, lineno = %d\n", node->width, node->lineno);
  int w = node->width;
  bool needInitMask = (node->type != NODE_MEMORY && node->type != NODE_WRITER) &&
    (((w < 64) && (w != 8 && w != 16 && w != 32 && w != 64)) || ((w > 64) && (w % 32 != 0)));
  if (needInitMask) {
    if (node->dimension.empty()) {
      emitBodyLock(1, "%s &= %s;\n", node->name.c_str(), bitMask(w).c_str());
    } else {
      int indent = 1;
      int dims = node->dimension.size();
      for (int i = 0; i < dims; i ++) {
        emitBodyLock(indent ++, "for (int i%d = 0; i%d < %d; i%d ++) {\n", i, i, node->dimension[i], i);
      }
      emitBodyLock(indent, "%s", node->name.c_str());
      for (int i = 0; i < dims; i ++) { emitBodyLock(0, "[i%d]", i); }
      emitBodyLock(0, "&= %s;\n", bitMask(w).c_str());
      for (int i = 0; i < dims; i ++) { emitBodyLock(-- indent, "}\n"); }
    }
  }

  /* save reset registers */
  if (node->isReset() && node->type == NODE_REG_SRC) {
    Assert(!node->isArray() && node->width <= BASIC_WIDTH, "%s is treated as reset (isArray: %d width: %d)", node->name.c_str(), node->isArray(), node->width);
    fprintf(fp, "%s %s;\n", widthUType(node->width).c_str(), RESET_NAME(node).c_str());
    if (needInitMask) {
      emitBodyLock(1, "%s = %s & %s;\n", RESET_NAME(node).c_str(), RESET_NAME(node).c_str(), bitMask(w).c_str());
    }
  }
}

bool graph::__emitSrcMt(int indent, bool canNewFile, bool alreadyEndFunc, const char *nextFuncDef, const char *fmt, ...) {
  bool newFile = false;
  if (srcFp == NULL || (srcFileBytes > (globalConfig.cppMaxSizeKB * 1024) && canNewFile)) {
    if (srcFp != NULL) {
      if (!alreadyEndFunc) fprintf(srcFp, "}"); // the end of the current function
      fclose(srcFp);
    }
    srcFp = std::fopen(format("%s%d.cpp", (globalConfig.OutputDir + "/" + name).c_str(), srcFileIdx).c_str(), "w");
    srcFileIdx ++;
    assert(srcFp != NULL);
    srcFileBytes = fprintf(srcFp, "#include \"%s.h\"\n", name.c_str());
    if (nextFuncDef != NULL) {
      srcFileBytes += fprintf(srcFp, "%s {\n", nextFuncDef);
    }
    newFile = true;
  }
  for (int i = 0; i < indent; i ++) fprintf(srcFp, "  ");
  va_list args;
  va_start(args, fmt);
  int bytes = vfprintf(srcFp, fmt, args);
  assert(bytes > 0);
  va_end(args);
  srcFileBytes += bytes;
  return newFile;
}

void graph::emitPrintfMt() {
  emitFuncDecl(0, "static std::string mtFormatDeferred(const char *fmt, va_list args) {\n");
  emitBodyLock(0,
  "  std::string result;\n"
  "  while (*fmt != 0) {\n"
  "    if (*fmt != '%%') { result.push_back(*fmt++); continue; }\n"
  "    ++fmt;\n"
  "    if (*fmt == '%%') { result.push_back(*fmt++); continue; }\n"
  "    const uint32_t bits = va_arg(args, uint32_t);\n"
  "    uint64_t value = 0;\n"
  "    if (bits <= 32) value = va_arg(args, uint32_t);\n"
  "    else if (bits <= 64) value = va_arg(args, uint64_t);\n"
  "    else { result += \"<unsupported-width>\"; ++fmt; continue; }\n"
  "    char buffer[64];\n"
  "    switch (*fmt) {\n"
  "      case 'd': std::snprintf(buffer, sizeof(buffer), \"%%lld\", static_cast<long long>(value)); result += buffer; break;\n"
  "      case 'c': result.push_back(static_cast<char>(value & 0xff)); break;\n"
  "      case 'x': std::snprintf(buffer, sizeof(buffer), \"%%llx\", static_cast<unsigned long long>(value)); result += buffer; break;\n"
  "      default: result.push_back('%%'); result.push_back(*fmt); break;\n"
  "    }\n"
  "    ++fmt;\n"
  "  }\n"
  "  return result;\n"
  "}\n"
  );
  emitFuncDecl(0, "void mtDeferAssert(const char *fmt, ...) {\n");
  emitBodyLock(0,
  "  va_list args;\n"
  "  va_start(args, fmt);\n"
  "  va_list measure;\n"
  "  va_copy(measure, args);\n"
  "  const int length = std::vsnprintf(nullptr, 0, fmt, measure);\n"
  "  va_end(measure);\n"
  "  std::string text;\n"
  "  if (length >= 0) {\n"
  "    std::vector<char> buffer(static_cast<size_t>(length) + 1);\n"
  "    std::vsnprintf(buffer.data(), buffer.size(), fmt, args);\n"
  "    text.assign(buffer.data(), static_cast<size_t>(length));\n"
  "  } else {\n"
  "    text = fmt;\n"
  "  }\n"
  "  mtDeferredEvents.push_back({true, std::move(text)});\n"
  "  va_end(args);\n"
  "}\n"
  );
  emitFuncDecl(0, "void gprintf(const char *fmt, ...) {\n");
  emitBodyLock(0,
  "  va_list args;\n"
  "  va_start(args, fmt);\n"
  "  mtDeferredEvents.push_back({false, mtFormatDeferred(fmt, args)});\n"
  "  va_end(args);\n"
  "}\n"
  );
  emitFuncDecl(0, "std::vector<MtDeferredEvent> mtTakeDeferredEvents() {\n");
  emitBodyLock(0,
  "  std::vector<MtDeferredEvent> result;\n"
  "  result.swap(mtDeferredEvents);\n"
  "  return result;\n"
  "}\n"
  );
  emitFuncDecl(0, "bool mtFlushDeferredEvents(std::vector<MtDeferredEvent>& events) {\n");
  emitBodyLock(0,
  "  if (events.empty()) return false;\n"
  "  bool failed = false;\n"
  "  for (const MtDeferredEvent& event : events) {\n"
  "    if (event.assertion) {\n"
  "      fprintf(stderr, \"\\33[1;31m%%s\\33[0m\\n\", event.text.c_str());\n"
  "      failed = true;\n"
  "    } else {\n"
  "      fputs(event.text.c_str(), stderr);\n"
  "    }\n"
  "  }\n"
  "  fflush(stderr);\n"
  "  events.clear();\n"
  "  return failed;\n"
  "}\n"
  );
}

void graph::cppEmitterMt() {
  CppEmitterMt mtEmitter(*this);
  if (!mtEmitter.enabled()) {
    fprintf(stderr, "[cppEmitter-mt] --mt-mode=on is required\n");
    Panic();
  }

  mtEmitter.prepare();

  srcFp = NULL;
  srcFileIdx = 0;

  FILE* header = genHeaderStartMt();
  mtEmitter.emitHeaderPreamble(header);
#ifdef DIFFTEST_PER_SIG
  sigFileMt = fopen((globalConfig.OutputDir + "/" + name + "_sigs.txt").c_str(), "w");
  Assert(sigFileMt != nullptr, "failed to open MT signal list in %s", globalConfig.OutputDir.c_str());
#endif

  /* class start*/
  fprintf(header, "class S%s {\npublic:\n", name.c_str());
  fprintf(header, "uint64_t cycles;\n");
  fprintf(header, "uint64_t LOG_START, LOG_END;\n");
  mtEmitter.emitClassMembers(header);
  emitPrintfMt();
  /* constrcutor */
  emitFuncDecl(0, "S%s::S%s() {\n", name.c_str(), name.c_str());
  emitBodyLock(1, "cycles = 0;\n");
  emitBodyLock(1, "LOG_START = 1;\n");
  emitBodyLock(1, "LOG_END = 0;\n");
  mtEmitter.emitConstructorInit();
  emitBodyLock(1, "init();\n");
  mtEmitter.emitConstructorStart();
  emitBodyLock(0, "}\n");

  /* initialization */
  emitFuncDecl(0, "void S%s::init() {\n", name.c_str());
  emitBodyLock(0, "#ifdef RANDOMIZE_INIT\n"
               "  srand((unsigned int)time(NULL));\n"
               "  for (uint32_t *p = &_var_start; p != &_var_end; p ++) {\n"
               "    *p = rand();\n"
               "  }\n"
               "// mask out the bits out of the width range\n");

  // header: node definition; src: node evaluation
  fprintf(header, "uint32_t _var_start;\n");
  for (Node* node : mtEmitter.emissionNodes()) {
    if (node->type == NODE_EXT) continue;
    const std::string emittedName = mtEmitter.isPackedRegister(node)
                                        ? mtEmitter.packedRegisterName(node)
                                        : std::string();
    genNodeDefMt(header, node,
                 mtEmitter.isTaskLocal(node) || mtEmitter.isWorkerLocal(node),
                 emittedName);
  }
  /* memory definition */
  for (Node* mem : memory) genNodeDefMt(header, mem, false);
  fprintf(header, "uint32_t _var_end;\n");

  emitBodyLock(0, "// initialize registers with reset value 0 to overwrite the rand() results\n" );
  emitBodyLock(1, "memset(&_var_start, 0, &_var_end - &_var_start);\n");

  emitBodyLock(0, "#else\n" // RANDOMIZE_INIT
               "  memset(&_var_start, 0, &_var_end - &_var_start);\n"
               "#endif\n");
  mtEmitter.emitStateInitialization();

  fprintf(header, "S%s();\n", name.c_str());
  fprintf(header, "void init();\n");

  emitBodyLock(0, "}\n");

   /* input/output interface */
  for (Node* node : input) {
    fprintf(header, "void set_%s(%s val);\n", node->name.c_str(), widthUType(node->width).c_str());
    genInterfaceInputMt(node);
  }
  for (Node* node : output) {
    fprintf(header, "%s get_%s();\n", widthUType(node->width).c_str(), node->name.c_str());
    genInterfaceOutputMt(node);
  }
  /* reset functions */
  mtEmitter.emitDefinitions();

  /* step wrapper */
  fprintf(header, "void step();\n");
  mtEmitter.emitStep();

  /* end of file */
  fprintf(header, "};\n"
                  "#endif\n");
  fclose(header);
  fclose(srcFp);
#ifdef DIFFTEST_PER_SIG
  fclose(sigFileMt);
#endif

  std::cout << "[cppEmitter] finish writing " << srcFileIdx << " cpp files to " + globalConfig.OutputDir + "/" << std::endl;
}

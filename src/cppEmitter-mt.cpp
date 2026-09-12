#include "cppEmitter-mt.h"

#include "common.h"
#include "mtTaskLowering.h"
#include "util.h"

#include <algorithm>
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

}  // namespace

CppEmitterMt::CppEmitterMt(graph& graph)
    : graph_(graph),
      enabled_(globalConfig.MtMode),
      workerCount_(positiveEnv("GSIM_THREADS", 1)),
      maxTasks_(positiveEnv("GSIM_MT_DENSE_VCONTRACT_MAXMT", 1600)),
      resetChunk_(resetChunkSize()) {}

bool CppEmitterMt::enabled() const { return enabled_; }


void CppEmitterMt::prepare() {
  if (!enabled()) return;
  MtTaskPartitioner::assignCppIds(graph_);
  for (SuperNode* super : graph_.sortedSuper) {
    for (Node* member : super->member) {
      if (member->status == VALID_NODE) member->updateActivate();
    }
  }
  MtTaskPartitioner::build(graph_, static_cast<MtTaskPlan&>(*this), maxTasks_);
  MtTaskLowerer::generateStmtTrees(graph_, static_cast<MtTaskPlan&>(*this));
  MtWorkerBuilder::build(graph_, static_cast<MtTaskPlan&>(*this), static_cast<MtWorkerPlan&>(*this),
                         workerCount_, resetChunk_);
}

bool CppEmitterMt::isTaskLocal(Node* node) const {
  return localNodes_.find(node) != localNodes_.end();
}

void CppEmitterMt::emitText(int indent, bool canStartFile, const std::string& text) {
  graph_.__emitSrcMt(indent, canStartFile, true, nullptr, "%s", text.c_str());
}

void CppEmitterMt::emitHeaderPreamble(FILE* header) const {
  if (!enabled()) return;
  fprintf(header, "#include <atomic>\n#include <thread>\n#include <sched.h>\n\n");
}

void CppEmitterMt::emitClassMembers(FILE* header) const {
  if (!enabled()) return;
  fprintf(header, "static constexpr int kMtWorkerCount = %d;\n", workerCount_);
  fprintf(header, "struct MtReadyToken { std::atomic<uint8_t> value{0}; };\n");
  fprintf(header, "struct alignas(64) MtDoneFlag { std::atomic<uint8_t> parity{0}; };\n");
  fprintf(header, "struct alignas(64) MtResetFlag { std::atomic<uint64_t> generation{0}; };\n");
  fprintf(header, "struct MtDispatch { void (S%s::*fn)(); uint32_t waitBegin, waitEnd, storeBegin, storeEnd; };\n",
          graph_.name.c_str());
  fprintf(header, "alignas(64) MtReadyToken mtReadyTokens[%d];\n", readySlotCount_);
  fprintf(header, "MtDoneFlag mtDoneFlags[%d];\n", std::max(1, workerCount_ - 1));
  fprintf(header, "std::vector<std::thread> mtThreads;\n");
  fprintf(header, "alignas(64) std::atomic<uint64_t> mtGeneration{0};\n");
  fprintf(header, "alignas(64) std::atomic<int> mtReadyWorkers{0};\n");
  fprintf(header, "alignas(64) std::atomic<bool> mtStop{false};\n");
  fprintf(header, "bool mtEnabled = false;\n");
  const size_t resetCount = std::max<size_t>(1, resets_.size());
  fprintf(header, "uint8_t mtAsyncResetValues[%zu]{};\n", resetCount);
  fprintf(header, "MtResetFlag mtAsyncResetReady[%zu];\n", resetCount);
  fprintf(header, "MtResetFlag mtAsyncResetRelease[%zu];\n", resetCount);
  fprintf(header, "MtResetFlag mtAsyncResetDone[%zu];\n",
          std::max<size_t>(1, resets_.size() * static_cast<size_t>(workerCount_)));
  fprintf(header, "static const uint32_t mtWaitSlots[%zu];\n", std::max<size_t>(1, waitSlots_.size()));
  fprintf(header, "static const uint32_t mtStoreSlots[%zu];\n", std::max<size_t>(1, storeSlots_.size()));
  for (int worker = 0; worker < workerCount_; ++worker) {
    fprintf(header, "static const MtDispatch mtDispatchW%d[%zu];\n", worker,
            std::max<size_t>(1, workerTasks_[static_cast<size_t>(worker)].size() +
                                   resetJoins_[static_cast<size_t>(worker)].size()));
  }
  fprintf(header, "void mtInit();\nvoid mtStart();\nvoid mtStopWorkers();\n");
  fprintf(header, "void mtWorkerLoop(int worker);\nvoid mtRunWorker(int worker, uint8_t parity);\n");
  fprintf(header, "void mtPinWorker(int worker);\nvoid resetAllMt();\nvoid stepMt();\n");
  for (const Reset& reset : resets_) {
    if (!reset.asynchronous) {
      fprintf(header, "void subResetMt%d();\n", reset.id);
      for (int chunk = 1; chunk <= reset.chunkCount; ++chunk) {
        fprintf(header, "void subResetMt%d_c%d();\n", reset.id, chunk);
      }
      continue;
    }
    fprintf(header, "void mtTriggerAsyncReset%d(bool resetValue);\n", reset.id);
    for (const Reset::Worker& worker : reset.workers) {
      fprintf(header, "void subResetMt%dW%d();\n", reset.id, worker.id);
      for (int chunk = 1; chunk <= worker.chunkCount; ++chunk) {
        fprintf(header, "void subResetMt%dW%d_c%d();\n", reset.id, worker.id, chunk);
      }
    }
    for (int worker : reset.participants) {
      fprintf(header, "void mtJoinAsyncReset%dW%d();\n", reset.id, worker);
    }
  }
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
        emitText(indent++, false, instruction.text + "\n");
        ++nesting;
        break;
      case SUPER_INFO_ELSE: emitText(indent - 1, false, instruction.text + "\n"); break;
      case SUPER_INFO_DEDENT:
        emitText(--indent, false, instruction.text + "\n");
        --nesting;
        break;
      case SUPER_INFO_STR: emitText(indent, false, instruction.text + "\n"); break;
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

void CppEmitterMt::emitResetFunction(const Reset& reset) {
  Assert(!reset.asynchronous, "async reset %d requires worker functions", reset.id);
  Node* resetNode = reset.super->resetNode;
  const std::string condition =
      resetNode->type == NODE_REG_SRC ? resetNode->name + "$RESET" : resetNode->name;
  emitResetBodyFunction("subResetMt" + std::to_string(reset.id), condition,
                        reset.body, reset.chunkCount);
}

void CppEmitterMt::emitAsyncResetWorkerFunction(const Reset& reset,
                                                const Reset::Worker& worker) {
  emitResetBodyFunction("subResetMt" + std::to_string(reset.id) + "W" +
                            std::to_string(worker.id),
                        "", worker.body, worker.chunkCount);
}

void CppEmitterMt::emitInstructions(const std::vector<InstInfo>& instructions, int indent) {
  const int initialIndent = indent;
  for (const InstInfo& inst : instructions) {
    switch (inst.infoType) {
      case SUPER_INFO_IF: emitText(indent++, false, inst.inst + "\n"); break;
      case SUPER_INFO_ELSE: emitText(indent - 1, false, inst.inst + "\n"); break;
      case SUPER_INFO_DEDENT: emitText(--indent, false, inst.inst + "\n"); break;
      case SUPER_INFO_STR: emitText(indent, false, inst.inst + "\n"); break;
      case SUPER_INFO_ASSIGN_BEG:
      case SUPER_INFO_ASSIGN_END: break;
    }
  }
  Assert(indent == initialIndent, "unbalanced MTask instruction tree");
}

void CppEmitterMt::emitTask(const Task& task, int indent) {
  Assert(task.insts != nullptr, "MTask has no lowered instruction stream");
  Assert(!task.cppIds.empty(), "cannot emit an empty MTask");
  SuperNode* first = byCppId_[static_cast<size_t>(task.cppIds.front())];

  for (Node* node : task.members) {
    if (task.localNodes.count(node) != 0) {
      emitText(indent, false, widthUType(node->width) + " " + node->name + "{};\n");
    }
  }

  auto resetValueName = [](Node* resetNode) {
    return resetNode->type == NODE_REG_SRC ? resetNode->name + "$RESET" : resetNode->name;
  };
  std::map<Node*, std::string> savedAsyncResetValues;
  auto saveAsyncResetValue = [&](Node* resetNode) {
    auto reset = asyncResetIds_.find(resetNode);
    Assert(reset != asyncResetIds_.end(), "missing MT async reset for %s", resetNode->name.c_str());
    std::string oldName = "__gsim_async_reset_old_" + std::to_string(reset->second);
    if (savedAsyncResetValues.emplace(resetNode, oldName).second) {
      emitText(indent, false, "uint8_t " + oldName + " = " + resetValueName(resetNode) + ";\n");
    }
  };
  auto emitAsyncReset = [&](Node* resetNode) {
    auto reset = asyncResetIds_.find(resetNode);
    Assert(reset != asyncResetIds_.end(), "missing MT async reset for %s", resetNode->name.c_str());
    auto oldValue = savedAsyncResetValues.find(resetNode);
    Assert(oldValue != savedAsyncResetValues.end(), "missing old value for MT async reset %s", resetNode->name.c_str());
    emitText(indent, false, "mtTriggerAsyncReset" + std::to_string(reset->second) + "(" +
                                oldValue->second + " || " + resetValueName(resetNode) + ");\n");
  };

  if (first->superType == SUPER_EXTMOD) {
    Assert(task.cppIds.size() == 1, "extmodule SuperNode must remain a standalone MTask");
    std::set<Node*> asyncResets;
    for (size_t i = 1; i < first->member.size(); ++i) {
      if (first->member[i]->isAsyncReset()) asyncResets.insert(first->member[i]);
    }
    for (Node* resetNode : asyncResets) saveAsyncResetValue(resetNode);
    emitInstructions(*task.insts, indent);
    for (Node* resetNode : asyncResets) emitAsyncReset(resetNode);
    return;
  }

  if (first->superType == SUPER_ASYNC_RESET) {
    Assert(task.cppIds.size() == 1, "async reset SuperNode must remain a standalone MTask");
    saveAsyncResetValue(first->resetNode);
  }
  emitInstructions(*task.insts, indent);
  if (first->superType == SUPER_ASYNC_RESET) emitAsyncReset(first->resetNode);
}

void CppEmitterMt::emitDefinitions() {
  if (!enabled()) return;
  const std::string className = "S" + graph_.name;

  for (const Reset& reset : resets_) {
    if (!reset.asynchronous) {
      emitResetFunction(reset);
      continue;
    }
    for (const Reset::Worker& worker : reset.workers) {
      emitAsyncResetWorkerFunction(reset, worker);
    }

    const auto workerReset = [&](int worker) -> const Reset::Worker* {
      for (const Reset::Worker& candidate : reset.workers) {
        if (candidate.id == worker) return &candidate;
      }
      return nullptr;
    };

    // Ready is published every cycle. The more expensive done/release barrier
    // is entered only while reset is asserted.
    emitText(0, true, "void " + className + "::mtTriggerAsyncReset" +
                          std::to_string(reset.id) + "(bool resetValue) {\n");
    emitText(1, false,
             "const uint64_t generation = mtGeneration.load(std::memory_order_relaxed);\n");
    emitText(1, false, "mtAsyncResetValues[" + std::to_string(reset.id) + "] = resetValue;\n");
    emitText(1, false, "mtAsyncResetReady[" + std::to_string(reset.id) +
                           "].generation.store(generation, std::memory_order_release);\n");
    emitText(1, false, "if (!resetValue) return;\n");
    if (workerReset(reset.triggerOwner) != nullptr) {
      emitText(1, false, "subResetMt" + std::to_string(reset.id) + "W" +
                             std::to_string(reset.triggerOwner) + "();\n");
    }
    for (int worker : reset.participants) {
      const size_t index = static_cast<size_t>(reset.id) * static_cast<size_t>(workerCount_) +
                           static_cast<size_t>(worker);
      emitText(1, false, "while (mtAsyncResetDone[" + std::to_string(index) +
                             "].generation.load(std::memory_order_acquire) != generation)\n");
      emitText(2, false, "__asm__ __volatile__(\"pause\" ::: \"memory\");\n");
    }
    emitText(1, false, "mtAsyncResetRelease[" + std::to_string(reset.id) +
                           "].generation.store(generation, std::memory_order_release);\n");
    emitText(0, false, "}\n");

    for (int worker : reset.participants) {
      const size_t index = static_cast<size_t>(reset.id) * static_cast<size_t>(workerCount_) +
                           static_cast<size_t>(worker);
      emitText(0, true, "void " + className + "::mtJoinAsyncReset" +
                            std::to_string(reset.id) + "W" + std::to_string(worker) + "() {\n");
      emitText(1, false,
               "const uint64_t generation = mtGeneration.load(std::memory_order_relaxed);\n");
      emitText(1, false, "while (mtAsyncResetReady[" + std::to_string(reset.id) +
                             "].generation.load(std::memory_order_acquire) != generation)\n");
      emitText(2, false, "__asm__ __volatile__(\"pause\" ::: \"memory\");\n");
      emitText(1, false, "if (!mtAsyncResetValues[" + std::to_string(reset.id) + "]) return;\n");
      if (workerReset(worker) != nullptr) {
        emitText(1, false, "subResetMt" + std::to_string(reset.id) + "W" +
                               std::to_string(worker) + "();\n");
      }
      emitText(1, false, "mtAsyncResetDone[" + std::to_string(index) +
                             "].generation.store(generation, std::memory_order_release);\n");
      emitText(1, false, "while (mtAsyncResetRelease[" + std::to_string(reset.id) +
                             "].generation.load(std::memory_order_acquire) != generation)\n");
      emitText(2, false, "__asm__ __volatile__(\"pause\" ::: \"memory\");\n");
      emitText(0, false, "}\n");
    }
  }

  emitText(0, true, "void " + className + "::resetAllMt() {\n");
  for (const Reset& reset : resets_) {
    if (!reset.asynchronous) emitText(1, false, "subResetMt" + std::to_string(reset.id) + "();\n");
  }
  emitText(0, false, "}\n");

  for (size_t taskId = 0; taskId < tasks_.size(); ++taskId) {
    emitText(0, true, "void " + className + "::mtTask" + std::to_string(taskId) + "() {\n");
    emitTask(tasks_[taskId], 1);
    emitText(0, false, "}\n");
  }

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
    const std::vector<ResetJoin>& joins = resetJoins_[static_cast<size_t>(worker)];
    const size_t dispatchCount = chain.size() + joins.size();
    std::string text = "const " + className + "::MtDispatch " + className + "::mtDispatchW" +
                       std::to_string(worker) + "[" +
                       std::to_string(std::max<size_t>(1, dispatchCount)) + "] = {\n";
    if (dispatchCount == 0) {
      text += "  {nullptr, 0, 0, 0, 0}\n";
    } else {
      size_t joinIndex = 0;
      for (size_t position = 0; position <= chain.size(); ++position) {
        while (joinIndex < joins.size() && joins[joinIndex].beforePosition == position) {
          const ResetJoin& join = joins[joinIndex++];
          text += "  {&" + className + "::mtJoinAsyncReset" + std::to_string(join.resetId) +
                  "W" + std::to_string(worker) + ",0,0,0,0},\n";
        }
        if (position == chain.size()) break;
        const int taskId = chain[position];
        const Task& task = tasks_[static_cast<size_t>(taskId)];
        text += "  {&" + className + "::mtTask" + std::to_string(taskId) + "," +
                std::to_string(task.waitBegin) + "," + std::to_string(task.waitEnd) + "," +
                std::to_string(task.storeBegin) + "," + std::to_string(task.storeEnd) + "},\n";
      }
      Assert(joinIndex == joins.size(), "unemitted async reset joins for worker %d", worker);
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
      "    mtDoneFlags[worker - 1].parity.store(static_cast<uint8_t>(generation & 1),\n"
      "                                         std::memory_order_release);\n"
      "    seen = generation;\n"
      "  }\n"
      "}\n";

  runtime += "void " + className + "::mtRunWorker(int worker, uint8_t parity) {\n";
  runtime += "  const MtDispatch* entries = nullptr; uint32_t count = 0;\n  switch (worker) {\n";
  for (int worker = 0; worker < workerCount_; ++worker) {
    runtime += "    case " + std::to_string(worker) + ": entries = mtDispatchW" + std::to_string(worker) +
               "; count = " +
               std::to_string(workerTasks_[static_cast<size_t>(worker)].size() +
                              resetJoins_[static_cast<size_t>(worker)].size()) +
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
  runtime += "  resetAllMt();\n";
  for (SuperNode* super : graph_.sortedSuper) {
    for (Node* member : super->member) {
      if (member->isReset() && member->type == NODE_REG_SRC) {
        runtime += "  " + member->name + "$RESET = " + member->name + ";\n";
      }
    }
  }
  runtime +=
      "  uint64_t generation = mtGeneration.fetch_add(1, std::memory_order_release) + 1;\n"
      "  uint8_t parity = static_cast<uint8_t>(generation & 1);\n";
  runtime += "  mtRunWorker(0, parity);\n";
  runtime +=
      "  for (int worker = 1; worker < kMtWorkerCount; ++worker) {\n"
      "    while (mtDoneFlags[worker - 1].parity.load(std::memory_order_acquire) != parity)\n"
      "      __asm__ __volatile__(\"pause\" ::: \"memory\");\n"
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

  fprintf(header, "\n#define gAssert(cond, ...) do {"
                     "if (!(cond)) {"
                       "fprintf(stderr, \"\\33[1;31m\");"
                       "fprintf(stderr, __VA_ARGS__);"
                       "fprintf(stderr, \"\\33[0m\\n\");"
                       "assert(cond);"
                     "}"
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
void graph::genDiffSigMt(FILE* fp, Node* node) {
  std::set<std::string> allNames;
  std::string diffNodeName = node->name;
  std::string originName = node->name;
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
      allNames.insert(diffNodeName + suffix[i]);
    }
  } else {
    allNames.insert(diffNodeName);
  }
  for (auto iter : allNames)
    fprintf(sigFileMt, "%d %d %s %s\n", node->sign, node->width, iter.c_str(), iter.c_str());
}
#endif

#if defined(DIFFTEST_PER_SIG) && defined(VERILATOR_DIFF)
void graph::genDiffSigMt(FILE* fp, Node* node) {
  std::string verilatorName = name + "__DOT__" + node->name;
  size_t pos;
  while ((pos = verilatorName.find("$$")) != std::string::npos) {
    verilatorName.replace(pos, 2, "_");
  }
  while ((pos = verilatorName.find("$")) != std::string::npos) {
    verilatorName.replace(pos, 1, "__DOT__");
  }
  std::map<std::string, std::string> allNames;
  std::string diffNodeName = node->name;
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

void graph::genNodeDefMt(FILE* fp, Node* node, bool taskLocal) {
  if (node->type == NODE_SPECIAL || node->type == NODE_REG_RESET || (node->status != VALID_NODE)) return;
  if (node->type == NODE_REG_DST && !node->regSplit) return;
  if (node->type == NODE_WRITER) return;
  if (taskLocal) return;
#if defined(GSIM_DIFF) || defined(VERILATOR_DIFF)
  genDiffSigMt(fp, node);
#endif
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
  emitFuncDecl(0, "void gprintf(const char *fmt, ...) {\n");
  emitBodyLock(0,
  "  FILE *fp = stderr;\n"
  "  va_list args;\n"
  "  va_start(args, fmt);\n"
  "  int fmt_idx = 0;\n"
  "  while (true) {\n"
  "    char c = fmt[fmt_idx ++];\n"
  "    switch (c) {\n"
  "      case '%%': break;\n"
  "      case 0: return;\n"
  "      default: fputc(c, fp); continue;\n"
  "    }\n"
  "\n"
  "    uint64_t lval = 0;\n"
  "    int bits = va_arg(args, uint32_t);\n"
  "    if      (bits <= 32) { lval = va_arg(args, uint32_t); }\n"
  "    else if (bits <= 64) { lval = va_arg(args, uint64_t); }\n"
  "    else                 { assert(0); }\n"
  "\n"
  "    c = fmt[fmt_idx ++];\n"
  "    switch (c) {\n"
  "      case 'd': fprintf(fp, \"%%ld\", lval); break;\n"
  "      case 'c': fputc(lval & 0xff, fp); break;\n"
  "      case 'x': fprintf(fp, \"%%lx\", lval); break;\n"
  "      default: assert(0);\n"
  "    }\n"
  "  }\n"
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
  for (SuperNode* super : sortedSuper) {
    // std::string insts;
    if (super->superType == SUPER_VALID || super->superType == SUPER_ASYNC_RESET) {
      for (Node* n : super->member) genNodeDefMt(header, n, mtEmitter.isTaskLocal(n));
    }
    if (super->superType == SUPER_EXTMOD) {
      for (size_t i = 1; i < super->member.size(); i ++) {
        genNodeDefMt(header, super->member[i], mtEmitter.isTaskLocal(super->member[i]));
      }
    }
  }
  /* memory definition */
  for (Node* mem : memory) genNodeDefMt(header, mem, false);
  fprintf(header, "uint32_t _var_end;\n");

  emitBodyLock(0, "// initialize registers with reset value 0 to overwrite the rand() results\n" );
  emitBodyLock(1, "memset(&_var_start, 0, &_var_end - &_var_start);\n");

  emitBodyLock(0, "#else\n" // RANDOMIZE_INIT
               "  memset(&_var_start, 0, &_var_end - &_var_start);\n"
               "#endif\n");

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

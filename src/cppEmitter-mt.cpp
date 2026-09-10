#include "cppEmitter-mt.h"

#include "common.h"
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

int taskCost(const SuperNode* super) {
  return std::max<int>(1, static_cast<int>(super->insts.size() + super->member.size()));
}

std::pair<size_t, size_t> resetBodyRange(const std::vector<InstInfo>& instructions,
                                        const Node* resetNode) {
  if (instructions.size() < 2 || instructions.front().infoType != SUPER_INFO_IF ||
      instructions.back().infoType != SUPER_INFO_DEDENT) {
    return {0, instructions.size()};
  }
  const std::string resetName = resetNode->type == NODE_REG_SRC
                                    ? resetNode->name + "$RESET"
                                    : resetNode->name;
  if (instructions.front().inst.find(resetName) == std::string::npos) return {0, instructions.size()};

  int nesting = 0;
  for (size_t i = 0; i < instructions.size(); ++i) {
    if (instructions[i].infoType == SUPER_INFO_IF) ++nesting;
    if (instructions[i].infoType == SUPER_INFO_DEDENT) --nesting;
    if (nesting == 0 && i + 1 != instructions.size()) return {0, instructions.size()};
  }
  return nesting == 0 ? std::make_pair<size_t, size_t>(1, instructions.size() - 1)
                      : std::make_pair<size_t, size_t>(0, instructions.size());
}

std::pair<size_t, size_t> resetBodyRange(const SuperNode* super) {
  return resetBodyRange(super->insts, super->resetNode);
}

std::vector<InstInfo> buildResetInstructions(const std::vector<Node*>& members) {
  StmtTree tree;
  tree.root = new StmtNode(OP_STMT_SEQ);
  for (Node* member : members) {
    for (ExpTree* assignment : member->assignTree) {
      std::vector<int> emptyPath;
      tree.mergeExpTree(assignment, emptyPath, emptyPath, member);
    }
  }
  std::vector<InstInfo> instructions;
  tree.compute(instructions);
  return instructions;
}

template <typename Instruction>
int countResetChunks(const std::vector<Instruction>& body, int chunkSize) {
  if (chunkSize <= 0) return 0;
  int chunks = 0;
  int statements = 0;
  int nesting = 0;
  for (const auto& instruction : body) {
    const SuperInfo type = static_cast<SuperInfo>(instruction.type);
    const bool statement = type == SUPER_INFO_IF || type == SUPER_INFO_ELSE ||
                           type == SUPER_INFO_STR;
    if (statement && statements >= chunkSize && nesting == 0) {
      ++chunks;
      statements = 0;
    }
    if (type == SUPER_INFO_IF) ++nesting;
    if (type == SUPER_INFO_DEDENT) --nesting;
    statements += statement;
  }
  Assert(nesting == 0, "unbalanced reset instructions while planning worker reset");
  return chunks;
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

  int taskCount = 0;
  for (SuperNode* super : graph_.sortedSuper) {
    if (super->cppId >= 0) taskCount = std::max(taskCount, super->cppId + 1);
  }
  byCppId_.assign(static_cast<size_t>(taskCount), nullptr);
  for (SuperNode* super : graph_.sortedSuper) {
    if (super->cppId >= 0) byCppId_[static_cast<size_t>(super->cppId)] = super;
  }
  for (int cppId = 0; cppId < taskCount; ++cppId) {
    Assert(byCppId_[static_cast<size_t>(cppId)] != nullptr, "missing SuperNode for cppId %d", cppId);
  }

  std::vector<std::set<int>> successors(static_cast<size_t>(taskCount));
  auto addSuperEdges = [&](int from, const std::set<SuperNode*>& targets) {
    for (SuperNode* target : targets) {
      if (target != nullptr && target->cppId >= 0 && target->cppId != from) {
        successors[static_cast<size_t>(from)].insert(target->cppId);
      }
    }
  };
  for (int cppId = 0; cppId < taskCount; ++cppId) {
    SuperNode* super = byCppId_[static_cast<size_t>(cppId)];
    addSuperEdges(cppId, super->next);
    addSuperEdges(cppId, super->depNext);
  }

  // Dense evaluation needs dependency rank rather than cppId scan order.
  // Activity-mode cppId order may intentionally defer a backward activation to
  // the next cycle; eagerly evaluating that target can otherwise mix new state
  // with stale combinational values.
  std::vector<int> dependencyInDegree(static_cast<size_t>(taskCount), 0);
  for (int from = 0; from < taskCount; ++from) {
    for (int target : successors[static_cast<size_t>(from)]) {
      ++dependencyInDegree[static_cast<size_t>(target)];
    }
  }
  std::vector<int> readyQueue;
  readyQueue.reserve(static_cast<size_t>(taskCount));
  for (int cppId = 0; cppId < taskCount; ++cppId) {
    if (dependencyInDegree[static_cast<size_t>(cppId)] == 0) readyQueue.push_back(cppId);
  }
  std::vector<int> dependencyRank(static_cast<size_t>(taskCount), -1);
  std::vector<int> dependencyOrder;
  dependencyOrder.reserve(static_cast<size_t>(taskCount));
  for (size_t head = 0; head < readyQueue.size(); ++head) {
    int cppId = readyQueue[head];
    dependencyRank[static_cast<size_t>(cppId)] = static_cast<int>(dependencyOrder.size());
    dependencyOrder.push_back(cppId);
    for (int target : successors[static_cast<size_t>(cppId)]) {
      int& degree = dependencyInDegree[static_cast<size_t>(target)];
      if (--degree == 0) readyQueue.push_back(target);
    }
  }
  Assert(static_cast<int>(dependencyOrder.size()) == taskCount,
         "dense dependency graph has a cycle (%zu/%d nodes ranked)",
         dependencyOrder.size(), taskCount);

  // Activation edges pointing backwards in dependency rank carry work into the
  // next simulated cycle. Forward activation edges constrain dense evaluation
  // so an eagerly recomputed target observes a consistent current-cycle cone.
  for (int from = 0; from < taskCount; ++from) {
    for (Node* node : byCppId_[static_cast<size_t>(from)]->member) {
      for (int target : node->nextActiveId) {
        if (target >= 0 && target < taskCount &&
            dependencyRank[static_cast<size_t>(from)] < dependencyRank[static_cast<size_t>(target)]) {
          successors[static_cast<size_t>(from)].insert(target);
        }
      }
    }
  }
  topologicalCppIds_ = dependencyOrder;
  for (int from = 0; from < taskCount; ++from) {
    for (int target : successors[static_cast<size_t>(from)]) {
      Assert(dependencyRank[static_cast<size_t>(from)] < dependencyRank[static_cast<size_t>(target)],
             "non-forward dense edge %d -> %d", from, target);
    }
  }

  long long totalCost = 0;
  for (int cppId : topologicalCppIds_) totalCost += taskCost(byCppId_[static_cast<size_t>(cppId)]);
  int targetCost = std::max<long long>(1, (totalCost + maxTasks_ - 1) / maxTasks_);

  auto formTasks = [&](int costLimit) {
    std::vector<Task> result;
    Task current;
    auto flush = [&]() {
      if (!current.cppIds.empty()) result.push_back(std::move(current));
      current = Task();
    };
    for (int cppId : topologicalCppIds_) {
      SuperNode* super = byCppId_[static_cast<size_t>(cppId)];
      int cost = taskCost(super);
      if (!current.cppIds.empty() && current.cost + cost > costLimit) {
        flush();
      }
      current.cppIds.push_back(cppId);
      current.cost += cost;
    }
    flush();
    return result;
  };

  tasks_ = formTasks(targetCost);
  if (static_cast<int>(tasks_.size()) > maxTasks_) {
    fprintf(stderr,
            "[cppEmitter-mt] target MAXMT=%d produced %zu dependency-safe tasks\n",
            maxTasks_, tasks_.size());
  }

  std::vector<int> taskByCppId(static_cast<size_t>(taskCount), -1);
  for (size_t taskId = 0; taskId < tasks_.size(); ++taskId) {
    for (int cppId : tasks_[taskId].cppIds) taskByCppId[static_cast<size_t>(cppId)] = taskId;
  }
  std::vector<std::set<int>> taskSuccessors(tasks_.size());
  std::vector<std::set<int>> taskPredecessors(tasks_.size());
  for (int from = 0; from < taskCount; ++from) {
    int fromTask = taskByCppId[static_cast<size_t>(from)];
    for (int target : successors[static_cast<size_t>(from)]) {
      int toTask = taskByCppId[static_cast<size_t>(target)];
      if (fromTask == toTask) continue;
      Assert(fromTask < toTask, "non-forward dense MTask edge %d -> %d", fromTask, toTask);
      taskSuccessors[static_cast<size_t>(fromTask)].insert(toTask);
      taskPredecessors[static_cast<size_t>(toTask)].insert(fromTask);
    }
  }
  for (size_t taskId = 0; taskId < tasks_.size(); ++taskId) {
    tasks_[taskId].successors.assign(taskSuccessors[taskId].begin(), taskSuccessors[taskId].end());
    tasks_[taskId].predecessors.assign(taskPredecessors[taskId].begin(), taskPredecessors[taskId].end());
  }

  // Schedule only ready MTasks, then renumber by that schedule. This produces a
  // topological fixed-worker program order. Critical-path priority breaks equal
  // earliest-start times.
  const int denseTaskCount = static_cast<int>(tasks_.size());
  std::vector<int> remainingPredecessors(static_cast<size_t>(denseTaskCount), 0);
  std::vector<long long> priority(static_cast<size_t>(denseTaskCount), 0);
  for (int taskId = 0; taskId < denseTaskCount; ++taskId) {
    remainingPredecessors[static_cast<size_t>(taskId)] =
        static_cast<int>(tasks_[static_cast<size_t>(taskId)].predecessors.size());
  }
  for (int taskId = denseTaskCount - 1; taskId >= 0; --taskId) {
    long long successorPriority = 0;
    for (int successor : tasks_[static_cast<size_t>(taskId)].successors) {
      successorPriority = std::max(successorPriority, priority[static_cast<size_t>(successor)]);
    }
    priority[static_cast<size_t>(taskId)] =
        tasks_[static_cast<size_t>(taskId)].cost + successorPriority;
  }

  std::vector<int> readyTasks;
  for (int taskId = 0; taskId < denseTaskCount; ++taskId) {
    if (remainingPredecessors[static_cast<size_t>(taskId)] == 0) readyTasks.push_back(taskId);
  }
  std::vector<long long> completion(static_cast<size_t>(denseTaskCount), 0);
  std::vector<long long> workerAvailable(static_cast<size_t>(workerCount_), 0);
  std::vector<int> scheduledOwner(static_cast<size_t>(denseTaskCount), -1);
  std::vector<int> scheduleOrder;
  scheduleOrder.reserve(static_cast<size_t>(denseTaskCount));
  while (!readyTasks.empty()) {
    int bestReadyIndex = -1;
    int bestTask = -1;
    int bestWorker = 0;
    long long bestStart = LLONG_MAX;
    for (size_t readyIndex = 0; readyIndex < readyTasks.size(); ++readyIndex) {
      int taskId = readyTasks[readyIndex];
      const Task& task = tasks_[static_cast<size_t>(taskId)];
      for (int worker = 0; worker < workerCount_; ++worker) {
        long long start = workerAvailable[static_cast<size_t>(worker)];
        for (int predecessor : task.predecessors) {
          long long predecessorEnd = completion[static_cast<size_t>(predecessor)];
          if (scheduledOwner[static_cast<size_t>(predecessor)] != worker) {
            predecessorEnd += tasks_[static_cast<size_t>(predecessor)].cost * 30LL / 100LL;
          }
          start = std::max(start, predecessorEnd);
        }
        bool better = start < bestStart;
        if (start == bestStart && bestTask >= 0) {
          better = priority[static_cast<size_t>(taskId)] > priority[static_cast<size_t>(bestTask)] ||
                   (priority[static_cast<size_t>(taskId)] == priority[static_cast<size_t>(bestTask)] &&
                    taskId < bestTask);
        }
        if (better || bestTask < 0) {
          bestStart = start;
          bestReadyIndex = static_cast<int>(readyIndex);
          bestTask = taskId;
          bestWorker = worker;
        }
      }
    }
    Assert(bestTask >= 0, "dense list scheduler failed with %zu ready tasks", readyTasks.size());
    scheduledOwner[static_cast<size_t>(bestTask)] = bestWorker;
    completion[static_cast<size_t>(bestTask)] =
        bestStart + std::max(1, tasks_[static_cast<size_t>(bestTask)].cost);
    workerAvailable[static_cast<size_t>(bestWorker)] = completion[static_cast<size_t>(bestTask)];
    scheduleOrder.push_back(bestTask);
    readyTasks[static_cast<size_t>(bestReadyIndex)] = readyTasks.back();
    readyTasks.pop_back();
    for (int successor : tasks_[static_cast<size_t>(bestTask)].successors) {
      int& remaining = remainingPredecessors[static_cast<size_t>(successor)];
      if (--remaining == 0) readyTasks.push_back(successor);
    }
  }
  Assert(static_cast<int>(scheduleOrder.size()) == denseTaskCount,
         "dense list scheduler covered %zu/%d tasks", scheduleOrder.size(), denseTaskCount);

  std::vector<int> newTaskId(static_cast<size_t>(denseTaskCount), -1);
  for (int newId = 0; newId < denseTaskCount; ++newId) {
    newTaskId[static_cast<size_t>(scheduleOrder[static_cast<size_t>(newId)])] = newId;
  }
  std::vector<Task> reorderedTasks(static_cast<size_t>(denseTaskCount));
  for (int oldId = 0; oldId < denseTaskCount; ++oldId) {
    int newId = newTaskId[static_cast<size_t>(oldId)];
    reorderedTasks[static_cast<size_t>(newId)] = std::move(tasks_[static_cast<size_t>(oldId)]);
    reorderedTasks[static_cast<size_t>(newId)].owner = scheduledOwner[static_cast<size_t>(oldId)];
  }
  for (Task& task : reorderedTasks) {
    for (int& predecessor : task.predecessors) {
      predecessor = newTaskId[static_cast<size_t>(predecessor)];
    }
    for (int& successor : task.successors) {
      successor = newTaskId[static_cast<size_t>(successor)];
    }
    std::sort(task.predecessors.begin(), task.predecessors.end());
    std::sort(task.successors.begin(), task.successors.end());
  }
  tasks_.swap(reorderedTasks);
  for (int taskId = 0; taskId < denseTaskCount; ++taskId) {
    for (int successor : tasks_[static_cast<size_t>(taskId)].successors) {
      Assert(taskId < successor, "renumbered dense edge is not forward: %d -> %d", taskId, successor);
    }
  }

  workerTasks_.assign(static_cast<size_t>(workerCount_), std::vector<int>());
  std::vector<int> workerPosition(tasks_.size(), -1);
  std::vector<int> taskByFinalCppId(static_cast<size_t>(taskCount), -1);
  for (size_t taskId = 0; taskId < tasks_.size(); ++taskId) {
    int owner = tasks_[taskId].owner;
    workerPosition[taskId] = static_cast<int>(workerTasks_[static_cast<size_t>(owner)].size());
    workerTasks_[static_cast<size_t>(owner)].push_back(taskId);
    for (int cppId : tasks_[taskId].cppIds) {
      taskByFinalCppId[static_cast<size_t>(cppId)] = static_cast<int>(taskId);
    }
  }

  // Reset each register on the worker that normally owns its update MTask.
  // The trigger task is kept intact so oldReset || newReset is still computed once.
  int resetId = 0;
  for (SuperNode* super : graph_.allReset) {
    if (super->resetNode->status == CONSTANT_NODE) continue;
    Reset reset;
    reset.super = super;
    reset.id = resetId++;
    reset.asynchronous = super->superType == SUPER_ASYNC_RESET;
    if (!reset.asynchronous) {
      const auto range = resetBodyRange(super);
      for (size_t i = range.first; i < range.second; ++i) {
        reset.body.push_back(
            {static_cast<uint8_t>(super->insts[i].infoType), super->insts[i].inst});
      }
      reset.chunkCount = countResetChunks(reset.body, resetChunk_);
      resets_.push_back(std::move(reset));
      continue;
    }

    const int triggerCppId = super->resetNode->super->cppId;
    Assert(triggerCppId >= 0 && triggerCppId < taskCount,
           "missing trigger SuperNode for async reset %s", super->resetNode->name.c_str());
    reset.triggerTask = taskByFinalCppId[static_cast<size_t>(triggerCppId)];
    Assert(reset.triggerTask >= 0, "missing trigger MTask for async reset %s",
           super->resetNode->name.c_str());
    reset.triggerOwner = tasks_[static_cast<size_t>(reset.triggerTask)].owner;

    std::map<int, std::vector<Node*>> membersByWorker;
    for (Node* member : super->member) {
      Assert(member->type == NODE_REG_RESET, "invalid async reset member %s",
             member->name.c_str());
      Node* reg = member->getResetSrc();
      int owner = reset.triggerOwner;
      auto findOwner = [&](Node* candidate) {
        if (candidate == nullptr || candidate->super == nullptr) return -1;
        const int cppId = candidate->super->cppId;
        if (cppId < 0 || cppId >= taskCount) return -1;
        const int taskId = taskByFinalCppId[static_cast<size_t>(cppId)];
        return taskId >= 0 ? tasks_[static_cast<size_t>(taskId)].owner : -1;
      };
      int registerOwner = findOwner(reg);
      if (registerOwner < 0 && reg->regSplit && reg->getDst()->status == VALID_NODE) {
        registerOwner = findOwner(reg->getDst());
      }
      if (registerOwner >= 0) owner = registerOwner;
      membersByWorker[owner].push_back(member);
    }

    for (const auto& entry : membersByWorker) {
      std::vector<InstInfo> instructions = buildResetInstructions(entry.second);
      const auto range = resetBodyRange(instructions, super->resetNode);
      Assert(range.first == 1 && range.second + 1 == instructions.size(),
             "async reset %s does not have a single outer reset condition",
             super->resetNode->name.c_str());
      Reset::Worker worker;
      worker.id = entry.first;
      for (size_t i = range.first; i < range.second; ++i) {
        worker.body.push_back(
            {static_cast<uint8_t>(instructions[i].infoType), instructions[i].inst});
      }
      worker.chunkCount = countResetChunks(worker.body, resetChunk_);
      reset.workers.push_back(std::move(worker));
    }

    for (int worker = 0; worker < workerCount_; ++worker) {
      if (worker == reset.triggerOwner) continue;
      const std::vector<int>& chain = workerTasks_[static_cast<size_t>(worker)];
      const bool ownsResetState = membersByWorker.find(worker) != membersByWorker.end();
      auto position = std::upper_bound(chain.begin(), chain.end(), reset.triggerTask);
      if (position == chain.end() && !ownsResetState) continue;
      reset.participants.push_back(worker);
    }

    asyncResetIds_[super->resetNode] = reset.id;
    resets_.push_back(std::move(reset));
  }

  // A remote worker rendezvous before its first post-trigger task. This keeps
  // pre-trigger work ahead of reset and prevents post-trigger work from seeing
  // partially reset state.
  resetJoins_.assign(static_cast<size_t>(workerCount_), std::vector<ResetJoin>());
  for (const Reset& reset : resets_) {
    if (!reset.asynchronous) continue;
    for (int worker : reset.participants) {
      const std::vector<int>& chain = workerTasks_[static_cast<size_t>(worker)];
      const auto position = std::upper_bound(chain.begin(), chain.end(), reset.triggerTask);
      resetJoins_[static_cast<size_t>(worker)].push_back(
          {reset.id, static_cast<size_t>(position - chain.begin())});
    }
  }
  for (std::vector<ResetJoin>& joins : resetJoins_) {
    std::sort(joins.begin(), joins.end(), [&](const ResetJoin& lhs, const ResetJoin& rhs) {
      if (lhs.beforePosition != rhs.beforePosition) return lhs.beforePosition < rhs.beforePosition;
      const int lhsTrigger = resets_[static_cast<size_t>(lhs.resetId)].triggerTask;
      const int rhsTrigger = resets_[static_cast<size_t>(rhs.resetId)].triggerTask;
      if (lhsTrigger != rhsTrigger) return lhsTrigger < rhsTrigger;
      return lhs.resetId < rhs.resetId;
    });
  }

  struct TokenGroup {
    int producerOwner = -1;
    int consumerOwner = -1;
    int consumer = -1;
    int publisher = -1;
    int slot = -1;
  };
  std::vector<TokenGroup> groups;
  std::map<std::pair<int, int>, std::vector<int>> groupsByOwnerPair;
  for (size_t consumer = 0; consumer < tasks_.size(); ++consumer) {
    std::map<int, std::vector<int>> sourcesByOwner;
    for (int predecessor : tasks_[consumer].predecessors) {
      int owner = tasks_[static_cast<size_t>(predecessor)].owner;
      if (owner != tasks_[consumer].owner) {
        sourcesByOwner[owner].push_back(predecessor);
      }
    }
    for (const auto& entry : sourcesByOwner) {
      int publisher = *std::max_element(entry.second.begin(), entry.second.end(), [&](int lhs, int rhs) {
        return workerPosition[static_cast<size_t>(lhs)] < workerPosition[static_cast<size_t>(rhs)];
      });
      TokenGroup group;
      group.producerOwner = entry.first;
      group.consumerOwner = tasks_[consumer].owner;
      group.consumer = consumer;
      group.publisher = publisher;
      int groupId = static_cast<int>(groups.size());
      groups.push_back(group);
      groupsByOwnerPair[{group.producerOwner, group.consumerOwner}].push_back(groupId);
    }
  }

  int nextSlot = 0;
  for (const auto& bank : groupsByOwnerPair) {
    nextSlot = (nextSlot + 63) & ~63;
    for (int groupId : bank.second) groups[static_cast<size_t>(groupId)].slot = nextSlot++;
    nextSlot = (nextSlot + 63) & ~63;
  }
  readySlotCount_ = std::max(1, nextSlot);
  for (const TokenGroup& group : groups) {
    tasks_[static_cast<size_t>(group.consumer)].waits.push_back(group.slot);
    tasks_[static_cast<size_t>(group.publisher)].stores.push_back(group.slot);
  }

  for (Task& task : tasks_) {
    task.waitBegin = waitSlots_.size();
    waitSlots_.insert(waitSlots_.end(), task.waits.begin(), task.waits.end());
    task.waitEnd = waitSlots_.size();
    task.storeBegin = storeSlots_.size();
    storeSlots_.insert(storeSlots_.end(), task.stores.begin(), task.stores.end());
    task.storeEnd = storeSlots_.size();
  }

  size_t edgeCount = 0;
  for (const Task& task : tasks_) edgeCount += task.successors.size();
  fprintf(stderr,
          "[cppEmitter-mt] workers=%d supernodes=%d mtasks=%zu edges=%zu tokens=%zu\n",
          workerCount_, taskCount, tasks_.size(), edgeCount, groups.size());
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
    printf("reset.id %d, uint %d\n", reset.id, !reset.asynchronous);
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

void CppEmitterMt::emitSuperNode(SuperNode* super, int indent) {
  auto emitInstructions = [&](const std::vector<InstInfo>& instructions) {
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
  };
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

  if (super->superType == SUPER_EXTMOD) {
    std::set<Node*> asyncResets;
    for (size_t i = 1; i < super->member.size(); ++i) {
      if (super->member[i]->isAsyncReset()) asyncResets.insert(super->member[i]);
    }
    for (Node* resetNode : asyncResets) saveAsyncResetValue(resetNode);
    emitInstructions(super->insts);
    for (Node* resetNode : asyncResets) emitAsyncReset(resetNode);
    return;
  }

  if (super->superType == SUPER_ASYNC_RESET) saveAsyncResetValue(super->resetNode);
  for (Node* node : super->member) {
    if (node->isLocal()) emitText(indent, false, widthUType(node->width) + " " + node->name + "{};\n");
  }
  emitInstructions(super->insts);
  if (super->superType == SUPER_ASYNC_RESET) emitAsyncReset(super->resetNode);
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
    for (int cppId : tasks_[taskId].cppIds) emitSuperNode(byCppId_[static_cast<size_t>(cppId)], 1);
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

static int superId = 0;
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

void graph::genNodeDefMt(FILE* fp, Node* node) {
  if (node->type == NODE_SPECIAL || node->type == NODE_REG_RESET || (node->status != VALID_NODE)) return;
  if (node->type == NODE_REG_DST && !node->regSplit) return;
  if (node->type == NODE_WRITER) return;
  if (node->isLocal()) return;
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

  for (SuperNode* super : sortedSuper) {
    if (!super->instsEmpty() || super->superType == SUPER_EXTMOD || super->superType == SUPER_ASYNC_RESET) {
      super->cppId = superId ++;
    }
  }

  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      if (member->status == VALID_NODE) {
        member->updateActivate();
      }
    }
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
      for (Node* n : super->member) genNodeDefMt(header, n);
    }
    if (super->superType == SUPER_EXTMOD) {
      for (size_t i = 1; i < super->member.size(); i ++) genNodeDefMt(header, super->member[i]);
    }
  }
  /* memory definition */
  for (Node* mem : memory) genNodeDefMt(header, mem);
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

  printf("[cppEmitter] define %ld nodes %d superNodes\n", definedNode.size(), superId);
  std::cout << "[cppEmitter] finish writing " << srcFileIdx << " cpp files to " + globalConfig.OutputDir + "/" << std::endl;
}

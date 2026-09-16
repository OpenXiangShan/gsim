#include "mtTaskSchedule.h"

#include "common.h"

#include <algorithm>
#include <climits>
#include <map>
#include <set>
#include <utility>

namespace {

int loweredTaskCost(const MtTask& task) {
  const size_t instructionCount = task.insts == nullptr ? 0 : task.insts->size();
  return std::max<int>(1, static_cast<int>(instructionCount) +
                              task.globalNodeCount * globalConfig.MtScheduleGlobalWeight);
}

bool taskEmitsCode(const MtTask& task) {
  if (task.insts != nullptr && !task.insts->empty()) return true;
  return task.kind == MtTaskKind::ExtModule || task.kind == MtTaskKind::AsyncReset;
}

void compactEmptyTasks(MtTaskPlan& plan) {
  const std::vector<MtTask> original = plan.tasks_;
  std::vector<int> oldToNew(original.size(), -1);
  std::vector<MtTask> compacted;
  compacted.reserve(original.size());
  for (size_t oldId = 0; oldId < original.size(); ++oldId) {
    if (!taskEmitsCode(original[oldId])) continue;
    oldToNew[oldId] = static_cast<int>(compacted.size());
    MtTask task = original[oldId];
    task.predecessors.clear();
    task.successors.clear();
    task.waits.clear();
    task.stores.clear();
    task.waitBegin = task.waitEnd = task.storeBegin = task.storeEnd = 0;
    compacted.push_back(std::move(task));
  }
  if (compacted.size() == original.size()) return;

  std::vector<std::set<int>> successors(compacted.size());
  for (size_t oldSource = 0; oldSource < original.size(); ++oldSource) {
    const int newSource = oldToNew[oldSource];
    if (newSource < 0) continue;
    std::vector<int> pending = original[oldSource].successors;
    std::set<int> visited;
    while (!pending.empty()) {
      const int oldTarget = pending.back();
      pending.pop_back();
      if (!visited.insert(oldTarget).second) continue;
      const int newTarget = oldToNew[static_cast<size_t>(oldTarget)];
      if (newTarget >= 0) {
        if (newTarget != newSource) successors[static_cast<size_t>(newSource)].insert(newTarget);
      } else {
        const std::vector<int>& next = original[static_cast<size_t>(oldTarget)].successors;
        pending.insert(pending.end(), next.begin(), next.end());
      }
    }
  }

  std::vector<std::set<int>> predecessors(compacted.size());
  for (size_t source = 0; source < successors.size(); ++source) {
    for (int target : successors[source]) {
      predecessors[static_cast<size_t>(target)].insert(static_cast<int>(source));
    }
  }
  for (size_t taskId = 0; taskId < compacted.size(); ++taskId) {
    compacted[taskId].successors.assign(successors[taskId].begin(), successors[taskId].end());
    compacted[taskId].predecessors.assign(predecessors[taskId].begin(), predecessors[taskId].end());
  }
  plan.tasks_.swap(compacted);

  plan.taskByNode_.clear();
  for (size_t taskId = 0; taskId < plan.tasks_.size(); ++taskId) {
    for (Node* node : plan.tasks_[taskId].members) plan.taskByNode_[node] = taskId;
  }
}

void collectWorkerLocalNodes(MtTaskPlan& plan, const graph& graph) {
  plan.workerLocalOwners_.clear();

  std::set<Node*> resetDependencies;
  for (SuperNode* reset : graph.allReset) {
    for (Node* member : reset->member) {
      for (ExpTree* assignment : member->assignTree) {
        assignment->getRelyNodes(resetDependencies);
      }
    }
  }

  std::unordered_map<Node*, int> ownerByNode;
  std::unordered_set<Node*> ambiguousNodes;
  for (const MtTask& task : plan.tasks_) {
    for (Node* node : task.members) {
      auto inserted = ownerByNode.emplace(node, task.owner);
      if (!inserted.second && inserted.first->second != task.owner) {
        ambiguousNodes.insert(node);
      }
    }
  }

  for (const MtTask& task : plan.tasks_) {
    for (Node* node : task.members) {
      if (task.localNodes.count(node) != 0 || ambiguousNodes.count(node) != 0 ||
          node->status != VALID_NODE || node->type != NODE_OTHERS || node->isReset() ||
          node->isClock || resetDependencies.count(node) != 0) {
        continue;
      }

      bool workerLocal = true;
      auto checkUse = [&](Node* use) {
        if (!workerLocal || use == nullptr) return;
        auto owner = ownerByNode.find(use);
        if (owner == ownerByNode.end() || owner->second != task.owner) workerLocal = false;
      };
      for (Node* use : node->next) checkUse(use);
      for (Node* use : node->depNext) checkUse(use);
      if (workerLocal) plan.workerLocalOwners_[node] = task.owner;
    }
  }
}

std::pair<size_t, size_t> resetBodyRange(const std::vector<InstInfo>& instructions,
                                        const Node* resetNode) {
  if (instructions.size() < 2 || instructions.front().infoType != SUPER_INFO_IF ||
      instructions.back().infoType != SUPER_INFO_DEDENT) {
    return {0, instructions.size()};
  }
  // The lowered tree always names the real reset node. A register reset source
  // is replaced by its $RESET snapshot only when the outer condition is
  // emitted, after this common wrapper has been removed.
  if (instructions.front().inst.find(resetNode->name) == std::string::npos) {
    return {0, instructions.size()};
  }

  int nesting = 0;
  for (size_t i = 0; i < instructions.size(); ++i) {
    if (instructions[i].infoType == SUPER_INFO_IF) ++nesting;
    if (instructions[i].infoType == SUPER_INFO_DEDENT) --nesting;
    if (nesting == 0 && i + 1 != instructions.size()) return {0, instructions.size()};
  }
  return nesting == 0 ? std::make_pair<size_t, size_t>(1, instructions.size() - 1)
                      : std::make_pair<size_t, size_t>(0, instructions.size());
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

void appendInstructions(std::vector<MtReset::Instruction>& destination,
                        const std::vector<InstInfo>& source,
                        size_t begin = 0, size_t end = SIZE_MAX) {
  end = std::min(end, source.size());
  for (size_t index = begin; index < end; ++index) {
    destination.push_back(
        {static_cast<uint8_t>(source[index].infoType), source[index].inst});
  }
}

bool isCycleStartRegisterUpdate(const Node* node) {
  return node->status == VALID_NODE && node->type == NODE_REG_SRC &&
         node->reset != ASYRESET;
}

size_t registerStorageBytes(const Node* node) {
  size_t bytes = static_cast<size_t>(widthBits(node->width)) / 8;
  for (int dimension : node->dimension) {
    bytes *= static_cast<size_t>(upperPower2(dimension));
  }
  return std::max<size_t>(1, bytes);
}

size_t memoryWriteStorageBytes(const Node* port) {
  size_t entries = 1;
  for (int dimension : port->dimension) {
    entries *= static_cast<size_t>(upperPower2(dimension));
  }
  const size_t dataBytes = static_cast<size_t>(widthBits(port->width)) / 8;
  return sizeof(uint64_t) + entries * (std::max<size_t>(1, dataBytes) + 1);
}

void appendMemoryCommit(MtStateUpdate& update, Node* port) {
  Assert(port->type == NODE_WRITER || port->type == NODE_READWRITER,
         "invalid staged memory writer %s", port->name.c_str());
  Assert(port->parent != nullptr && port->parent->type == NODE_MEMORY,
         "memory writer %s has no memory parent", port->name.c_str());

  std::string suffix;
  for (size_t dimension = 0; dimension < port->dimension.size(); ++dimension) {
    const std::string index = "__gsim_mw_i" + std::to_string(dimension);
    update.body.push_back(
        {static_cast<uint8_t>(SUPER_INFO_IF),
         "for (int " + index + " = 0; " + index + " < " +
             std::to_string(port->dimension[dimension]) + "; ++" + index + ") {"});
    suffix += "[" + index + "]";
  }

  const std::string valid = mtMemoryWriteValidName(port) + suffix;
  const std::string data = mtMemoryWriteDataName(port) + suffix;
  const std::string destination = port->parent->name + "[" +
                                  mtMemoryWriteAddressName(port) + "]" + suffix;
  update.body.push_back(
      {static_cast<uint8_t>(SUPER_INFO_IF), "if (" + valid + ") {"});
  update.body.push_back(
      {static_cast<uint8_t>(SUPER_INFO_STR), destination + " = " + data + ";"});
  update.body.push_back({static_cast<uint8_t>(SUPER_INFO_DEDENT), "}"});

  for (size_t dimension = 0; dimension < port->dimension.size(); ++dimension) {
    update.body.push_back({static_cast<uint8_t>(SUPER_INFO_DEDENT), "}"});
  }
  const std::string validBase = mtMemoryWriteValidName(port);
  update.body.push_back(
      {static_cast<uint8_t>(SUPER_INFO_STR),
       "memset(&" + validBase + ", 0, sizeof(" + validBase + "));"});
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

void buildStateUpdates(graph& graph, MtWorkerPlan& workers, int workerCount,
                       int chunkSize) {
  workers.stateUpdates_.assign(static_cast<size_t>(workerCount), MtStateUpdate());
  for (int worker = 0; worker < workerCount; ++worker) {
    workers.stateUpdates_[static_cast<size_t>(worker)].worker = worker;
  }

  std::vector<Node*> registers;
  for (Node* reg : graph.regsrc) {
    if (!isCycleStartRegisterUpdate(reg)) continue;
    Assert(reg->regSplit && reg->getDst()->status == VALID_NODE,
           "packed MT register %s has no valid destination storage",
           reg->name.c_str());
    registers.push_back(reg);
  }
  std::sort(registers.begin(), registers.end(), [](Node* lhs, Node* rhs) {
    const size_t lhsCost = registerStorageBytes(lhs);
    const size_t rhsCost = registerStorageBytes(rhs);
    if (lhsCost != rhsCost) return lhsCost > rhsCost;
    return lhs->id < rhs->id;
  });

  std::vector<size_t> workerLoad(static_cast<size_t>(workerCount), 0);
  std::map<Node*, int> ownerByRegister;
  for (Node* reg : registers) {
    const size_t cost = registerStorageBytes(reg);
    const int worker =
        static_cast<int>(std::min_element(workerLoad.begin(), workerLoad.end()) -
                         workerLoad.begin());
    workerLoad[static_cast<size_t>(worker)] += cost;
    ownerByRegister[reg] = worker;
    MtStateUpdate& update = workers.stateUpdates_[static_cast<size_t>(worker)];
    update.registerStorageBytes += cost;
    update.registers.push_back(reg);
  }

  const std::vector<size_t> registerLoad = workerLoad;
  size_t memoryWriterCount = 0;
  size_t memoryWriteBytes = 0;
  std::vector<std::pair<Node*, std::vector<Node*>>> memoryWriters;
  for (Node* memory : graph.memory) {
    if (memory->status != VALID_NODE) continue;
    std::vector<Node*> writers;
    for (Node* port : memory->member) {
      if (port->status == VALID_NODE &&
          (port->type == NODE_WRITER || port->type == NODE_READWRITER)) {
        writers.push_back(port);
      }
    }
    if (!writers.empty()) memoryWriters.emplace_back(memory, std::move(writers));
  }
  std::sort(memoryWriters.begin(), memoryWriters.end(), [](const auto& lhs, const auto& rhs) {
    auto cost = [](const std::vector<Node*>& writers) {
      size_t result = 0;
      for (Node* writer : writers) result += memoryWriteStorageBytes(writer);
      return result;
    };
    const size_t lhsCost = cost(lhs.second);
    const size_t rhsCost = cost(rhs.second);
    if (lhsCost != rhsCost) return lhsCost > rhsCost;
    return lhs.first->id < rhs.first->id;
  });
  for (const auto& entry : memoryWriters) {
    size_t cost = 0;
    for (Node* writer : entry.second) cost += memoryWriteStorageBytes(writer);
    const int worker =
        static_cast<int>(std::min_element(workerLoad.begin(), workerLoad.end()) -
                         workerLoad.begin());
    workerLoad[static_cast<size_t>(worker)] += cost;
    MtStateUpdate& update = workers.stateUpdates_[static_cast<size_t>(worker)];
    update.memoryWriteBytes += cost;
    update.memoryWriters.insert(update.memoryWriters.end(), entry.second.begin(),
                                entry.second.end());
    memoryWriterCount += entry.second.size();
    memoryWriteBytes += cost;
  }

  struct SyncResetGroup {
    SuperNode* super = nullptr;
    std::vector<std::vector<Node*>> resetDestinationsByWorker;
  };
  std::vector<SyncResetGroup> syncGroups;
  for (SuperNode* super : graph.allReset) {
    if (super->superType != SUPER_UINT_RESET ||
        super->resetNode->status == CONSTANT_NODE) {
      continue;
    }
    SyncResetGroup group;
    group.super = super;
    group.resetDestinationsByWorker.resize(static_cast<size_t>(workerCount));
    for (Node* member : super->member) {
      Assert(member->type == NODE_REG_RESET, "invalid sync reset member %s",
             member->name.c_str());
      Node* reg = member->getResetSrc();
      auto owner = ownerByRegister.find(reg);
      if (owner == ownerByRegister.end()) continue;
      if (member->name != reg->getDst()->name) continue;
      group.resetDestinationsByWorker[static_cast<size_t>(owner->second)].push_back(member);
    }
    syncGroups.push_back(std::move(group));
  }

  const size_t resetBatchSize =
      chunkSize > 0 ? std::max<size_t>(1, static_cast<size_t>(chunkSize))
                    : SIZE_MAX;
  for (SyncResetGroup& group : syncGroups) {
    Node* resetNode = group.super->resetNode;
    const std::string resetName = resetNode->type == NODE_REG_SRC
                                      ? resetNode->name + "$RESET"
                                      : resetNode->name;
    for (int worker = 0; worker < workerCount; ++worker) {
      const std::vector<Node*>& resetDestinations =
          group.resetDestinationsByWorker[static_cast<size_t>(worker)];
      MtStateUpdate& update = workers.stateUpdates_[static_cast<size_t>(worker)];
      for (size_t begin = 0; begin < resetDestinations.size();) {
        const size_t end = std::min(resetDestinations.size(), begin + resetBatchSize);
        std::vector<Node*> resetMembers(resetDestinations.begin() + begin,
                                        resetDestinations.begin() + end);
        const std::vector<InstInfo> resetInstructions = buildResetInstructions(resetMembers);
        const auto resetRange = resetBodyRange(resetInstructions, resetNode);
        Assert(resetRange.first == 1 && resetRange.second + 1 == resetInstructions.size(),
               "sync reset %s does not have a single outer reset condition",
               resetNode->name.c_str());
        update.body.push_back(
            {static_cast<uint8_t>(SUPER_INFO_IF), "if (unlikely(" + resetName + ")) {"});
        appendInstructions(update.body, resetInstructions, resetRange.first,
                           resetRange.second);
        update.body.push_back(
            {static_cast<uint8_t>(SUPER_INFO_DEDENT), "}"});
        begin = end;
      }
    }
  }

  for (int worker = 0; worker < workerCount; ++worker) {
    MtStateUpdate& update = workers.stateUpdates_[static_cast<size_t>(worker)];
    if (!update.registers.empty()) {
      const std::string source = "mtRegisterSrcW" + std::to_string(worker);
      const std::string destination = "mtRegisterDstW" + std::to_string(worker);
      update.body.push_back(
          {static_cast<uint8_t>(SUPER_INFO_STR),
           "memcpy(&" + source + ", &" + destination + ", sizeof(" + source + "));"});
    }
    for (Node* writer : update.memoryWriters) appendMemoryCommit(update, writer);
    update.chunkCount = countResetChunks(update.body, chunkSize);
  }

  const auto registerLimits = std::minmax_element(registerLoad.begin(), registerLoad.end());
  const auto stateLimits = std::minmax_element(workerLoad.begin(), workerLoad.end());
  size_t registerBytes = 0;
  for (size_t bytes : registerLoad) registerBytes += bytes;
  fprintf(stderr,
          "[cppEmitter-mt] register-blocks registers=%zu bytes=%zu min-worker-bytes=%zu "
          "max-worker-bytes=%zu\n",
          registers.size(), registerBytes, *registerLimits.first, *registerLimits.second);
  fprintf(stderr,
          "[cppEmitter-mt] memory-commits writers=%zu pending-bytes=%zu "
          "min-worker-state-bytes=%zu max-worker-state-bytes=%zu\n",
          memoryWriterCount, memoryWriteBytes, *stateLimits.first, *stateLimits.second);
}


}  // namespace

void MtWorkerBuilder::build(graph& graph, MtTaskPlan& tasks, MtWorkerPlan& workers,
                            int workerCount, int resetChunk) {
  const int groupCount = tasks.partitionGroupCount_;
  compactEmptyTasks(tasks);
  for (MtTask& task : tasks.tasks_) task.cost = loweredTaskCost(task);
  // Schedule only ready MTasks, then renumber by that schedule. This produces a
  // topological fixed-worker program order. Critical-path priority breaks equal
  // earliest-start times.
  const int denseTaskCount = static_cast<int>(tasks.tasks_.size());
  std::vector<int> remainingPredecessors(static_cast<size_t>(denseTaskCount), 0);
  std::vector<long long> priority(static_cast<size_t>(denseTaskCount), 0);
  for (int taskId = 0; taskId < denseTaskCount; ++taskId) {
    remainingPredecessors[static_cast<size_t>(taskId)] =
        static_cast<int>(tasks.tasks_[static_cast<size_t>(taskId)].predecessors.size());
  }
  for (int taskId = denseTaskCount - 1; taskId >= 0; --taskId) {
    long long successorPriority = 0;
    for (int successor : tasks.tasks_[static_cast<size_t>(taskId)].successors) {
      successorPriority = std::max(successorPriority, priority[static_cast<size_t>(successor)]);
    }
    priority[static_cast<size_t>(taskId)] =
        tasks.tasks_[static_cast<size_t>(taskId)].cost + successorPriority;
  }

  std::vector<int> readyTasks;
  for (int taskId = 0; taskId < denseTaskCount; ++taskId) {
    if (remainingPredecessors[static_cast<size_t>(taskId)] == 0) readyTasks.push_back(taskId);
  }
  std::vector<long long> completion(static_cast<size_t>(denseTaskCount), 0);
  std::vector<long long> workerAvailable(static_cast<size_t>(workerCount), 0);
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
      const MtTask& task = tasks.tasks_[static_cast<size_t>(taskId)];
      for (int worker = 0; worker < workerCount; ++worker) {
        long long start = workerAvailable[static_cast<size_t>(worker)];
        for (int predecessor : task.predecessors) {
          long long predecessorEnd = completion[static_cast<size_t>(predecessor)];
          if (scheduledOwner[static_cast<size_t>(predecessor)] != worker) {
            predecessorEnd += tasks.tasks_[static_cast<size_t>(predecessor)].cost * 30LL / 100LL;
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
        bestStart + std::max(1, tasks.tasks_[static_cast<size_t>(bestTask)].cost);
    workerAvailable[static_cast<size_t>(bestWorker)] = completion[static_cast<size_t>(bestTask)];
    scheduleOrder.push_back(bestTask);
    readyTasks[static_cast<size_t>(bestReadyIndex)] = readyTasks.back();
    readyTasks.pop_back();
    for (int successor : tasks.tasks_[static_cast<size_t>(bestTask)].successors) {
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
  std::vector<MtTask> reorderedTasks(static_cast<size_t>(denseTaskCount));
  for (int oldId = 0; oldId < denseTaskCount; ++oldId) {
    int newId = newTaskId[static_cast<size_t>(oldId)];
    reorderedTasks[static_cast<size_t>(newId)] = std::move(tasks.tasks_[static_cast<size_t>(oldId)]);
    reorderedTasks[static_cast<size_t>(newId)].owner = scheduledOwner[static_cast<size_t>(oldId)];
  }
  for (MtTask& task : reorderedTasks) {
    for (int& predecessor : task.predecessors) {
      predecessor = newTaskId[static_cast<size_t>(predecessor)];
    }
    for (int& successor : task.successors) {
      successor = newTaskId[static_cast<size_t>(successor)];
    }
    std::sort(task.predecessors.begin(), task.predecessors.end());
    std::sort(task.successors.begin(), task.successors.end());
  }
  tasks.tasks_.swap(reorderedTasks);
  tasks.taskByNode_.clear();
  for (size_t taskId = 0; taskId < tasks.tasks_.size(); ++taskId) {
    for (Node* node : tasks.tasks_[taskId].members) tasks.taskByNode_[node] = taskId;
  }
  for (int taskId = 0; taskId < denseTaskCount; ++taskId) {
    for (int successor : tasks.tasks_[static_cast<size_t>(taskId)].successors) {
      Assert(taskId < successor, "renumbered dense edge is not forward: %d -> %d", taskId, successor);
    }
  }

  collectWorkerLocalNodes(tasks, graph);

  workers.workerTasks_.assign(static_cast<size_t>(workerCount), std::vector<int>());
  std::vector<int> workerPosition(tasks.tasks_.size(), -1);
  for (size_t taskId = 0; taskId < tasks.tasks_.size(); ++taskId) {
    int owner = tasks.tasks_[taskId].owner;
    workerPosition[taskId] = static_cast<int>(workers.workerTasks_[static_cast<size_t>(owner)].size());
    workers.workerTasks_[static_cast<size_t>(owner)].push_back(taskId);
  }

  buildStateUpdates(graph, workers, workerCount, resetChunk);

  // Async-reset registers retain their mid-cycle rendezvous. Normal registers,
  // synchronous-reset registers, and memory writes use the cycle-start state
  // phase built above.
  int resetId = 0;
  for (SuperNode* super : graph.allReset) {
    if (super->resetNode->status == CONSTANT_NODE) continue;
    if (super->superType != SUPER_ASYNC_RESET) continue;
    MtReset reset;
    reset.super = super;
    reset.id = resetId++;
    reset.asynchronous = super->superType == SUPER_ASYNC_RESET;
    Assert(reset.asynchronous, "non-async reset entered async reset planner");

    auto triggerTask = tasks.taskByNode_.find(super->resetNode);
    Assert(triggerTask != tasks.taskByNode_.end(),
           "missing trigger MTask for async reset %s", super->resetNode->name.c_str());
    reset.triggerTask = triggerTask->second;
    Assert(reset.triggerTask >= 0, "missing trigger MTask for async reset %s",
           super->resetNode->name.c_str());
    reset.triggerOwner = tasks.tasks_[static_cast<size_t>(reset.triggerTask)].owner;

    std::map<int, std::vector<Node*>> membersByWorker;
    for (Node* member : super->member) {
      Assert(member->type == NODE_REG_RESET, "invalid async reset member %s",
             member->name.c_str());
      Node* reg = member->getResetSrc();
      int owner = reset.triggerOwner;
      auto findOwner = [&](Node* candidate) {
        if (candidate == nullptr) return -1;
        auto task = tasks.taskByNode_.find(candidate);
        return task != tasks.taskByNode_.end()
                   ? tasks.tasks_[static_cast<size_t>(task->second)].owner
                   : -1;
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
      MtReset::Worker worker;
      worker.id = entry.first;
      for (size_t i = range.first; i < range.second; ++i) {
        worker.body.push_back(
            {static_cast<uint8_t>(instructions[i].infoType), instructions[i].inst});
      }
      worker.chunkCount = countResetChunks(worker.body, resetChunk);
      reset.workers.push_back(std::move(worker));
    }

    for (int worker = 0; worker < workerCount; ++worker) {
      if (worker == reset.triggerOwner) continue;
      const std::vector<int>& chain = workers.workerTasks_[static_cast<size_t>(worker)];
      const bool ownsResetState = membersByWorker.find(worker) != membersByWorker.end();
      auto position = std::upper_bound(chain.begin(), chain.end(), reset.triggerTask);
      if (position == chain.end() && !ownsResetState) continue;
      reset.participants.push_back(worker);
    }

    workers.asyncResetIds_[super->resetNode] = reset.id;
    workers.resets_.push_back(std::move(reset));
  }

  // A remote worker rendezvous before its first post-trigger task. This keeps
  // pre-trigger work ahead of reset and prevents post-trigger work from seeing
  // partially reset state.
  workers.resetJoins_.assign(static_cast<size_t>(workerCount), std::vector<MtResetJoin>());
  for (const MtReset& reset : workers.resets_) {
    if (!reset.asynchronous) continue;
    for (int worker : reset.participants) {
      const std::vector<int>& chain = workers.workerTasks_[static_cast<size_t>(worker)];
      const auto position = std::upper_bound(chain.begin(), chain.end(), reset.triggerTask);
      workers.resetJoins_[static_cast<size_t>(worker)].push_back(
          {reset.id, static_cast<size_t>(position - chain.begin())});
    }
  }
  for (std::vector<MtResetJoin>& joins : workers.resetJoins_) {
    std::sort(joins.begin(), joins.end(), [&](const MtResetJoin& lhs, const MtResetJoin& rhs) {
      if (lhs.beforePosition != rhs.beforePosition) return lhs.beforePosition < rhs.beforePosition;
      const int lhsTrigger = workers.resets_[static_cast<size_t>(lhs.resetId)].triggerTask;
      const int rhsTrigger = workers.resets_[static_cast<size_t>(rhs.resetId)].triggerTask;
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
  for (size_t consumer = 0; consumer < tasks.tasks_.size(); ++consumer) {
    std::map<int, std::vector<int>> sourcesByOwner;
    for (int predecessor : tasks.tasks_[consumer].predecessors) {
      int owner = tasks.tasks_[static_cast<size_t>(predecessor)].owner;
      if (owner != tasks.tasks_[consumer].owner) {
        sourcesByOwner[owner].push_back(predecessor);
      }
    }
    for (const auto& entry : sourcesByOwner) {
      int publisher = *std::max_element(entry.second.begin(), entry.second.end(), [&](int lhs, int rhs) {
        return workerPosition[static_cast<size_t>(lhs)] < workerPosition[static_cast<size_t>(rhs)];
      });
      TokenGroup group;
      group.producerOwner = entry.first;
      group.consumerOwner = tasks.tasks_[consumer].owner;
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
  workers.readySlotCount_ = std::max(1, nextSlot);
  for (const TokenGroup& group : groups) {
    tasks.tasks_[static_cast<size_t>(group.consumer)].waits.push_back(group.slot);
    tasks.tasks_[static_cast<size_t>(group.publisher)].stores.push_back(group.slot);
  }

  for (MtTask& task : tasks.tasks_) {
    task.waitBegin = workers.waitSlots_.size();
    workers.waitSlots_.insert(workers.waitSlots_.end(), task.waits.begin(), task.waits.end());
    task.waitEnd = workers.waitSlots_.size();
    task.storeBegin = workers.storeSlots_.size();
    workers.storeSlots_.insert(workers.storeSlots_.end(), task.stores.begin(), task.stores.end());
    task.storeEnd = workers.storeSlots_.size();
  }

  size_t edgeCount = 0;
  for (const MtTask& task : tasks.tasks_) edgeCount += task.successors.size();
  fprintf(stderr,
          "[cppEmitter-mt] workers=%d groups=%d mtasks=%zu edges=%zu tokens=%zu\n",
          workerCount, groupCount, tasks.tasks_.size(), edgeCount, groups.size());
}

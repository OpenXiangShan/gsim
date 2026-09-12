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
  return std::max<int>(1, static_cast<int>(instructionCount) + task.globalNodeCount);
}

bool taskEmitsCode(const MtTask& task, const MtTaskPlan& plan) {
  if (task.insts != nullptr && !task.insts->empty()) return true;
  for (int cppId : task.cppIds) {
    const SuperType type = plan.byCppId_[static_cast<size_t>(cppId)]->superType;
    if (type == SUPER_EXTMOD || type == SUPER_ASYNC_RESET) return true;
  }
  return false;
}

void compactEmptyTasks(MtTaskPlan& plan) {
  const std::vector<MtTask> original = plan.tasks_;
  std::vector<int> oldToNew(original.size(), -1);
  std::vector<MtTask> compacted;
  compacted.reserve(original.size());
  for (size_t oldId = 0; oldId < original.size(); ++oldId) {
    if (!taskEmitsCode(original[oldId], plan)) continue;
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

  plan.taskByCppId_.assign(plan.byCppId_.size(), -1);
  for (size_t taskId = 0; taskId < plan.tasks_.size(); ++taskId) {
    for (int cppId : plan.tasks_[taskId].cppIds) {
      plan.taskByCppId_[static_cast<size_t>(cppId)] = static_cast<int>(taskId);
    }
  }
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


}  // namespace

void MtWorkerBuilder::build(graph& graph, MtTaskPlan& tasks, MtWorkerPlan& workers,
                            int workerCount, int resetChunk) {
  const int taskCount = static_cast<int>(tasks.byCppId_.size());
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
  for (int taskId = 0; taskId < denseTaskCount; ++taskId) {
    for (int successor : tasks.tasks_[static_cast<size_t>(taskId)].successors) {
      Assert(taskId < successor, "renumbered dense edge is not forward: %d -> %d", taskId, successor);
    }
  }

  workers.workerTasks_.assign(static_cast<size_t>(workerCount), std::vector<int>());
  std::vector<int> workerPosition(tasks.tasks_.size(), -1);
  std::vector<int> taskByFinalCppId(static_cast<size_t>(taskCount), -1);
  for (size_t taskId = 0; taskId < tasks.tasks_.size(); ++taskId) {
    int owner = tasks.tasks_[taskId].owner;
    workerPosition[taskId] = static_cast<int>(workers.workerTasks_[static_cast<size_t>(owner)].size());
    workers.workerTasks_[static_cast<size_t>(owner)].push_back(taskId);
    for (int cppId : tasks.tasks_[taskId].cppIds) {
      taskByFinalCppId[static_cast<size_t>(cppId)] = static_cast<int>(taskId);
    }
  }

  // MtReset each register on the worker that normally owns its update MTask.
  // The trigger task is kept intact so oldReset || newReset is still computed once.
  int resetId = 0;
  for (SuperNode* super : graph.allReset) {
    if (super->resetNode->status == CONSTANT_NODE) continue;
    MtReset reset;
    reset.super = super;
    reset.id = resetId++;
    reset.asynchronous = super->superType == SUPER_ASYNC_RESET;
    if (!reset.asynchronous) {
      const auto range = resetBodyRange(super);
      for (size_t i = range.first; i < range.second; ++i) {
        reset.body.push_back(
            {static_cast<uint8_t>(super->insts[i].infoType), super->insts[i].inst});
      }
      reset.chunkCount = countResetChunks(reset.body, resetChunk);
      workers.resets_.push_back(std::move(reset));
      continue;
    }

    const int triggerCppId = super->resetNode->super->cppId;
    Assert(triggerCppId >= 0 && triggerCppId < taskCount,
           "missing trigger SuperNode for async reset %s", super->resetNode->name.c_str());
    reset.triggerTask = taskByFinalCppId[static_cast<size_t>(triggerCppId)];
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
        if (candidate == nullptr || candidate->super == nullptr) return -1;
        const int cppId = candidate->super->cppId;
        if (cppId < 0 || cppId >= taskCount) return -1;
        const int taskId = taskByFinalCppId[static_cast<size_t>(cppId)];
        return taskId >= 0 ? tasks.tasks_[static_cast<size_t>(taskId)].owner : -1;
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
  size_t instructionCount = 0;
  size_t globalNodeCount = 0;
  size_t scheduleCost = 0;
  for (const MtTask& task : tasks.tasks_) {
    instructionCount += task.insts == nullptr ? 0 : task.insts->size();
    globalNodeCount += static_cast<size_t>(std::max(0, task.globalNodeCount));
    scheduleCost += static_cast<size_t>(std::max(0, task.cost));
  }
  fprintf(stderr,
          "[cppEmitter-mt] cost insts=%zu global_nodes=%zu schedule=%zu\n",
          instructionCount, globalNodeCount, scheduleCost);
  fprintf(stderr,
          "[cppEmitter-mt] workers=%d supernodes=%d mtasks=%zu edges=%zu tokens=%zu\n",
          workerCount, taskCount, tasks.tasks_.size(), edgeCount, groups.size());
}

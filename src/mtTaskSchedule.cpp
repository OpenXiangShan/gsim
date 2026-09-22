#include "mtTaskSchedule.h"

#include "common.h"

#include <algorithm>
#include <climits>
#include <map>
#include <numeric>
#include <set>
#include <unordered_map>
#include <utility>

namespace {

int loweredTaskCost(const MtTask& task) {
  const size_t instructionCount = task.insts == nullptr ? 0 : task.insts->size();
  return std::max<int>(1, static_cast<int>(instructionCount) +
                              task.globalNodeCount * globalConfig.MtScheduleGlobalWeight);
}

bool taskEmitsCode(const MtTask& task) {
  if (task.insts != nullptr && !task.insts->empty()) return true;
  return task.kind == MtTaskKind::ExtModule;
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
    task.localWaits.clear();
    task.waitBegin = task.waitEnd = task.storeBegin = task.storeEnd = 0;
    task.localWaitBegin = task.localWaitEnd = 0;
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

size_t transitivelyReduceTaskEdges(MtTaskPlan& plan) {
  const size_t taskCount = plan.tasks_.size();
  if (taskCount == 0) return 0;
  const size_t wordCount = (taskCount + 63) / 64;
  std::vector<std::vector<uint64_t>> reachable(
      taskCount, std::vector<uint64_t>(wordCount, 0));
  size_t removed = 0;

  for (size_t reverse = taskCount; reverse > 0; --reverse) {
    const size_t source = reverse - 1;
    std::vector<int> successors = plan.tasks_[source].successors;
    std::sort(successors.begin(), successors.end());
    successors.erase(std::unique(successors.begin(), successors.end()),
                     successors.end());

    std::vector<uint64_t> covered(wordCount, 0);
    std::vector<int> reduced;
    reduced.reserve(successors.size());
    for (int successor : successors) {
      Assert(successor > static_cast<int>(source) &&
                 successor < static_cast<int>(taskCount),
             "cannot reduce non-forward MTask edge %zu -> %d", source,
             successor);
      const size_t successorIndex = static_cast<size_t>(successor);
      const uint64_t bit = uint64_t{1} << (successorIndex & 63);
      if ((covered[successorIndex >> 6] & bit) != 0) {
        ++removed;
        continue;
      }
      reduced.push_back(successor);
      covered[successorIndex >> 6] |= bit;
      for (size_t word = 0; word < wordCount; ++word) {
        covered[word] |= reachable[successorIndex][word];
      }
    }
    plan.tasks_[source].successors.swap(reduced);
    reachable[source].swap(covered);
  }

  std::vector<std::vector<int>> predecessors(taskCount);
  for (size_t source = 0; source < taskCount; ++source) {
    for (int successor : plan.tasks_[source].successors) {
      predecessors[static_cast<size_t>(successor)].push_back(
          static_cast<int>(source));
    }
  }
  for (size_t taskId = 0; taskId < taskCount; ++taskId) {
    plan.tasks_[taskId].predecessors.swap(predecessors[taskId]);
  }
  return removed;
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
         node->regSplit && node->regNext != nullptr &&
         (node->regNext->status == VALID_NODE ||
          node->regNext->status == CONSTANT_NODE);
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

void buildStateUpdates(graph& graph, const MtTaskPlan& tasks,
                       MtWorkerPlan& workers, int workerCount, int chunkSize) {
  workers.stateUpdates_.assign(static_cast<size_t>(workerCount), MtStateUpdate());
  for (int worker = 0; worker < workerCount; ++worker) {
    workers.stateUpdates_[static_cast<size_t>(worker)].worker = worker;
  }

  std::vector<Node*> registers;
  for (Node* reg : graph.regsrc) {
    if (!isCycleStartRegisterUpdate(reg)) continue;
    Assert(reg->regSplit &&
               (reg->getDst()->status == VALID_NODE ||
                reg->getDst()->status == CONSTANT_NODE),
           "packed MT register %s has neither valid nor constant destination storage",
           reg->name.c_str());
    registers.push_back(reg);
  }
  std::sort(registers.begin(), registers.end(), [](Node* lhs, Node* rhs) {
    const size_t lhsCost = registerStorageBytes(lhs);
    const size_t rhsCost = registerStorageBytes(rhs);
    if (lhsCost != rhsCost) return lhsCost > rhsCost;
    return lhs->id < rhs->id;
  });

  size_t totalRegisterBytes = 0;
  for (Node* reg : registers) totalRegisterBytes += registerStorageBytes(reg);
  const size_t targetRegisterBytes = std::max<size_t>(
      1, (totalRegisterBytes + static_cast<size_t>(workerCount) - 1) /
             static_cast<size_t>(workerCount));

  std::vector<int> preferredOwner(registers.size(), -1);
  for (size_t index = 0; index < registers.size(); ++index) {
    Node* dst = registers[index]->getDst();
    if (dst->status != VALID_NODE) continue;
    auto task = tasks.taskByNode_.find(dst);
    Assert(task != tasks.taskByNode_.end(),
           "register destination %s has no MTask owner", dst->name.c_str());
    const int taskId = task->second;
    Assert(taskId >= 0 && taskId < static_cast<int>(tasks.tasks_.size()),
           "invalid MTask %d for register destination %s", taskId,
           dst->name.c_str());
    preferredOwner[index] = tasks.tasks_[static_cast<size_t>(taskId)].owner;
  }

  // Preserve the old size-only LPT placement as a baseline for locality
  // reporting. The affinity-aware placement below retains the same descending
  // size order, but admits a preferred owner while it remains within the
  // average register-byte target. An indivisible register larger than the
  // target may occupy an otherwise empty preferred worker.
  std::vector<size_t> baselineLoad(static_cast<size_t>(workerCount), 0);
  size_t baselineLocalRegisters = 0;
  size_t baselineLocalBytes = 0;
  for (size_t index = 0; index < registers.size(); ++index) {
    const size_t cost = registerStorageBytes(registers[index]);
    const int worker = static_cast<int>(
        std::min_element(baselineLoad.begin(), baselineLoad.end()) -
        baselineLoad.begin());
    baselineLoad[static_cast<size_t>(worker)] += cost;
    if (worker == preferredOwner[index]) {
      ++baselineLocalRegisters;
      baselineLocalBytes += cost;
    }
  }

  std::vector<size_t> workerLoad(static_cast<size_t>(workerCount), 0);
  std::map<Node*, int> ownerByRegister;
  size_t preferredRegisters = 0;
  size_t preferredBytes = 0;
  size_t localRegisters = 0;
  size_t localBytes = 0;
  for (size_t index = 0; index < registers.size(); ++index) {
    Node* reg = registers[index];
    const size_t cost = registerStorageBytes(reg);
    const int preferred = preferredOwner[index];
    if (preferred >= 0) {
      ++preferredRegisters;
      preferredBytes += cost;
    }
    const int lightest = static_cast<int>(
        std::min_element(workerLoad.begin(), workerLoad.end()) -
        workerLoad.begin());
    int worker = lightest;
    if (preferred >= 0) {
      const size_t preferredLoad = workerLoad[static_cast<size_t>(preferred)];
      if ((preferredLoad <= targetRegisterBytes &&
           cost <= targetRegisterBytes - preferredLoad) ||
          (preferredLoad == 0 && cost > targetRegisterBytes)) {
        worker = preferred;
      }
    }
    workerLoad[static_cast<size_t>(worker)] += cost;
    if (worker == preferred) {
      ++localRegisters;
      localBytes += cost;
    }
    ownerByRegister[reg] = worker;
    MtStateUpdate& update = workers.stateUpdates_[static_cast<size_t>(worker)];
    update.registerStorageBytes += cost;
    update.registers.push_back(reg);
    Node* dst = reg->getDst();
    if (dst->status == CONSTANT_NODE) {
      Assert(reg->dimension.empty() && dst->computeInfo != nullptr,
             "constant MT register destination %s is not a scalar constant",
             dst->name.c_str());
      update.body.push_back(
          {static_cast<uint8_t>(SUPER_INFO_STR),
           dst->name + " = " + dst->computeInfo->valStr + ";"});
    }
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
          "[cppEmitter-mt] register-locality mapped-registers=%zu local-registers=%zu->%zu "
          "local-bytes=%zu->%zu/%zu target-worker-bytes=%zu\n",
          preferredRegisters, baselineLocalRegisters, localRegisters,
          baselineLocalBytes, localBytes, preferredBytes, targetRegisterBytes);
  fprintf(stderr,
          "[cppEmitter-mt] memory-commits writers=%zu pending-bytes=%zu "
          "min-worker-state-bytes=%zu max-worker-state-bytes=%zu\n",
          memoryWriterCount, memoryWriteBytes, *stateLimits.first, *stateLimits.second);
}

struct ScheduledInterval {
  int task = -1;
  long long start = 0;
  long long finish = 0;
};

struct WorkerSchedule {
  std::vector<int> owner;
  std::vector<std::vector<int>> chains;
  long long makespan = 0;
};

using EdgeCommunicationNodes =
    std::vector<std::unordered_map<int, std::unordered_set<Node*>>>;

EdgeCommunicationNodes buildEdgeCommunicationNodes(const MtTaskPlan& tasks) {
  const size_t taskCount = tasks.tasks_.size();
  EdgeCommunicationNodes nodes(taskCount);
  for (size_t consumerId = 0; consumerId < taskCount; ++consumerId) {
    const MtTask& consumer = tasks.tasks_[consumerId];
    std::unordered_map<int, std::unordered_set<Node*>> nodesByProducer;
    for (Node* consumerNode : consumer.members) {
      for (Node* predecessorNode : consumerNode->prev) {
        auto found = tasks.taskByNode_.find(predecessorNode);
        if (found == tasks.taskByNode_.end()) continue;
        const int producerId = found->second;
        if (producerId < 0 || producerId == static_cast<int>(consumerId) ||
            producerId >= static_cast<int>(taskCount)) {
          continue;
        }
        const MtTask& producer = tasks.tasks_[static_cast<size_t>(producerId)];
        if (producer.localNodes.count(predecessorNode) == 0) {
          nodesByProducer[producerId].insert(predecessorNode);
        }
      }
    }
    for (auto& entry : nodesByProducer) {
      nodes[consumerId][entry.first] = std::move(entry.second);
    }
  }
  return nodes;
}

size_t edgeCommunicationNodeCount(const EdgeCommunicationNodes& nodes,
                                  int consumer, int producer) {
  const auto& byProducer = nodes[static_cast<size_t>(consumer)];
  auto found = byProducer.find(producer);
  return found == byProducer.end() ? 0 : found->second.size();
}

long long workerCommunicationReadyTime(
    const MtTaskPlan& tasks, const EdgeCommunicationNodes& nodes,
    int consumer, int candidateWorker, const std::vector<int>& owner,
    const std::vector<long long>& completion, long long readyAt) {
  std::unordered_map<int, long long> producerCompletion;
  std::unordered_map<int, std::unordered_set<Node*>> nodesByWorker;
  for (int predecessor : tasks.tasks_[static_cast<size_t>(consumer)].predecessors) {
    const int producerWorker = owner[static_cast<size_t>(predecessor)];
    if (producerWorker < 0 || producerWorker == candidateWorker) continue;
    producerCompletion[producerWorker] = std::max(
        producerCompletion[producerWorker], completion[static_cast<size_t>(predecessor)]);
    const auto& byProducer = nodes[static_cast<size_t>(consumer)];
    auto found = byProducer.find(predecessor);
    if (found != byProducer.end()) {
      auto& remoteNodes = nodesByWorker[producerWorker];
      remoteNodes.insert(found->second.begin(), found->second.end());
    }
  }
  for (const auto& entry : producerCompletion) {
    const size_t nodeCount = nodesByWorker[entry.first].size();
    const long long communication =
        static_cast<long long>(nodeCount) * globalConfig.MtScheduleCommNodeWeight;
    readyAt = std::max(readyAt, entry.second + communication);
  }
  return readyAt;
}

size_t actualCrossWorkerCommunicationNodes(
    const MtTaskPlan& tasks, const EdgeCommunicationNodes& nodes) {
  size_t total = 0;
  for (size_t consumer = 0; consumer < tasks.tasks_.size(); ++consumer) {
    const int consumerWorker = tasks.tasks_[consumer].owner;
    std::unordered_map<int, std::unordered_set<Node*>> remoteNodes;
    for (const auto& entry : nodes[consumer]) {
      const int producer = entry.first;
      if (tasks.tasks_[static_cast<size_t>(producer)].owner == consumerWorker) continue;
      const int producerWorker = tasks.tasks_[static_cast<size_t>(producer)].owner;
      auto& workerNodes = remoteNodes[producerWorker];
      workerNodes.insert(entry.second.begin(), entry.second.end());
    }
    for (const auto& entry : remoteNodes) total += entry.second.size();
  }
  return total;
}

WorkerSchedule scheduleList(const MtTaskPlan& tasks, int workerCount,
                            const EdgeCommunicationNodes& communicationNodes) {
  const int taskCount = static_cast<int>(tasks.tasks_.size());
  WorkerSchedule result;
  result.owner.assign(static_cast<size_t>(taskCount), -1);
  result.chains.assign(static_cast<size_t>(workerCount), std::vector<int>());

  std::vector<int> remaining(static_cast<size_t>(taskCount), 0);
  std::vector<long long> completion(static_cast<size_t>(taskCount), 0);
  std::vector<long long> workerAvailable(static_cast<size_t>(workerCount), 0);
  std::vector<long long> priority(static_cast<size_t>(taskCount), 0);
  for (int taskId = taskCount - 1; taskId >= 0; --taskId) {
    long long successorPriority = 0;
    for (int successor : tasks.tasks_[static_cast<size_t>(taskId)].successors) {
      successorPriority = std::max(successorPriority, priority[static_cast<size_t>(successor)]);
    }
    priority[static_cast<size_t>(taskId)] =
        std::max(1, tasks.tasks_[static_cast<size_t>(taskId)].cost) + successorPriority;
  }
  std::vector<int> ready;
  for (int taskId = 0; taskId < taskCount; ++taskId) {
    remaining[static_cast<size_t>(taskId)] =
        static_cast<int>(tasks.tasks_[static_cast<size_t>(taskId)].predecessors.size());
    if (remaining[static_cast<size_t>(taskId)] == 0) ready.push_back(taskId);
  }

  while (!ready.empty()) {
    int bestIndex = -1;
    int bestTask = -1;
    int bestWorker = -1;
    long long bestStart = LLONG_MAX;
    for (size_t index = 0; index < ready.size(); ++index) {
      const int taskId = ready[index];
      const MtTask& task = tasks.tasks_[static_cast<size_t>(taskId)];
      for (int worker = 0; worker < workerCount; ++worker) {
        long long start = workerAvailable[static_cast<size_t>(worker)];
        for (int predecessor : task.predecessors) {
          start = std::max(start, completion[static_cast<size_t>(predecessor)]);
        }
        start = workerCommunicationReadyTime(
            tasks, communicationNodes, taskId, worker, result.owner, completion, start);
        bool better = start < bestStart;
        if (start == bestStart && bestTask >= 0) {
          better = priority[static_cast<size_t>(taskId)] >
                       priority[static_cast<size_t>(bestTask)] ||
                   (priority[static_cast<size_t>(taskId)] ==
                        priority[static_cast<size_t>(bestTask)] &&
                    taskId < bestTask);
        }
        if (better) {
          bestStart = start;
          bestIndex = static_cast<int>(index);
          bestTask = taskId;
          bestWorker = worker;
        }
      }
    }
    Assert(bestTask >= 0, "list scheduler failed with %zu ready tasks", ready.size());
    const long long finish = bestStart + std::max(1, tasks.tasks_[static_cast<size_t>(bestTask)].cost);
    result.owner[static_cast<size_t>(bestTask)] = bestWorker;
    completion[static_cast<size_t>(bestTask)] = finish;
    workerAvailable[static_cast<size_t>(bestWorker)] = finish;
    result.chains[static_cast<size_t>(bestWorker)].push_back(bestTask);
    result.makespan = std::max(result.makespan, finish);

    ready[static_cast<size_t>(bestIndex)] = ready.back();
    ready.pop_back();
    for (int successor : tasks.tasks_[static_cast<size_t>(bestTask)].successors) {
      if (--remaining[static_cast<size_t>(successor)] == 0) ready.push_back(successor);
    }
  }
  return result;
}

WorkerSchedule scheduleHeft(const MtTaskPlan& tasks, int workerCount,
                            const EdgeCommunicationNodes& communicationNodes) {
  const int taskCount = static_cast<int>(tasks.tasks_.size());
  WorkerSchedule result;
  result.owner.assign(static_cast<size_t>(taskCount), -1);
  result.chains.assign(static_cast<size_t>(workerCount), std::vector<int>());

  // HEFT upward rank. Task IDs are already a forward topological order, so
  // successors have larger IDs and can be processed in reverse order.
  std::vector<long long> rank(static_cast<size_t>(taskCount), 0);
  for (int taskId = taskCount - 1; taskId >= 0; --taskId) {
    long long tail = 0;
    for (int successor : tasks.tasks_[static_cast<size_t>(taskId)].successors) {
      const long long averageCommunication =
          static_cast<long long>(edgeCommunicationNodeCount(communicationNodes, successor, taskId)) *
          globalConfig.MtScheduleCommNodeWeight * std::max(0, workerCount - 1) /
          std::max(1, workerCount);
      tail = std::max(tail, averageCommunication + rank[static_cast<size_t>(successor)]);
    }
    rank[static_cast<size_t>(taskId)] =
        std::max(1, tasks.tasks_[static_cast<size_t>(taskId)].cost) + tail;
  }

  std::vector<int> order(static_cast<size_t>(taskCount));
  std::iota(order.begin(), order.end(), 0);
  std::sort(order.begin(), order.end(), [&](int lhs, int rhs) {
    if (rank[static_cast<size_t>(lhs)] != rank[static_cast<size_t>(rhs)]) {
      return rank[static_cast<size_t>(lhs)] > rank[static_cast<size_t>(rhs)];
    }
    return lhs < rhs;
  });

  std::vector<long long> completion(static_cast<size_t>(taskCount), 0);
  std::vector<std::map<long long, ScheduledInterval>> timelines(
      static_cast<size_t>(workerCount));

  for (int taskId : order) {
    const MtTask& task = tasks.tasks_[static_cast<size_t>(taskId)];
    const long long duration = std::max(1, task.cost);
    int bestWorker = -1;
    long long bestStart = LLONG_MAX;
    long long bestFinish = LLONG_MAX;

    for (int worker = 0; worker < workerCount; ++worker) {
      long long readyAt = 0;
      for (int predecessor : task.predecessors) {
        readyAt = std::max(readyAt, completion[static_cast<size_t>(predecessor)]);
      }
      readyAt = workerCommunicationReadyTime(
          tasks, communicationNodes, taskId, worker, result.owner, completion, readyAt);

      long long start = readyAt;
      auto& timeline = timelines[static_cast<size_t>(worker)];
      auto next = timeline.lower_bound(start);
      if (next != timeline.begin()) {
        auto previous = std::prev(next);
        if (previous->second.finish > start) {
          start = previous->second.finish;
          next = std::next(previous);
        }
      }
      while (next != timeline.end() && start + duration > next->second.start) {
        start = std::max(start, next->second.finish);
        ++next;
      }
      const long long finish = start + duration;
      if (finish < bestFinish ||
          (finish == bestFinish &&
           (start < bestStart || (start == bestStart && worker < bestWorker)))) {
        bestWorker = worker;
        bestStart = start;
        bestFinish = finish;
      }
    }

    Assert(bestWorker >= 0, "HEFT scheduler failed to place task %d", taskId);
    result.owner[static_cast<size_t>(taskId)] = bestWorker;
    completion[static_cast<size_t>(taskId)] = bestFinish;
    result.makespan = std::max(result.makespan, bestFinish);
    ScheduledInterval placed{taskId, bestStart, bestFinish};
    timelines[static_cast<size_t>(bestWorker)].emplace(placed.start, placed);
  }

  for (int worker = 0; worker < workerCount; ++worker) {
    for (const auto& entry : timelines[static_cast<size_t>(worker)]) {
      result.chains[static_cast<size_t>(worker)].push_back(entry.second.task);
    }
  }
  return result;
}


}  // namespace

void MtWorkerBuilder::build(graph& graph, MtTaskPlan& tasks, MtWorkerPlan& workers,
                            int workerCount, int resetChunk) {
  const int groupCount = tasks.partitionGroupCount_;
  compactEmptyTasks(tasks);
  for (MtTask& task : tasks.tasks_) task.cost = loweredTaskCost(task);
  const EdgeCommunicationNodes communicationNodes = buildEdgeCommunicationNodes(tasks);
  size_t communicationEdges = 0;
  long long communicationUnits = 0;
  for (const auto& byProducer : communicationNodes) {
    communicationEdges += byProducer.size();
    for (const auto& entry : byProducer) communicationUnits += entry.second.size();
  }
  WorkerSchedule schedule = globalConfig.MtScheduler == "list"
                                ? scheduleList(tasks, workerCount, communicationNodes)
                                : scheduleHeft(tasks, workerCount, communicationNodes);
  for (size_t taskId = 0; taskId < tasks.tasks_.size(); ++taskId) {
    tasks.tasks_[taskId].owner = schedule.owner[taskId];
  }
  const size_t actualCrossWorkerNodes =
      actualCrossWorkerCommunicationNodes(tasks, communicationNodes);
  fprintf(stderr,
          "[cppEmitter-mt] scheduler=%s estimated-makespan=%lld "
          "potential-comm-edges=%zu potential-comm-nodes=%lld "
          "actual-cross-worker-nodes=%zu comm-node-weight=%d\n",
          globalConfig.MtScheduler.c_str(), schedule.makespan, communicationEdges,
          communicationUnits, actualCrossWorkerNodes,
          globalConfig.MtScheduleCommNodeWeight);

  const size_t transitiveEdgesRemoved = transitivelyReduceTaskEdges(tasks);

  collectWorkerLocalNodes(tasks, graph);

  workers.workerTasks_ = std::move(schedule.chains);
  std::vector<int> workerPosition(tasks.tasks_.size(), -1);
  for (int worker = 0; worker < workerCount; ++worker) {
    const std::vector<int>& chain = workers.workerTasks_[static_cast<size_t>(worker)];
    for (size_t position = 0; position < chain.size(); ++position) {
      const int taskId = chain[position];
      Assert(taskId >= 0 && taskId < static_cast<int>(tasks.tasks_.size()),
             "worker %d contains invalid task %d", worker, taskId);
      Assert(tasks.tasks_[static_cast<size_t>(taskId)].owner == worker,
             "task %d owner mismatch: task=%d worker=%d", taskId,
             tasks.tasks_[static_cast<size_t>(taskId)].owner, worker);
      workerPosition[static_cast<size_t>(taskId)] = static_cast<int>(position);
    }
  }

  for (size_t taskId = 0; taskId < tasks.tasks_.size(); ++taskId) {
    MtTask& task = tasks.tasks_[taskId];
    task.localWaits.clear();
    for (int predecessor : task.predecessors) {
      const MtTask& source = tasks.tasks_[static_cast<size_t>(predecessor)];
      if (source.owner != task.owner) continue;
      Assert(workerPosition[static_cast<size_t>(predecessor)] < workerPosition[taskId],
             "same-worker predecessor is not earlier: %d -> %zu", predecessor, taskId);
      task.localWaits.push_back(workerPosition[static_cast<size_t>(predecessor)]);
    }
    std::sort(task.localWaits.begin(), task.localWaits.end());
    task.localWaits.erase(std::unique(task.localWaits.begin(), task.localWaits.end()),
                          task.localWaits.end());
  }

  buildStateUpdates(graph, tasks, workers, workerCount, resetChunk);

  // Async reset is checked after the speculative task pass. Its body is kept
  // independent of worker ownership because the rare replay path is serial.
  int resetId = 0;
  for (SuperNode* super : graph.allReset) {
    if (super->resetNode->status == CONSTANT_NODE) continue;
    if (super->superType != SUPER_ASYNC_RESET) continue;
    MtReset reset;
    reset.super = super;
    reset.id = resetId++;
    for (Node* member : super->member) {
      Assert(member->type == NODE_REG_RESET, "invalid async reset member %s",
             member->name.c_str());
    }
    std::vector<InstInfo> instructions = buildResetInstructions(super->member);
    const auto range = resetBodyRange(instructions, super->resetNode);
    Assert(range.first == 1 && range.second + 1 == instructions.size(),
           "async reset %s does not have a single outer reset condition",
           super->resetNode->name.c_str());
    for (size_t i = range.first; i < range.second; ++i) {
      reset.body.push_back(
          {static_cast<uint8_t>(instructions[i].infoType), instructions[i].inst});
    }
    reset.chunkCount = countResetChunks(reset.body, resetChunk);
    workers.resets_.push_back(std::move(reset));
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
      // A compressed cross-worker token is published by the last source in
      // the producer's static chain. With out-of-order ready scanning, make
      // that ordering assumption explicit: the publisher cannot run until
      // every source represented by its token has completed locally.
      for (int source : entry.second) {
        if (source == publisher) continue;
        tasks.tasks_[static_cast<size_t>(publisher)].localWaits.push_back(
            workerPosition[static_cast<size_t>(source)]);
      }
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
    std::sort(task.localWaits.begin(), task.localWaits.end());
    task.localWaits.erase(std::unique(task.localWaits.begin(), task.localWaits.end()),
                          task.localWaits.end());
    task.waitBegin = workers.waitSlots_.size();
    workers.waitSlots_.insert(workers.waitSlots_.end(), task.waits.begin(), task.waits.end());
    task.waitEnd = workers.waitSlots_.size();
    task.storeBegin = workers.storeSlots_.size();
    workers.storeSlots_.insert(workers.storeSlots_.end(), task.stores.begin(), task.stores.end());
    task.storeEnd = workers.storeSlots_.size();
    task.localWaitBegin = workers.localWaitSlots_.size();
    workers.localWaitSlots_.insert(workers.localWaitSlots_.end(), task.localWaits.begin(),
                                   task.localWaits.end());
    task.localWaitEnd = workers.localWaitSlots_.size();
  }

  size_t edgeCount = 0;
  for (const MtTask& task : tasks.tasks_) edgeCount += task.successors.size();
  size_t localEdgeCount = 0;
  for (const MtTask& task : tasks.tasks_) localEdgeCount += task.localWaits.size();
  fprintf(stderr,
          "[cppEmitter-mt] workers=%d groups=%d mtasks=%zu edges=%zu tokens=%zu "
          "local-edges=%zu transitive-edges-removed=%zu\n",
          workerCount, groupCount, tasks.tasks_.size(), edgeCount, groups.size(),
          localEdgeCount, transitiveEdgesRemoved);
}

#include "mtTaskLowering.h"

#include "common.h"

#include <algorithm>
#include <cstdio>
#include <map>
#include <set>

extern int maxConcatNum;
std::string computeExtMod(Node* ext, std::vector<InstInfo>& instructions);

namespace {

int taskForNode(const Node* node, const MtTaskPlan& plan) {
  if (node == nullptr) return -1;
  auto found = plan.taskByNode_.find(const_cast<Node*>(node));
  return found == plan.taskByNode_.end() ? -1 : found->second;
}

ENode* combineMtWriteCondition(ENode* lhs, ENode* rhs) {
  if (lhs == nullptr) return rhs;
  ENode* condition = new ENode(OP_AND);
  condition->setWidth(1, false);
  condition->addChild(lhs);
  condition->addChild(rhs);
  return condition;
}

ENode* negateMtWriteCondition(ENode* condition) {
  ENode* result = new ENode(OP_NOT);
  result->setWidth(1, false);
  result->addChild(condition);
  return result;
}

struct SparseWriteValue {
  ENode* value = nullptr;
  ENode* enable = nullptr;
};

void collectSparseWriteValues(ENode* root, ENode* condition,
                              std::vector<SparseWriteValue>& writes) {
  if (root == nullptr || root->opType == OP_INVALID) return;
  if (root->opType == OP_WHEN || root->opType == OP_RESET) {
    Assert(root->getChildNum() >= 2, "invalid conditional register-array write");
    ENode* branchCondition = root->getChild(0);
    collectSparseWriteValues(
        root->getChild(1),
        combineMtWriteCondition(condition == nullptr ? nullptr : condition->dup(),
                                branchCondition->dup()),
        writes);
    if (root->getChildNum() >= 3) {
      collectSparseWriteValues(
          root->getChild(2),
          combineMtWriteCondition(condition == nullptr ? nullptr : condition->dup(),
                                  negateMtWriteCondition(branchCondition->dup())),
          writes);
    }
    return;
  }
  writes.push_back(
      {root->dup(), condition == nullptr ? allocIntEnode(1, "1") : condition});
}

size_t sparseRegisterStorageBytes(const Node* node) {
  size_t bytes = static_cast<size_t>(widthBits(node->width)) / 8;
  for (int dimension : node->dimension) {
    bytes *= static_cast<size_t>(upperPower2(dimension));
  }
  return std::max<size_t>(1, bytes);
}

ENode* sparseWriteAddress(ENode* index) {
  if (index->opType == OP_INDEX_INT) {
    Assert(index->values.size() == 1, "constant array index has no value");
    return allocIntEnode(std::max(1, index->width), std::to_string(index->values[0]));
  }
  Assert(index->opType == OP_INDEX && index->getChildNum() == 1,
         "unsupported sparse register-array index operation %d", index->opType);
  return index->getChild(0)->dup();
}

void selectSparseRegisters(graph& graph, MtTaskPlan& plan) {
  plan.sparseRegisters_.clear();
  plan.sparseRegisterBySource_.clear();
  plan.sparseRegisterByDestination_.clear();
  if (globalConfig.MtSparseRegisterMaxWrites == 0) return;

  size_t selectedBytes = 0;
  size_t selectedWrites = 0;
  for (Node* source : graph.regsrc) {
    if (source->status != VALID_NODE || source->type != NODE_REG_SRC ||
        !source->regSplit || source->dimension.size() != 1 ||
        source->regNext == nullptr || source->getDst()->status != VALID_NODE) {
      if (globalConfig.LogLevel > 1 && source->isArray()) {
        fprintf(stderr,
                "[cppEmitter-mt] keep register array %s: status=%d type=%d split=%d "
                "dimensions=%zu destination-status=%d\n",
                source->name.c_str(), source->status, source->type, source->regSplit,
                source->dimension.size(),
                source->regNext == nullptr ? -1 : source->getDst()->status);
      }
      continue;
    }
    const size_t storageBytes = sparseRegisterStorageBytes(source);
    if (storageBytes < static_cast<size_t>(globalConfig.MtSparseRegisterMinBytes)) continue;

    Node* destination = source->getDst();
    std::vector<std::pair<ENode*, SparseWriteValue>> writes;
    bool eligible = !destination->assignTree.empty();
    for (ExpTree* assignment : destination->assignTree) {
      ENode* lvalue = assignment->getlval();
      if (lvalue == nullptr || lvalue->getNode() != destination ||
          lvalue->getChildNum() != source->dimension.size()) {
        eligible = false;
        break;
      }
      std::set<Node*> dependencies;
      getENodeRelyNodes(assignment->getRoot(), dependencies);
      if (dependencies.count(destination) != 0) {
        eligible = false;
        break;
      }
      ENode* index = lvalue->getChild(0);
      if (index == nullptr ||
          (index->opType != OP_INDEX_INT && index->opType != OP_INDEX)) {
        eligible = false;
        break;
      }
      std::vector<SparseWriteValue> assignmentWrites;
      collectSparseWriteValues(assignment->getRoot(), nullptr, assignmentWrites);
      for (SparseWriteValue& write : assignmentWrites) {
        writes.emplace_back(index, write);
      }
      if (writes.size() > static_cast<size_t>(globalConfig.MtSparseRegisterMaxWrites)) {
        eligible = false;
        break;
      }
    }
    if (!eligible || writes.empty()) continue;

    MtSparseRegister sparse;
    sparse.source = source;
    sparse.storageBytes = storageBytes;
    for (size_t ordinal = 0; ordinal < writes.size(); ++ordinal) {
      Node* writer = source->dup(
          NODE_WRITER, source->name + "$SPARSE_WRITE$" + std::to_string(ordinal));
      writer->dimension.clear();
      writer->parent = source;

      ENode* write = new ENode(OP_WRITE_MEM);
      write->memoryNode = source;
      write->setWidth(source->width, source->sign);
      write->addChild(sparseWriteAddress(writes[ordinal].first));
      write->addChild(writes[ordinal].second.value);
      write->addChild(writes[ordinal].second.enable);

      sparse.writers.push_back(writer);
      sparse.writeTrees.push_back(new ExpTree(write, new ENode(writer)));
    }

    const size_t index = plan.sparseRegisters_.size();
    plan.sparseRegisterBySource_[source] = index;
    plan.sparseRegisterByDestination_[destination] = index;
    plan.sparseRegisters_.push_back(std::move(sparse));
    selectedBytes += storageBytes;
    selectedWrites += writes.size();
  }
  fprintf(stderr,
          "[cppEmitter-mt] sparse-register-arrays arrays=%zu bytes=%zu write-sites=%zu "
          "min-bytes=%d max-writes=%d\n",
          plan.sparseRegisters_.size(), selectedBytes, selectedWrites,
          globalConfig.MtSparseRegisterMinBytes,
          globalConfig.MtSparseRegisterMaxWrites);
}

void collectMtMemoryWrites(ENode* root, ENode* condition,
                           std::vector<ENode*>& writes) {
  if (root == nullptr) return;
  if (root->opType == OP_WHEN || root->opType == OP_RESET) {
    Assert(root->getChildNum() >= 2, "invalid conditional memory write");
    ENode* branchCondition = root->getChild(0);
    collectMtMemoryWrites(
        root->getChild(1),
        combineMtWriteCondition(condition == nullptr ? nullptr : condition->dup(),
                                branchCondition->dup()),
        writes);
    if (root->getChildNum() >= 3) {
      collectMtMemoryWrites(
          root->getChild(2),
          combineMtWriteCondition(condition == nullptr ? nullptr : condition->dup(),
                                  negateMtWriteCondition(branchCondition->dup())),
          writes);
    }
    return;
  }
  if (root->opType == OP_INVALID || root->opType == OP_EMPTY ||
      root->opType == OP_READ_MEM) {
    return;
  }
  Assert(root->opType == OP_WRITE_MEM,
         "unexpected operation %d in MT memory writer", root->opType);
  ENode* write = root->dup();
  Assert(write->getChildNum() == 2, "MT memory writer already has an enable");
  write->addChild(condition == nullptr ? allocIntEnode(1, "1") : condition->dup());
  writes.push_back(write);
}

void makeMtMemoryWriteEnablesExplicit(MtTaskPlan& plan) {
  for (MtTask& task : plan.tasks_) {
    for (Node* node : task.members) {
      if (node->type != NODE_WRITER && node->type != NODE_READWRITER) continue;
      for (ExpTree* assignment : node->assignTree) {
        std::vector<ENode*> writes;
        collectMtMemoryWrites(assignment->getRoot(), nullptr, writes);
        if (writes.empty()) continue;
        Assert(writes.size() == 1,
               "MT memory writer %s has %zu writes in one assignment tree",
               node->name.c_str(), writes.size());
        assignment->setRoot(writes.front());
        assignment->clearInfo();
      }
    }
  }
}

// generateStmtTree() restores direct expression dependencies for values whose
// only consumer is in the same SuperNode. At MTask granularity the equivalent
// scope is the complete task, so these edges may cross a SuperNode boundary but
// must never cross an MTask boundary.
void completeTaskLocalDependencies(graph& graph, MtTaskPlan& plan) {
  for (int taskId = static_cast<int>(plan.tasks_.size()) - 1; taskId >= 0; --taskId) {
    MtTask& task = plan.tasks_[static_cast<size_t>(taskId)];
    for (auto iter = task.members.rbegin(); iter != task.members.rend(); ++iter) {
      Node* node = *iter;
      int dependentCount = 0;
      for (Node* next : node->depNext) {
        if (taskForNode(next, plan) == taskId) ++dependentCount;
      }
      if (dependentCount != 1 || node->next.size() != 1) continue;

      bool leavesTask = false;
      for (Node* next : node->next) {
        if (taskForNode(next, plan) != taskId) {
          leavesTask = true;
          break;
        }
      }
      if (!leavesTask) node->updateConnect();
    }
  }
  MtTaskPartitioner::useDirectDependencies(graph);

  // A cross-task dependency discovered here would make the scheduler DAG that
  // was built before lowering stale.
  for (size_t taskId = 0; taskId < plan.tasks_.size(); ++taskId) {
    const MtTask& task = plan.tasks_[taskId];
    for (Node* node : task.members) {
      for (Node* next : node->depNext) {
        const int nextTask = taskForNode(next, plan);
        if (nextTask < 0 || nextTask == static_cast<int>(taskId)) continue;
        Assert(std::find(task.successors.begin(), task.successors.end(), nextTask) !=
                   task.successors.end(),
               "MT lowering discovered an unplanned dependency from task %zu to task %d",
               taskId, nextTask);
      }
    }
  }
}

void orderTaskMembers(MtTask& task) {
  std::set<Node*> scope(task.members.begin(), task.members.end());
  std::map<Node*, int> remainingPredecessors;
  std::map<Node*, size_t> originalPosition;
  for (size_t index = 0; index < task.members.size(); ++index) {
    Node* node = task.members[index];
    originalPosition[node] = index;
    int count = 0;
    for (Node* prev : node->depPrev) {
      if (scope.count(prev) != 0) ++count;
    }
    remainingPredecessors[node] = count;
  }

  std::vector<Node*> ready;
  for (Node* node : task.members) {
    if (remainingPredecessors[node] == 0) ready.push_back(node);
  }

  std::vector<Node*> ordered;
  ordered.reserve(task.members.size());
  while (!ready.empty()) {
    auto first = std::min_element(ready.begin(), ready.end(), [&](Node* lhs, Node* rhs) {
      return originalPosition[lhs] < originalPosition[rhs];
    });
    Node* node = *first;
    ready.erase(first);
    ordered.push_back(node);
    for (Node* next : node->depNext) {
      if (scope.count(next) != 0 && --remainingPredecessors[next] == 0) {
        ready.push_back(next);
      }
    }
  }

  Assert(ordered.size() == task.members.size(),
         "cannot topologically order MTask members (%zu/%zu)",
         ordered.size(), task.members.size());
  task.members.swap(ordered);
}

void collectTaskLocalNodes(graph& graph, MtTaskPlan& plan) {
  std::set<Node*> resetDependencies;
  for (SuperNode* reset : graph.allReset) {
    for (Node* member : reset->member) {
      for (ExpTree* assignment : member->assignTree) {
        assignment->getRelyNodes(resetDependencies);
      }
    }
  }

  plan.localNodes_.clear();
  auto hasModelStorage = [](const Node* node) {
    if (node->status != VALID_NODE || node->type == NODE_SPECIAL ||
        node->type == NODE_REG_RESET || node->type == NODE_WRITER) {
      return false;
    }
    return node->type != NODE_REG_DST || node->regSplit;
  };
  for (size_t taskId = 0; taskId < plan.tasks_.size(); ++taskId) {
    MtTask& task = plan.tasks_[taskId];
    task.localNodes.clear();
    task.globalNodeCount = 0;
    for (Node* node : task.members) {
      bool crossesTask = resetDependencies.count(node) != 0;
      for (Node* next : node->next) {
        const int nextTask = taskForNode(next, plan);
        if (nextTask >= 0 && nextTask != static_cast<int>(taskId)) {
          crossesTask = true;
          break;
        }
      }
      task.globalNodeCount += crossesTask && hasModelStorage(node) &&
                              !mtIsCycleStartRegisterUpdate(node);

      if (node->status != VALID_NODE || node->type != NODE_OTHERS ||
          node->isReset() || resetDependencies.count(node) != 0) {
        continue;
      }

      bool taskLocal = true;
      for (Node* next : node->next) {
        if (taskForNode(next, plan) != static_cast<int>(taskId)) {
          taskLocal = false;
          break;
        }
      }
      if (taskLocal && node->isArray()) {
        for (Node* next : node->depNext) {
          if (taskForNode(next, plan) != static_cast<int>(taskId)) {
            taskLocal = false;
            break;
          }
        }
      }
      if (!taskLocal) continue;

      task.localNodes.insert(node);
      plan.localNodes_.insert(node);
    }
  }
}

void mergeNodeAssignments(StmtTree& tree, Node* node, const MtTaskPlan* plan,
                          std::vector<int>& predecessorPath,
                          std::vector<int>& nodePath) {
  // Every register with stored or constant next-state commits in the
  // cycle-start phase. A late async reset overwrites src before replay.
  Assert(!mtIsCycleStartRegisterUpdate(node),
         "cycle-start register %s leaked into compute lowering", node->name.c_str());
  if (plan != nullptr) {
    auto sparse = plan->sparseRegisterByDestination_.find(node);
    if (sparse != plan->sparseRegisterByDestination_.end()) {
      const MtSparseRegister& reg = plan->sparseRegisters_[sparse->second];
      for (ExpTree* write : reg.writeTrees) {
        tree.mergeExpTree(write, predecessorPath, nodePath, node);
      }
      return;
    }
  }
  for (ExpTree* assignment : node->assignTree) {
    tree.mergeExpTree(assignment, predecessorPath, nodePath, node);
    if (node->type != NODE_REG_SRC || node->reset != ASYRESET ||
        !node->regSplit || node->getDst()->status != VALID_NODE) {
      continue;
    }
    ENode* root = assignment->getRoot();
    if (root->nodePtr == node->getDst() && root->getChildNum() == 0) continue;

    ENode* lvalue = assignment->getlval()->dup();
    lvalue->nodePtr = node->getDst();
    tree.mergeExpTree(new ExpTree(root, lvalue), predecessorPath, nodePath, nullptr);
  }
}

void predecessorPath(Node* node, const std::set<Node*>& scope,
                     const std::map<Node*, std::vector<int>>& allPaths,
                     std::vector<int>& path) {
  for (Node* prev : node->depPrev) {
    if (scope.count(prev) == 0) continue;
    auto found = allPaths.find(prev);
    Assert(found != allPaths.end(), "path of %s does not exist in its MTask",
           prev->name.c_str());
    const std::vector<int>& previous = found->second;
    for (size_t depth = 0; depth < previous.size(); ++depth) {
      if (depth >= path.size()) {
        path.push_back(previous[depth]);
      } else if (path[depth] <= previous[depth]) {
        path[depth] = previous[depth];
      } else {
        break;
      }
    }
  }
}

void buildTaskTree(MtTask& task, const MtTaskPlan& plan) {
  task.stmtTree = new StmtTree();
  task.stmtTree->root = new StmtNode(OP_STMT_SEQ);
  std::set<Node*> scope(task.members.begin(), task.members.end());
  std::map<Node*, std::vector<int>> allPaths;
  for (Node* node : task.members) {
    std::vector<int> prevPath;
    std::vector<int> nodePath;
    predecessorPath(node, scope, allPaths, prevPath);
    mergeNodeAssignments(*task.stmtTree, node, &plan, prevPath, nodePath);
    allPaths[node] = std::move(nodePath);
  }
  task.insts = new std::vector<InstInfo>();
  task.stmtTree->compute(*task.insts);
}

void buildResetTree(SuperNode* super) {
  super->stmtTree = new StmtTree();
  super->stmtTree->root = new StmtNode(OP_STMT_SEQ);
  for (Node* node : super->member) {
    std::vector<int> emptyPath;
    std::vector<int> nodePath;
    mergeNodeAssignments(*super->stmtTree, node, nullptr, emptyPath, nodePath);
  }
  super->insts.clear();
  super->stmtTree->compute(super->insts);
}

void updateTaskRoots(const MtTaskPlan& plan) {
  auto groupFor = [&](Node* node) {
    auto found = plan.partitionGroupByNode_.find(node);
    return found == plan.partitionGroupByNode_.end() ? -1 : found->second;
  };
  for (Node* node : plan.emissionNodes_) {
    node->nodeIsRoot = node->next.size() != 1 || node->isReset() || node->isExt();
    const int group = groupFor(node);
    for (Node* next : node->next) {
      if (groupFor(next) != group) node->nodeIsRoot = true;
    }
    for (Node* previous : node->prev) {
      if (previous->type == NODE_REG_SRC && !previous->regSplit &&
          groupFor(previous->getDst()) == group) {
        node->nodeIsRoot = true;
      }
    }
  }
}

}  // namespace

void MtTaskLowerer::generateStmtTrees(graph& graph, MtTaskPlan& plan) {
  maxConcatNum = 0;
  selectSparseRegisters(graph, plan);
  completeTaskLocalDependencies(graph, plan);
  makeMtMemoryWriteEnablesExplicit(plan);
  for (MtTask& task : plan.tasks_) orderTaskMembers(task);
  collectTaskLocalNodes(graph, plan);

  updateTaskRoots(plan);

  for (MtTask& task : plan.tasks_) {
    Assert(!task.members.empty(), "cannot lower an empty MTask");
    if (task.kind == MtTaskKind::ExtModule) {
      Node* ext = task.members.front();
      Assert(ext->type == NODE_EXT, "extmodule MTask must start with NODE_EXT");
      task.stmtTree = new StmtTree();
      task.stmtTree->root = new StmtNode(OP_STMT_SEQ);
      std::vector<InstInfo> instructions;
      graph.extDecl.push_back(computeExtMod(ext, instructions));
      task.insts = new std::vector<InstInfo>(std::move(instructions));
      continue;
    }
    buildTaskTree(task, plan);
  }

  // Reset SuperNodes are invoked outside normal MTask dispatch and retain the
  // same lowering used by the active implementation.
  for (SuperNode* super : graph.allReset) buildResetTree(super);
}

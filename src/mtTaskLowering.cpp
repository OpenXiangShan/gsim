#include "mtTaskLowering.h"

#include "common.h"

#include <algorithm>
#include <map>
#include <set>

extern int maxConcatNum;
std::string computeExtMod(SuperNode* super);

namespace {

int taskForNode(const Node* node, const MtTaskPlan& plan) {
  if (node == nullptr || node->super == nullptr || node->super->cppId < 0) return -1;
  const size_t cppId = static_cast<size_t>(node->super->cppId);
  return cppId < plan.taskByCppId_.size() ? plan.taskByCppId_[cppId] : -1;
}

void collectTaskMembers(MtTaskPlan& plan) {
  for (MtTask& task : plan.tasks_) {
    task.members.clear();
    for (int cppId : task.cppIds) {
      SuperNode* super = plan.byCppId_[static_cast<size_t>(cppId)];
      task.members.insert(task.members.end(), super->member.begin(), super->member.end());
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
  graph.connectDep();

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
      task.globalNodeCount += crossesTask && hasModelStorage(node);

      if (node->status != VALID_NODE || node->type != NODE_OTHERS ||
          node->isArray() || node->isReset() || resetDependencies.count(node) != 0) {
        continue;
      }

      bool taskLocal = true;
      for (Node* next : node->next) {
        if (taskForNode(next, plan) != static_cast<int>(taskId)) {
          taskLocal = false;
          break;
        }
      }
      if (!taskLocal) continue;

      task.localNodes.insert(node);
      plan.localNodes_.insert(node);
    }
  }
}

void mergeNodeAssignments(StmtTree& tree, Node* node,
                          std::vector<int>& predecessorPath,
                          std::vector<int>& nodePath) {
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

void buildTaskTree(MtTask& task) {
  task.stmtTree = new StmtTree();
  task.stmtTree->root = new StmtNode(OP_STMT_SEQ);
  std::set<Node*> scope(task.members.begin(), task.members.end());
  std::map<Node*, std::vector<int>> allPaths;
  for (Node* node : task.members) {
    std::vector<int> prevPath;
    std::vector<int> nodePath;
    predecessorPath(node, scope, allPaths, prevPath);
    mergeNodeAssignments(*task.stmtTree, node, prevPath, nodePath);
    allPaths[node] = std::move(nodePath);
  }
  task.insts = new std::vector<InstInfo>();
  task.stmtTree->compute(*task.insts);
  task.cost = std::max<int>(1, static_cast<int>(task.insts->size()) + task.globalNodeCount);
}

void buildResetTree(SuperNode* super) {
  super->stmtTree = new StmtTree();
  super->stmtTree->root = new StmtNode(OP_STMT_SEQ);
  for (Node* node : super->member) {
    std::vector<int> emptyPath;
    std::vector<int> nodePath;
    mergeNodeAssignments(*super->stmtTree, node, emptyPath, nodePath);
  }
  super->insts.clear();
  super->stmtTree->compute(super->insts);
}

}  // namespace

void MtTaskLowerer::generateStmtTrees(graph& graph, MtTaskPlan& plan) {
  maxConcatNum = 0;
  collectTaskMembers(plan);
  completeTaskLocalDependencies(graph, plan);
  for (MtTask& task : plan.tasks_) orderTaskMembers(task);
  collectTaskLocalNodes(graph, plan);

  for (SuperNode* super : graph.sortedSuper) {
    for (Node* node : super->member) node->updateIsRoot();
  }

  for (MtTask& task : plan.tasks_) {
    Assert(!task.cppIds.empty(), "cannot lower an empty MTask");
    SuperNode* first = plan.byCppId_[static_cast<size_t>(task.cppIds.front())];
    if (first->superType == SUPER_EXTMOD) {
      Assert(task.cppIds.size() == 1, "extmodule SuperNode must remain a standalone MTask");
      task.stmtTree = new StmtTree();
      task.stmtTree->root = new StmtNode(OP_STMT_SEQ);
      first->insts.clear();
      graph.extDecl.push_back(computeExtMod(first));
      task.insts = new std::vector<InstInfo>(first->insts);
      task.cost = std::max<int>(1, static_cast<int>(task.insts->size()) + task.globalNodeCount);
      continue;
    }
    if (first->superType == SUPER_ASYNC_RESET) {
      Assert(task.cppIds.size() == 1, "async reset SuperNode must remain a standalone MTask");
    }
    buildTaskTree(task);
  }

  // Reset SuperNodes are invoked outside normal MTask dispatch and retain the
  // same lowering used by the active implementation.
  for (SuperNode* super : graph.allReset) buildResetTree(super);
}

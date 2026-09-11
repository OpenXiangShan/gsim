#include "mtTaskPartition.h"

#include "common.h"

#include <algorithm>
#include <set>
#include <utility>

namespace {

int taskCost(const SuperNode* super) {
  return std::max<int>(1, static_cast<int>(super->insts.size() + super->member.size()));
}

}  // namespace

void MtTaskPartitioner::build(graph& graph, MtTaskPlan& plan, int maxTasks) {
  int taskCount = 0;
  for (SuperNode* super : graph.sortedSuper) {
    if (super->cppId >= 0) taskCount = std::max(taskCount, super->cppId + 1);
  }
  plan.byCppId_.assign(static_cast<size_t>(taskCount), nullptr);
  for (SuperNode* super : graph.sortedSuper) {
    if (super->cppId >= 0) plan.byCppId_[static_cast<size_t>(super->cppId)] = super;
  }
  for (int cppId = 0; cppId < taskCount; ++cppId) {
    Assert(plan.byCppId_[static_cast<size_t>(cppId)] != nullptr, "missing SuperNode for cppId %d", cppId);
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
    SuperNode* super = plan.byCppId_[static_cast<size_t>(cppId)];
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
    for (Node* node : plan.byCppId_[static_cast<size_t>(from)]->member) {
      for (int target : node->nextActiveId) {
        if (target >= 0 && target < taskCount &&
            dependencyRank[static_cast<size_t>(from)] < dependencyRank[static_cast<size_t>(target)]) {
          successors[static_cast<size_t>(from)].insert(target);
        }
      }
    }
  }
  plan.topologicalCppIds_ = dependencyOrder;
  for (int from = 0; from < taskCount; ++from) {
    for (int target : successors[static_cast<size_t>(from)]) {
      Assert(dependencyRank[static_cast<size_t>(from)] < dependencyRank[static_cast<size_t>(target)],
             "non-forward dense edge %d -> %d", from, target);
    }
  }

  long long totalCost = 0;
  for (int cppId : plan.topologicalCppIds_) totalCost += taskCost(plan.byCppId_[static_cast<size_t>(cppId)]);
  int targetCost = std::max<long long>(1, (totalCost + maxTasks - 1) / maxTasks);

  auto formTasks = [&](int costLimit) {
    std::vector<MtTask> result;
    MtTask current;
    auto flush = [&]() {
      if (!current.cppIds.empty()) result.push_back(std::move(current));
      current = MtTask();
    };
    for (int cppId : plan.topologicalCppIds_) {
      SuperNode* super = plan.byCppId_[static_cast<size_t>(cppId)];
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

  plan.tasks_ = formTasks(targetCost);
  if (static_cast<int>(plan.tasks_.size()) > maxTasks) {
    fprintf(stderr,
            "[cppEmitter-mt] target MAXMT=%d produced %zu dependency-safe tasks\n",
            maxTasks, plan.tasks_.size());
  }

  std::vector<int> taskByCppId(static_cast<size_t>(taskCount), -1);
  for (size_t taskId = 0; taskId < plan.tasks_.size(); ++taskId) {
    for (int cppId : plan.tasks_[taskId].cppIds) taskByCppId[static_cast<size_t>(cppId)] = taskId;
  }
  std::vector<std::set<int>> taskSuccessors(plan.tasks_.size());
  std::vector<std::set<int>> taskPredecessors(plan.tasks_.size());
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
  for (size_t taskId = 0; taskId < plan.tasks_.size(); ++taskId) {
    plan.tasks_[taskId].successors.assign(taskSuccessors[taskId].begin(), taskSuccessors[taskId].end());
    plan.tasks_[taskId].predecessors.assign(taskPredecessors[taskId].begin(), taskPredecessors[taskId].end());
  }


}

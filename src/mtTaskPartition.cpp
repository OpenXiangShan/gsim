#include "mtTaskPartition.h"

#include "common.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <utility>

namespace {

constexpr size_t kMaxNodesPerGroup = 7000;
constexpr size_t kMaxSiblingNodes = 30;

int expressionOperationCount(const ExpTree* tree) {
  int operations = 0;
  std::stack<const ENode*> pending;
  if (tree->getRoot() != nullptr) pending.push(tree->getRoot());
  if (tree->getlval() != nullptr) {
    for (ENode* child : tree->getlval()->child) {
      if (child != nullptr) pending.push(child);
    }
  }
  while (!pending.empty()) {
    const ENode* expression = pending.top();
    pending.pop();
    if (expression->nodePtr == nullptr && expression->opType != OP_EMPTY &&
        expression->opType != OP_INT && expression->opType != OP_INVALID) {
      ++operations;
    }
    for (ENode* child : expression->child) {
      if (child != nullptr) pending.push(child);
    }
  }
  return std::max(1, operations);
}

int nodeOperationCount(const Node* node) {
  int operations = 0;
  for (const ExpTree* tree : node->assignTree) operations += expressionOperationCount(tree);
  return std::max(1, operations) + globalConfig.MtPartitionNodeWeight;
}

int groupCost(const std::vector<Node*>& members) {
  int operations = 0;
  for (const Node* node : members) operations += nodeOperationCount(node);
  return std::max(1, operations);
}

MtTaskKind taskKind(const SuperNode* super) {
  if (super->superType == SUPER_EXTMOD) return MtTaskKind::ExtModule;
  return MtTaskKind::Normal;
}

// An MTask under construction. The extra fields exist only while the Node graph
// is being coarsened; Node::super is never mutated and no SuperNode is created.
struct Group : MtTask {
  std::set<int> prev;
  std::set<int> next;
  std::set<int> depPrev;
  std::set<int> depNext;
  uintptr_t orderKey = 0;
  int topologyKey = -1;
  int order = -1;
  bool alive = true;
};

class DirectMTaskCoarsener {
 public:
  explicit DirectMTaskCoarsener(graph& graph) : graph_(graph) {}

  void build(MtTaskPlan& plan, int targetTasks) {
    initialize();
    topologicalOrder();
    // mergeWhenGroups();
    mergeOut1Groups();
    mergeIn1Groups();
    mergeSiblingGroups();
    topologicalOrder();
    formFinalTasks(plan, targetTasks);
  }

 private:
  graph& graph_;
  std::vector<Group> groups_;
  std::unordered_map<Node*, int> groupByNode_;
  std::vector<int> order_;

  bool alive(int group) const {
    return group >= 0 && static_cast<size_t>(group) < groups_.size() &&
           groups_[static_cast<size_t>(group)].alive;
  }

  int groupFor(Node* node) const {
    auto found = groupByNode_.find(node);
    return found == groupByNode_.end() ? -1 : found->second;
  }

  void initialize() {
    groups_.reserve(graph_.sortedSuper.size());
    for (SuperNode* super : graph_.sortedSuper) {
      if (super->member.empty() && super->superType != SUPER_EXTMOD) {
        continue;
      }
      Group group;
      group.members = super->member;
      group.kind = taskKind(super);
      group.orderKey = reinterpret_cast<uintptr_t>(super);
      group.topologyKey = super->id;
      const int id = static_cast<int>(groups_.size());
      groups_.push_back(std::move(group));
      order_.push_back(id);
      for (Node* node : groups_.back().members) groupByNode_[node] = id;
    }
    rebuildEdges();
    fprintf(stderr, "[MtTaskPartitioner] initialize %zu MTask candidates from Node graph\n",
            groups_.size());
  }

  void rebuildEdges() {
    for (Group& group : groups_) {
      group.prev.clear();
      group.next.clear();
      group.depPrev.clear();
      group.depNext.clear();
    }
    for (size_t sourceId = 0; sourceId < groups_.size(); ++sourceId) {
      Group& source = groups_[sourceId];
      if (!source.alive) continue;
      auto addEdge = [&](Node* targetNode, bool direct) {
        const int targetId = groupFor(targetNode);
        if (!alive(targetId) || targetId == static_cast<int>(sourceId)) return;
        Group& target = groups_[static_cast<size_t>(targetId)];
        source.depNext.insert(targetId);
        target.depPrev.insert(static_cast<int>(sourceId));
        if (direct) {
          source.next.insert(targetId);
          target.prev.insert(static_cast<int>(sourceId));
        }
      };
      auto addIncomingEdge = [&](Node* sourceNode, bool direct) {
        const int predecessorId = groupFor(sourceNode);
        if (!alive(predecessorId) || predecessorId == static_cast<int>(sourceId)) return;
        Group& predecessor = groups_[static_cast<size_t>(predecessorId)];
        predecessor.depNext.insert(static_cast<int>(sourceId));
        source.depPrev.insert(predecessorId);
        if (direct) {
          predecessor.next.insert(static_cast<int>(sourceId));
          source.prev.insert(predecessorId);
        }
      };
      for (Node* node : source.members) {
        for (Node* predecessor : node->prev) addIncomingEdge(predecessor, true);
        for (Node* predecessor : node->depPrev) addIncomingEdge(predecessor, false);
        for (Node* target : node->next) addEdge(target, true);
        for (Node* target : node->depNext) addEdge(target, false);
      }
    }
  }

  std::vector<int> sortedByKey(const std::set<int>& values) const {
    std::vector<int> result(values.begin(), values.end());
    std::sort(result.begin(), result.end(), [&](int lhs, int rhs) {
      return groups_[static_cast<size_t>(lhs)].orderKey <
             groups_[static_cast<size_t>(rhs)].orderKey;
    });
    return result;
  }

  std::vector<int> sortedByTopologyKey(const std::set<int>& values) const {
    std::vector<int> result(values.begin(), values.end());
    std::sort(result.begin(), result.end(), [&](int lhs, int rhs) {
      return groups_[static_cast<size_t>(lhs)].topologyKey <
             groups_[static_cast<size_t>(rhs)].topologyKey;
    });
    return result;
  }

  void topologicalOrder() {
    rebuildEdges();
    std::vector<int> remaining(groups_.size(), 0);
    std::vector<int> ready;
    for (int id : order_) {
      if (!alive(id)) continue;
      remaining[static_cast<size_t>(id)] =
          static_cast<int>(groups_[static_cast<size_t>(id)].depPrev.size());
      if (remaining[static_cast<size_t>(id)] == 0) ready.push_back(id);
    }

    std::vector<int> result;
    while (!ready.empty()) {
      const int id = ready.back();
      ready.pop_back();
      result.push_back(id);
      for (int next : sortedByTopologyKey(groups_[static_cast<size_t>(id)].depNext)) {
        if (--remaining[static_cast<size_t>(next)] == 0) ready.push_back(next);
      }
    }
    size_t aliveCount = 0;
    for (const Group& group : groups_) aliveCount += group.alive;
    Assert(result.size() == aliveCount,
           "direct MTask coarsening graph has a cycle (%zu/%zu)", result.size(), aliveCount);
    order_.swap(result);

    int nodeOrder = 1;
    for (size_t position = 0; position < order_.size(); ++position) {
      Group& group = groups_[static_cast<size_t>(order_[position])];
      group.order = static_cast<int>(position);
      for (size_t member = 0; member < group.members.size(); ++member) {
        group.members[member]->order = nodeOrder++;
        group.members[member]->orderInSuper = static_cast<int>(member);
      }
    }
  }

  void moveMembers(int sourceId, int targetId, bool prepend) {
    Assert(alive(sourceId) && alive(targetId) && sourceId != targetId,
           "invalid direct MTask merge %d -> %d", sourceId, targetId);
    Group& source = groups_[static_cast<size_t>(sourceId)];
    Group& target = groups_[static_cast<size_t>(targetId)];
    Assert(source.kind == MtTaskKind::Normal && target.kind == MtTaskKind::Normal,
           "cannot merge special direct MTask groups");
    if (prepend) {
      target.members.insert(target.members.begin(), source.members.begin(), source.members.end());
    } else {
      target.members.insert(target.members.end(), source.members.begin(), source.members.end());
    }
    for (Node* node : source.members) groupByNode_[node] = targetId;
    source.members.clear();
    source.alive = false;
  }

  void mergeOutInto(int sourceId, int targetId) {
    Group& source = groups_[static_cast<size_t>(sourceId)];
    const std::set<int> sourcePrev = source.prev;
    const std::set<int> sourceNext = source.next;
    const std::set<int> sourceDepPrev = source.depPrev;
    const std::set<int> sourceDepNext = source.depNext;
    moveMembers(sourceId, targetId, true);
    Group& target = groups_[static_cast<size_t>(targetId)];

    target.prev.erase(sourceId);
    target.depPrev.erase(sourceId);
    for (int predecessorId : sourcePrev) {
      target.prev.insert(predecessorId);
      target.depPrev.insert(predecessorId);
    }
    for (int predecessorId : sourceDepPrev) {
      Group& predecessor = groups_[static_cast<size_t>(predecessorId)];
      if (sourcePrev.count(predecessorId) != 0) {
        predecessor.next.erase(sourceId);
        predecessor.depNext.erase(sourceId);
        predecessor.next.insert(targetId);
        predecessor.depNext.insert(targetId);
        target.prev.insert(predecessorId);
        target.depPrev.insert(predecessorId);
      } else {
        predecessor.depNext.erase(sourceId);
        predecessor.depNext.insert(targetId);
        target.depPrev.insert(predecessorId);
      }
    }
    for (int successorId : sourceDepNext) {
      if (sourceNext.count(successorId) != 0) continue;
      Group& successor = groups_[static_cast<size_t>(successorId)];
      successor.depPrev.erase(sourceId);
      successor.depPrev.insert(targetId);
      target.depNext.insert(successorId);
    }
    source.prev.clear();
    source.next.clear();
    source.depPrev.clear();
    source.depNext.clear();
  }

  void mergeInInto(int sourceId, int targetId) {
    Group& source = groups_[static_cast<size_t>(sourceId)];
    const std::set<int> sourcePrev = source.prev;
    const std::set<int> sourceNext = source.next;
    const std::set<int> sourceDepPrev = source.depPrev;
    const std::set<int> sourceDepNext = source.depNext;
    moveMembers(sourceId, targetId, false);
    Group& target = groups_[static_cast<size_t>(targetId)];

    target.next.erase(sourceId);
    target.depNext.erase(sourceId);
    for (int successorId : sourceNext) {
      target.next.insert(successorId);
      target.depNext.insert(successorId);
    }
    for (int successorId : sourceDepNext) {
      Group& successor = groups_[static_cast<size_t>(successorId)];
      if (sourceNext.count(successorId) != 0) {
        successor.prev.erase(sourceId);
        successor.depPrev.erase(sourceId);
        successor.prev.insert(targetId);
        successor.depPrev.insert(targetId);
        target.next.insert(successorId);
        target.depNext.insert(successorId);
      } else {
        successor.depPrev.erase(sourceId);
        successor.depPrev.insert(targetId);
        target.depNext.insert(successorId);
      }
    }
    for (int predecessorId : sourceDepPrev) {
      if (sourcePrev.count(predecessorId) != 0) continue;
      Group& predecessor = groups_[static_cast<size_t>(predecessorId)];
      predecessor.depNext.erase(sourceId);
      predecessor.depNext.insert(targetId);
      target.depPrev.insert(predecessorId);
    }
    source.prev.clear();
    source.next.clear();
    source.depPrev.clear();
    source.depNext.clear();
  }

  void convertSmallWhensToMux() {
    for (int id : order_) {
      if (!alive(id) || groups_[static_cast<size_t>(id)].kind != MtTaskKind::Normal) continue;
      std::map<Node*, int> conditionCount;
      std::stack<ENode*> pending;
      std::stack<ENode*> whens;
      for (Node* member : groups_[static_cast<size_t>(id)].members) {
        for (ExpTree* tree : member->assignTree) {
          if (tree->getRoot()->opType == OP_WHEN) pending.push(tree->getRoot());
        }
      }
      while (!pending.empty()) {
        ENode* expression = pending.top();
        pending.pop();
        if (expression->opType != OP_WHEN) continue;
        ++conditionCount[expression->getChild(0)->getNode()];
        whens.push(expression);
        for (ENode* child : expression->child) {
          if (child != nullptr) pending.push(child);
        }
      }
      while (!whens.empty()) {
        ENode* expression = whens.top();
        whens.pop();
        if (conditionCount[expression->getChild(0)->getNode()] > globalConfig.When2muxBound) {
          continue;
        }
        if (expression->getChild(1) != nullptr &&
            expression->getChild(1)->opType != OP_WHEN &&
            expression->getChild(1)->opType != OP_INVALID &&
            expression->getChild(2) != nullptr &&
            expression->getChild(2)->opType != OP_WHEN &&
            expression->getChild(2)->opType != OP_INVALID) {
          expression->opType = OP_MUX;
        }
      }
    }
  }

  void mergeWhenGroups() {
    std::map<int, std::set<int>> allConditions;
    std::map<int, int> conditionForGroup;
    std::vector<int> times(groups_.size(), 0);
    for (int id : order_) {
      const Group& group = groups_[static_cast<size_t>(id)];
      if (group.kind != MtTaskKind::Normal) continue;
      Assert(group.members.size() <= 1, "invalid initial direct MTask group size %zu",
             group.members.size());
      for (Node* member : group.members) {
        if (member->isArray() || member->assignTree.size() != 1 ||
            member->assignTree[0]->getRoot()->opType != OP_WHEN) {
          continue;
        }
        Node* condition = member->assignTree[0]->getRoot()->getChild(0)->getNode();
        const int conditionGroup = groupFor(condition);
        if (conditionGroup < 0) continue;
        allConditions[conditionGroup].insert(id);
        conditionForGroup[id] = conditionGroup;
      }
    }

    std::queue<int> normal;
    std::queue<int> conditions;
    std::set<int> waitingConditions;
    auto addCondition = [&](int id) {
      int nearlyReady = 0;
      for (int dependent : allConditions[id]) {
        if (times[static_cast<size_t>(dependent)] + 1 ==
            static_cast<int>(groups_[static_cast<size_t>(dependent)].depPrev.size())) {
          ++nearlyReady;
        }
      }
      if (nearlyReady >= 2) conditions.push(id);
      else waitingConditions.insert(id);
    };
    auto releaseCondition = [&](int id) {
      auto found = waitingConditions.find(id);
      if (found == waitingConditions.end()) return;
      waitingConditions.erase(found);
      conditions.push(id);
    };

    for (int id : order_) {
      if (!groups_[static_cast<size_t>(id)].depPrev.empty()) continue;
      if (allConditions.count(id) != 0) addCondition(id);
      else normal.push(id);
    }

    std::map<int, std::vector<int>> mergeSets;
    while (!normal.empty()) {
      const int id = normal.front();
      normal.pop();
      for (int next : sortedByKey(groups_[static_cast<size_t>(id)].depNext)) {
        ++times[static_cast<size_t>(next)];
        if (times[static_cast<size_t>(next)] + 1 ==
            static_cast<int>(groups_[static_cast<size_t>(next)].depPrev.size())) {
          auto condition = conditionForGroup.find(next);
          if (condition != conditionForGroup.end()) releaseCondition(condition->second);
        }
        if (times[static_cast<size_t>(next)] ==
            static_cast<int>(groups_[static_cast<size_t>(next)].depPrev.size())) {
          if (allConditions.count(next) != 0) addCondition(next);
          else normal.push(next);
        }
      }
      while (normal.empty() && (!conditions.empty() || !waitingConditions.empty())) {
        if (conditions.empty()) {
          auto first = std::min_element(waitingConditions.begin(), waitingConditions.end(),
                                        [&](int lhs, int rhs) {
            return groups_[static_cast<size_t>(lhs)].orderKey <
                   groups_[static_cast<size_t>(rhs)].orderKey;
          });
          releaseCondition(*first);
        }
        const int condition = conditions.front();
        conditions.pop();
        std::vector<int> merge;
        for (int next : sortedByKey(groups_[static_cast<size_t>(condition)].depNext)) {
          ++times[static_cast<size_t>(next)];
          if (times[static_cast<size_t>(next)] ==
              static_cast<int>(groups_[static_cast<size_t>(next)].depPrev.size())) {
            if (allConditions[condition].count(next) != 0) merge.push_back(next);
            normal.push(next);
          }
        }
        if (merge.size() > static_cast<size_t>(globalConfig.MergeWhenSize)) {
          mergeSets[condition] = std::move(merge);
        }
      }
    }

    for (const auto& entry : mergeSets) {
      const std::vector<int>& merge = entry.second;
      if (merge.empty() || !alive(merge.front())) continue;
      const int target = merge.front();
      for (size_t i = 1; i < merge.size(); ++i) {
        if (alive(merge[i])) moveMembers(merge[i], target, false);
      }
    }
    order_.erase(std::remove_if(order_.begin(), order_.end(),
                                [&](int id) { return !alive(id); }),
                 order_.end());
    topologicalOrder();
    convertSmallWhensToMux();
  }

  void mergeOut1Groups() {
    for (auto position = order_.rbegin(); position != order_.rend(); ++position) {
      const int id = *position;
      if (!alive(id)) continue;
      Group& group = groups_[static_cast<size_t>(id)];
      if (group.kind != MtTaskKind::Normal || group.next.size() != 1) continue;
      const int targetId = *group.next.begin();
      Group& target = groups_[static_cast<size_t>(targetId)];
      if (!target.alive || target.kind != MtTaskKind::Normal ||
          target.members.size() > kMaxNodesPerGroup) {
        continue;
      }
      bool canMerge = true;
      for (int dependent : group.depNext) {
        if (groups_[static_cast<size_t>(dependent)].order < target.order) canMerge = false;
      }
      if (!canMerge) continue;
      mergeOutInto(id, targetId);
    }
    order_.erase(std::remove_if(order_.begin(), order_.end(),
                                [&](int id) { return !alive(id); }),
                 order_.end());
  }

  void mergeIn1Groups() {
    for (int id : order_) {
      if (!alive(id)) continue;
      Group& group = groups_[static_cast<size_t>(id)];
      if (group.kind != MtTaskKind::Normal || group.prev.size() != 1) continue;
      const int targetId = *group.prev.begin();
      Group& target = groups_[static_cast<size_t>(targetId)];
      if (!target.alive || target.kind != MtTaskKind::Normal ||
          target.members.size() > kMaxNodesPerGroup) {
        continue;
      }
      bool canMerge = true;
      for (int dependency : group.depPrev) {
        if (groups_[static_cast<size_t>(dependency)].order > target.order) canMerge = false;
      }
      if (!canMerge) continue;
      mergeInInto(id, targetId);
    }
    order_.erase(std::remove_if(order_.begin(), order_.end(),
                                [&](int id) { return !alive(id); }),
                 order_.end());
  }

  void mergeSiblingGroups() {
    using Key = std::pair<std::vector<int>, std::vector<int>>;
    std::map<Key, int> representative;
    for (int id : order_) {
      if (!alive(id)) continue;
      Group& group = groups_[static_cast<size_t>(id)];
      if (group.kind != MtTaskKind::Normal || group.prev.empty()) continue;
      Key key{{group.prev.begin(), group.prev.end()}, {group.depPrev.begin(), group.depPrev.end()}};
      auto found = representative.find(key);
      if (found == representative.end()) {
        representative.emplace(std::move(key), id);
        continue;
      }
      const int targetId = found->second;
      if (groups_[static_cast<size_t>(targetId)].members.size() < kMaxSiblingNodes) {
        moveMembers(id, targetId, false);
      } else {
        found->second = id;
      }
    }
    order_.erase(std::remove_if(order_.begin(), order_.end(),
                                [&](int id) { return !alive(id); }),
                 order_.end());
    rebuildEdges();
  }

  std::vector<int> dependencyOrder() const {
    std::vector<int> cppId(groups_.size(), -1);
    for (size_t i = 0; i < order_.size(); ++i) cppId[static_cast<size_t>(order_[i])] = i;
    std::vector<int> remaining(groups_.size(), 0);
    for (int id : order_) {
      for (int successor : groups_[static_cast<size_t>(id)].depNext) {
        ++remaining[static_cast<size_t>(successor)];
      }
    }
    std::vector<int> ready;
    for (int id : order_) {
      if (remaining[static_cast<size_t>(id)] == 0) ready.push_back(id);
    }
    std::vector<int> result;
    result.reserve(order_.size());
    for (size_t head = 0; head < ready.size(); ++head) {
      const int id = ready[head];
      result.push_back(id);
      std::vector<int> successors(groups_[static_cast<size_t>(id)].depNext.begin(),
                                  groups_[static_cast<size_t>(id)].depNext.end());
      std::sort(successors.begin(), successors.end(), [&](int lhs, int rhs) {
        return cppId[static_cast<size_t>(lhs)] < cppId[static_cast<size_t>(rhs)];
      });
      for (int successor : successors) {
        if (--remaining[static_cast<size_t>(successor)] == 0) ready.push_back(successor);
      }
    }
    Assert(result.size() == order_.size(),
           "direct MTask dependency graph has a cycle (%zu/%zu)", result.size(), order_.size());
    return result;
  }

  void addActivationEdges(const std::vector<int>& dependencyOrder,
                          std::vector<std::set<int>>& successors) const {
    std::vector<int> rank(groups_.size(), -1);
    for (size_t i = 0; i < dependencyOrder.size(); ++i) {
      rank[static_cast<size_t>(dependencyOrder[i])] = i;
    }
    auto add = [&](int source, Node* target) {
      const int destination = groupFor(target);
      if (!alive(destination) || destination == source) return;
      if (rank[static_cast<size_t>(source)] < rank[static_cast<size_t>(destination)]) {
        successors[static_cast<size_t>(source)].insert(destination);
      }
    };
    for (int source : order_) {
      for (Node* node : groups_[static_cast<size_t>(source)].members) {
        if (node->status != VALID_NODE) continue;
        for (Node* next : node->next) add(source, next);
        if (node->type == NODE_WRITER) {
          for (Node* port : node->parent->member) {
            if (port->type == NODE_READER && port->status == VALID_NODE) add(source, port);
          }
        }
        if (node->type == NODE_READWRITER) {
          for (Node* port : node->parent->member) {
            if (port == node) {
              if (port->parent->extraInfo != "new") add(source, node);
            } else if ((port->type == NODE_READER || port->type == NODE_READWRITER) &&
                       port->status == VALID_NODE) {
              add(source, port);
            }
          }
        }
      }
    }
  }

  void formFinalTasks(MtTaskPlan& plan, int targetTasks) {
    plan.partitionGroupCount_ = static_cast<int>(order_.size());
    plan.partitionGroupByNode_.clear();
    plan.emissionNodes_.clear();
    for (int groupId : order_) {
      for (Node* node : groups_[static_cast<size_t>(groupId)].members) {
        plan.partitionGroupByNode_[node] = groupId;
        plan.emissionNodes_.push_back(node);
      }
    }

    std::vector<std::set<int>> groupSuccessors(groups_.size());
    for (int groupId : order_) {
      groupSuccessors[static_cast<size_t>(groupId)] =
          groups_[static_cast<size_t>(groupId)].depNext;
    }
    const std::vector<int> taskOrder = dependencyOrder();
    addActivationEdges(taskOrder, groupSuccessors);

    long long totalCost = 0;
    for (int groupId : taskOrder) {
      totalCost += groupCost(groups_[static_cast<size_t>(groupId)].members);
    }
    const int targetCost =
        std::max<long long>(1, (totalCost + targetTasks - 1) / targetTasks);

    plan.tasks_.clear();
    std::vector<int> taskByGroup(groups_.size(), -1);
    MtTask current;
    auto flush = [&]() {
      if (current.members.empty()) return;
      const int taskId = static_cast<int>(plan.tasks_.size());
      for (Node* node : current.members) taskByGroup[static_cast<size_t>(groupFor(node))] = taskId;
      plan.tasks_.push_back(std::move(current));
      current = MtTask();
    };

    for (int groupId : taskOrder) {
      const Group& group = groups_[static_cast<size_t>(groupId)];
      const int cost = groupCost(group.members);
      if (group.kind != MtTaskKind::Normal) {
        flush();
        current.members = group.members;
        current.kind = group.kind;
        current.cost = cost;
        flush();
        continue;
      }
      if (!current.members.empty() && current.cost + cost > targetCost) flush();
      current.members.insert(current.members.end(), group.members.begin(), group.members.end());
      current.cost += cost;
    }
    flush();

    if (static_cast<int>(plan.tasks_.size()) > targetTasks) {
      fprintf(stderr,
              "[cppEmitter-mt] target MTask count=%d produced %zu dependency-safe tasks\n",
              targetTasks, plan.tasks_.size());
    }

    std::vector<std::set<int>> taskSuccessors(plan.tasks_.size());
    std::vector<std::set<int>> taskPredecessors(plan.tasks_.size());
    for (int sourceGroup : taskOrder) {
      const int sourceTask = taskByGroup[static_cast<size_t>(sourceGroup)];
      for (int targetGroup : groupSuccessors[static_cast<size_t>(sourceGroup)]) {
        const int targetTask = taskByGroup[static_cast<size_t>(targetGroup)];
        if (sourceTask == targetTask) continue;
        Assert(sourceTask < targetTask, "non-forward direct MTask edge %d -> %d",
               sourceTask, targetTask);
        taskSuccessors[static_cast<size_t>(sourceTask)].insert(targetTask);
        taskPredecessors[static_cast<size_t>(targetTask)].insert(sourceTask);
      }
    }

    plan.taskByNode_.clear();
    for (size_t taskId = 0; taskId < plan.tasks_.size(); ++taskId) {
      MtTask& task = plan.tasks_[taskId];
      task.successors.assign(taskSuccessors[taskId].begin(), taskSuccessors[taskId].end());
      task.predecessors.assign(taskPredecessors[taskId].begin(), taskPredecessors[taskId].end());
      for (Node* node : task.members) plan.taskByNode_[node] = static_cast<int>(taskId);
    }

    fprintf(stderr, "[MtTaskPartitioner] coarsen MTask candidates %zu -> %zu\n",
            groups_.size(), order_.size());
  }
};

}  // namespace

void MtTaskPartitioner::build(graph& graph, MtTaskPlan& plan, int targetTasks) {
  graph.orderAllNodes();
  graph.mergeResetAll();
  useDirectDependencies(graph);
  DirectMTaskCoarsener(graph).build(plan, targetTasks);
}

void MtTaskPartitioner::useDirectDependencies(graph& graph) {
  for (SuperNode* super : graph.sortedSuper) {
    for (Node* node : super->member) {
      node->depPrev = node->prev;
      node->depNext = node->next;
    }
  }
}

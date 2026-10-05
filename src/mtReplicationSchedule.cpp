#include "mtReplicationSchedule.h"
#include "common.h"
#include "mtCostModel.h"
#include "mtTaskSchedule.h"

#include <algorithm>
#include <cmath>
#include <tuple>
#include <unordered_set>

namespace {
bool stable(Node* n) {
  return n->type == NODE_INP || n->status == CONSTANT_NODE || mtIsCycleStartRegisterUpdate(n);
}


struct ExpressionInputs {
  std::vector<Node*> nodes;
  int operations = 0;
  std::string rejection;
};

// This validates only the node being copied. Its inputs are classified by the
// cone traversal, not required to already be local as in single-node copying.
ExpressionInputs expressionInputs(Node* node, int budget) {
  ExpressionInputs result;
  if (node->type != NODE_OTHERS || node->isClock || node->isReset() ||
      node->assignTree.size() != 1) {
    result.rejection = "node-kind-or-assignment";
    return result;
  }
  ExpTree* tree = node->assignTree.front();
  if (!tree->getlval() || tree->getlval()->getNode() != node ||
      tree->getlval()->getChildNum() != 0) {
    result.rejection = "non-scalar-assignment";
    return result;
  }
  std::unordered_set<Node*> inputs;
  std::vector<const ENode*> pending{tree->getRoot()};
  while (!pending.empty()) {
    const ENode* expression = pending.back();
    pending.pop_back();
    if (!expression || expression->width > BASIC_WIDTH) {
      result.rejection = "expression-width-or-null";
      return result;
    }
    if (expression->nodePtr) {
      inputs.insert(expression->nodePtr);
    } else if (expression->opType != OP_INT) {
      switch (expression->opType) {
        case OP_ADD: case OP_SUB: case OP_AND: case OP_OR: case OP_XOR:
        case OP_NOT: case OP_MUX: case OP_EQ: case OP_NEQ:
        case OP_LT: case OP_LEQ: case OP_GT: case OP_GEQ:
        case OP_BITS: case OP_HEAD: case OP_TAIL: case OP_PAD:
        case OP_ASUINT: case OP_ASSINT: case OP_CAT:
          ++result.operations;
          break;
        default:
          result.rejection = "unsupported-operation";
          return result;
      }
      if (result.operations > budget) {
        result.rejection = "cone-budget";
        return result;
      }
    }
    for (const ENode* child : expression->child) pending.push_back(child);
  }
  // Never silently remove ordering-only dependencies of an interior node.
  for (Node* dependency : node->depPrev) {
    if (inputs.count(dependency) == 0) {
      result.rejection = "interior-ordering-dependency";
      return result;
    }
  }
  result.nodes.assign(inputs.begin(), inputs.end());
  std::sort(result.nodes.begin(), result.nodes.end(), [](Node* lhs, Node* rhs) {
    return lhs->id < rhs->id;
  });
  result.operations = std::max(1, result.operations);
  return result;
}

}  // namespace


MtReplicationCone mtBuildAvailableCone(
    const std::vector<Node*>& roots, int maxOps,
    const std::function<bool(Node*)>& available) {
  MtReplicationCone result;
  struct Frame { Node* node; std::vector<Node*> inputs; size_t next = 0; };
  std::vector<Frame> stack;
  std::unordered_map<Node*, bool> visited;  // false while on the DFS stack
  auto enter = [&](Node* node) {
    if (!node || (node->status != VALID_NODE && node->status != CONSTANT_NODE)) {
      result.rejection = "node-status";
      return;
    }
    if (stable(node)) {
      result.sourceInputs.push_back(node);
      visited[node] = true;
      return;
    }
    // Boundary values are not copied: arrays and expensive operations may be
    // reused when available, but their reads and tokens remain in the plan.
    if (available(node)) {
      result.boundaryInputs.push_back(node);
      visited[node] = true;
      return;
    }
    if (node->isArray() || node->width < 1 || node->width > BASIC_WIDTH) {
      result.rejection = "node-status-array-or-width";
      return;
    }
    ExpressionInputs inputs = expressionInputs(node, maxOps - result.operations);
    if (!inputs.rejection.empty()) { result.rejection = inputs.rejection; return; }
    if (inputs.operations > maxOps - result.operations) {
      result.rejection = "cone-budget";
      return;
    }
    result.operations += inputs.operations;
    visited[node] = false;
    stack.push_back({node, std::move(inputs.nodes), 0});
  };
  for (Node* root : roots) {
    if (visited.count(root) == 0) enter(root);
    while (result.valid() && !stack.empty()) {
      Frame& frame = stack.back();
      if (frame.next == frame.inputs.size()) {
        visited[frame.node] = true;
        result.nodes.push_back(frame.node);
        stack.pop_back();
      } else {
        Node* input = frame.inputs[frame.next++];
        auto found = visited.find(input);
        if (found == visited.end()) enter(input);
        else if (!found->second) result.rejection = "cone-cycle";
      }
    }
    if (!result.valid()) return result;
  }
  return result;
}

static void mtReplaceConeReferences(ExpTree* tree,
                             const std::unordered_map<Node*, Node*>& replacements) {
  std::vector<ENode*> pending{tree->getRoot(), tree->getlval()};
  while (!pending.empty()) {
    ENode* expression = pending.back(); pending.pop_back();
    if (!expression) continue;
    auto replacement = replacements.find(expression->nodePtr);
    if (replacement != replacements.end()) expression->nodePtr = replacement->second;
    expression->computeInfo = nullptr;
    for (ENode* child : expression->child) pending.push_back(child);
  }
}

namespace {
using Inputs = std::map<int, std::set<Node*>>;
using Replacements = std::unordered_map<Node*, Node*>;
using Timeline = std::map<long long, MtScheduledInterval>;

long long gap(const Timeline& timeline, long long ready, long long duration,
              long long extraStart = -1, long long extraFinish = -1) {
  long long start = ready;
  bool extra = extraStart >= 0;
  auto skip = [&](long long begin, long long end) {
    if (start + duration <= begin) return true;
    start = std::max(start, end);
    return false;
  };
  for (const auto& entry : timeline) {
    if (extra && extraStart <= entry.second.start) {
      if (skip(extraStart, extraFinish)) return start;
      extra = false;
    }
    if (skip(entry.second.start, entry.second.finish)) return start;
  }
  if (extra) skip(extraStart, extraFinish);
  return start;
}

struct Ready { long long time = 0, checks = 0; };
Ready readiness(const Inputs& inputs, int worker, const std::vector<int>& owners,
                const std::vector<long long>& completion) {
  Ready result;
  struct Remote { long long finish = 0; std::set<Node*> signals; };
  std::map<int, Remote> remote;
  for (const auto& edge : inputs) {
    Assert(owners.at(edge.first) >= 0, "HEFT replica has unplaced boundary %d", edge.first);
    result.time = std::max(result.time, completion[edge.first]);
    const int owner = owners[edge.first];
    if (owner == worker) continue;
    auto& source = remote[owner];
    source.finish = std::max(source.finish, completion[edge.first]);
    source.signals.insert(edge.second.begin(), edge.second.end());
  }
  for (const auto& entry : remote) {
    result.time = std::max(result.time, entry.second.finish + mtcost::tokenLatency +
        static_cast<long long>(entry.second.signals.size()) * globalConfig.MtScheduleCommNodeWeight);
  }
  result.checks = remote.size() * mtcost::remoteCheck;
  return result;
}

// Replace full C++ identifiers, including '$', without touching strings or comments.
std::string redirectText(const std::string& text,
                        const std::unordered_map<std::string, std::string>& names) {
  std::string result;
  result.reserve(text.size());
  for (size_t i = 0; i < text.size();) {
    const size_t begin = i;
    if (text[i] == '"' || text[i] == '\'') {
      const char quote = text[i++];
      while (i < text.size()) {
        if (text[i] == '\\') { i = std::min(i + 2, text.size()); continue; }
        if (text[i++] == quote) break;
      }
    } else if (text.compare(i, 2, "//") == 0) {
      i = text.find('\n', i);
      if (i == std::string::npos) i = text.size();
    } else if (text.compare(i, 2, "/*") == 0) {
      i = text.find("*/", i + 2);
      i = i == std::string::npos ? text.size() : i + 2;
    } else if (mtcost::identifier(text[i])) {
      while (i < text.size() && mtcost::identifier(text[i])) ++i;
      auto found = names.find(text.substr(begin, i - begin));
      if (found != names.end()) { result += found->second; continue; }
    } else ++i;
    result.append(text, begin, i - begin);
  }
  return result;
}

void redirectInstructions(std::vector<InstInfo>& insts, const Replacements& replacements) {
  std::unordered_map<std::string, std::string> names;
  for (const auto& e : replacements) names.emplace(e.first->name, e.second->name);
  for (auto& inst : insts) {
    if (inst.infoType == SUPER_INFO_ASSIGN_BEG || inst.infoType == SUPER_INFO_ASSIGN_END) continue;
    inst.inst = redirectText(inst.inst, names);
  }
}

std::vector<int> sources(const Inputs& inputs) {
  std::vector<int> result;
  for (const auto& e : inputs) result.push_back(e.first);
  return result;
}
}  // namespace

struct MtHeftReplication::Impl {
  graph& g;
  MtTaskPlan& tasks;
  int workers;
  size_t originalCount;
  std::vector<Inputs> data;
  std::vector<std::set<int>> ordering;
  std::vector<Replacements> replicas, rewrites;
  std::unordered_map<Node*, std::vector<InstInfo>> loweredCopies;
  struct Decision { MtHeftPlacement placement; int replica; };
  std::vector<Decision> decisions;
  std::map<std::string, size_t> rejected;
  size_t trials = 0, copiedNodes = 0, copiedOps = 0, reusedInputs = 0, removedEdges = 0;

  Impl(graph& graph, MtTaskPlan& plan, int count)
      : g(graph), tasks(plan), workers(count), originalCount(plan.tasks_.size()),
        data(originalCount), ordering(originalCount), replicas(count), rewrites(originalCount) {
    std::vector<std::set<Node*>> writes(originalCount);
    for (size_t i = 0; i < originalCount; ++i) {
      Assert(tasks.tasks_[i].insts != nullptr, "replication requires lowered tasks");
      for (Node* n : tasks.tasks_[i].members) {
        if (n->type == NODE_WRITER || n->type == NODE_READWRITER) writes[i].insert(n->parent);
      }
    }
    for (size_t i = 0; i < originalCount; ++i) {
      std::set<int> extra;
      std::set<Node*> reads;
      for (Node* n : tasks.tasks_[i].members) {
        if (n->type == NODE_READER || n->type == NODE_READWRITER) reads.insert(n->parent);
        for (Node* prev : n->prev) {
          auto source = tasks.taskByNode_.find(prev);
          if (stable(prev) || source == tasks.taskByNode_.end() || source->second == static_cast<int>(i)) continue;
          data[i][source->second].insert(prev);
        }
        for (Node* prev : n->depPrev) {
          if (n->prev.count(prev)) continue;
          auto source = tasks.taskByNode_.find(prev);
          if (source != tasks.taskByNode_.end() && source->second != static_cast<int>(i)) extra.insert(source->second);
        }
      }
      for (int source : tasks.tasks_[i].predecessors) {
        bool ordered = !data[i].count(source) || extra.count(source);
        for (Node* memory : writes[source]) ordered |= reads.count(memory) != 0;
        if (ordered) ordering[i].insert(source);
      }
    }
  }

  const std::vector<InstInfo>& lower(Node* original) {
    auto found = loweredCopies.find(original);
    if (found != loweredCopies.end()) return found->second;
    // Safe scalar cones only. Fresh expression caches, same real emitter.
    StmtTree tree;
    tree.root = new StmtNode(OP_STMT_SEQ);
    tree.addSeq(original->assignTree.front(), original);
    std::vector<InstInfo> insts;
    tree.compute(insts);
    return loweredCopies.emplace(original, std::move(insts)).first->second;
  }

  // The caller owns the instruction storage: stack-backed for evaluation,
  // transferred to the task only when the candidate is committed.
  MtTask lowerCone(const MtReplicationCone& cone, std::vector<InstInfo>& instructions) {
    for (Node* node : cone.nodes) {
      for (const InstInfo& instruction : lower(node)) {
        if (instruction.infoType != SUPER_INFO_ASSIGN_BEG &&
            instruction.infoType != SUPER_INFO_ASSIGN_END) {
          instructions.push_back(instruction);
        }
      }
    }
    MtTask block;
    block.insts = &instructions;
    // Export the copies for later same-worker reuse. Cost includes dispatch
    // overhead and global writes, exactly like any other lowered task.
    block.globalNodeCount = cone.nodes.size();
    block.cost = mtLoweredTaskCost(block);
    return block;
  }

  static bool better(const MtHeftPlacement& a, const MtHeftPlacement& b) {
    return std::tie(a.finish, a.extraWork, a.start, a.cone.operations) <
           std::tie(b.finish, b.extraWork, b.start, b.cone.operations);
  }

  MtHeftPlacement estimate(const MtHeftPlacement& native, MtReplicationCone cone,
      const Replacements& reuse, const std::vector<int>& owners,
      const std::vector<long long>& completion, const Timeline& timeline) {
    MtHeftPlacement p = native;
    p.cone = std::move(cone);
    p.redirects = reuse;
    const std::set<Node*> copied(p.cone.nodes.begin(), p.cone.nodes.end());
    Inputs incoming, boundaries;
    for (int source : ordering[native.task]) incoming[source];
    auto add = [&](Inputs& inputs, Node* n) {
      if (stable(n)) return;
      auto replacement = reuse.find(n);
      if (replacement != reuse.end()) n = replacement->second;
      auto source = tasks.taskByNode_.find(n);
      Assert(source != tasks.taskByNode_.end(), "unmapped lowered replication input %s", n->name.c_str());
      inputs[source->second].insert(n);
    };
    for (const auto& edge : data[native.task]) for (Node* input : edge.second) {
      if (!copied.count(input)) add(incoming, input);
    }
    for (Node* input : p.cone.boundaryInputs) add(boundaries, input);
    p.dependencies = sources(incoming);
    p.copyDependencies = sources(boundaries);
    const Ready ready = readiness(incoming, native.worker, owners, completion);
    long long consumerReady = ready.time;
    if (!p.cone.nodes.empty()) {
      std::vector<InstInfo> instructions;
      p.copyCost = lowerCone(p.cone, instructions).cost;
      const Ready boundary = readiness(boundaries, native.worker, owners, completion);
      p.extraWork = p.copyCost + boundary.checks;
      p.copyStart = gap(timeline, boundary.time, p.extraWork);
      p.copyFinish = p.copyStart + p.extraWork;
      consumerReady = std::max(consumerReady, p.copyFinish);
    }
    const auto& task = tasks.tasks_[native.task];
    const double groups = (workers - 1) * (1.0 - std::pow(1.0 - 1.0 / workers, task.successors.size()));
    const long long duration = task.cost + ready.checks + static_cast<long long>(std::ceil(groups * mtcost::remotePublish));
    p.start = gap(timeline, consumerReady, duration, p.copyStart, p.copyFinish);
    p.finish = p.start + duration;
    return p;
  }

  MtHeftPlacement evaluate(MtHeftPlacement native, const std::vector<int>& owners,
      const std::vector<long long>& completion, const Timeline& timeline) {
    MtHeftPlacement best = native;
    if (tasks.tasks_[native.task].kind != MtTaskKind::Normal) return best;
    Replacements reuse;
    for (const auto& edge : data[native.task]) for (Node* input : edge.second) {
      auto found = replicas[native.worker].find(input);
      if (found != replicas[native.worker].end()) reuse.insert(*found);
    }
    if (!reuse.empty()) {
      auto trial = estimate(native, {}, reuse, owners, completion, timeline);
      if (better(trial, best)) best = std::move(trial);
    }
    std::vector<long long> cuts{0};
    for (const auto& interval : timeline) {
      if (interval.second.finish >= native.start) break;
      cuts.push_back(interval.second.finish);
    }
    if (cuts.size() > 8) {
      const long long last = cuts.back();
      cuts.resize(7);
      cuts.push_back(last);
    }
    std::set<std::vector<int>> attempted;
    for (long long cut : cuts) {
      Replacements local;
      for (const auto& entry : replicas[native.worker]) {
        if (completion[tasks.taskByNode_.at(entry.second)] <= cut) local.insert(entry);
      }
      auto available = [&](Node* n) {
        if (local.count(n)) return true;
        auto source = tasks.taskByNode_.find(n);
        if (source == tasks.taskByNode_.end() || owners[source->second] < 0) return false;
        if (tasks.tasks_[source->second].localNodes.count(n)) return false;
        long long at = completion[source->second];
        if (owners[source->second] != native.worker) at += mtcost::tokenLatency + globalConfig.MtScheduleCommNodeWeight;
        return at <= cut;
      };
      std::vector<std::pair<long long, int>> blockers;
      for (const auto& edge : data[native.task]) {
        if (owners[edge.first] == native.worker) continue;
        const long long ready = completion[edge.first] + mtcost::tokenLatency +
            static_cast<long long>(edge.second.size()) * globalConfig.MtScheduleCommNodeWeight;
        if (ready > cut) blockers.emplace_back(ready, edge.first);
      }
      std::sort(blockers.begin(), blockers.end(), [](const auto& a, const auto& b) {
        return a.first != b.first ? a.first > b.first : a.second < b.second;
      });
      auto consider = [&](const std::vector<Node*>& roots) {
        ++trials;
        auto cone = mtBuildAvailableCone(roots, globalConfig.MtReplicationMaxOps, available);
        if (!cone.valid()) { ++rejected[cone.rejection]; return false; }
        if (cone.nodes.empty()) return true;
        std::vector<int> key;
        for (Node* n : cone.nodes) {
          auto source = tasks.taskByNode_.find(n);
          if (source == tasks.taskByNode_.end() || source->second == native.task) {
            ++rejected["unmapped-or-self-input"]; return false;
          }
          key.push_back(n->id);
        }
        for (Node* n : cone.boundaryInputs) {
          auto found = local.find(n);
          if (found != local.end()) key.push_back(-found->second->id - 1);
        }
        if (!attempted.insert(key).second) return true;
        Replacements redirects = local;
        for (const auto& entry : reuse) redirects.insert(entry);
        for (Node* n : cone.nodes) redirects.erase(n);
        auto trial = estimate(native, std::move(cone), redirects, owners, completion, timeline);
        if (better(trial, best)) best = std::move(trial);
        else ++rejected["no-finish-improvement"];
        return true;
      };
      std::vector<Node*> roots;
      for (const auto& blocker : blockers) {
        std::vector<Node*> additions;
        bool eligible = true;
        for (Node* input : data[native.task].at(blocker.second)) {
          if (available(input)) continue;
          if (input->next.size() < static_cast<size_t>(globalConfig.MtReplicationMinFanout)) eligible = false;
          additions.push_back(input);
        }
        if (!eligible) { ++rejected["root-fanout"]; continue; }
        if (additions.empty()) continue;
        std::sort(additions.begin(), additions.end(), [](Node* a, Node* b) { return a->id < b->id; });
        if (!roots.empty()) consider(additions);
        auto combined = roots;
        combined.insert(combined.end(), additions.begin(), additions.end());
        if (consider(combined)) roots.swap(combined);
      }
    }
    return best;
  }
};

MtHeftReplication::MtHeftReplication(graph& g, MtTaskPlan& tasks, int workers)
    : impl_(std::make_unique<Impl>(g, tasks, workers)) {}
MtHeftReplication::~MtHeftReplication() = default;
MtHeftPlacement MtHeftReplication::evaluate(MtHeftPlacement native,
    const std::vector<int>& owners, const std::vector<long long>& completion, const Timeline& timeline) {
  return impl_->evaluate(std::move(native), owners, completion, timeline);
}

int MtHeftReplication::commit(const MtHeftPlacement& p) {
  auto& s = *impl_;
  auto& tasks = s.tasks;
  Replacements replacements = p.redirects;
  int replicaId = -1;
  if (!p.cone.nodes.empty()) {
    replicaId = tasks.tasks_.size();
    auto instructions = std::make_unique<std::vector<InstInfo>>();
    MtTask replica = s.lowerCone(p.cone, *instructions);
    replica.owner = p.worker;
    replica.predecessors = p.copyDependencies;
    auto* super = new SuperNode();
    s.g.sortedSuper.push_back(super);
    const int group = tasks.partitionGroupCount_++;
    for (Node* original : p.cone.nodes) {
      Node* copy = original->dup(NODE_OTHERS, original->name + "$MT_REP$" + std::to_string(s.copiedNodes++));
      copy->nodeIsRoot = true;
      replacements[original] = copy;
      ExpTree* assignment = original->assignTree.front()->dup();
      mtReplaceConeReferences(assignment, replacements);
      copy->assignTree.push_back(assignment);
      super->add_member(copy);
      replica.members.push_back(copy);
      s.g.allNodes.push_back(copy);
      tasks.emissionNodes_.push_back(copy);
      tasks.taskByNode_[copy] = replicaId;
      tasks.partitionGroupByNode_[copy] = group;
      s.replicas[p.worker][original] = copy;
    }
    redirectInstructions(*replica.insts, replacements);
    Assert(mtLoweredTaskCost(replica) == p.copyCost, "replica estimate/emission cost mismatch");
    tasks.tasks_.push_back(std::move(replica));
    instructions.release();  // Instruction ownership follows the stored MtTask.
    s.copiedOps += p.cone.operations;
  }
  Replacements direct;
  for (const auto& edge : s.data[p.task]) for (Node* input : edge.second) {
    auto found = replacements.find(input);
    if (found != replacements.end()) direct.insert(*found);
  }
  s.reusedInputs += p.cone.nodes.empty() ? direct.size() : 0;
  // Original expressions stay intact until the search ends, so overlapping
  // candidates cannot accidentally clone another worker's redirected tree.
  s.rewrites[p.task] = direct;
  auto& consumer = tasks.tasks_[p.task];
  if (!direct.empty()) redirectInstructions(*consumer.insts, direct);
  consumer.predecessors = p.dependencies;
  if (replicaId >= 0) consumer.predecessors.push_back(replicaId);
  Assert(mtLoweredTaskCost(consumer) == p.cost, "consumer estimate/emission cost mismatch");
  s.decisions.push_back({p, replicaId});
  return replicaId;
}

void MtHeftReplication::finish() {
  auto& s = *impl_;
  for (size_t i = 0; i < s.originalCount; ++i) {
    if (s.rewrites[i].empty()) continue;
    for (Node* n : s.tasks.tasks_[i].members) for (auto* tree : n->assignTree) mtReplaceConeReferences(tree, s.rewrites[i]);
  }
  // Reconnect changed data references while preserving extra ordering edges.
  for (size_t i = 0; i < s.tasks.tasks_.size(); ++i) {
    if (i < s.originalCount && s.rewrites[i].empty()) continue;
    for (Node* n : s.tasks.tasks_[i].members) {
      const auto oldPrev = n->prev;
      std::set<Node*> orderingPrev;
      for (Node* prev : n->depPrev) if (!oldPrev.count(prev)) orderingPrev.insert(prev);
      for (Node* prev : oldPrev) { prev->next.erase(n); prev->depNext.erase(n); }
      n->prev.clear();
      n->depPrev = std::move(orderingPrev);
      n->updateConnect();
    }
  }
  for (auto& task : s.tasks.tasks_) task.successors.clear();
  for (size_t i = 0; i < s.tasks.tasks_.size(); ++i) {
    auto& task = s.tasks.tasks_[i];
    std::sort(task.predecessors.begin(), task.predecessors.end());
    task.predecessors.erase(std::unique(task.predecessors.begin(), task.predecessors.end()), task.predecessors.end());
    for (int source : task.predecessors) s.tasks.tasks_[source].successors.push_back(i);
    if (i < s.originalCount) for (const auto& edge : s.data[i]) {
      s.removedEdges += !std::binary_search(task.predecessors.begin(), task.predecessors.end(), edge.first);
    }
  }
}

void MtHeftReplication::report(const std::vector<int>& newIds, long long makespan) const {
  const auto& s = *impl_;
  fprintf(stderr, "[cppEmitter-mt] schedule-replication mode=lowered-heft objective=rank-eft "
      "cost=lowered estimated-makespan=%lld edges=%zu copied-nodes=%zu copied-ops=%zu "
      "replica-tasks=%zu reused-inputs=%zu cone-trials=%zu\n", makespan, s.removedEdges,
      s.copiedNodes, s.copiedOps, s.tasks.tasks_.size() - s.originalCount, s.reusedInputs, s.trials);
  for (const auto& e : s.rejected) fprintf(stderr, "[cppEmitter-mt] schedule-replication rejected-%s=%zu\n", e.first.c_str(), e.second);
  if (globalConfig.OutputDir.empty()) return;
  FILE* out = std::fopen((globalConfig.OutputDir + "/mt-replication-decisions.csv").c_str(), "w");
  Assert(out != nullptr, "cannot write lowered HEFT decisions");
  fprintf(out, "task,original_task,worker,start,finish,wait_start,wait_finish,task_cost,"
               "replica_task,copy_start,copy_finish,copy_cost,copied_nodes,copied_ops\n");
  for (const auto& entry : s.decisions) {
    const auto& p = entry.placement;
    fprintf(out, "%d,%d,%d,%lld,%lld,%lld,%lld,%d,%d,%lld,%lld,%d,%zu,%d\n",
        newIds[p.task], p.task, p.worker, p.start, p.finish, p.waitStart, p.waitFinish, p.cost,
        entry.replica < 0 ? -1 : newIds[entry.replica], p.copyStart, p.copyFinish, p.copyCost,
        p.cone.nodes.size(), p.cone.operations);
  }
  Assert(std::fclose(out) == 0, "cannot finish lowered HEFT report");
}

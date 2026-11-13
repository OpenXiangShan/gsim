#include "common.h"
#include "NodeEvolution.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <queue>
#include <stack>
#include <set>
#include <map>
#include <memory>
#include <tuple>
#include <utility>

bool nextVarConnect(Node* node);
bool nameExist(std::string str);
Node* changeName(std::string oldName, std::string newName);

static std::set<Node*> fullyVisited;
static std::set<Node*> partialVisited;

static std::map<Node*, std::vector<Node*>> splitArrayMap;
static std::map<Node*, bool> arraySplitMap;
static NodeEvolutionTrace* splitTrace = nullptr;
static std::map<Node*, const char*> arrayKeepReasons;

// Bundle memory ports share address expressions, and bundle registers share
// reset conditions. An in-place rewrite must trace every physical owner.
class ArrayRewriteOwners {
 public:
  ArrayRewriteOwners(const std::set<Node*>& nodes, Node* array) {
    for (Node* node : nodes) {
      const auto collect = [&](ExpTree* tree) {
        if (!tree) return;
        trees[tree].push_back(node);
        std::vector<std::pair<ENode*, ENode*>> pending{{tree->getRoot(), nullptr},
                                                    {tree->getlval(), nullptr}};
        while (!pending.empty()) {
          auto [expression, parent] = pending.back();
          pending.pop_back();
          if (!expression) continue;
          if (expression->nodePtr == array) {
            expressions[expression].push_back(node);
            if (parent) expressions[parent].push_back(node);
          }
          for (ENode* child : expression->child) pending.emplace_back(child, expression);
        }
      };
      for (ExpTree* tree : node->assignTree) collect(tree);
      collect(node->resetTree);
    }
  }

  void rewritten(ExpTree* tree, ENode* expression) {
    if (expression) {
      const auto found = expressions.find(expression);
      if (found != expressions.end()) changed.insert(found->second.begin(), found->second.end());
    } else {
      const auto found = trees.find(tree);
      if (found != trees.end()) changed.insert(found->second.begin(), found->second.end());
    }
  }

  std::set<Node*> changed;
  std::set<Node*> direct;

 private:
  std::map<ENode*, std::vector<Node*>> expressions;
  std::map<ExpTree*, std::vector<Node*>> trees;
};

static ArrayRewriteOwners* splitRewriteOwners = nullptr;

static std::vector<Node*> traceInventory(graph* g) {
  std::vector<Node*> nodes;
  const auto append = [&](Node* node) {
    if (node->status != DEAD_NODE) nodes.push_back(node);
  };
  for (Node* node : g->allNodes) append(node);
  for (Node* node : g->memory) append(node);
  for (const auto& entry : splitArrayMap)
    for (Node* member : entry.second) append(member);
  std::sort(nodes.begin(), nodes.end(), [](const Node* a, const Node* b) { return a->id < b->id; });
  nodes.erase(std::unique(nodes.begin(), nodes.end()), nodes.end());
  return nodes;
}

static void rememberKeep(Node* node, const char* reason) {
  if (splitTrace) arrayKeepReasons[node] = reason;
}

const std::map<Node*, std::vector<Node*>>& graph::splitArraySources() const {
  return splitArrayMap;
}

static std::string splitArrayMapEscape(const std::string& text) {
  std::string out;
  for (unsigned char ch : text) {
    switch (ch) {
      case '\\': out += "\\\\"; break;
      case '"': out += "\\\""; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (ch >= 0x20) out += static_cast<char>(ch);
    }
  }
  return out;
}

static void dumpSplitArrayMap(graph* g) {
  if (globalConfig.SplitArrayMapPath.empty()) return;
  using Row = std::tuple<std::string, size_t, const Node*, const Node*>;
  std::vector<Row> rows;
  for (const auto& entry : splitArrayMap) {
    for (size_t index = 0; index < entry.second.size(); ++index) {
      rows.emplace_back(entry.first->name, index, entry.first, entry.second[index]);
    }
  }
  std::sort(rows.begin(), rows.end(), [](const Row& lhs, const Row& rhs) {
    if (std::get<0>(lhs) != std::get<0>(rhs)) return std::get<0>(lhs) < std::get<0>(rhs);
    return std::get<1>(lhs) < std::get<1>(rhs);
  });
  const std::filesystem::path path(globalConfig.SplitArrayMapPath);
  if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path);
  if (!out.is_open()) {
    fprintf(stderr, "Cannot open split array map: %s\n", path.c_str());
    exit(1);
  }
  auto dumpDimension = [&out](const Node* node) {
    out << "[";
    for (size_t index = 0; index < node->dimension.size(); ++index) {
      if (index != 0) out << ", ";
      out << node->dimension[index];
    }
    out << "]";
  };
  out << "{\n  \"formatVersion\": 1,\n  \"parents\": " << splitArrayMap.size()
      << ",\n  \"members\": " << rows.size() << ",\n  \"entries\": [\n";
  for (size_t index = 0; index < rows.size(); ++index) {
    const Node* parent = std::get<2>(rows[index]);
    const Node* member = std::get<3>(rows[index]);
    out << "    {\"parentId\": " << parent->id
        << ", \"parentName\": \"" << splitArrayMapEscape(parent->name)
        << "\", \"parentWidth\": " << parent->width << ", \"parentDimension\": ";
    dumpDimension(parent);
    out << ", \"memberIndex\": " << std::get<1>(rows[index])
        << ", \"memberId\": " << member->id
        << ", \"memberName\": \"" << splitArrayMapEscape(member->name)
        << "\", \"memberWidth\": " << member->width << ", \"memberDimension\": ";
    dumpDimension(member);
    out << "}";
    if (index + 1 != rows.size()) out << ",";
    out << "\n";
  }
  out << "  ]\n}\n";
}

ExpTree* dupTreeWithIdx(ExpTree* tree, std::vector<int>& index, Node* node) {
  ENode* lvalue = tree->getlval()->dup();
  for (int idx : index) {
    ENode* idxNode = new ENode(OP_INDEX_INT);
    idxNode->addVal(idx);
    lvalue->addChild(idxNode);
  }
  ENode* rvalue = tree->getRoot()->dup();
  int groupIdx = 0;
  for (size_t i = 0; i < index.size(); i ++) {
    groupIdx = groupIdx * node->dimension[i + node->dimension.size() - index.size()] + index[i];
  }
  std::stack<std::tuple<ENode*, ENode*, int>> s;
  s.push(std::make_tuple(rvalue, nullptr, -1));
  while(!s.empty()) {
    auto top = s.top();
    ENode* top_enode = std::get<0>(top);
    ENode* parent = std::get<1>(top);
    int top_idx = std::get<2>(top);
    s.pop();
    for (size_t i = 0; i < top_enode->getChildNum(); i ++) {
      if (!top_enode->getChild(i)) continue;
    }
    if (top_enode->getNode()) {
      Assert(top_enode->getNode()->isArray(), "%s(%p) is not array", top_enode->getNode()->name.c_str(), top_enode);
      for (int idx : index) {
        ENode* idxNode = new ENode(OP_INDEX_INT);
        idxNode->addVal(idx);
        top_enode->addChild(idxNode);
      }
    } else if (top_enode->opType == OP_GROUP) {
      ENode* select = top_enode->getChild(groupIdx);
      if (!parent) rvalue = select;
      else parent->setChild(top_idx, select);
      top_enode = select;
      continue;
    }
    if (top_enode->opType == OP_WHEN || top_enode->opType == OP_MUX) {
      if (top_enode->getChild(1)) s.push(std::make_tuple(top_enode->getChild(1), top_enode, 1));
      if (top_enode->getChild(2)) s.push(std::make_tuple(top_enode->getChild(2), top_enode, 2));
    } else if (top_enode->opType == OP_RESET) {
      if (top_enode->getChild(1)) s.push(std::make_tuple(top_enode->getChild(1), top_enode, 1));
    }
  }
  ExpTree* ret = new ExpTree(rvalue, lvalue);
  return ret;
}

bool point2self(Node* node) {
  std::set<Node*> nextNodes;
  std::stack<Node*> s;
  std::set<Node*> visited;
  for (Node* next : node->next) {
    s.push(next);
    nextNodes.insert(next);
    if (next == node) return true;
  }
  while(!s.empty()) {
    Node* top = s.top();
    s.pop();
    if (visited.find(top) != visited.end()) continue;
    visited.insert(top);
    for (Node* next : top->next) {
      if (next == node) return true;
      if (nextNodes.find(next) != nextNodes.end()) continue;
      s.push(next);
    }
  }
  return false;
}

Node* getSplitArray(graph* g, const char*& reason) {
  /* array points to itself */
  for (Node* node : partialVisited) {
    if (node->isArray() && node->next.find(node) != node->next.end()) {
      reason = "direct_array_self_dependency";
      return node;
    }
  }

  for (Node* node : partialVisited) {
    if (node->isArray() && point2self(node)) {
      reason = "array_dependency_cycle";
      return node;
    }
  }

  for (Node* node : g->halfConstantArray) {
    if (fullyVisited.find(node) != fullyVisited.end() || splitArrayMap.find(node) != splitArrayMap.end()) continue;
    if (point2self(node)) {
      reason = "half_constant_array_dependency_cycle";
      return node;
    }
  }
  for (Node* node : partialVisited) {
    int time = 0;
    for (Node* prev : node->prev) {
      if (fullyVisited.find(prev) != fullyVisited.end()) time ++;
    }
    printf("current %s %d/%ld\n", node->name.c_str(), time, node->prev.size());
    for (Node* prev : node->prev) {
      if (fullyVisited.find(prev) == fullyVisited.end()) printf("  missing %s\n", prev->name.c_str());
    }
  }

  Panic();

}

void fillEmptyENodeWhen(ENode* newENode, ENode* oldENode) {
  std::stack<ENode*> s;
  s.push(newENode);
  while(!s.empty()) {
    ENode* top = s.top();
    s.pop();
    for (ENode* childENode : top->child) {
      if (childENode) s.push(childENode);
    }
    if (top->opType == OP_WHEN) {
      if (!top->getChild(1)) top->setChild(1, oldENode->dup());
      if (!top->getChild(2)) top->setChild(2, oldENode->dup());
    }
  }
}

void fillEmptyWhen(ExpTree* newTree, ENode* oldNode) {
  fillEmptyENodeWhen(newTree->getRoot(), oldNode);
}

bool subTreeEq(ENode* enode1, ENode* enode2);
int countEmptyENodeWhen(ENode* enode);

ExpTree* mergeWhenTree(ExpTree* tree1, ExpTree* tree2) {
  int emptyWhen = countEmptyENodeWhen(tree2->getRoot());
  if (emptyWhen == 0) return tree2;
  if (emptyWhen == 1 && tree1->getRoot()->opType != OP_WHEN) {
    fillEmptyWhen(tree2, tree1->getRoot());
    return tree2;
  }
  bool canMerge = true;
  bool mergeIndi = false;
  std::queue<std::pair<ENode*, ENode*>> enodeQueue;
  enodeQueue.push((std::make_pair(tree1->getRoot(), tree2->getRoot())));
  while (!enodeQueue.empty()) {
    ENode* enode1, *enode2;
    std::tie(enode1, enode2) = enodeQueue.front();
    enodeQueue.pop();
    if (enode1->opType == OP_WHEN && enode2->opType == OP_WHEN) {
      if (subTreeEq(enode1->getChild(0), enode2->getChild(0))) {
        if (enode1->getChild(1) && enode2->getChild(1)) enodeQueue.push(std::make_pair(enode1->getChild(1), enode2->getChild(1)));
        if (enode1->getChild(2) && enode2->getChild(2)) enodeQueue.push(std::make_pair(enode1->getChild(2), enode2->getChild(2)));
        mergeIndi = true;
      } else {
        if (mergeIndi) canMerge = false;
        else Assert(enodeQueue.empty(), "should not reach here");
      }
    }
  }

  if (!canMerge) return nullptr;

  std::queue<std::tuple<ENode*, ENode*, ENode*, int>> s;
  s.push((std::make_tuple(tree1->getRoot(), tree2->getRoot(), nullptr, -1)));
  ExpTree* ret = nullptr;
  while (!s.empty()) {
    ENode* enode1, *enode2, *parent2;
    int idx2;
    std::tie(enode1, enode2, parent2, idx2) = s.front();
    s.pop();
    if (enode1->opType == OP_WHEN && enode2->opType == OP_WHEN) {
      if (subTreeEq(enode1->getChild(0), enode2->getChild(0))) {
        if (enode1->getChild(1) && enode2->getChild(1)) s.push(std::make_tuple(enode1->getChild(1), enode2->getChild(1), enode2, 1));
        if (enode1->getChild(2) && enode2->getChild(2)) s.push(std::make_tuple(enode1->getChild(2), enode2->getChild(2), enode2, 2));
        if (!enode2->getChild(1) && enode1->getChild(1)) enode2->setChild(1, enode1->getChild(1));
        if (!enode2->getChild(2) && enode1->getChild(2)) enode2->setChild(2, enode1->getChild(2));
        ret = tree2;
      } else {
        if (ret) Panic();
        else Assert(s.empty(), "should not reach here");
      }
    } else {
      if (ret) fillEmptyENodeWhen(enode2, enode1);
      else {
        Assert(s.empty(), "should not reach here");
      }
    }
  }

  return ret;
}

void distributeTree(Node* node, ExpTree* tree, std::vector<Node*>& arrayMember) {
  Assert(arrayMember.size() == node->arrayEntryNum(), "arrayMember size %ld != arrayEntryNum %ld", arrayMember.size(), node->arrayEntryNum());
  int begin, end;
  std::tie(begin, end) = tree->getlval()->getIdx(node);
  Assert(begin >= 0 && end >= begin, "Invalid index for array %s: %d-%d", node->name.c_str(), begin, end);
  if (begin == end) {
    int idx = begin;
    if (arrayMember[idx]->assignTree.size() == 0) arrayMember[idx]->assignTree.push_back(tree);
    else {
      ExpTree* replaceTree = mergeWhenTree(arrayMember[idx]->assignTree.back(), tree);
      if (replaceTree) arrayMember[idx]->assignTree.back() = replaceTree;
      else arrayMember[idx]->assignTree.push_back(tree);
    }
  } else {
    for (int idx = begin; idx <= end; idx ++) {
      /* compute index for all array operands in tree */
      std::vector<int> subIdx(node->dimension.size() - tree->getlval()->getChildNum());
      int tmp = idx;
      for (size_t i = node->dimension.size() - 1; i >= tree->getlval()->getChildNum(); i --) {
        subIdx[i - tree->getlval()->getChildNum()] = tmp % node->dimension[i];
        tmp /= node->dimension[i];
        if (i == 0) break;
      }

      ExpTree* newTree = dupTreeWithIdx(tree, subIdx, node);
      /* duplicate tree with idx */
      if (arrayMember[idx]->assignTree.size() == 0) arrayMember[idx]->assignTree.push_back(newTree);
      else {
        ExpTree* replaceTree = mergeWhenTree(arrayMember[idx]->assignTree.back(), newTree);
        if (replaceTree) arrayMember[idx]->assignTree.back() = replaceTree;
        else arrayMember[idx]->assignTree.push_back(newTree);
      }
    }
  }
}

void splitAssignMent(Node* node, ExpTree* tree, std::vector<ExpTree*>& newTrees) {
  int range = 1;
  for (int i = tree->getlval()->getChildNum(); i < (int)node->dimension.size(); i ++) range *= node->dimension[i];

  for (int idx = 0; idx < range; idx ++) {
    std::vector<int> subIdx(node->dimension.size() - tree->getlval()->getChildNum());
    int tmp = idx;
    for (size_t i = node->dimension.size() - 1; i >= tree->getlval()->getChildNum(); i --) {
      subIdx[i - tree->getlval()->getChildNum()] = tmp % node->dimension[i];
      tmp /= node->dimension[i];
    }
    ExpTree* dupTree = dupTreeWithIdx(tree, subIdx, node);
    newTrees.push_back(dupTree);
  }
}

bool ExpTree::updateWithSplittedArray(Node* node, Node* array, std::vector<Node*>& arrayMember) {
  bool changed = false;
  std::stack<std::tuple<ENode*, ENode*, int>> s;
  s.push(std::make_tuple(getRoot(), nullptr, 0));
  s.push(std::make_tuple(getlval(), nullptr, -2));
  while(!s.empty()) {
    auto top_tuple = s.top();
    s.pop();
    ENode* top_enode = std::get<0>(top_tuple);
    ENode* top_parent = std::get<1>(top_tuple);
    int top_idx = std::get<2>(top_tuple);
    if (!top_enode->nodePtr || top_enode->nodePtr != array) {
      for (size_t i = 0; i < top_enode->getChildNum(); i ++) {
        if (top_enode->getChild(i)) s.push(std::make_tuple(top_enode->getChild(i), top_enode, top_idx < 0 ? top_idx : i));
      }
      continue;
    }
    auto range = top_enode->getIdx(top_enode->nodePtr);
    if (range.first < 0) {
      changed = true;
      if (array->dimension.size() > 1) TODO();
      ENode* enode = nullptr;
      for (int i = array->arrayEntryNum() - 1; i >= 0; i --) {
        Node* member = arrayMember[i];
        if (enode) {
          ENode* mux = new ENode(OP_MUX);
          ENode* cond = new ENode(OP_EQ);
          ENode* idx = new ENode(OP_INT);
          idx->strVal = std::to_string(i);
          idx->width = upperLog2(array->arrayEntryNum());
          cond->addChild(top_enode->getChild(0)->getChild(0)->dup());  // first dimension
          cond->addChild(idx);
          mux->addChild(cond);
          mux->addChild(new ENode(member));
          mux->addChild(enode);
          enode = mux;
        } else {
          enode = new ENode(member);
        }
      }
      if (splitRewriteOwners) splitRewriteOwners->rewritten(this, top_parent);
      if (top_parent) top_parent->setChild(top_idx, enode);
      else setRoot(enode);
    } else if (range.first == range.second) {
      changed = true;
      if (splitRewriteOwners) splitRewriteOwners->rewritten(this, top_enode);
      top_enode->nodePtr = arrayMember[range.first];
      top_enode->child.clear();
    } else {
      if (top_idx < 0) continue; // lvalue
      changed = true;
      ENode* group = new ENode(OP_GROUP);
      group->width = top_enode->width;
      for (int i = range.first; i <= range.second; i ++) {
        Node* member = arrayMember[i];
        ENode* enode = new ENode(member);
        enode->width = member->width;
        enode->sign = member->sign;
        group->addChild(enode);
      }
      if (splitRewriteOwners) splitRewriteOwners->rewritten(this, top_parent);
      if (top_parent) top_parent->setChild(top_idx, group);
      else setRoot(group);
    }
  }
  return changed;
}

void graph::splitArrayNode(Node* node, const char* reason) {
  node->status = DEAD_NODE;
  /* remove prev connection */
  for (Node* prev : node->depPrev) prev->eraseNext(node);
  for (SuperNode* super : node->super->depPrev) super->eraseNext(node->super);
  for (Node* next : node->depNext) next->erasePrev(node);
  for (SuperNode* super : node->super->depNext) super->erasePrev(node->super);
  node->super->clear_relation();

  for (Node* memberInSuper : node->super->member) {
    if (memberInSuper != node)
      memberInSuper->constructSuperConnect();
  }
  /* create new node */
  int entryNum = node->arrayEntryNum();
  std::vector<Node*> arrayMember;
  for (int i = 0; i < entryNum; i ++) {
    arrayMember.push_back(node->arrayMemberNode(i));
  }
  splitArrayMap[node] = arrayMember;
  if (splitTrace) {
    std::ostringstream geometry;
    geometry << "{\"partition\":true,\"memberCount\":" << entryNum << ",\"physicalShape\":[";
    for (size_t i = 0; i < node->dimension.size(); ++i) {
      if (i) geometry << ',';
      geometry << node->dimension[i];
    }
    geometry << "]}";
    splitTrace->event("split", reason, {node}, arrayMember, geometry.str());
  }
  /* distribute arrayVal */
  for (ExpTree* tree : node->assignTree) distributeTree(node, tree, arrayMember);
  if (node->resetTree) {
    for (size_t idx = 0; idx < node->arrayEntryNum(); idx ++) {
      /* compute index for all array operands in tree */
      std::vector<int> subIdx(node->dimension.size());
      int tmp = idx;
      for (int i = node->dimension.size() - 1; i >= 0; i --) {
        subIdx[i] = tmp % node->dimension[i];
        tmp /= node->dimension[i];
      }
      if (node->resetTree) {
        ExpTree* newTree = dupTreeWithIdx(node->resetTree, subIdx, node);
        arrayMember[idx]->resetTree = newTree;
      }
    }
  }

  std::set<Node*> checkNodes;
  /* construct connections */
  if ((node->type == NODE_REG_SRC || node->type == NODE_REG_DST) && splitArrayMap.find(node->getBindReg()) != splitArrayMap.end()) {
    Node* regBind = node->getBindReg();
    for (Node* member : splitArrayMap[regBind]) {
      checkNodes.insert(member);
    }
  }
  for (Node* member : arrayMember) {
    checkNodes.insert(member);
  }
  for (Node* next : node->depNext) {
    checkNodes.insert(next);
    if (next != node) next->erasePrev(node);
  }
  std::unique_ptr<ArrayRewriteOwners> rewriteOwners;
  if (splitTrace) {
    rewriteOwners = std::make_unique<ArrayRewriteOwners>(checkNodes, node);
    splitRewriteOwners = rewriteOwners.get();
  }
  for (Node* n : checkNodes) {
    bool rewritten = false;
    for (ExpTree* tree : n->assignTree)
      rewritten |= tree->updateWithSplittedArray(n, node, arrayMember);
    if (n->resetTree) rewritten |= n->resetTree->updateWithSplittedArray(n, node, arrayMember);
    if (rewriteOwners && rewritten) {
      rewriteOwners->direct.insert(n);
      rewriteOwners->changed.insert(n);
    }
  }
  splitRewriteOwners = nullptr;
  if (rewriteOwners) {
    for (Node* n : rewriteOwners->changed) {
      if (n == node) continue;
      splitTrace->event("rewrite", rewriteOwners->direct.count(n) ?
          "split_array_reference_rewrite" : "split_array_shared_expression_rewrite", {n}, {n},
          "{\"arrayParent\":{\"kind\":\"gsim-node\",\"id\":" + std::to_string(node->id) + ",\"generation\":0}}");
    }
  }

  for (Node* n : checkNodes) {
    n->updateConnect();
    if (n->type == NODE_REG_SRC) {
      n->updateDep();
    }
  }
  /* clear node connection */
  node->clear_relation();
  for (Node* member : arrayMember) member->constructSuperConnect();
  for (Node* member : arrayMember) {
    if (member->super->prev.size() == 0) supersrc.push_back(member->super);
  }
}

void graph::splitArray() {
  fullyVisited.clear();
  partialVisited.clear();
  splitArrayMap.clear();
  arraySplitMap.clear();
  arrayKeepReasons.clear();
  std::unique_ptr<NodeEvolutionTrace> trace;
  if (!globalConfig.NodeEvolutionTracePath.empty()) {
    trace = std::make_unique<NodeEvolutionTrace>(globalConfig.NodeEvolutionTracePath, name, "splitArray",
                                               globalConfig.NodeEvolutionInputSha256);
    splitTrace = trace.get();
    trace->inventory("before", traceInventory(this));
  }
  std::map<Node*, int> times;
  std::stack<Node*> s;
  int num = 0;

  for (SuperNode* super : supersrc) {
    for (Node* node : super->member) {
      if (node->prev.size() == 0) {
        s.push(node);
        fullyVisited.insert(node);
      }
    }
  }

  while(!s.empty()) {
    Node* top = s.top();
    s.pop();

    for (Node* next : top->next) {
      if (times.find(next) == times.end()) {
        times[next] = 0;
        partialVisited.insert(next);
      }
      times[next] ++;

      if (times[next] == (int)next->prev.size()) {
        s.push(next);
        Assert(partialVisited.find(next) != partialVisited.end(), "%s not found in partialVisited", next->name.c_str());
        partialVisited.erase(next);
        Assert(fullyVisited.find(next) == fullyVisited.end(), "%s is already in fullyVisited", next->name.c_str());
        fullyVisited.insert(next);
      }
    }

    while (s.size() == 0 && partialVisited.size() != 0) {
      /* split arrays in partialVisited until s.size() != 0 */
      const char* reason = nullptr;
      Node* node = getSplitArray(this, reason);
      Assert(splitArrayMap.find(node) == splitArrayMap.end(), "%s is already splitted", node->name.c_str());
      num ++;
      splitArrayNode(node, reason);
      /* add into s and visitedSet */
      for (Node* member : splitArrayMap[node]) {
        times[member] = 0;
        for (Node* prev : member->prev) {
          if (fullyVisited.find(prev) != fullyVisited.end()) {
            times[member] ++;
          }
        }
        if (times[member] == (int)member->prev.size()) {
          s.push(member);
          fullyVisited.insert(member);
        } else {
          partialVisited.insert(member);
        }
      }
      /* erase array */
      partialVisited.erase(node);
    }
  }
  Assert(partialVisited.size() == 0, "partial is not empty!");
  printf("[splitArray] split %d arrays\n", num);
  splitOptionalArray();

  supersrc.erase(
    std::remove_if(supersrc.begin(), supersrc.end(), [](const SuperNode* n) {return n->member[0]->status == DEAD_NODE; }),
    supersrc.end()
  );
  dumpSplitArrayMap(this);
  if (trace) {
    const auto nodes = traceInventory(this);
    for (Node* node : nodes) {
      if (!node->isArray()) continue;
      const auto reason = arrayKeepReasons.find(node);
      trace->event("retain", reason != arrayKeepReasons.end() ? reason->second :
          node->type == NODE_MEMORY ? "memory_storage_not_split" : "array_not_selected_for_optional_split",
          {node}, {node});
    }
    trace->inventory("after", nodes);
    trace->finish();
    splitTrace = nullptr;
  }
}

Node* Node::arrayMemberNode(int idx) {
  Assert(isArray(), "%s is not array", name.c_str());
  std::vector<int>index(dimension);
  int dividend = dimension.back();
  int divisor = idx;

  for (int i = (int)dimension.size() - 1; i >= 0; i --) {
    dividend = dimension[i];
    index[i] = divisor % dividend;
    divisor = divisor / dividend;
  }
  std::string memberName = name;
  size_t i;
  for (i = 0; i < index.size(); i ++) {
    memberName += "_" + std::to_string(index[i]);
  }

  if (nameExist(memberName)) {
    std::string newName = memberName + "_0";
    Assert(!nameExist(newName), "%s already exist", newName.c_str());
    Node* renamed = changeName(memberName, newName);
    if (splitTrace) splitTrace->event("rewrite", "split_member_name_collision_rename", {renamed}, {renamed},
        "{\"arrayParent\":{\"kind\":\"gsim-node\",\"id\":" + std::to_string(id) + ",\"generation\":0}}");
  }

  Node* member = new Node(type);
  member->name = memberName;
  member->clock = clock;
  member->reset = reset;
  member->setType(width, sign);
  member->constructSuperNode();
  member->lineno = lineno;
  member->sourceKind = sourceKind;
  member->sourceLine = sourceLine;
  member->sourceOwnerId = sourceOwnerId;
  member->sourceEffectKind = sourceEffectKind;
  member->sourceOriginRole = sourceOriginRole;
  member->sourceMemoryDepth = sourceMemoryDepth;
  member->semanticPath = semanticPath;
  member->semanticScopeDepth = semanticScopeDepth;
  member->semanticShape = semanticShape;
  member->semanticShapeFieldDepths = semanticShapeFieldDepths;
  member->semanticIndices = semanticIndices;
  member->semanticAxes = semanticAxes;
  Assert(semanticAxes.size() == index.size(), "source axes mismatch for %s", name.c_str());
  for (size_t axis = 0; axis < index.size(); ++axis)
    member->semanticIndices[semanticAxes[axis]] = index[axis];
  member->semanticAxes.clear();
  for ( ; i < dimension.size(); i ++) member->dimension.push_back(dimension[i]);

  return member;
}

bool nextVarConnect(Node* node) {
  for (Node* next : node->next) {
    std::stack<std::pair<ENode*, bool>> s;
    for (ExpTree* tree : next->assignTree) {
      s.push(std::make_pair(tree->getRoot(), false));
      s.push(std::make_pair(tree->getlval(), false));
    }
    while (!s.empty()) {
      ENode* top;
      bool anyMux;
      std::tie(top, anyMux) = s.top();
      s.pop();
      if (top->getNode() == node) {
        int beg, end;
        std::tie(beg, end) = top->getIdx(node);
        if (beg < 0) return true;
      }
      for (size_t i = 0; i < top->getChildNum(); i ++) {
        if (top->getChild(i)) s.push(std::make_pair(top->getChild(i), anyMux || (top->opType == OP_MUX)));
      }
    }
  }
  return false;
}
void graph::checkNodeSplit(Node* node) {
  if (arraySplitMap.find(node) != arraySplitMap.end()) return;
  if (!node->isArray()) return;
  if ((node->type != NODE_OTHERS && node->type != NODE_REG_SRC) || node->arrayEntryNum() <= 1) {
    arraySplitMap[node] = false;
    rememberKeep(node, node->arrayEntryNum() <= 1 ? "single_array_member" : "unsupported_array_node_kind");
    return;
  }
  for (Node* prev : node->prev) checkNodeSplit(prev);
  bool prevSplitted = true;
  for (Node* prev : node->prev) {
    if (prev->isArray() && !arraySplitMap[prev]) prevSplitted = false;
  }
  Node* treeNode = node->type == NODE_REG_SRC ? node->getDst() : node;
  bool anyVarIdx = false;
  const char* keepReason = nullptr;
  int beg, end;
  for (ExpTree* tree : treeNode->assignTree) {
    std::tie(beg, end) = tree->getlval()->getIdx(treeNode);
    if (beg < 0 || (beg != end && !prevSplitted)) {
      anyVarIdx = true;
      if (!keepReason) keepReason = beg < 0 ? "dynamic_write_index" : "unsplit_aggregate_input";
    }
  }

  if (!anyVarIdx && !nextVarConnect(node)) {
    node->status = DEAD_NODE;
    if (node->type == NODE_REG_SRC) node->getDst()->status = DEAD_NODE;
    arraySplitMap[node] = true;
  } else {
    arraySplitMap[node] = false;
    rememberKeep(node, anyVarIdx ? keepReason : "dynamic_consumer_index");
  }
}

/* splitted separately assigned, no variable index acceesing arrays */
void graph::splitOptionalArray() {
  int num = 0;
  for (Node* node : fullyVisited) {
    checkNodeSplit(node);
  }
  regsrc.erase(
    std::remove_if(regsrc.begin(), regsrc.end(), [](const Node* n){ return n->status == DEAD_NODE; }),
        regsrc.end()
  );
#ifdef ORDERED_TOPO_SORT
  std::vector<Node*> sortedArray;
  for (auto iter : arraySplitMap) {
    if (iter.second) sortedArray.push_back(iter.first);
  }
  std::sort(sortedArray.begin(), sortedArray.end(), [](Node* a, Node* b) {return a->id < b->id;});
  for (Node* arrayNode : sortedArray) {
#else
  for (auto iter : arraySplitMap) {
    if (!iter.second) continue;
    Node* arrayNode = iter.first;
#endif
    splitArrayNode(arrayNode, "static_indexed_array_access");
    if (arrayNode->type == NODE_REG_SRC) {
      splitArrayNode(arrayNode->getDst(), "paired_register_array_split");
      Assert(splitArrayMap[arrayNode].size() == splitArrayMap[arrayNode->getDst()].size(), "%p %s member not match", arrayNode, arrayNode->name.c_str());
      for (size_t i = 0; i < splitArrayMap[arrayNode].size(); i ++) {
        regsrc.push_back(splitArrayMap[arrayNode][i]);
        splitArrayMap[arrayNode][i]->bindReg(splitArrayMap[arrayNode->getDst()][i]);
        splitArrayMap[arrayNode][i]->updateDep();
        if (splitTrace) {
          const std::vector<Node*> pair = {splitArrayMap[arrayNode][i], splitArrayMap[arrayNode->getDst()][i]};
          splitTrace->event("rewrite", "register_member_pair_bound", pair, pair,
              "{\"memberIndex\":" + std::to_string(i) + "}");
        }
      }
    }
    num ++;
  }
  printf("[splitOptionalArray] split %d arrays\n", num);
}

#include "common.h"
#include <stack>

bool checkENodeEq(ENode* enode1, ENode* enode2);
void getENodeRelyNodes(ENode* enode, std::set<Node*>& allNodes);

bool checkCondENodeSame(ENode* enode1, ENode* enode2) {
  if (!enode1 && !enode2) return true;
  if (!enode1 || !enode2) return false;
  if (enode1->opType != enode2->opType) return false;
  if (enode1->width != enode2->width || enode1->sign != enode2->sign) return false;
  if (enode1->child.size() != enode2->child.size()) return false;
  if (enode1->opType == OP_INT && enode1->strVal != enode2->strVal) return false;
  if (enode1->values.size() != enode2->values.size()) return false;
  if ((!enode1->getNode() && enode2->getNode()) || (enode1->getNode() && !enode2->getNode())) return false;
  if (enode1->getNode() && enode2->getNode() && enode1->getNode() != enode2->getNode()) return false;
  for (size_t i = 0; i < enode1->values.size(); i ++) {
    if (enode1->values[i] != enode2->values[i]) return false;
  }
  for (size_t i = 0; i < enode1->child.size(); i ++) {
    if (!checkCondENodeSame(enode1->child[i], enode2->child[i])) return false;
  }
  return true;
}

bool checkCondEq(StmtNode* stmtNode, ENode* enode) {
  if ((enode->opType == OP_WHEN || enode->opType == OP_RESET) && stmtNode->type == OP_STMT_WHEN) {
    if (stmtNode->child[0]->isENode && checkCondENodeSame(enode->getChild(0), stmtNode->getChild(0)->enode)) {
      return true;
    }
  }
  return false;
}

static bool containsSinkCandidate(ENode* root) {
  if (root == nullptr) return false;
  if (root->opType != OP_WHEN && root->opType != OP_RESET) return true;
  for (size_t index = 1; index < root->child.size(); ++index) {
    if (containsSinkCandidate(root->child[index])) return true;
  }
  return false;
}

void addDepthPath(std::vector<int>& path, int depth, int nodeIdx) {
  if (path.size() <= depth) path.push_back(nodeIdx);
  else if(nodeIdx > path[depth]){
    path[depth] = nodeIdx;
    path.resize(depth + 1);
  }
}

void maxPath(std::vector<int>& dst, std::vector<int>& path1, std::vector<int>& path2) {
  int isFirst = false;
  for (size_t i = 0; i < path1.size(); i ++) {
    if (i >= path2.size() || path1[i] > path2[i]) {
      isFirst = true;
      break;
    } else if (path1[i] < path2[i]) {
      isFirst = false;
      break;
    }
  }
  if (isFirst) dst = path1;
  else dst = path2;
}

/* generate whens according to enode, and add stmtNode into child */
void StmtNode::generateWhenTree(ENode* lvalue, ENode* root, std::vector<int>& subPath, Node* belong) {
  Assert(root->opType == OP_WHEN || root->opType == OP_RESET, "require when");
  StmtNode* stmt = nullptr;
  std::stack<std::tuple<StmtNode*, ENode*, int>> s;
  s.push(std::make_tuple(stmt, root, 1));
  addDepthPath(subPath, 0, getChildNum());
  while (!s.empty()) {
    StmtNode* parent;
    ENode* enode;
    int depth;
    std::tie(parent, enode, depth) = s.top();
    s.pop();
    if (enode->opType == OP_WHEN || enode->opType == OP_RESET) {
      StmtNode* stmtNode = new StmtNode(OP_STMT_WHEN);
      stmtNode->addChild(new StmtNode(enode->getChild(0)->dup(), belong));
      stmtNode->addChild(new StmtNode(OP_STMT_SEQ));
      stmtNode->addChild(new StmtNode(OP_STMT_SEQ));
      if (enode->getChild(1)) {
        s.push(std::make_tuple(stmtNode->getChild(1), enode->getChild(1), depth + 1));
      }
      if (enode->getChildNum() >= 3 && enode->getChild(2)) {
        s.push(std::make_tuple(stmtNode->getChild(2), enode->getChild(2), depth + 1));
      }
      if (parent) {
        parent->addChild(stmtNode);
        addDepthPath(subPath, depth , parent->getChildNum() - 1);
      }
      else stmt = stmtNode;
    } else {
      parent->addChild(new StmtNode(new ExpTree(enode->dup(), lvalue->dup()), belong));
      addDepthPath(subPath, depth, parent->child.size() - 1);
    }
  }
  addChild(stmt);
}

/**
 * @brief Merges individual expression trees from node into statement tree of a supernode
 *
 * Traverse simultaneously of both statement tree and expression tree, merge the expression
 * tree node into the existing or newly created statement tree node
 * For conditional operations, add to exists statement node if possible. Since the branches of
 * STMT_WHEN can contain multiple statements
 * @param tree The expression tree to be merged
 * @param prevPath Path information within all depPrev of the node this tree belongs to
 * @param[in/out] nodePath Path information for the current node being processed
 * @param belong The node that this expression tree belongs to
 */
void StmtTree::mergeExpTree(ExpTree* tree, std::vector<int>& prevPath, std::vector<int>& nodePath, Node* belong) {
  hasSinkCandidate |= containsSinkCandidate(tree->getRoot());
  if (tree->getRoot() != nullptr &&
      (tree->getRoot()->opType == OP_WHEN || tree->getRoot()->opType == OP_RESET)) {
    hasWhen = true;
  }
  std::vector<int> path;
  maxPath(path, prevPath, nodePath);
  std::stack<std::tuple<StmtNode*, ENode*, int, bool>> s;
  s.push(std::make_tuple(root, tree->getRoot(), 1, true)); // Start with the root of both tree, with depth 1(the children of root StmtTree)
  addDepthPath(nodePath, 0, 0);// The root is always index 0 of depth 0
  while (!s.empty()) {
    StmtNode* stmtNode;
    ENode* enode;
    int depth;
    bool anyLimit;
    std::tie(stmtNode, enode, depth, anyLimit) = s.top();
    s.pop();

    if (anyLimit && depth >= path.size()) anyLimit = false;
    /* For any stmtNode and enode pair. Check:
     * A. They are both conditional node, and share the same condition : */
    if (checkCondEq(stmtNode, enode)) { // directly merges into existing conditional nodes with matching conditions
      if (enode->getChild(1)) {
        s.push(std::make_tuple(stmtNode->getChild(1), enode->getChild(1), depth , anyLimit));
      }
      if (enode->getChildNum() >= 3 && enode->getChild(2)) {
        s.push(std::make_tuple(stmtNode->getChild(2), enode->getChild(2), depth , anyLimit));
      }
    } else {
      /* B. The enode is conditional operation */
      if (enode->opType == OP_WHEN || enode->opType == OP_RESET) {
        /* B1. And stmtNode is a STMT_SEQ, which potentially contains a STMT_WHEN node that meets cond A */
        if (stmtNode->type == OP_STMT_SEQ) {
          int match = -1;
          for (size_t i = 0; i < stmtNode->child.size(); i ++) {
            if (stmtNode->getChild(i)->type == OP_STMT_WHEN &&  /* this check is redundant */
                checkCondEq(stmtNode->getChild(i), enode) &&
                (!anyLimit || path[depth] <= i)) { // if anyLimit is set, don't insert enode beyond the dependency at current depth
              match = i;
              // If, at any depth, the current dependency goes greater than path[depth] (e.g. path=[0,2,2,2,2], current=[0,2,3])
              // Then we don't need to consider the path at following depth anymore
              bool nextLimit = (!anyLimit || path[depth] < i) ? false : anyLimit;
              s.push(std::make_tuple(stmtNode->getChild(i), enode, depth + 1, nextLimit));
              break;
            }
          }/* B1a. A suitable STMT_WHEN node is found and enode is inserted into it*/
          if (match >= 0) {
            addDepthPath(nodePath, depth, match);
            /* B1b. No suitable STMT_WHEN node is found thus a new one is created */
          } else {
            std::vector<int> subPath;
            stmtNode->generateWhenTree(tree->getlval(), enode, subPath, belong);
            for (size_t i = 0; i < subPath.size(); i ++) {
              addDepthPath(nodePath, depth + i, subPath[i]);
            }
          }
        /* B2. StmtNode is not a SEQ. The situation is unknown */
        } else {
          TODO();
        }
      /* C. For non-conditional operations */
      } else {
        Node* node = tree->getlval()->getNode();
        Assert(stmtNode->type == OP_STMT_SEQ, "stmtNode %d is not seq", stmtNode->type);
        /* For non-array, first try to deduplicate it. Avoid xxx = yyy; ... ... xxx = zzz; */
        if (!node->isArray() && enode->opType != OP_INVALID &&
            !(globalConfig.MtMode && node->type == NODE_READWRITER)) {
          for (int i = 0; i < stmtNode->getChildNum(); i ++) {
            StmtNode* child = stmtNode->getChild(i);
            if (child->type == OP_STMT_NODE) {
              Assert(!child->isENode, "invalid stmtNode %d", child->type);
              Node* childNode = child->tree->getlval()->getNode();
              Assert(childNode, "invalid node");
              if (childNode == node) {
                stmtNode->eraseChild(i);
              }
            }
          }
        }
        /* Insert the node to the earliest possible position to minimize dependency path.
           Allowing more extensive conditional operations merging */
        int start = 0; // Caculated earliest possible position
        bool found = false;
        if (anyLimit) {
          /* Even path might not describing the current branch, it's always safe as long as not get out of bound */
          start = std::min((int)stmtNode->child.size(), path[depth]);
        }
        /* Find the earliest non-conditional operation (NODE or SEQ) to insert */
        for (int i = start; i < stmtNode->getChildNum(); i++) {
          /* For STMT_Node, elevate it to a STMT_SEQ, then append  */
          if (stmtNode->getChild(i)->type == OP_STMT_NODE) {
            const auto tmp = stmtNode->getChild(i);
            stmtNode->setChild(i, new StmtNode(OP_STMT_SEQ));
            stmtNode->getChild(i)->addChild(tmp);
            stmtNode->getChild(i)->addChild(new StmtNode(new ExpTree(enode->dup(), tree->getlval()->dup()), belong));
            addDepthPath(nodePath, depth, i);
            found = true;break;
          /* For STMT_SEQ, just append */
          }else if(stmtNode->getChild(i)->type == OP_STMT_SEQ){
            stmtNode->getChild(i)->addChild(new StmtNode(new ExpTree(enode->dup(), tree->getlval()->dup()), belong));
            addDepthPath(nodePath, depth, i);
            found = true;break;
          }
        }
        /* No suitable sibling found. Append to current stmtNode (must be a OP_STMT_SEQ) */
        if (!found){
          stmtNode->addChild(new StmtNode(new ExpTree(enode->dup(), tree->getlval()->dup()), belong));
          addDepthPath(nodePath, depth, stmtNode->child.size() - 1);
        }
      }
    }
  }
}

void StmtTree::addSeq(ExpTree* tree, Node* belong) {
  Assert(root->type == OP_STMT_SEQ, "root is not seq");
  StmtNode* stmtNode = new StmtNode(tree->dup(), belong);
  root->addChild(stmtNode);
}

void StmtTree::mergeStmtTree(StmtTree* tree) {
  Assert(root->type == OP_STMT_SEQ && tree->root->type == OP_STMT_SEQ, "root is not seq");
  for (StmtNode* stmtNode : tree->root->child) {
    root->addChild(stmtNode);
  }
}

namespace {

struct SinkControl {
  StmtNode* when = nullptr;
  int branch = 0; // 1 = then, 2 = else
};

struct SinkUse {
  std::vector<SinkControl> controls;
  bool outputUse = false;
  StmtNode* seq = nullptr;
  size_t index = 0;
};

struct SinkDef {
  StmtNode* stmt = nullptr;
  StmtNode* seq = nullptr;
  size_t index = 0;
  size_t order = 0;
  std::vector<SinkControl> controls;
};

struct SinkMove {
  StmtNode* stmt = nullptr;
  StmtNode* seq = nullptr;
  StmtNode* target = nullptr;
  size_t index = 0;
  size_t order = 0;
  size_t targetIndex = 0;
};

struct SinkTreeInfo {
  std::map<Node*, SinkDef> defs;
  std::map<Node*, std::vector<SinkUse>> uses;
  std::map<StmtNode*, std::pair<StmtNode*, size_t>> whenParents;
  std::map<StmtNode*, std::pair<StmtNode*, size_t>> seqParents;
  std::set<Node*> duplicateDefs;
  size_t nextOrder = 0;
};

bool sameControl(const SinkControl& left, const SinkControl& right) {
  return left.when == right.when && left.branch == right.branch;
}

bool controlPrefix(const std::vector<SinkControl>& prefix,
                   const std::vector<SinkControl>& path) {
  if (prefix.size() > path.size()) return false;
  for (size_t i = 0; i < prefix.size(); ++i) {
    if (!sameControl(prefix[i], path[i])) return false;
  }
  return true;
}

size_t commonControlDepth(const std::vector<SinkUse>& uses) {
  if (uses.empty()) return 0;
  size_t depth = uses.front().controls.size();
  for (const SinkUse& use : uses) depth = std::min(depth, use.controls.size());
  for (size_t index = 0; index < depth; ++index) {
    for (const SinkUse& use : uses) {
      if (!sameControl(uses.front().controls[index], use.controls[index])) return index;
    }
  }
  return depth;
}

bool pureSinkTree(ENode* node) {
  if (node == nullptr) return true;
  switch (node->opType) {
    case OP_READ_MEM:
    case OP_WRITE_MEM:
    case OP_EXT_FUNC:
    case OP_PRINTF:
    case OP_ASSERT:
    case OP_EXIT:
    case OP_RESET:
      return false;
    default:
      break;
  }
  for (ENode* child : node->child) if (!pureSinkTree(child)) return false;
  return true;
}

void collectRefs(ENode* root, std::set<Node*>& refs) {
  if (root != nullptr) getENodeRelyNodes(root, refs);
}

void collectSinkTree(StmtNode* stmt, StmtNode* containingSeq, size_t index,
                     const std::vector<SinkControl>& controls, SinkTreeInfo& info) {
  if (stmt == nullptr) return;
  if (stmt->type == OP_STMT_SEQ) {
    info.seqParents[stmt] = {containingSeq, index};
    for (size_t child = 0; child < stmt->child.size(); ++child) {
      collectSinkTree(stmt->child[child], stmt, child, controls, info);
    }
    return;
  }
  if (stmt->type == OP_STMT_WHEN) {
    Assert(stmt->child.size() == 3, "invalid statement when shape");
    std::set<Node*> conditionRefs;
    if (stmt->child[0] != nullptr && stmt->child[0]->isENode)
      collectRefs(stmt->child[0]->enode, conditionRefs);
    for (Node* ref : conditionRefs)
      info.uses[ref].push_back({controls, false, containingSeq, index});
    info.whenParents[stmt] = {containingSeq, index};
    for (int branch = 1; branch <= 2; ++branch) {
      std::vector<SinkControl> branchControls = controls;
      branchControls.push_back({stmt, branch});
      collectSinkTree(stmt->child[branch], stmt, branch, branchControls, info);
    }
    return;
  }
  if (stmt->type != OP_STMT_NODE || stmt->isENode || stmt->tree == nullptr) return;

  Node* defined = stmt->tree->getlval() == nullptr ? nullptr : stmt->tree->getlval()->getNode();
  if (defined != nullptr) {
    auto [iter, inserted] = info.defs.emplace(
        defined, SinkDef{stmt, containingSeq, index, info.nextOrder++, controls});
    if (!inserted) info.duplicateDefs.insert(defined);
  } else {
    ++info.nextOrder;
  }
  std::set<Node*> refs;
  collectRefs(stmt->tree->getRoot(), refs);
  collectRefs(stmt->tree->getlval(), refs);
  if (defined != nullptr) refs.erase(defined);
  const OPType op = stmt->tree->getRoot()->opType;
  const bool outputUse = op == OP_PRINTF || op == OP_ASSERT || op == OP_EXIT;
  for (Node* ref : refs)
    info.uses[ref].push_back({controls, outputUse, containingSeq, index});
}

StmtNode* sinkTarget(StmtNode* root, const std::vector<SinkControl>& controls) {
  StmtNode* current = root;
  for (const SinkControl& control : controls) {
    if (current == nullptr || current->type != OP_STMT_SEQ) return nullptr;
    StmtNode* found = nullptr;
    for (StmtNode* child : current->child) {
      if (child == control.when) {
        found = child;
        break;
      }
    }
    if (found == nullptr || found->child.size() != 3) return nullptr;
    current = found->child[control.branch];
  }
  return current != nullptr && current->type == OP_STMT_SEQ ? current : nullptr;
}

bool dependencyAvailable(Node* node, const std::vector<SinkControl>& target,
                         const SinkTreeInfo& info) {
  std::set<Node*> dependencies(node->prev.begin(), node->prev.end());
  dependencies.insert(node->depPrev.begin(), node->depPrev.end());
  for (Node* dependency : dependencies) {
    if (dependency == nullptr || dependency->super != node->super) continue;
    auto found = info.defs.find(dependency);
    if (found == info.defs.end()) continue; // input/state source
    // Only move nodes whose dependencies are outside the target region.
    // This avoids changing sibling ordering during the batch rewrite.
    if (found->second.controls.size() >= target.size() ||
        !controlPrefix(found->second.controls, target)) return false;
  }
  return true;
}

bool definitionBeforeWhen(const SinkDef& def, StmtNode* when,
                          const SinkTreeInfo& info) {
  auto parent = info.whenParents.find(when);
  if (parent == info.whenParents.end()) return false;
  // mergeExpTree may wrap a definition in extra SEQ nodes. They are
  // transparent for ordering, but a WHEN boundary must never be crossed.
  StmtNode* sequence = def.seq;
  size_t index = def.index;
  while (sequence != parent->second.first) {
    auto sequenceParent = info.seqParents.find(sequence);
    if (sequenceParent == info.seqParents.end() ||
        sequenceParent->second.first == nullptr ||
        sequenceParent->second.first->type != OP_STMT_SEQ) {
      return false;
    }
    index = sequenceParent->second.second;
    sequence = sequenceParent->second.first;
  }
  return index < parent->second.second;
}

size_t useIndexInTarget(const SinkUse& use, StmtNode* target,
                        size_t targetDepth, const SinkTreeInfo& info) {
  if (use.controls.size() > targetDepth) {
    const StmtNode* when = use.controls[targetDepth].when;
    for (size_t index = 0; index < target->child.size(); ++index) {
      if (target->child[index] == when) return index;
    }
    return target->child.size();
  }

  StmtNode* seq = use.seq;
  size_t index = use.index;
  while (seq != target) {
    auto parent = info.seqParents.find(seq);
    if (parent == info.seqParents.end() || parent->second.first == nullptr) {
      return target->child.size();
    }
    index = parent->second.second;
    seq = parent->second.first;
  }
  return index;
}

int collectSinkMoves(StmtTree* tree, SinkTreeInfo& info,
                     const std::set<Node*>& blockedNodes,
                     std::vector<SinkMove>& moves) {
  for (auto& entry : info.defs) {
    Node* node = entry.first;
    SinkDef& def = entry.second;
    if (blockedNodes.count(node) != 0) continue;
    if (info.duplicateDefs.count(node) != 0 || node->type != NODE_OTHERS ||
        !node->isLocal() || node->isArray() || node->assignTree.size() != 1 ||
        !pureSinkTree(node->assignTree.front()->getRoot())) continue;
    if (node->super == nullptr) continue;
    for (Node* successor : node->next) {
      if (successor != nullptr && successor->super != node->super) {
        def.stmt = nullptr;
        break;
      }
    }
    if (def.stmt == nullptr) continue;
    auto uses = info.uses.find(node);
    if (uses == info.uses.end() || uses->second.empty()) continue;
    if (!std::all_of(uses->second.begin(), uses->second.end(),
                     [](const SinkUse& use) { return use.outputUse; })) continue;
    const size_t commonDepth = commonControlDepth(uses->second);
    if (commonDepth <= def.controls.size()) continue;
    std::vector<SinkControl> targetControls(
        uses->second.front().controls.begin(),
        uses->second.front().controls.begin() + static_cast<std::ptrdiff_t>(commonDepth));
    if (!controlPrefix(def.controls, targetControls) ||
        !dependencyAvailable(node, targetControls, info)) continue;
    if (!definitionBeforeWhen(def, targetControls[def.controls.size()].when, info)) continue;
    StmtNode* target = sinkTarget(tree->root, targetControls);
    if (target == nullptr || target == def.seq) continue;
    size_t targetIndex = target->child.size();
    for (const SinkUse& use : uses->second) {
      targetIndex = std::min(targetIndex,
                             useIndexInTarget(use, target, targetControls.size(), info));
    }
    moves.push_back({def.stmt, def.seq, target, def.index, def.order, targetIndex});
  }
  return static_cast<int>(moves.size());
}

} // namespace

int StmtTree::sinkLocalComputations(const std::set<Node*>& blockedNodes) {
  if (!hasWhen || !hasSinkCandidate) return 0;
  SinkTreeInfo info;
  collectSinkTree(root, nullptr, 0, {}, info);
  std::vector<SinkMove> moves;
  const int moved = collectSinkMoves(this, info, blockedNodes, moves);
  if (moved == 0) return 0;

  // Remove from each source sequence backwards so the collected indices stay
  // valid while multiple definitions are moved from the same sequence.
  std::vector<SinkMove*> removalOrder;
  removalOrder.reserve(moves.size());
  for (SinkMove& move : moves) removalOrder.push_back(&move);
  std::sort(removalOrder.begin(), removalOrder.end(), [](const SinkMove* left,
                                                         const SinkMove* right) {
    if (left->seq != right->seq) return left->seq < right->seq;
    return left->index > right->index;
  });
  for (SinkMove* move : removalOrder) {
    Assert(move->index < move->seq->child.size(), "invalid sink definition index");
    Assert(move->stmt->tree != nullptr && move->stmt->tree->getlval() != nullptr,
           "sink definition has no expression tree");
    move->seq->child.erase(move->seq->child.begin() + static_cast<std::ptrdiff_t>(move->index));
  }

  // Insert in original statement order. This preserves the ordering of pure
  // definitions that share the same target region.
  std::sort(moves.begin(), moves.end(), [](const SinkMove& left, const SinkMove& right) {
    return left.order < right.order;
  });
  std::map<StmtNode*, size_t> inserted;
  for (SinkMove& move : moves) {
    size_t removedBefore = 0;
    for (const SinkMove& other : moves) {
      if (other.seq == move.target && other.index < move.targetIndex) ++removedBefore;
    }
    const size_t base = move.targetIndex >= removedBefore
        ? move.targetIndex - removedBefore : 0;
    const size_t offset = base + inserted[move.target]++;
    move.target->child.insert(move.target->child.begin() +
                                  static_cast<std::ptrdiff_t>(offset),
                              move.stmt);
    // Local values are displayed after normal evaluation under ENABLE_LOG.
    // Preserve that diagnostic value at the old location without paying for
    // the duplicate computation in production builds.
    sinkDebugStmts.push_back(new StmtNode(
        new ExpTree(move.stmt->tree->getRoot()->dup(), move.stmt->tree->getlval()->dup()),
        move.stmt->belong));
  }
  return moved;
}

/**
 * @brief for all the depPrev of a node, calculates the longest(max) path
 *
 * Traverses all dependencies of a node in the same superNode and calculates the longest path,
 * storing the result in `prevPath`.
 * A path is the dependency relation inside a StmtTree. Represented as std::vector<int>, each integer is 0-based index
 * of the child of a OP_STMT_SEQ at a specific depth level in the StmtTree.
 * The root node of a StmtTree, that is a OP_STMT_SEQ, is the depth 0, and it's children OP_STMT_SEQ are depth 1, etc.
 * If there are multiple path at a given depth (branchs intrudoced by OP_STMT_WHEN), the path with greatest index is
 * taken. This could generate false postive dependency at many situation but never false negative
 * dependency.
 *
 * @param node The node to analyze
 * @param[out] prevPath Path information of the depPrev of the node
 * @param allPath A map of all paths. The path of depPrev(s) are read from it.
 *
 */
void prevOrderPath(Node* node, std::vector<int>& prevPath, std::map<Node*, std::vector<int>>& allPath) {
  if (node->depPrev.size() == 0) return;
  for (Node* prev : node->depPrev) {
    if (prev->super != node->super) continue;
    Assert(allPath.find(prev) != allPath.end(), "path of %s not exits", prev->name.c_str());
    /* merge prevPath */
    for (size_t i = 0; i < allPath[prev].size(); i ++) {
      if (i >= prevPath.size()) prevPath.push_back(allPath[prev][i]); // Path of given depth doesn't exist -> insert
      else if (prevPath[i] <= allPath[prev][i]) prevPath[i] = allPath[prev][i]; // A longer path of given depth was found -> replace
      else break;
    }
  }
}

void SuperNode::reorderMember() {
  std::vector<Node*> newMember;
  std::map<Node*, int> nodePrev;
  for (Node* node : member) {
    nodePrev[node] = 0;
    for (Node* prev : node->depPrev) {
      if (prev->super == this) nodePrev[node] ++;
    }
  }
  std::stack<Node*> s;
  for (auto iter : nodePrev) {
    if (iter.second == 0) {
      newMember.push_back(iter.first);
      s.push(iter.first);
    }
  }
  while (!s.empty()) {
    Node* node = s.top();
    s.pop();
    for (Node* next : node->depNext) {
      if (next->super == this) {
        nodePrev[next] --;
        if (nodePrev[next] == 0) {
          newMember.push_back(next);
          s.push(next);
        }
      }
    }
  }
  Assert(newMember.size() == member.size(), "invalid size %ld %ld\n", newMember.size(), member.size());
  member = newMember;
}

void graph::generateStmtTree() {
  orderAllNodes();
  std::map<Node*, std::vector<int>> allPath; // order in seq
  size_t sinkTreeCount = 0;
  size_t sinkMovedCount = 0;
  /* add when path for nodes with len(next in same SN) == 1 */
  for (int superIdx = sortedSuper.size() - 1; superIdx >= 0; superIdx --) {
    SuperNode* super = sortedSuper[superIdx];
    if (super->superType != SUPER_VALID) continue;
    for (int nodeIdx = super->member.size() - 1; nodeIdx >= 0; nodeIdx --) {
      Node* member = super->member[nodeIdx];
      int depNextInSuper = 0;
      for (Node* next : member->depNext) {
        if (next->super == super) depNextInSuper ++;
      }
      if (depNextInSuper != 1 || member->next.size() != 1) continue; // TODO: change to "no loop in SuperNode"
      bool anyExtNext = false;
      for (Node* next : member->next) {
        if (next->super != super) {
          anyExtNext = true;
          break;
        }
      }
      if (anyExtNext) continue;
      /* update connection */
      member->updateConnect();
    }
  }
  connectDep();
  /* reorder nodes in superNode */
  for (SuperNode* super : sortedSuper) {
    super->reorderMember();
  }

  // Placement is decided with partition-wide visibility. A value consumed by
  // another SuperNode must remain at its SuperNode boundary; otherwise the
  // consumer can observe a value that is only computed inside a conditional
  // branch of the producer.
  std::set<Node*> blockedSinkNodes;
  if (globalConfig.RepCutMode) {
    for (SuperNode* super : sortedSuper) {
      for (Node* node : super->member) {
        for (Node* next : node->next) {
          if (next != nullptr && next->super != super) blockedSinkNodes.insert(node);
        }
        for (Node* next : node->depNext) {
          if (next != nullptr && next->super != super) blockedSinkNodes.insert(node);
        }
      }
    }
  }

  for (SuperNode* super : sortedSuper) {
    super->stmtTree = new StmtTree();
    super->stmtTree->root = new StmtNode(OP_STMT_SEQ);
    for (Node* node : super->member) {
      std::vector<int> prevPath; // The max path within all depPrev node within the same supernode
      std::vector<int> nodePath; // For node with multiple assignTree, the path of lastest visited tree
      prevOrderPath(node, prevPath, allPath);
      for (ExpTree* tree : node->assignTree) {
        super->stmtTree->mergeExpTree(tree, prevPath, nodePath, node);
        /* update reg_dst when asyreset arrives */
        if (node->type == NODE_REG_SRC && node->reset == ASYRESET && node->regSplit && node->getDst()->status == VALID_NODE) {
          if (tree->getRoot()->nodePtr && tree->getRoot()->nodePtr == node->getDst() && tree->getRoot()->getChildNum() == 0) {

          } else {
            ENode* lvalue = tree->getlval()->dup();
            lvalue->nodePtr = node->getDst();
            ExpTree* dstTree = new ExpTree(tree->getRoot(), lvalue);
            super->stmtTree->mergeExpTree(dstTree, prevPath, nodePath, nullptr);
          }
        }
      }
      allPath[node] = nodePath;
    }
    if (globalConfig.RepCutMode) {
      const int moved = super->stmtTree->sinkLocalComputations(blockedSinkNodes);
      ++sinkTreeCount;
      sinkMovedCount += moved;
    }
  }
  if (globalConfig.RepCutMode) {
    fprintf(stderr, "[StmtTree] sink trees=%zu moved=%zu\n", sinkTreeCount, sinkMovedCount);
  }
  for (SuperNode* super : allReset) {
    super->stmtTree = new StmtTree();
    super->stmtTree->root = new StmtNode(OP_STMT_SEQ);
    for (Node* node : super->member) {
      for (ExpTree* tree : node->assignTree) {
        std::vector<int> emptyPath;
        super->stmtTree->mergeExpTree(tree, emptyPath, emptyPath, node);
      }
    }
  }
}

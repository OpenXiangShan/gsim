/**
 * @file flattenNodes.cpp
 * @brief Flatten expression trees so that every node holds at most one
 *        compute-semantics enode.
 *
 * A compute enode is any internal operator enode (arithmetic, logic, mux,
 * ...). Ref enodes (nodePtr != nullptr) and constant/invalid leaves are kept
 * in place to express connectivity. OP_WHEN enodes are treated as
 * conditional-assignment skeleton (control, not compute) and always stay in
 * the current owner node; only the value/cond expressions below them are
 * extracted. OP_GROUP (aggregate gather) and OP_INDEX / OP_INDEX_INT (array
 * index selectors inside ref enodes) are likewise kept in place since they
 * are not standalone scalar values.
 *
 * Every extracted compute enode is moved into a fresh NODE_OTHERS node that
 * carries exactly that enode as its single-tree assignTree; the original
 * child slot is replaced by a bare ref enode to the new node. Each new node
 * gets its own SuperNode so the unchanged graphPartition pipeline sees a
 * finer-grained graph.
 *
 * The pass runs right before graphPartition (after the last removeDeadNodes),
 * i.e. after all width/usedBit/constant analysis. It therefore fills in
 * width/sign/usedBit itself and rebuilds all connectivity at the end.
 */

#include "common.h"
#include <map>
#include <stack>

namespace {

struct FlattenStats {
  size_t candidateNodes = 0;
  size_t flattenedNodes = 0;
  size_t newNodes = 0;
  size_t keptWhenEnodes = 0;
  size_t leafEnodes = 0;
  std::map<int, size_t> opHistogram;
};

/* leaves are never extracted: ref enodes express connectivity (their array
   index children, if any, stay opaque), constants/invalids are pure values */
bool isFlattenLeaf(ENode* enode) {
  if (enode->getNode()) return true;
  switch (enode->opType) {
    case OP_EMPTY:
    case OP_INT:
    case OP_INVALID:
      return true;
    default:
      return false;
  }
}

/* only plain value-holding nodes are flattened; registers keep their
   src/dst pairing and reset trees, memory ports / special / ext nodes keep
   their structural enode anchors (see instsGenerator owner-type asserts) */
bool isFlattenTarget(Node* node) {
  if (!node || node->status != VALID_NODE) return false;
  switch (node->type) {
    case NODE_OTHERS:
    case NODE_OUT:
    case NODE_REG_DST:
      break;
    default:
      return false;
  }
  if (node->isArray()) return false;
  return true;
}

} // namespace

/* count compute-semantics enodes under the same rules as the pass itself:
   refs are opaque (but their OP_INDEX children stay reachable), OP_GROUP is
   opaque, OP_WHEN / OP_INDEX / OP_INDEX_INT are control/index (not compute) */
static void countComputeEnodes(ENode* root, size_t& compute) {
  if (!root) return;
  std::stack<ENode*> work;
  work.push(root);
  while (!work.empty()) {
    ENode* cur = work.top();
    work.pop();
    if (cur->getNode()) {
      for (ENode* child : cur->child) {
        if (child && (child->opType == OP_INDEX || child->opType == OP_INDEX_INT)) work.push(child);
      }
      continue;
    }
    switch (cur->opType) {
      case OP_EMPTY:
      case OP_INT:
      case OP_INVALID:
      case OP_GROUP:
        continue;
      case OP_WHEN:
      case OP_INDEX:
      case OP_INDEX_INT:
        break;
      default:
        compute ++;
        break;
    }
    for (ENode* child : cur->child) {
      if (child) work.push(child);
    }
  }
}

void graph::flattenNodes() {
  FlattenStats stats;
  std::vector<SuperNode*> newSupers;
  std::set<ENode*> visited;
  uint64_t nameCounter = 0;

  std::vector<Node*> workNodes;
  for (SuperNode* super : sortedSuper) {
    for (Node* node : super->member) {
      if (isFlattenTarget(node)) workNodes.push_back(node);
    }
  }

  /* pre-flatten: per-node compute-enode distribution (verification) */
  size_t preHist[6] = {0, 0, 0, 0, 0, 0}; /* 0, 1, 2, 3-5, 6-10, >10 */
  size_t preComputeTotal = 0;
  for (Node* node : workNodes) {
    size_t compute = 0;
    for (ExpTree* tree : node->assignTree) countComputeEnodes(tree ? tree->getRoot() : nullptr, compute);
    preComputeTotal += compute;
    if (compute == 0) preHist[0] ++;
    else if (compute == 1) preHist[1] ++;
    else if (compute == 2) preHist[2] ++;
    else if (compute <= 5) preHist[3] ++;
    else if (compute <= 10) preHist[4] ++;
    else preHist[5] ++;
  }
  printf("[flattenNodes] pre-flatten candidate %ld nodes, compute-enode distribution: 0:%ld 1:%ld 2:%ld 3-5:%ld 6-10:%ld >10:%ld (total compute %ld)\n",
         workNodes.size(), preHist[0], preHist[1], preHist[2], preHist[3], preHist[4], preHist[5], preComputeTotal);

  for (Node* node : workNodes) {
    stats.candidateNodes ++;
    bool flattened = false;
    for (ExpTree* tree : node->assignTree) {
      if (!tree || !tree->getRoot()) continue;
      std::stack<std::pair<ENode*, Node*>> work;
      work.push(std::make_pair(tree->getRoot(), node));
      while (!work.empty()) {
        ENode* cur = work.top().first;
        Node* owner = work.top().second;
        work.pop();
        Assert(visited.insert(cur).second, "flattenNodes: enode %d is shared between trees", cur->id);
        bool curIsRef = cur->getNode() != nullptr;
        for (size_t idx = 0; idx < cur->getChildNum(); idx ++) {
          ENode* child = cur->getChild(idx);
          if (!child) continue;
          if (curIsRef) {
            /* ref enodes are opaque connections; only their array-index
               enodes stay reachable so index expressions can be flattened */
            if (child->opType != OP_INDEX && child->opType != OP_INDEX_INT) continue;
            work.push(std::make_pair(child, owner));
            continue;
          }
          if (isFlattenLeaf(child)) {
            stats.leafEnodes ++;
            continue;
          }
          if (child->opType == OP_GROUP) {
            /* aggregate gather stays untouched (array-typed value) */
            stats.leafEnodes ++;
            continue;
          }
          if (child->opType == OP_WHEN) {
            /* conditional-assignment skeleton stays in the owner node */
            stats.keptWhenEnodes ++;
            work.push(std::make_pair(child, owner));
            continue;
          }
          if (child->opType == OP_INDEX || child->opType == OP_INDEX_INT) {
            /* index selector is not a standalone value: keep, recurse below */
            work.push(std::make_pair(child, owner));
            continue;
          }
          /* move the compute enode into a fresh node, leave a ref at the use site */
          Node* tmp = new Node(NODE_OTHERS);
          tmp->name = owner->name + "$flat" + std::to_string(nameCounter ++);
          tmp->width = child->width;
          tmp->sign = child->sign;
          tmp->usedBit = child->usedBit >= 0 ? child->usedBit : child->width;
          tmp->isClock = child->isClock;
          tmp->reset = child->reset;
          tmp->lineno = owner->lineno;
          SuperNode* super = new SuperNode(tmp);
          tmp->set_super(super);
          tmp->assignTree.push_back(new ExpTree(child, tmp));
          tmp->assignTree.back()->getlval()->setWidth(child->width, child->sign);
          newSupers.push_back(super);

          ENode* ref = new ENode(tmp);
          ref->width = child->width;
          ref->sign = child->sign;
          ref->usedBit = child->usedBit;
          ref->isClock = child->isClock;
          ref->reset = child->reset;
          cur->setChild(idx, ref);

          stats.newNodes ++;
          stats.opHistogram[child->opType] ++;
          flattened = true;
          work.push(std::make_pair(child, tmp));
        }
      }
    }
    if (flattened) stats.flattenedNodes ++;
  }

  for (SuperNode* super : newSupers) sortedSuper.push_back(super);
  reconnectAll();
  resort();

  /* children were rewired: drop any stale constant-analysis valInfo cache */
  for (SuperNode* super : sortedSuper) {
    for (Node* node : super->member) {
      for (ExpTree* tree : node->assignTree) tree->clearInfo();
    }
  }

  printf("[flattenNodes] candidate %ld flattened %ld newNodes %ld keptWhen %ld leaves %ld\n",
         stats.candidateNodes, stats.flattenedNodes, stats.newNodes, stats.keptWhenEnodes, stats.leafEnodes);
  for (auto& entry : stats.opHistogram) {
    printf("[flattenNodes]   moved op %d: %ld\n", entry.first, entry.second);
  }

  /* post-flatten verification: every tree of every candidate node must hold
     at most one compute enode; every new temp node exactly one */
  size_t violations = 0;
  size_t multiTreeNodes = 0;
  for (Node* node : workNodes) {
    if (node->assignTree.size() > 1) multiTreeNodes ++;
    for (ExpTree* tree : node->assignTree) {
      size_t compute = 0;
      countComputeEnodes(tree ? tree->getRoot() : nullptr, compute);
      if (compute > 1) {
        if (violations < 10) printf("[flattenNodes] VERIFY-FAIL node %s tree holds %ld compute enodes\n", node->name.c_str(), compute);
        violations ++;
      }
    }
  }
  size_t newNodeBad = 0;
  for (SuperNode* super : newSupers) {
    size_t compute = 0;
    for (Node* node : super->member) {
      for (ExpTree* tree : node->assignTree) countComputeEnodes(tree ? tree->getRoot() : nullptr, compute);
    }
    if (compute != 1) newNodeBad ++;
  }
  printf("[flattenNodes] post-check: %ld candidate trees with >1 compute enode (%ld multi-tree candidate nodes), %ld new nodes with compute!=1\n",
         violations, multiTreeNodes, newNodeBad);
}

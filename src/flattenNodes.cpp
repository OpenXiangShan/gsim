/**
 * @file flattenNodes.cpp
 * @brief Flatten expression trees so that every node holds at most one
 *        compute-semantics enode.
 *
 * A compute enode is any internal operator enode (arithmetic, logic, mux,
 * ...). Ref enodes (nodePtr != nullptr) and constant/invalid leaves are kept
 * in place to express connectivity. OP_GROUP (aggregate gather) and
 * OP_INDEX / OP_INDEX_INT (array index selectors inside ref enodes) are kept
 * in place since they are not standalone scalar values.
 *
 * OP_WHEN conditional-assignment skeletons are expanded to data-flow muxes
 * before extraction (control -> data conversion), so no when structure
 * survives on flatten targets:
 *   when(cond, then, else)  ->  mux(cond, then, else)
 *   when(cond, then, none)  ->  mux(cond, then, hold)
 * The hold value follows the executable-GRH exporter semantics
 * (ExecutableGrhExporter::lowerAssignedNode):
 *   - REG_DST trees:      hold = reference to the paired NODE_REG_SRC;
 *   - other nodes, first assignTree: hold = zero constant of the site width;
 *   - subsequent assignTrees (source order): hold = previous tree's value.
 * To make the previous-tree value referenceable, a node with multiple
 * assignTrees is first split into a chain of single-tree nodes
 * (NODE_OTHERS "$whold<i>") that feed the original node, which keeps only
 * its last tree.
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
  size_t expandedWhenEnodes = 0;
  size_t holdRefSites = 0;
  size_t holdZeroSites = 0;
  size_t holdChainNodes = 0;
  size_t holdOnlyTrees = 0;
  size_t sizedIntBranches = 0;
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

ENode* makeHoldRef(Node* src) {
  ENode* ref = new ENode(src);
  ref->width = src->width;
  ref->sign = src->sign;
  ref->usedBit = src->usedBit;
  ref->isClock = src->isClock;
  ref->reset = src->reset;
  return ref;
}

ENode* makeHoldZero(const ENode* site, const Node* owner) {
  int width = site->width > 0 ? site->width : owner->width;
  bool sign = site->width > 0 ? site->sign : owner->sign;
  if (width <= 0) width = 1;
  ENode* zero = allocIntEnode(width, "0", sign);
  zero->usedBit = site->usedBit >= 0 ? site->usedBit : width;
  return zero;
}

/* convert every OP_WHEN below root (and root itself) into OP_MUX, filling
   missing/invalid branches with the tree-entry hold value; a bare invalid /
   empty root degenerates to the hold value directly */
void expandWhenEnodes(ExpTree* tree, Node* holdNode, Node* owner, FlattenStats& stats) {
  ENode* root = tree->getRoot();
  const bool degenerateRoot = !root ||
      (!root->getNode() && (root->opType == OP_INVALID || root->opType == OP_EMPTY));
  if (degenerateRoot) {
    if (holdNode) {
      tree->setRoot(makeHoldRef(holdNode));
    } else {
      int width = owner->width > 0 ? owner->width : 1;
      ENode* zero = allocIntEnode(width, "0", owner->sign);
      zero->usedBit = owner->usedBit >= 0 ? owner->usedBit : width;
      tree->setRoot(zero);
    }
    stats.holdOnlyTrees ++;
    return;
  }
  std::stack<ENode*> work;
  work.push(root);
  while (!work.empty()) {
    ENode* cur = work.top();
    work.pop();
    if (!cur || cur->getNode()) continue;
    switch (cur->opType) {
      case OP_EMPTY:
      case OP_INT:
      case OP_INVALID:
      case OP_GROUP:
        continue;
      case OP_WHEN: {
        while (cur->getChildNum() < 3) cur->addChild(nullptr);
        for (size_t idx = 1; idx <= 2; idx ++) {
          ENode* branch = cur->getChild(idx);
          /* a ref enode carries opType OP_EMPTY by default — it is a real
             value, not a missing branch; only non-ref INVALID/EMPTY/null
             branches are hold sites */
          const bool missing = !branch ||
              (!branch->getNode() && (branch->opType == OP_INVALID || branch->opType == OP_EMPTY));
          if (!missing) continue;
          if (holdNode) {
            cur->setChild(idx, makeHoldRef(holdNode));
            stats.holdRefSites ++;
          } else {
            cur->setChild(idx, makeHoldZero(cur, owner));
            stats.holdZeroSites ++;
          }
        }
        /* pre-existing zero-width integer literal children were sized through
           the when's fallback propagation in the exporter; mux children get no
           such fallback context, so size them in place (the literal value is
           width-agnostic) */
        ENode* condChild = cur->getChild(0);
        if (condChild && !condChild->getNode() && condChild->opType == OP_INT && condChild->width == 0) {
          condChild->width = 1;
          if (condChild->usedBit < 0) condChild->usedBit = 1;
        }
        for (size_t idx = 1; idx <= 2; idx ++) {
          ENode* branch = cur->getChild(idx);
          if (!branch || branch->getNode() || branch->opType != OP_INT || branch->width != 0) continue;
          branch->width = cur->width > 0 ? cur->width : (owner->width > 0 ? owner->width : 1);
          branch->sign = cur->sign;
          if (branch->usedBit < 0) branch->usedBit = branch->width;
          stats.sizedIntBranches ++;
        }
        cur->opType = OP_MUX;
        stats.expandedWhenEnodes ++;
        work.push(cur->getChild(0));
        work.push(cur->getChild(1));
        work.push(cur->getChild(2));
        break;
      }
      default:
        for (size_t idx = 0; idx < cur->getChildNum(); idx ++) {
          if (cur->getChild(idx)) work.push(cur->getChild(idx));
        }
        break;
    }
  }
}

/* count compute-semantics enodes under the same rules as the pass itself:
   refs are opaque (but their OP_INDEX children stay reachable), OP_GROUP is
   opaque, OP_INDEX / OP_INDEX_INT are index (not compute); after the when
   expansion no OP_WHEN may remain on flatten targets, so OP_WHEN is counted
   as a violation by the post-check below */
void countComputeEnodes(ENode* root, size_t& compute) {
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

size_t countWhenEnodes(ENode* root) {
  if (!root) return 0;
  size_t ret = 0;
  std::stack<ENode*> work;
  work.push(root);
  while (!work.empty()) {
    ENode* cur = work.top();
    work.pop();
    if (!cur || cur->getNode()) continue;
    if (cur->opType == OP_WHEN) ret ++;
    for (ENode* child : cur->child) {
      if (child) work.push(child);
    }
  }
  return ret;
}

} // namespace

void graph::flattenNodes() {
  FlattenStats stats;
  std::vector<SuperNode*> newSupers;
  std::set<Node*> holdChainNodes;
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
  size_t preWhenTotal = 0;
  for (Node* node : workNodes) {
    size_t compute = 0;
    for (ExpTree* tree : node->assignTree) {
      countComputeEnodes(tree ? tree->getRoot() : nullptr, compute);
      preWhenTotal += countWhenEnodes(tree ? tree->getRoot() : nullptr);
    }
    preComputeTotal += compute;
    if (compute == 0) preHist[0] ++;
    else if (compute == 1) preHist[1] ++;
    else if (compute == 2) preHist[2] ++;
    else if (compute <= 5) preHist[3] ++;
    else if (compute <= 10) preHist[4] ++;
    else preHist[5] ++;
  }
  printf("[flattenNodes] pre-flatten candidate %ld nodes, compute-enode distribution: 0:%ld 1:%ld 2:%ld 3-5:%ld 6-10:%ld >10:%ld (total compute %ld, when skeletons %ld)\n",
         workNodes.size(), preHist[0], preHist[1], preHist[2], preHist[3], preHist[4], preHist[5], preComputeTotal, preWhenTotal);

  for (Node* node : workNodes) {
    stats.candidateNodes ++;

    /* phase 1: when -> mux expansion with exporter-equivalent hold semantics;
       multi-tree nodes are split into a single-tree chain so the previous
       tree value is referenceable as the next tree's hold */
    std::vector<Node*> owners;
    {
      const size_t treeCount = node->assignTree.size();
      std::vector<Node*> chain;
      chain.reserve(treeCount);
      for (size_t i = 0; i < treeCount; i ++) {
        Node* owner = node;
        if (treeCount > 1 && i + 1 < treeCount) {
          owner = new Node(NODE_OTHERS);
          owner->name = node->name + "$whold" + std::to_string(i);
          owner->width = node->width;
          owner->sign = node->sign;
          owner->usedBit = node->usedBit;
          owner->isClock = node->isClock;
          owner->reset = node->reset;
          owner->lineno = node->lineno;
          SuperNode* super = new SuperNode(owner);
          owner->set_super(super);
          newSupers.push_back(super);
          holdChainNodes.insert(owner);
          stats.holdChainNodes ++;
        }
        chain.push_back(owner);
      }
      Node* holdNode = nullptr;
      if (node->type == NODE_REG_DST && node->regNext) holdNode = node->regNext;
      for (size_t i = 0; i < treeCount; i ++) {
        ExpTree* tree = node->assignTree[i];
        Node* owner = chain[i];
        Node* treeHold = (i == 0) ? holdNode : chain[i - 1];
        expandWhenEnodes(tree, treeHold, node, stats);
        if (owner != node) {
          owner->assignTree.push_back(new ExpTree(tree->getRoot(), owner));
          owners.push_back(owner);
        }
      }
      if (treeCount > 1) {
        node->assignTree.erase(node->assignTree.begin(),
                               node->assignTree.begin() + (treeCount - 1));
      }
      owners.push_back(node);
    }

    /* phase 2: extraction walk — move every compute enode below each tree
       root into a fresh single-tree node, leaving a ref at the use site */
    bool flattened = false;
    for (Node* ownerNode : owners) {
      for (ExpTree* tree : ownerNode->assignTree) {
        if (!tree || !tree->getRoot()) continue;
        std::stack<std::pair<ENode*, Node*>> work;
        work.push(std::make_pair(tree->getRoot(), ownerNode));
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
              /* unreachable after the when expansion; keep defensively */
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

  printf("[flattenNodes] candidate %ld flattened %ld newNodes %ld expandedWhen %ld holdRef %ld holdZero %ld holdChain %ld holdOnlyTree %ld sizedInt %ld keptWhen %ld leaves %ld\n",
         stats.candidateNodes, stats.flattenedNodes, stats.newNodes, stats.expandedWhenEnodes,
         stats.holdRefSites, stats.holdZeroSites, stats.holdChainNodes, stats.holdOnlyTrees,
         stats.sizedIntBranches, stats.keptWhenEnodes, stats.leafEnodes);
  for (auto& entry : stats.opHistogram) {
    printf("[flattenNodes]   moved op %d: %ld\n", entry.first, entry.second);
  }

  /* post-flatten verification: every tree of every candidate node must hold
     at most one compute enode; every new temp node exactly one; no when
     skeleton may survive on flatten targets */
  size_t violations = 0;
  size_t multiTreeNodes = 0;
  size_t whenResidue = 0;
  for (Node* node : workNodes) {
    if (node->assignTree.size() > 1) multiTreeNodes ++;
    for (ExpTree* tree : node->assignTree) {
      size_t compute = 0;
      countComputeEnodes(tree ? tree->getRoot() : nullptr, compute);
      whenResidue += countWhenEnodes(tree ? tree->getRoot() : nullptr);
      if (compute > 1) {
        if (violations < 10) printf("[flattenNodes] VERIFY-FAIL node %s tree holds %ld compute enodes\n", node->name.c_str(), compute);
        violations ++;
      }
    }
  }
  size_t newNodeBad = 0;
  for (SuperNode* super : newSupers) {
    if (!super->member.empty() && holdChainNodes.count(super->member.front())) continue;
    /* hold chain nodes legitimately carry 0..1 compute enodes */
    size_t compute = 0;
    for (Node* node : super->member) {
      for (ExpTree* tree : node->assignTree) countComputeEnodes(tree ? tree->getRoot() : nullptr, compute);
    }
    if (compute != 1) newNodeBad ++;
  }
  printf("[flattenNodes] post-check: %ld candidate trees with >1 compute enode (%ld multi-tree candidate nodes), %ld new nodes with compute!=1, %ld when skeletons left on targets\n",
         violations, multiTreeNodes, newNodeBad, whenResidue);
}

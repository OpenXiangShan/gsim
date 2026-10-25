#include "common.h"
#include <cstdio>
#include <queue>
#include <stack>
#include <set>
#include <map>
#include <tuple>
#include <utility>

bool nameExist(std::string str);
ExpTree* mergeWhenTree(ExpTree* tree1, ExpTree* tree2);

static std::set<Node*> fullyVisited;
static std::set<Node*> partialVisited;

static std::map<Node*, std::vector<Node*>> splitArrayMap;
/* Generated members are not inserted into allSignals, so keep a separate
 * name set to prevent members from different arrays colliding with each
 * other.  It is reset at the beginning of each graph split pass. */
static std::set<std::string> splitGeneratedNames;

static std::vector<int> allArrayDims(Node* node) {
  std::vector<int> dims;
  for (size_t i = 0; i < node->dimension.size(); ++i) {
    dims.push_back(static_cast<int>(i));
  }
  return dims;
}

static bool isSplitDim(const std::vector<int>& splitDims, int dim) {
  return std::find(splitDims.begin(), splitDims.end(), dim) != splitDims.end();
}

static size_t splitMemberCount(Node* node, const std::vector<int>& splitDims) {
  size_t count = 1;
  for (int dim : splitDims) {
    Assert(dim >= 0 && dim < static_cast<int>(node->dimension.size()),
           "invalid split dimension %d for %s", dim, node->name.c_str());
    count *= static_cast<size_t>(node->dimension[dim]);
  }
  return count;
}

static std::vector<int> splitMemberCoords(Node* node, const std::vector<int>& splitDims,
                                          size_t memberIndex) {
  std::vector<int> coords(splitDims.size(), 0);
  for (size_t i = splitDims.size(); i-- > 0;) {
    const int dim = splitDims[i];
    coords[i] = static_cast<int>(memberIndex % static_cast<size_t>(node->dimension[dim]));
    memberIndex /= static_cast<size_t>(node->dimension[dim]);
  }
  return coords;
}

static size_t splitMemberIndex(Node* node, const std::vector<int>& splitDims,
                               const std::vector<int>& coords) {
  Assert(splitDims.size() == coords.size(), "split coordinate size mismatch");
  size_t index = 0;
  for (size_t i = 0; i < splitDims.size(); ++i) {
    Assert(coords[i] >= 0 && coords[i] < node->dimension[splitDims[i]],
           "split coordinate out of bounds for %s", node->name.c_str());
    index = index * static_cast<size_t>(node->dimension[splitDims[i]]) +
            static_cast<size_t>(coords[i]);
  }
  return index;
}

static std::string allocateSplitName(const std::string& baseName) {
  std::string memberName = baseName;
  if (nameExist(memberName) || splitGeneratedNames.find(memberName) != splitGeneratedNames.end()) {
    unsigned suffix = 0;
    do {
      memberName = baseName + "_" + std::to_string(suffix++);
    } while (nameExist(memberName) || splitGeneratedNames.find(memberName) != splitGeneratedNames.end());
  }
  splitGeneratedNames.insert(memberName);
  return memberName;
}

static Node* splitArrayMemberNode(Node* node, const std::vector<int>& splitDims,
                                  const std::vector<int>& coords) {
  Assert(splitDims.size() == coords.size(), "split coordinate size mismatch");
  std::string baseName = node->name;
  for (int coord : coords) baseName += "_" + std::to_string(coord);
  std::string memberName = allocateSplitName(baseName);

  Node* member = new Node(node->type);
  member->name = memberName;
  member->clock = node->clock;
  member->reset = node->reset;
  member->setType(node->width, node->sign);
  member->constructSuperNode();
  member->lineno = node->lineno;
  for (size_t dim = 0; dim < node->dimension.size(); ++dim) {
    if (!isSplitDim(splitDims, static_cast<int>(dim))) {
      member->dimension.push_back(node->dimension[dim]);
    }
  }
  return member;
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
      ENode* select = nullptr;
      if (groupIdx >= 0 && groupIdx < static_cast<int>(top_enode->getChildNum())) {
        select = top_enode->getChild(groupIdx);
      } else {
        select = new ENode(OP_INVALID);
      }
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

Node* getSplitArray(graph* g) {
  /* array points to itself */
  for (Node* node : partialVisited) {
    if (node->isArray() && node->next.find(node) != node->next.end()) return node; // point to self directly
  }

  for (Node* node : partialVisited) {
    if (node->isArray() && point2self(node)) return node;
  }

  for (Node* node : g->halfConstantArray) {
    if (fullyVisited.find(node) != fullyVisited.end() || splitArrayMap.find(node) != splitArrayMap.end()) continue;
    if (point2self(node)) return node;
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

static ENode* buildSplitSlice(ENode* ref, Node* array, std::vector<Node*>& arrayMember,
                                const std::vector<int>& splitDims) {
  const size_t prefix = ref->getChildNum();
  std::vector<int> omittedSizes;
  for (size_t dim = prefix; dim < array->dimension.size(); ++dim) {
    omittedSizes.push_back(array->dimension[dim]);
  }
  size_t count = 1;
  for (int size : omittedSizes) {
    Assert(size > 0, "invalid omitted dimension of %s", array->name.c_str());
    Assert(count <= SIZE_MAX / static_cast<size_t>(size), "split slice of %s is too large",
           array->name.c_str());
    count *= static_cast<size_t>(size);
  }

  auto leafAt = [&](size_t flat) {
    std::vector<int> suffix(omittedSizes.size(), 0);
    size_t value = flat;
    for (size_t i = omittedSizes.size(); i-- > 0;) {
      suffix[i] = static_cast<int>(value % static_cast<size_t>(omittedSizes[i]));
      value /= static_cast<size_t>(omittedSizes[i]);
    }
    std::vector<int> splitCoords;
    splitCoords.reserve(splitDims.size());
    for (int dim : splitDims) {
      if (dim < static_cast<int>(prefix)) {
        ENode* index = ref->getChild(dim);
        Assert(index && index->opType == OP_INDEX_INT && index->values.size() == 1,
               "split index of %s is not constant", array->name.c_str());
        splitCoords.push_back(index->values[0]);
      } else {
        splitCoords.push_back(suffix[static_cast<size_t>(dim) - prefix]);
      }
    }
    Node* member = arrayMember[splitMemberIndex(array, splitDims, splitCoords)];
    ENode* leaf = new ENode(member);
    leaf->width = member->width;
    leaf->sign = member->sign;
    for (size_t dim = 0; dim < array->dimension.size(); ++dim) {
      if (isSplitDim(splitDims, static_cast<int>(dim))) continue;
      if (dim < prefix) leaf->addChild(ref->getChild(dim)->dup());
      else {
        ENode* index = new ENode(OP_INDEX_INT);
        index->addVal(suffix[dim - prefix]);
        leaf->addChild(index);
      }
    }
    return leaf;
  };

  if (count == 1) return leafAt(0);
  ENode* group = new ENode(OP_GROUP);
  group->width = ref->width;
  group->sign = array->sign;
  for (size_t flat = 0; flat < count; ++flat) group->addChild(leafAt(flat));
  return group;
}

static bool constantSplitCoords(ENode* ref, Node* array, const std::vector<int>& splitDims,
                                 std::vector<int>& coords) {
  if (!ref || ref->nodePtr != array) return false;
  Assert(ref->getChildNum() <= array->dimension.size(),
         "too many indices for array %s", array->name.c_str());
  coords.clear();
  coords.reserve(splitDims.size());
  for (int dim : splitDims) {
    if (dim >= static_cast<int>(ref->getChildNum())) return false;
    ENode* index = ref->getChild(dim);
    if (!index || index->opType != OP_INDEX_INT || index->values.size() != 1) return false;
    coords.push_back(index->values[0]);
  }
  return true;
}

static bool rewriteFixedSplitReference(ENode* enode, Node* array,
                                       std::vector<Node*>& arrayMember,
                                       const std::vector<int>& splitDims) {
  std::vector<int> coords;
  if (!constantSplitCoords(enode, array, splitDims, coords)) return false;
  const size_t memberIndex = splitMemberIndex(array, splitDims, coords);
  Assert(memberIndex < arrayMember.size(), "split member index out of bounds for %s",
         array->name.c_str());
  std::vector<ENode*> remaining;
  remaining.reserve(enode->getChildNum());
  for (size_t dim = 0; dim < enode->getChildNum(); ++dim) {
    if (!isSplitDim(splitDims, static_cast<int>(dim))) remaining.push_back(enode->getChild(dim));
  }
  enode->nodePtr = arrayMember[memberIndex];
  enode->child = std::move(remaining);
  return true;
}

void ExpTree::updateWithSplittedArray(Node* node, Node* array, std::vector<Node*>& arrayMember,
                                      const std::vector<int>& splitDims) {
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
    if (!splitDims.empty() && rewriteFixedSplitReference(top_enode, array, arrayMember, splitDims)) {
      /* A partially split array keeps the non-split index expressions. */
      for (size_t i = 0; i < top_enode->getChildNum(); ++i) {
        if (top_enode->getChild(i)) s.push(std::make_tuple(top_enode->getChild(i), top_enode, i));
      }
      continue;
    }

    bool splitVariable = false;
    bool splitOmitted = false;
    for (int dim : splitDims) {
      if (dim >= static_cast<int>(top_enode->getChildNum())) {
        splitOmitted = true;
        continue;
      }
      ENode* index = top_enode->getChild(dim);
      if (!index || index->opType != OP_INDEX_INT || index->values.size() != 1) splitVariable = true;
    }
    if (splitVariable) {
      /* Preserve the old dynamic-index lowering for a fully split 1-D array. */
      if (splitDims.size() != array->dimension.size() || array->dimension.size() > 1) {
        Assert(splitDims.size() != array->dimension.size(),
               "dynamic index cannot be remapped after splitting %s", array->name.c_str());
        TODO();
      }
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
      if (top_parent) top_parent->setChild(top_idx, enode);
      else setRoot(enode);
      continue;
    }
    if (splitOmitted) {
      /* Distribution must already have given every split dimension a literal
       * index.  A remaining bulk reference is a value, so replace it with the
       * flat slice in the original row-major order. */
      Assert(top_idx >= 0, "lvalue of %s omits a split dimension", array->name.c_str());
      ENode* slice = buildSplitSlice(top_enode, array, arrayMember, splitDims);
      if (top_parent) top_parent->setChild(top_idx, slice);
      else setRoot(slice);
      s.push(std::make_tuple(slice, top_parent, top_idx));
      continue;
    }
    Assert(0, "reference to %s was not remapped", array->name.c_str());
  }
}

static bool fixedSplitMember(ENode* ref, Node* array, const std::vector<int>& splitDims,
                             size_t& memberIndex) {
  std::vector<int> coords;
  if (!constantSplitCoords(ref, array, splitDims, coords)) return false;
  memberIndex = splitMemberIndex(array, splitDims, coords);
  return true;
}

static bool remainingShapeMatches(ENode* ref, const std::vector<int>& omittedSizes) {
  if (!ref || !ref->nodePtr || !ref->nodePtr->isArray()) return false;
  const size_t rank = ref->nodePtr->dimension.size();
  if (ref->getChildNum() > rank || rank - ref->getChildNum() != omittedSizes.size()) return false;
  for (size_t i = 0; i < omittedSizes.size(); ++i) {
    if (ref->nodePtr->dimension[ref->getChildNum() + i] != omittedSizes[i]) return false;
  }
  return true;
}

static void rewriteSuffix(ExpTree* tree, ENode* enode, ENode* parent, int parentIdx,
                          const std::vector<int>& omittedSizes, const std::vector<int>& suffix,
                          size_t product, int flat, bool isLvalue) {
  if (!enode) return;
  if (enode->opType == OP_GROUP && product != 0 && enode->getChildNum() == product) {
    ENode* selected = nullptr;
    if (flat >= 0 && flat < static_cast<int>(enode->getChildNum())) selected = enode->getChild(flat);
    else selected = new ENode(OP_INVALID);
    if (parent) parent->setChild(parentIdx, selected);
    else if (isLvalue) tree->setlval(selected);
    else tree->setRoot(selected);
    rewriteSuffix(tree, selected, parent, parentIdx, omittedSizes, suffix, product, flat, isLvalue);
    return;
  }
  const size_t childCount = enode->getChildNum();
  for (size_t i = 0; i < childCount; ++i) {
    if (enode->getChild(i)) {
      rewriteSuffix(tree, enode->getChild(i), enode, static_cast<int>(i),
                    omittedSizes, suffix, product, flat, false);
    }
  }
  if (remainingShapeMatches(enode, omittedSizes)) {
    for (int coord : suffix) {
      ENode* index = new ENode(OP_INDEX_INT);
      index->addVal(coord);
      enode->addChild(index);
    }
  }
}

static ExpTree* dupTreeWithSuffix(ExpTree* tree, const std::vector<int>& omittedSizes,
                                  const std::vector<int>& suffix) {
  ExpTree* copy = tree->dup();
  size_t product = 1;
  int flat = 0;
  for (size_t i = 0; i < omittedSizes.size(); ++i) {
    product *= static_cast<size_t>(omittedSizes[i]);
    flat = flat * omittedSizes[i] + suffix[i];
  }
  rewriteSuffix(copy, copy->getlval(), nullptr, -1, omittedSizes, suffix, product, flat, true);
  rewriteSuffix(copy, copy->getRoot(), nullptr, -1, omittedSizes, suffix, product, flat, false);
  return copy;
}

static void placeMemberTree(Node* member, ExpTree* tree, Node* oldArray) {
  if (member->assignTree.empty()) {
    member->assignTree.push_back(tree);
    return;
  }
  /* Variable indices do not identify one element, so keep those trees apart.
   * Only identical constant ranges can share the existing when-merge. */
  int begin, end, previousBegin, previousEnd;
  std::tie(begin, end) = tree->getlval()->getIdx(oldArray);
  std::tie(previousBegin, previousEnd) = member->assignTree.back()->getlval()->getIdx(oldArray);
  if (begin >= 0 && begin == previousBegin && end == previousEnd) {
    ExpTree* replaceTree = mergeWhenTree(member->assignTree.back(), tree);
    if (replaceTree) {
      member->assignTree.back() = replaceTree;
      return;
    }
  }
  member->assignTree.push_back(tree);
}

static ENode* buildArraySlice(Node* source, const std::vector<int>& splitDims,
                              const std::vector<int>& coords) {
  std::vector<int> keptDims;
  size_t keptCount = 1;
  for (size_t dim = 0; dim < source->dimension.size(); ++dim) {
    if (!isSplitDim(splitDims, static_cast<int>(dim))) {
      keptDims.push_back(static_cast<int>(dim));
      keptCount *= static_cast<size_t>(source->dimension[dim]);
    }
  }
  ENode* group = new ENode(OP_GROUP);
  group->width = source->width;
  group->sign = source->sign;
  for (size_t flat = 0; flat < keptCount; ++flat) {
    std::vector<int> fullCoords(source->dimension.size(), 0);
    for (size_t i = 0; i < splitDims.size(); ++i) fullCoords[splitDims[i]] = coords[i];
    size_t value = flat;
    for (size_t i = keptDims.size(); i-- > 0;) {
      const int dim = keptDims[i];
      fullCoords[dim] = static_cast<int>(value % static_cast<size_t>(source->dimension[dim]));
      value /= static_cast<size_t>(source->dimension[dim]);
    }
    ENode* leaf = new ENode(source);
    leaf->width = source->width;
    leaf->sign = source->sign;
    for (int coord : fullCoords) {
      ENode* index = new ENode(OP_INDEX_INT);
      index->addVal(coord);
      leaf->addChild(index);
    }
    group->addChild(leaf);
  }
  return group;
}

static bool isWholeRegisterUpdate(Node* node, ExpTree* tree) {
  return node->type == NODE_REG_SRC && node->getDst() != nullptr &&
         tree->getlval() && tree->getlval()->nodePtr == node &&
         tree->getlval()->getChildNum() == 0 && tree->getRoot() &&
         tree->getRoot()->nodePtr == node->getDst() &&
         tree->getRoot()->getChildNum() == 0;
}

static ExpTree* duplicateRegisterUpdateForMember(Node* node, ExpTree* tree, Node* member,
                                                  const std::vector<int>& splitDims,
                                                  const std::vector<int>& coords) {
  ExpTree* copy = tree->dup();
  copy->getlval()->nodePtr = member;
  copy->getlval()->child.clear();
  /* A register source has exactly one update tree, even when kept dimensions
   * remain.  Element trees would make constant analysis reject the register. */
  copy->setRoot(buildArraySlice(node->getDst(), splitDims, coords));
  return copy;
}

static void distributePartiallySplitTree(Node* node, ExpTree* tree,
                                          std::vector<Node*>& arrayMember,
                                          const std::vector<int>& splitDims) {
  if (isWholeRegisterUpdate(node, tree)) {
    for (size_t i = 0; i < arrayMember.size(); ++i) {
      placeMemberTree(arrayMember[i], duplicateRegisterUpdateForMember(
          node, tree, arrayMember[i], splitDims, splitMemberCoords(node, splitDims, i)), node);
    }
    return;
  }
  size_t memberIndex = 0;
  if (fixedSplitMember(tree->getlval(), node, splitDims, memberIndex)) {
    Assert(memberIndex < arrayMember.size(), "split member index out of bounds for %s",
           node->name.c_str());
    placeMemberTree(arrayMember[memberIndex], tree, node);
    return;
  }

  ENode* lvalue = tree->getlval();
  Assert(lvalue && lvalue->nodePtr == node,
         "assignment to %s does not fix every split dimension", node->name.c_str());
  const size_t prefix = lvalue->getChildNum();
  Assert(prefix < node->dimension.size(),
         "assignment to %s does not fix every split dimension", node->name.c_str());
  std::vector<int> omittedSizes;
  bool omitsSplit = false;
  for (size_t dim = prefix; dim < node->dimension.size(); ++dim) {
    omittedSizes.push_back(node->dimension[dim]);
    if (isSplitDim(splitDims, static_cast<int>(dim))) omitsSplit = true;
  }
  Assert(omitsSplit, "assignment to %s does not fix every split dimension", node->name.c_str());

  size_t count = 1;
  for (int size : omittedSizes) {
    Assert(size > 0 && count <= SIZE_MAX / static_cast<size_t>(size),
           "assignment slice of %s is too large", node->name.c_str());
    count *= static_cast<size_t>(size);
  }
  for (size_t flat = 0; flat < count; ++flat) {
    std::vector<int> suffix(omittedSizes.size(), 0);
    size_t value = flat;
    for (size_t i = omittedSizes.size(); i-- > 0;) {
      suffix[i] = static_cast<int>(value % static_cast<size_t>(omittedSizes[i]));
      value /= static_cast<size_t>(omittedSizes[i]);
    }
    ExpTree* copy = dupTreeWithSuffix(tree, omittedSizes, suffix);
    Assert(fixedSplitMember(copy->getlval(), node, splitDims, memberIndex),
           "expanded assignment to %s does not fix every split dimension", node->name.c_str());
    placeMemberTree(arrayMember[memberIndex], copy, node);
  }
}

static ExpTree* duplicateResetForMember(ExpTree* resetTree, Node* oldNode, Node* member,
                                        const std::vector<int>& splitDims,
                                        const std::vector<int>& coords) {
  ExpTree* copy = resetTree->dup();
  Assert(copy->getlval() && copy->getlval()->nodePtr == oldNode,
         "reset lvalue does not refer to %s", oldNode->name.c_str());
  copy->getlval()->nodePtr = member;
  copy->getlval()->child.clear();
  if (copy->getRoot()->opType == OP_RESET && copy->getRoot()->getChildNum() >= 2) {
    ENode* value = copy->getRoot()->getChild(1);
    bool sameShape = value && value->nodePtr &&
                     value->nodePtr->dimension.size() == oldNode->dimension.size();
    if (sameShape) {
      for (size_t dim = 0; dim < oldNode->dimension.size(); ++dim) {
        if (value->nodePtr->dimension[dim] != oldNode->dimension[dim]) {
          sameShape = false;
          break;
        }
      }
    }
    if (value && value->nodePtr && value->nodePtr->isArray() &&
        value->getChildNum() == 0 && sameShape) {
      copy->getRoot()->setChild(1, buildArraySlice(value->nodePtr, splitDims, coords));
    }
  }
  return copy;
}

void graph::splitArrayNode(Node* node) {
  splitArrayNode(node, allArrayDims(node));
}

void graph::splitArrayNode(Node* node, const std::vector<int>& splitDims) {
  Assert(!splitDims.empty(), "no dimensions selected for array %s", node->name.c_str());
  Assert(node->isArray(), "%s is not array", node->name.c_str());
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
  /* create one node for every combination of selected dimensions */
  const size_t memberNum = splitMemberCount(node, splitDims);
  std::vector<Node*> arrayMember;
  arrayMember.reserve(memberNum);
  for (size_t i = 0; i < memberNum; ++i) {
    arrayMember.push_back(splitArrayMemberNode(node, splitDims,
                                               splitMemberCoords(node, splitDims, i)));
  }
  splitArrayMap[node] = arrayMember;
  /* distribute arrayVal */
  const bool fullySplit = splitDims.size() == node->dimension.size();
  for (ExpTree* tree : node->assignTree) {
    if (fullySplit) distributeTree(node, tree, arrayMember);
    else distributePartiallySplitTree(node, tree, arrayMember, splitDims);
  }
  if (node->resetTree) {
    for (size_t idx = 0; idx < memberNum; ++idx) {
      if (fullySplit) {
        std::vector<int> subIdx = splitMemberCoords(node, splitDims, idx);
        arrayMember[idx]->resetTree = dupTreeWithIdx(node->resetTree, subIdx, node);
      } else {
        arrayMember[idx]->resetTree = duplicateResetForMember(
            node->resetTree, node, arrayMember[idx], splitDims,
            splitMemberCoords(node, splitDims, idx));
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
  for (Node* n : checkNodes) {
    for (ExpTree* tree : n->assignTree) {
      tree->updateWithSplittedArray(n, node, arrayMember, splitDims);
    }
    if (n->resetTree) {
      n->resetTree->updateWithSplittedArray(n, node, arrayMember, splitDims);
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
  std::map<Node*, int> times;
  std::stack<Node*> s;
  int num = 0;
  splitGeneratedNames.clear();

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
      Node* node = getSplitArray(this);
      Assert(splitArrayMap.find(node) == splitArrayMap.end(), "%s is already splitted", node->name.c_str());
      num ++;
      splitArrayNode(node);
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
}

static bool nextVarConnect(Node* node) {
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

static std::map<Node*, bool> arraySplitMap;

void graph::checkNodeSplit(Node* node) {
  if (arraySplitMap.find(node) != arraySplitMap.end()) return;
  if (!node->isArray()) return;
  if ((node->type != NODE_OTHERS && node->type != NODE_REG_SRC) || node->arrayEntryNum() <= 1) {
    arraySplitMap[node] = false;
    return;
  }
  for (Node* prev : node->prev) checkNodeSplit(prev);
  bool prevSplitted = true;
  for (Node* prev : node->prev) {
    if (prev->isArray() && !arraySplitMap[prev]) prevSplitted = false;
  }
  Node* treeNode = node->type == NODE_REG_SRC ? node->getDst() : node;
  bool anyVarIdx = false;
  int beg, end;
  for (ExpTree* tree : treeNode->assignTree) {
    std::tie(beg, end) = tree->getlval()->getIdx(treeNode);
    if (beg < 0 || (beg != end && !prevSplitted)) anyVarIdx = true;
  }

  if (!anyVarIdx && !nextVarConnect(node)) {
    node->status = DEAD_NODE;
    if (node->type == NODE_REG_SRC) node->getDst()->status = DEAD_NODE;
    arraySplitMap[node] = true;
  } else {
    arraySplitMap[node] = false;
  }
}

struct DimAccess {
  std::vector<bool> variable;
  std::vector<bool> explicitConstant;
};

static void noteArrayAccess(ENode* enode, std::map<Node*, DimAccess>& accesses) {
  if (!enode) return;
  if (enode->nodePtr && enode->nodePtr->isArray()) {
    Node* array = enode->nodePtr;
    DimAccess& info = accesses[array];
    if (info.variable.size() != array->dimension.size()) {
      info.variable.assign(array->dimension.size(), false);
      info.explicitConstant.assign(array->dimension.size(), false);
    }
    Assert(enode->getChildNum() <= array->dimension.size(),
           "too many indices for array %s", array->name.c_str());
    /* A missing suffix is a bulk slice, not a variable index.  Only an
     * explicit non-literal index blocks that dimension. */
    for (size_t dim = 0; dim < enode->getChildNum(); ++dim) {
      ENode* index = enode->getChild(dim);
      if (index && index->opType == OP_INDEX_INT && index->values.size() == 1) {
        info.explicitConstant[dim] = true;
      } else {
        info.variable[dim] = true;
      }
    }
  }
  for (ENode* child : enode->child) {
    if (child) noteArrayAccess(child, accesses);
  }
}

static void absorbAccess(DimAccess& merged, Node* node, const std::map<Node*, DimAccess>& accesses) {
  auto found = accesses.find(node);
  if (found == accesses.end()) return;
  for (size_t dim = 0; dim < merged.variable.size() && dim < found->second.variable.size(); ++dim) {
    merged.variable[dim] = merged.variable[dim] || found->second.variable[dim];
    merged.explicitConstant[dim] = merged.explicitConstant[dim] || found->second.explicitConstant[dim];
  }
}

/* Dimensions that are indexed somewhere, and only ever with a literal.
 * Bulk slices do not block the dimension; they are rewritten while splitting. */
static std::vector<int> constantAxisDims(Node* node, const std::map<Node*, DimAccess>& accesses) {
  DimAccess merged;
  merged.variable.assign(node->dimension.size(), false);
  merged.explicitConstant.assign(node->dimension.size(), false);
  absorbAccess(merged, node, accesses);
  if (node->type == NODE_REG_SRC && node->getDst()) absorbAccess(merged, node->getDst(), accesses);

  std::vector<int> splitDims;
  for (size_t dim = 0; dim < node->dimension.size(); ++dim) {
    if (node->dimension[dim] > 1 && merged.explicitConstant[dim] && !merged.variable[dim]) {
      splitDims.push_back(static_cast<int>(dim));
    }
  }
  return splitDims;
}

static std::vector<Node*> liveSplitNodes() {
  std::set<Node*> seen;
  std::vector<Node*> nodes;
  auto add = [&](Node* node) {
    if (!node || node->status == DEAD_NODE || !seen.insert(node).second) return;
    nodes.push_back(node);
  };
  for (Node* node : fullyVisited) add(node);
  for (const auto& entry : splitArrayMap) {
    for (Node* member : entry.second) add(member);
  }
  for (size_t i = 0; i < nodes.size(); ++i) {
    Node* node = nodes[i];
    for (Node* next : node->next) add(next);
    for (Node* next : node->depNext) add(next);
    for (Node* prev : node->prev) add(prev);
    if (node->type == NODE_REG_SRC || node->type == NODE_REG_DST) add(node->getBindReg());
  }
  return nodes;
}

void graph::splitConstantAxes() {
  std::map<Node*, DimAccess> accesses;
  std::vector<Node*> nodes = liveSplitNodes();
  for (Node* owner : nodes) {
    for (ExpTree* tree : owner->assignTree) {
      if (tree->getRoot()) noteArrayAccess(tree->getRoot(), accesses);
      if (tree->getlval()) noteArrayAccess(tree->getlval(), accesses);
    }
    if (owner->resetTree) {
      if (owner->resetTree->getRoot()) noteArrayAccess(owner->resetTree->getRoot(), accesses);
      if (owner->resetTree->getlval()) noteArrayAccess(owner->resetTree->getlval(), accesses);
    }
  }

  std::vector<std::pair<Node*, std::vector<int>>> selected;
  for (Node* node : nodes) {
    if (splitArrayMap.find(node) != splitArrayMap.end()) continue;
    if (!node->isArray() || node->status == DEAD_NODE) continue;
    if ((node->type != NODE_OTHERS && node->type != NODE_REG_SRC) || node->arrayEntryNum() <= 1) continue;
    std::vector<int> splitDims = constantAxisDims(node, accesses);
    if (splitDims.empty()) continue;
    selected.push_back(std::make_pair(node, splitDims));
  }
#ifdef ORDERED_TOPO_SORT
  std::sort(selected.begin(), selected.end(), [](const auto& a, const auto& b) {
    return a.first->id < b.first->id;
  });
#endif
  for (const auto& item : selected) {
    item.first->status = DEAD_NODE;
    if (item.first->type == NODE_REG_SRC) item.first->getDst()->status = DEAD_NODE;
  }
  regsrc.erase(
    std::remove_if(regsrc.begin(), regsrc.end(), [](const Node* n){ return n->status == DEAD_NODE; }),
        regsrc.end()
  );
  int num = 0;
  for (const auto& item : selected) {
    Node* arrayNode = item.first;
    splitArrayNode(arrayNode, item.second);
    if (arrayNode->type == NODE_REG_SRC) {
      splitArrayNode(arrayNode->getDst(), item.second);
      Assert(splitArrayMap[arrayNode].size() == splitArrayMap[arrayNode->getDst()].size(),
             "%p %s member not match", arrayNode, arrayNode->name.c_str());
      for (size_t i = 0; i < splitArrayMap[arrayNode].size(); i ++) {
        regsrc.push_back(splitArrayMap[arrayNode][i]);
        splitArrayMap[arrayNode][i]->bindReg(splitArrayMap[arrayNode->getDst()][i]);
        splitArrayMap[arrayNode][i]->updateDep();
      }
    }
    num ++;
  }
  printf("[splitConstantAxis] split %d arrays\n", num);
}

/* Fully split arrays with no variable index, then split any remaining
 * dimension that is only ever indexed by a literal. */
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
    splitArrayNode(arrayNode);
    if (arrayNode->type == NODE_REG_SRC) {
      splitArrayNode(arrayNode->getDst());
      Assert(splitArrayMap[arrayNode].size() == splitArrayMap[arrayNode->getDst()].size(), "%p %s member not match", arrayNode, arrayNode->name.c_str());
      for (size_t i = 0; i < splitArrayMap[arrayNode].size(); i ++) {
        regsrc.push_back(splitArrayMap[arrayNode][i]);
        splitArrayMap[arrayNode][i]->bindReg(splitArrayMap[arrayNode->getDst()][i]);
        splitArrayMap[arrayNode][i]->updateDep();
      }
    }
    num ++;
  }
  printf("[splitOptionalArray] split %d arrays\n", num);
  splitConstantAxes();
}

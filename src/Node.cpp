#include "common.h"
#include <queue>
#include <map>

int Node::counter = 1;

void Node::updateConnect() {
  if (type == NODE_REG_SRC) return;
  std::queue<ENode*> q;
  for (ExpTree* tree : assignTree) {
    q.push(tree->getRoot());
    for (size_t i = 0; i < tree->getlval()->getChildNum(); i ++) q.push(tree->getlval()->getChild(i));
  }

  while (!q.empty()) {
    ENode* top = q.front();
    q.pop();
    Node* prevNode = top->getNode();
    if (prevNode) {
      if (prevNode->isArray() && prevNode->arraySplitted()) {
        ArrayMemberList* list = top->getArrayMember(prevNode);
        for (Node* arrayMember : list->member) {
          addPrev(arrayMember);
          arrayMember->addNext(this);
        }
      } else {
        addPrev(prevNode);
        prevNode->addNext(this);
      }
    }
    for (size_t i = 0; i < top->getChildNum(); i ++) {
      if (top->getChild(i)) q.push(top->getChild(i));
    }
  }
  if (type == NODE_READER) {
    Node* memory = parent;
    for (Node* port : memory->member) {
      if (port->type == NODE_WRITER) {
        addNext(port);
        port->addPrev(this);
      }
    }
  } else if (type == NODE_WRITER) {
    Node* memory = parent;
    for (Node* port : memory->member) {
      if (port->type == NODE_READER) {
        addPrev(port);
        port->addNext(this);
      }
    }
  }
}

void Node::updateDep(){ // only reg_src can reach here
  std::set<Node*> prevNodes;
  for (ExpTree* tree : assignTree) tree->getRelyNodes(prevNodes);
  for (Node* n : prevNodes) {
    depNext.insert(n);
    n->depPrev.insert(this);
  }
  std::set<Node*> nextNodes;
  if (reset == ASYRESET) {
    getENodeRelyNodes(resetTree->getRoot()->getChild(0), nextNodes);
  }
  for (Node* n : nextNodes) {
    depNext.insert(n);
    n->depPrev.insert(this);
    for (Node* srcNext : next) {
      srcNext->addDepPrev(n);
      n->addDepNext(srcNext);
    }
    if (getDst()) {
      getDst()->depPrev.insert(n);
      n->depNext.insert(getDst());
    }
  }
  std::set<Node*> resetValNodes;
  if (resetTree) {
    Assert(resetTree->getRoot()->opType == OP_RESET, "invalid resetTree");
    getENodeRelyNodes(resetTree->getRoot()->getChild(1), resetValNodes);
  }
  for (Node* n : resetValNodes) {
    if (n->type == NODE_REG_SRC) {
      depNext.insert(n);
      n->depPrev.insert(this);
    } else {
      depPrev.insert(n);
      n->depNext.insert(this);
    }
  }
}

void Node::constructSuperNode() {
  if (super) return;
  switch (type) {
    case NODE_INVALID:
    case NODE_MEMORY:
      Panic();
    case NODE_EXT: {
      super = new SuperNode(this);
      super->superType = SUPER_EXTMOD;
      for (Node* node : member) {
        if (node->type == NODE_EXT_OUT) super->add_member(node);
      }
      break;
    }
    case NODE_EXT_OUT:
      parent->constructSuperNode();
      break;
    default:
      super = new SuperNode(this);
  }
}

void Node::constructSuperConnect() {
  Assert(this->super, "empty super for %s", name.c_str());
  for (Node* n : prev) {
    Assert(n->super, "empty super for prev %s", n->name.c_str());
    if (n->super == this->super) continue;
    super->connectPrev(n->super);
  }
  for (Node* n : next) {
    Assert(n->super, "empty super for next %s", n->name.c_str());
    if (n->super == this->super) continue;
    super->connectNext(n->super);
  }
  for (Node* n : depPrev) {
    if (n->super == this->super) continue;
    this->super->depPrev.insert(n->super);
    n->super->depNext.insert(this->super);
  }
  for (Node* n : depNext) {
    if (n->super == this->super) continue;
    this->super->depNext.insert(n->super);
    n->super->depPrev.insert(this->super);
  }
}

void Node::updateInfo(TypeInfo* info) {
  width = info->width;
  sign = info->sign;
  dimension.insert(dimension.end(), info->dimension.begin(), info->dimension.end());
  if (info->isClock()) isClock = true;
  reset = info->getReset();
}

Node* Node::dup(NodeType _type, std::string _name) {
  Node* duplicate = new Node(_type == NODE_INVALID ? type : _type);
  duplicate->name = _name.empty() ? name : _name;
  duplicate->width = width;
  duplicate->sign = sign;
  duplicate->isClock = isClock;
  duplicate->reset = reset;
  duplicate->lineno = lineno;
  for (int dim : dimension) duplicate->dimension.push_back(dim);
  for (int param : paramsInt) duplicate->paramsInt.push_back(param);

  return duplicate;
}

AggrParentNode::AggrParentNode(std::string _name, TypeInfo* info) {
  name = _name;
  if (info) {
    member.insert(member.end(), info->aggrMember.begin(), info->aggrMember.end());
    parent.insert(parent.end(), info->aggrParent.begin(), info->aggrParent.end());
  }
  // parent.insert(parent.end(), info->aggrParent.begin(), info->aggrParent.end());
}

void TypeInfo::mergeInto(TypeInfo* info) {
  aggrMember.insert(aggrMember.end(), info->aggrMember.begin(), info->aggrMember.end());
  aggrParent.insert(aggrParent.end(), info->aggrParent.begin(), info->aggrParent.end());
}

void TypeInfo::addDim(int num) {
  if (isAggr()) {
    for (auto entry : aggrMember) entry.first->dimension.insert(entry.first->dimension.begin(), num);
    dimension.insert(dimension.begin(), num);
  } else {
    dimension.insert(dimension.begin(), num);
  }
}

void TypeInfo::newParent(std::string name) {
  aggrParent.push_back(new AggrParentNode(name, this));
}

void TypeInfo::flip() {
  for (size_t i = 0; i < aggrMember.size(); i ++) aggrMember[i].second = !aggrMember[i].second;
}

void Node::addUpdateTree() {
  ENode* dstENode = new ENode(getDst());
  dstENode->width = width;
  assignTree.push_back(new ExpTree(dstENode, this));
  if (reset == UINTRESET || reset == ASYRESET) {
    ENode* whenNode = new ENode(OP_RESET);
    whenNode->addChild(resetCond->getRoot());
    whenNode->addChild(resetVal->getRoot());
    resetTree = new ExpTree(whenNode, this);
  }
}

bool Node::anyExtEdge() {
  for (Node* nextNode : next) {
    if (nextNode->super != super) {
      return true;
    }
  }
  return false;
}

bool Node::anyNextActive() {
  return nextActiveId.size() != 0;
}
bool Node::needActivate() {
  return nextNeedActivate.size() != 0;
}

void Node::updateActivate() {
  for (Node* nextNode : next) {
    if (nextNode->super != super) {
      if (nextNode->super->cppId != -1)
        nextActiveId.insert(nextNode->super->cppId);
    } else if (super->findIndex(this) >= super->findIndex(nextNode)) {
      if (super->cppId != -1)
        nextActiveId.insert(super->cppId);
    }
  }
  if (type == NODE_REG_DST) {
    nextActiveId.insert(getSrc()->super->cppId);
  }
  if (type == NODE_WRITER) {
    for (Node* port : parent->member) {
      if (port->type == NODE_READER && (port->status == VALID_NODE || port->status == MERGED_NODE) && port->super->cppId != -1)
        nextActiveId.insert(port->super->cppId);
    }
  }
  if (type == NODE_READWRITER) {
    for (Node* port : parent->member) {
      if (port == this) {
        if (port->parent->extraInfo != "new") nextActiveId.insert(super->cppId);
      }
      else if ((port->type == NODE_READER || port->type == NODE_READWRITER)
         && (port->status == VALID_NODE || port->status == MERGED_NODE) && port->super->cppId != -1)
        nextActiveId.insert(port->super->cppId);
    }
  }
}

void Node::updateNeedActivate(std::set<int>& alwaysActive) {
  for (int id : nextActiveId) {
    if (alwaysActive.find(id) == alwaysActive.end()) {
      nextNeedActivate.insert(id);
    }
  }
}

void Node::removeConnection() {
  for (Node* prevNode : prev) prevNode->eraseNext(this);
  for (Node* nextNode : next) nextNode->erasePrev(this);
}

ExpTree* dupTreeWithIdx(ExpTree* tree, std::vector<int>& index, Node* node);

void splitTree(Node* node, ExpTree* tree, std::vector<ExpTree*>& newTrees) {
  if (node->dimension.size() == tree->getlval()->getChildNum()) {
    newTrees.push_back(tree);
    return;
  }
  int newBeg, newEnd;
  std::tie(newBeg, newEnd) = tree->getlval()->getIdx(node);
  if (newBeg < 0 || newBeg == newEnd) {
    newTrees.push_back(tree);
    return;
  }
  for (int i = newBeg; i <= newEnd; i ++) {
    std::vector<int>newDim (node->dimension.size() - tree->getlval()->getChildNum());
    int dim = i;
    for (size_t j = node->dimension.size() - 1; j >= tree->getlval()->getChildNum() ; j --) {
      newDim[j-tree->getlval()->getChildNum()] = dim % node->dimension[j];
      dim = dim / node->dimension[j];
      if (j == 0) break;
    }
    ExpTree* newTree = dupTreeWithIdx(tree, newDim, node);
    newTrees.push_back(newTree);
  }

}

void Node::addArrayVal(ExpTree* val) {
  if (val->getRoot()->opType == OP_INVALID) return;
  if (assignTree.size() == 1 && assignTree[0]->getRoot()->opType != OP_WHEN) {
    ExpTree* oldTree = assignTree[0];
    assignTree.clear();
    std::vector<ExpTree*> newTrees;
    splitTree(this, oldTree, newTrees);
    for (ExpTree* tree : newTrees) assignTree.push_back(tree);
  }
  if (val->getRoot()->opType == OP_WHEN) {
    assignTree.push_back(val);
  } else {
    int newBeg, newEnd;
    std::tie(newBeg, newEnd) = val->getlval()->getIdx(this);
    if (newBeg < 0 || assignTree.size() == 0) {
      assignTree.push_back(val);
      return;
    }
    std::vector<ExpTree*> newTrees;
    splitTree(this, val, newTrees);
    for (ExpTree* tree : newTrees) {
      int beg, end;
      std::tie(beg, end) = tree->getlval()->getIdx(this);
      int replaceIdx = -1;
      for (size_t i = 0; i < assignTree.size(); i ++) {
        int prevBeg, prevEnd;
        std::tie(prevBeg, prevEnd) = assignTree[i]->getlval()->getIdx(this);
        if (prevBeg == beg && prevEnd == end) {
          replaceIdx = i;
          break;
        }
      }
      if (replaceIdx >= 0) {
        assignTree.erase(assignTree.begin() + replaceIdx);
      }
      assignTree.push_back(tree);
    }
  }
}

void Node::clear_relation() {
  prev.clear();
  next.clear();
  depPrev.clear();
  depNext.clear();
}

void Node::addPrev(Node* node) {
  prev.insert(node);
  depPrev.insert(node);
}

void Node::addPrev(std::set<Node*>& node) {
  prev.insert(node.begin(), node.end());
  depPrev.insert(node.begin(), node.end());
}

void Node::addPrev(std::vector<Node*>& node) {
  prev.insert(node.begin(), node.end());
  depPrev.insert(node.begin(), node.end());
}

void Node::erasePrev(Node* node) {
  prev.erase(node);
  depPrev.erase(node);
}

void Node::addNext(Node* node) {
  next.insert(node);
  depNext.insert(node);
}

void Node::eraseNext(Node* node) {
  next.erase(node);
  depNext.erase(node);
}

void Node::addNext(std::set<Node*>& node) {
  next.insert(node.begin(), node.end());
  depNext.insert(node.begin(), node.end());
}

void Node::addNext(std::vector<Node*>& node) {
  next.insert(node.begin(), node.end());
  depNext.insert(node.begin(), node.end());
}

void Node::clearPrev() {
  prev.clear();
  depPrev.clear();
}

void Node::addDepPrev(Node* node) {
  depPrev.insert(node);
}

void Node::eraseDepPrev(Node* node) {
  depPrev.erase(node);
}

void Node::eraseDepNext(Node* node) {
  depNext.erase(node);
}

void Node::addDepNext(Node* node) {
  depNext.insert(node);
}

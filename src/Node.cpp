#include "common.h"
#include <queue>
#include <map>

void Node::updateConnect() {
  std::queue<ENode*> q;
  if (valTree) q.push(valTree->getRoot());
  if (isArray()) {
    for (ExpTree* tree : arrayVal) {
      if (!tree) continue;
      q.push(tree->getRoot());
      if (tree->getlval()) {
        for (int i = 0; i < tree->getlval()->getChildNum(); i ++) q.push(tree->getlval()->getChild(i));
      }
    }
  }
  while (!q.empty()) {
    ENode* top = q.front();
    q.pop();
    Node* prevNode = top->getNode();
    if (prevNode) {
      if (prevNode->isArray() && prevNode->arraySplitted()) {
        ArrayMemberList* list = top->getArrayMember(prevNode);
        for (Node* arrayMember : list->member) {
          prev.insert(arrayMember);
          arrayMember->next.insert(this);
        }
      } else {
        prev.insert(prevNode);
        prevNode->next.insert(this);
      }
    }
    for (int i = 0; i < top->getChildNum(); i ++) {
      if (top->getChild(i)) q.push(top->getChild(i));
    }
  }
}
static std::set<Node*>inferredNodes;
/*
infer width of node and all ENodes in valTree
inverse topological order or [infer all encountered nodes]
*/
void Node::inferWidth() {
  if (inferredNodes.find(this) != inferredNodes.end()) return;
  inferredNodes.insert(this);
  if (type == NODE_INVALID) return;
  if (isArray() && arraySplitted()) {
    for (Node* member : arrayMember) member->inferWidth();
  }
  if (valTree) {
    valTree->getRoot()->inferWidth();
  }
  if (width == -1) {
    if (valTree) {
      setType(valTree->getRoot()->width, valTree->getRoot()->sign);
      isClock = valTree->getRoot()->isClock;
    }
    else setType(0, false);
  }

  for (ExpTree* arrayTree : arrayVal) {
    if (!arrayTree) continue;
    arrayTree->getRoot()->inferWidth();
    arrayTree->getlval()->inferWidth();
  }
}

/* construct superNodes for all memory_member in the port the (member) belongs to */
static void memSuper(Node* member) {
  Node* port = member->parent;
  Node* memory = port->parent;
  SuperNode* super = new SuperNode();

  if (port->type == NODE_READER) {
    /* merge addr & en & clk*/
    super->add_member(port->get_member(READER_ADDR));
    super->add_member(port->get_member(READER_EN));
    super->add_member(port->get_member(READER_CLK));
    /* data */
    if (memory->rlatency >= 1) {
      /* split date from addr & en */
      super = new SuperNode();
    }
    super->add_member(port->get_member(READER_DATA));
  } else if (port->type == NODE_WRITER) {
    for (Node* n : port->member)
      super->add_member(n);
  } else if (port->type == NODE_READWRITER) {
    super->add_member(port->get_member(READWRITER_ADDR));
    super->add_member(port->get_member(READWRITER_EN));
    super->add_member(port->get_member(READWRITER_CLK));
    super->add_member(port->get_member(READWRITER_WDATA));
    super->add_member(port->get_member(READWRITER_WMASK));
    super->add_member(port->get_member(READWRITER_WMODE));
    if (memory->rlatency >= 1) super = new SuperNode();
    super->add_member(port->get_member(READWRITER_RDATA));
  } else {
    Panic();
  }
}

void Node::constructSuperNode() {
  if (super) return;
  switch (type) {
    case NODE_INVALID:
    case NODE_MEMORY:
    case NODE_READER:
    case NODE_WRITER:
    case NODE_READWRITER:
      Panic();
    case NODE_MEM_MEMBER:
      memSuper(this);
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
    super->add_prev(n->super);
  }
  for (Node* n : next) {
    Assert(n->super, "empty super for next %s", n->name.c_str());
    if (n->super == this->super) continue;
    super->add_next(n->super);
  }
}

void Node::updateInfo(TypeInfo* info) {
  width = info->width;
  sign = info->sign;
  dimension.insert(dimension.end(), info->dimension.begin(), info->dimension.end());
  if (info->isClock()) isClock = true;
}

Node* Node::dup(NodeType _type, std::string _name) {
  Node* duplicate = new Node(_type == NODE_INVALID ? type : _type);
  duplicate->name = _name.empty() ? name : _name;
  duplicate->width = width;
  duplicate->sign = sign;
  duplicate->isClock = isClock;
  for (int dim : dimension) duplicate->dimension.push_back(dim);

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

void Node::addReset() {
  Assert(type == NODE_REG_SRC, "%s(%d) is not regsrc", name.c_str(), type);
  ENode* regTop = new ENode(OP_WHEN);

  regTop->addChild(resetCond->getRoot());
  regTop->addChild(resetVal->getRoot());
  if (valTree) {
    regTop->addChild(valTree->getRoot());
    valTree->setRoot(regTop);
  } else {
    regTop->addChild(nullptr);
    valTree = new ExpTree(regTop);
  }
  for (ExpTree* tree : arrayVal) {
    if (!tree) continue;
    ENode* arrayWhenTop = new ENode(OP_WHEN);
    arrayWhenTop->addChild(resetCond->getRoot());
    arrayWhenTop->addChild(nullptr);
    arrayWhenTop->addChild(tree->getRoot());
    tree->setRoot(arrayWhenTop);
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

bool Node::needActivate() {
  return nextActiveId.size() != 0;
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
}

void Node::removeConnection() {
  for (Node* prevNode : prev) prevNode->next.erase(this);
  for (Node* nextNode : next) nextNode->prev.erase(this);
}

void Node::addArrayVal(ExpTree* val) {
  if (val->getRoot()->opType == OP_INVALID) ;
  else arrayVal.push_back(val);
}
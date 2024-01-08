#include "common.h"
#include <queue>
#include <map>

void Node::updateConnect() {
  std::queue<ENode*> q;
  if (valTree) q.push(valTree->getRoot());
  if (isArray()) {
    for (ExpTree* tree : arrayVal) q.push(tree->getRoot());
  }
  while (!q.empty()) {
    ENode* top = q.front();
    q.pop();
    Node* prevNode = top->getNode();
    if (prevNode) {
      prev.insert(prevNode);
      prevNode->next.insert(this);
    }
    for (int i = 0; i < top->getChildNum(); i ++) {
      if (top->getChild(i)) q.push(top->getChild(i));
    }
  }
}
/*
infer width of node and all ENodes in valTree
inverse topological order or [infer all encountered nodes]
*/
void Node::inferWidth() {
  if (type == NODE_INVALID || type == NODE_SPECIAL) return;
  if (width != 0 && (!valTree || valTree->getRoot()->width != 0)) return;
  Assert(valTree && valTree->getRoot(), "can not infer width of %s through empty valTree", name.c_str());
  valTree->getRoot()->inferWidth();
  setType(valTree->getRoot()->width, valTree->getRoot()->sign);
}

/* construct superNodes for all memory_member in the port the (member) belongs to */
static void inline memSuper(Node* member) {
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
    case NODE_ARRAY_MEMBER:
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
    super->add_prev(n->super);
  }
  for (Node* n : next) {
    Assert(n->super, "empty super for next %s", n->name.c_str());
    super->add_next(n->super);
  }
}

void Node::updateInfo(TypeInfo* info) {
  width = info->width;
  sign = info->sign;
  dimension.insert(dimension.end(), info->dimension.begin(), info->dimension.end());
}

Node* Node::dup(NodeType _type, std::string _name) {
  Node* duplicate = new Node(_type == NODE_INVALID ? type : _type);
  duplicate->name = _name.empty() ? name : _name;
  duplicate->width = width;
  duplicate->sign = sign;
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
    for (Node* node : aggrMember) node->dimension.push_back(num);
  } else {
    dimension.push_back(num);
  }
}

void TypeInfo::newParent(std::string name) {
  aggrParent.push_back(new AggrParentNode(name, this));
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
    ENode* arrayWhenTop = new ENode(OP_WHEN);
    arrayWhenTop->addChild(resetCond->getRoot());
    arrayWhenTop->addChild(nullptr);
    arrayWhenTop->addChild(tree->getRoot());
    tree->setRoot(arrayWhenTop);
  }
}
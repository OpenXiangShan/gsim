#include "common.h"
#include <stack>
#include <queue>

#define w0 getChild(0)->width
#define w1 getChild(1)->width
#define w2 getChild(2)->width
#define s0 getChild(0)->sign
#define s1 getChild(1)->sign
#define s2 getChild(2)->sign

void ENode::inferWidth() {
  for (ENode* enode : child) {
    if (enode) enode->inferWidth();
  }
  if (nodePtr) {
    Node* realNode = nodePtr;
    if(nodePtr->isArray() && nodePtr->arraySplitted()) {
      ArrayMemberList* list = getArrayMember(nodePtr);
      realNode = list->member[0];
    }
    setWidth(realNode->width, realNode->sign);
    isClock = realNode->isClock;
  } else {
    isClock = false;
    switch (opType) {
      case OP_INT:
        Assert(width >= 0, "unset width %d in INT", width);
        break;
      case OP_ADD:  case OP_SUB:
        Assert(getChildNum() == 2 && s0 == s1, "invalid child");
        setWidth(MAX(w0, w1) + 1, s0);
        break;
      case OP_MUL:
        Assert(getChildNum() == 2 && s0 == s1, "invalid child");
        setWidth(w0 + w1, s0);
        break;
      case OP_CAT:
        Assert(getChildNum() == 2 && s0 == s1, "invalid child");
        setWidth(w0 + w1, false);
        break;
      case OP_DIV:
        Assert(getChildNum() == 2 && s0 == s1, "invalid child");
        setWidth(w0 + s0, s0);
        break;
      case OP_REM:
        Assert(getChildNum() == 2 && s0 == s1, "invalid child");
        setWidth(MIN(w0, w1), s0);
        break;
      case OP_LT: case OP_LEQ: case OP_GT: case OP_GEQ: case OP_EQ: case OP_NEQ:
        Assert(getChildNum() == 2 && s0 == s1, "invalid child");
        setWidth(1, false);
        break;
      case OP_DSHL:
        Assert(getChildNum() == 2 && !s1, "invalid child");
        setWidth(w0 + (1 << w1) - 1, s0);
        break;
      case OP_DSHR:
        Assert(getChildNum() == 2, "invalid child");
        setWidth(w0, s0);
        break;
      case OP_AND: // the width is actually max, while the upper bits are zeros
        Assert(getChildNum() == 2 && s0 == s1, "invalid child");
        setWidth(MAX(w0, w1), false);
        break;
      case OP_OR: case OP_XOR:
        Assert(getChildNum() == 2 && s0 == s1, "invalid child");
        setWidth(MAX(w0, w1), false);
        break;
      case OP_ASUINT:
        Assert(getChildNum() == 1, "invalid child");
        setWidth(w0, false);
        break;
      case OP_ASSINT:
        Assert(getChildNum() == 1, "invalid child");
        setWidth(w0, true);
        break;
      case OP_ASCLOCK:
        isClock = true;
      case OP_ASASYNCRESET:
        Assert(getChildNum() == 1, "invalid child");
        setWidth(1, false);
        break;
      case OP_CVT:
        Assert(getChildNum() == 1, "invalid child");
        setWidth(w0 + !s0, true);
        break;
      case OP_NEG:
        Assert(getChildNum() == 1, "invalid child");
        setWidth(w0 + 1, true);
        break;
      case OP_NOT:
        Assert(getChildNum() == 1, "invalid child");
        setWidth(w0, false);
        break;
      case OP_ANDR:
      case OP_ORR:
      case OP_XORR:
        Assert(getChildNum() == 1, "invalid child");
        setWidth(1, false);
        break;
      case OP_PAD:
        Assert(getChildNum() == 1 && values.size() == 1, "invalid child");
        setWidth(MAX(w0, values[0]), s0);
        break;
      case OP_SHL:
        Assert(getChildNum() == 1 && values.size() == 1, "invalid child");
        setWidth(w0 + values[0], s0);
        break;
      case OP_SHR:
        Assert(getChildNum() == 1 && values.size() == 1, "invalid child");
        setWidth(MAX(w0-values[0], 0), s0);
        break;
      case OP_HEAD:
        Assert(getChildNum() == 1 && values.size() == 1, "invalid child");
        setWidth(values[0], false);
        break;
      case OP_TAIL:
        //  UInt<1>(0h0) => tail(UInt<1>(0), 1)
        Assert(w0 >= values[0], "the width of argument(%d) in tail is no greater than %d", w0, values[0]);
        Assert(getChildNum() == 1 && values.size() == 1, "invalid child");
        setWidth(w0-values[0], false);
        break;
      case OP_BITS: // values[0] may exceeds w0 due to the shrink width of add
        Assert(getChildNum() == 1 && values.size() == 2, "invalid child");
        // Assert(w0 > values[0] && values[0] >= values[1], "invalid bits(%d, %d) with argument(%d) ", values[0], values[1], w0);
        setWidth(values[0]-values[1] + 1, false);
        break;
      case OP_BITS_NOSHIFT:
        Assert(getChildNum() == 1 && values.size() == 2, "invalid child");
        setWidth(values[0] + 1, false);
        break;
      case OP_MUX:
      case OP_WHEN:
        if (getChild(1) && getChild(2)) {
          if (w1 > w2) getChild(2)->setWidth(w1, s2);
          if (w2 > w1) getChild(1)->setWidth(w2, s1);
        }
        Assert(getChildNum() == 3 && (getChild(1) || getChild(2)), "invalid child");
        if (getChild(1) && getChild(2)) setWidth(MAX(w1, w2), s1);
        else if (getChild(1)) setWidth(w1, s1);
        else setWidth(w2, s2);
        break;
      case OP_STMT:
        for (ENode* enode : child) {
          setWidth(MAX(width, enode->width), enode->sign);
        }
        for (ENode* enode : child) {
          if (enode->width != width) enode->setWidth(width, sign);
        }
        break;
      case OP_GROUP:
        for (ENode* enode : child) {
          setWidth(MAX(width, enode->width), enode->sign);
        }
        break;
      case OP_READ_MEM:
        setWidth(memoryNode->width, memoryNode->sign);
        break;
      case OP_WRITE_MEM:
        setWidth(w1, s1);
        break;
      case OP_INVALID:
        setWidth(0, false);
        break;
      case OP_RESET:
        setWidth(w1, false);
        break;
      case OP_PRINTF:
      case OP_ASSERT:
        setWidth(1, false);
        break;
      case OP_INDEX_INT:
      case OP_INDEX:
        break;
      default:
        Assert(0, "invalid operand %d", opType);
    }
  }
}

/*
infer width of node and all ENodes in valTree
inverse topological order or [infer all encountered nodes]
*/
void Node::inferWidth() {
  if (type == NODE_INVALID) return;
  if (isArray() && arraySplitted()) {
    for (Node* member : arrayMember) member->inferWidth();
  }
  for (ExpTree* tree : assignTree) tree->getRoot()->inferWidth();
  for (ExpTree* tree : arrayVal) tree->getRoot()->inferWidth();
  if (updateTree) updateTree->getRoot()->inferWidth();
  if (resetTree) resetTree->getRoot()->inferWidth();
  if (resetVal) resetVal->getRoot()->inferWidth();

  if (width == -1) {
    int newWidth = 0;
    bool newSign = sign;
    bool newClock = isClock;
    for (ExpTree* tree : assignTree) {
      newWidth = MAX(newWidth, tree->getRoot()->width);
      newSign = tree->getRoot()->sign;
      newClock |= tree->getRoot()->isClock;
    }
    for (ExpTree* tree : arrayVal) {
      newWidth = MAX(newWidth, tree->getRoot()->width);
      newSign = tree->getRoot()->sign;
      newClock |= tree->getRoot()->isClock;
    }
    setType(newWidth, newSign);
    isClock = newClock;
  }

  for (ExpTree* arrayTree : arrayVal) {
    arrayTree->getlval()->inferWidth();
  }
}


void graph::inferAllWidth() {
  std::priority_queue<Node*, std::vector<Node*>, ordercmp> reinferNodes;
  std::set<Node*> uniqueNodes;
  std::set<Node*> fixedWidth;

  for (SuperNode* super : sortedSuper) {
    for (Node* node : super->member) {
      if (node->width != -1) fixedWidth.insert(node);
      reinferNodes.push(node);
    }
  }

  while (!reinferNodes.empty()) {
    Node* node = reinferNodes.top();
    reinferNodes.pop();
    int prevWidth = node->width;
    node->inferWidth();
    if (prevWidth != node->width) {
      for (Node* next : node->next) {
        if (uniqueNodes.find(next) == uniqueNodes.end() && fixedWidth.find(next) == fixedWidth.end()) {
          next->width = -1;
          reinferNodes.push(next);
        }
      }
    }
    if (node->type == NODE_REG_DST && node->width != node->getSrc()->width) {
      if (node->width < node->getSrc()->width) {
        node->width = node->getSrc()->width;
      } else {
        node->getSrc()->width = node->width;
        for (Node* next : node->getSrc()->next) {
          if (uniqueNodes.find(next) == uniqueNodes.end() && fixedWidth.find(next) == fixedWidth.end()) {
            next->width = -1;
            reinferNodes.push(next);
          }
        }
      }
    }
  }
  for (SuperNode* super : sortedSuper) {
    for (Node* node : super->member) {
      node->updateHeadTail();
      node->updateTreeWithNewWIdth();
    }
  }
}

void Node::updateHeadTail() {
  std::stack<ENode*> s;
  for (ExpTree* tree : assignTree) s.push(tree->getRoot());
  for (ExpTree* tree : arrayVal) s.push(tree->getRoot());
  if (updateTree) s.push(updateTree->getRoot());
  if (resetTree) s.push(resetTree->getRoot());
  if (resetVal) s.push(resetVal->getRoot());
  while (!s.empty()) {
    ENode* top = s.top();
    s.pop();
    if (top->opType == OP_HEAD || top->opType == OP_TAIL) top->values[0] = top->getChild(0)->width - top->values[0];
    for (ENode* enode : top->child) {
      if (enode) s.push(enode);
    }
  }
}
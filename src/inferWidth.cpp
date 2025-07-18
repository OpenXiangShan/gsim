#include "common.h"
#include <stack>
#include <queue>

#define w0 getChild(0)->width
#define w1 getChild(1)->width
#define w2 getChild(2)->width
#define s0 getChild(0)->sign
#define s1 getChild(1)->sign
#define s2 getChild(2)->sign

/*
 * Recursively calculates the width of each ENode (DFS)
 * For leaf ENodes, the width is directly derived from the corresponding Node.
 * For non-leaf ENodes, the width is determined based on:
 *     1. The widths of its child ENodes, and
 *     2. The operation represented by this ENode.
 */
void ENode::inferWidth() {
  for (ENode* enode : child) {
    if (enode) enode->inferWidth();
  }
  if (nodePtr) {
    setWidth(nodePtr->width, nodePtr->sign);
    isClock = nodePtr->isClock;
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
        Assert(getChildNum() == 1 && values.size() == 1, "invalid child");
        if (w0 < values[0]) setWidth(0, false);
        else setWidth(w0-values[0], false);
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
        Assert(getChildNum() == 3 && (getChild(1) || getChild(2)), "invalid child");
        if (getChild(1) && getChild(2)) setWidth(MAX(w1, w2), s1);
        else if (getChild(1)) setWidth(w1, s1);
        else setWidth(w2, s2);
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
      case OP_EXIT:
      case OP_EXT_FUNC:
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

void ENode::clearWidth() {
  std::stack<ENode*> s;
  for (ENode* childENode : child) {
    if (childENode) childENode->clearWidth();
  }
  if (opType != OP_INT) width = -1;
}

void Node::clearWidth() {
  for (ExpTree* tree : assignTree) {
    tree->getRoot()->clearWidth();
    tree->getlval()->clearWidth();
  }
  if (resetTree) resetTree->getRoot()->clearWidth();
}

/*
infer width of node and all ENodes in valTree
inverse topological order or [infer all encountered nodes]
*/
void Node::inferWidth() {
  if (type == NODE_INVALID) return;
  if (resetTree) resetTree->getRoot()->inferWidth();

  for (ExpTree* tree : assignTree) tree->getRoot()->inferWidth();

  if (width == -1) {
    int newWidth = 0;
    bool newSign = sign;
    bool newClock = isClock;
    if (type == NODE_REG_SRC) {
      if (resetTree) {
        newWidth = MAX(newWidth, resetTree->getRoot()->width);
        newSign = resetTree->getRoot()->sign;
      }
    }
    for (ExpTree* tree : assignTree) {
      newWidth = MAX(newWidth, tree->getRoot()->width);
      newSign = tree->getRoot()->sign;
      newClock |= tree->getRoot()->isClock;
    }

    setType(newWidth, newSign);
    isClock = newClock;
  }

  for (ExpTree* tree : assignTree) {
    tree->getlval()->inferWidth();
  }
}


void graph::inferAllWidth() {
  std::priority_queue<Node*, std::vector<Node*>, ordercmp> reinferNodes;
  std::set<Node*> uniqueNodes;
  std::set<Node*> fixedWidth;

  auto addRecomputeNext = [&uniqueNodes, &reinferNodes](Node* node) {
    for (Node* next : node->next) {
      if (uniqueNodes.find(next) == uniqueNodes.end()) {
        reinferNodes.push(next);
        uniqueNodes.insert(next);
      }
    }
  };
  auto addRecompute = [&uniqueNodes, &reinferNodes](Node* node) {
    if (uniqueNodes.find(node) == uniqueNodes.end()) {
      reinferNodes.push(node);
      uniqueNodes.insert(node);
    }
  };

  for (SuperNode* super : sortedSuper) {
    for (Node* node : super->member) {
      if (node->width != -1) fixedWidth.insert(node);
      reinferNodes.push(node);
      uniqueNodes.insert(node);
    }
  }

  while (!reinferNodes.empty()) {
    Node* node = reinferNodes.top();
    reinferNodes.pop();
    uniqueNodes.erase(node);
    int prevWidth = node->width;
    if (fixedWidth.find(node) == fixedWidth.end()) node->width = -1;
    node->clearWidth();
    node->inferWidth();
    if (prevWidth != node->width) {
      addRecomputeNext(node);
      if (node->type == NODE_REG_DST) addRecompute(node->getSrc());
    }

    if (node->type == NODE_WRITER && node->width > node->parent->width) {
      node->parent->width = node->width;
      for (Node* port : node->parent->member) {
        if (port->type == NODE_READER) {
          addRecompute(port);
        }
      }
    }
    if (reinferNodes.empty()) { // re-checking reset tree, the resetVal may be inferred after registers
      for (SuperNode* super : sortedSuper) {
        for (Node* node : super->member) {
          if (node->type != NODE_REG_SRC) continue;
          int srcWidth = node->width;
          int dstWidth = node->getDst()->width;
          if (node->width > node->getDst()->width) {
            node->getDst()->width = node->width;
          } else if (node->getDst()->width > node->width) {
            node->width = node->getDst()->width;
          }
          if (srcWidth != node->width) {
            addRecomputeNext(node);
          }
          if (dstWidth != node->getDst()->width) addRecomputeNext(node->getDst());
        }
      }
    }
  }
  for (SuperNode* super : sortedSuper) {
    for (Node* node : super->member) {
      if (node->resetTree) node->resetTree->getRoot()->inferWidth();
      node->updateHeadTail();
      node->updateTreeWithNewWIdth();
    }
  }
}

void Node::updateHeadTail() {
  std::stack<ENode*> s;
  for (ExpTree* tree : assignTree) s.push(tree->getRoot());
  if (resetTree) s.push(resetTree->getRoot());
  while (!s.empty()) {
    ENode* top = s.top();
    s.pop();
    if (top->opType == OP_HEAD || top->opType == OP_TAIL) top->values[0] = top->getChild(0)->width - top->values[0];
    for (ENode* enode : top->child) {
      if (enode) s.push(enode);
    }
  }
}
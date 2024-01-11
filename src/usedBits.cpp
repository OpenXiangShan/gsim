/*
 Compute the usedBits for every nodes and enodes
 can merged into inferWidth
*/

#include "common.h"
#include <map>
#include <stack>
#include <set>

#define Child(id, name) getChild(id)->name

static std::map<Node*, int> usedBits;
/* all nodes that may affect their previous ndoes*/
static std::vector<Node*> checkNodes;

void ENode::passWidthToChild() {
  std::vector<int>childBits;
  if (nodePtr) {
    if (usedBit > nodePtr->usedBit) {
      checkNodes.push_back(nodePtr);
    }
    nodePtr->update_usedBit(usedBit);
    if (nodePtr->type == NODE_REG_SRC) {
      if (nodePtr->usedBit != nodePtr->getDst()->usedBit) {
        checkNodes.push_back(nodePtr->getDst());
        nodePtr->getDst()->usedBit = nodePtr->usedBit;
      }
    }
    return;
  }
  if (child.size() == 0) return;
  switch (opType) {
    case OP_ADD:  case OP_SUB: case OP_OR: case OP_XOR: case OP_AND:
      childBits.push_back(usedBit);
      childBits.push_back(usedBit);
      break;
    case OP_MUL: case OP_DIV: case OP_REM: case OP_DSHL: case OP_DSHR:
    case OP_LT: case OP_LEQ: case OP_GT: case OP_GEQ: case OP_EQ: case OP_NEQ:
      childBits.push_back(Child(0, width));
      childBits.push_back(Child(1, width));
      break;
    case OP_CAT:
      childBits.push_back(MAX(usedBit - Child(1, width), 0));
      childBits.push_back(MIN(Child(1, width), usedBit));      
      break;
    case OP_CVT:
      childBits.push_back(Child(0, sign) ? usedBit : usedBit - 1);
      break;
    case OP_ASCLOCK: case OP_ASASYNCRESET: case OP_ANDR:
    case OP_ORR: case OP_XORR: case OP_INDEX_INT: case OP_INDEX:
      childBits.push_back(Child(0, width));
      break;
    case OP_ASUINT: case OP_ASSINT: case OP_NOT: case OP_NEG: case OP_PAD: case OP_TAIL:
      childBits.push_back(usedBit);
      break;
    case OP_SHL:
      childBits.push_back(MAX(0, usedBit - values[0]));
      break;
    case OP_SHR:
      childBits.push_back(usedBit + values[0]);
      break;
    case OP_HEAD:
      // childBits.push_back(Child(0, width) - (values[0] - usedBit));
      childBits.push_back(Child(0, width));
      break;
    case OP_BITS:
      childBits.push_back(MIN(usedBit + values[1], values[0] + 1));
      break;
    case OP_MUX:
    case OP_WHEN:
      childBits.push_back(1);
      childBits.push_back(usedBit);
      childBits.push_back(usedBit);
      break;
    default:
      Panic();
  }

  Assert(child.size() == childBits.size(), "child.size %ld childBits.size %ld in op %d", child.size(), childBits.size(), opType);
  for (size_t i = 0; i < child.size(); i ++) {
    if (!child[i]) continue;

    int realBits = MIN(child[i]->width, childBits[i]);

    if (child[i]->usedBit != realBits) {
      child[i]->usedBit = realBits;
      child[i]->passWidthToChild();
    }
  }

}

/* the with of node->next may be updated, thus node should also re-compute */
void Node::passWidthToPrev() {
  for (ExpTree* tree : arrayVal) {
    if (usedBit != tree->getRoot()->usedBit) {
      tree->getRoot()->usedBit = usedBit;
      tree->getRoot()->passWidthToChild();
    }
    if (tree->getlval() && tree->getlval()->usedBit != tree->getlval()->width) {
      tree->getlval()->usedBit = tree->getlval()->width;
      tree->getlval()->passWidthToChild();
    }
  }
  if (!valTree) return;
  Assert(usedBit >= 0, "invalid usedBit %d in node %s", usedBit, name.c_str());
  if (usedBit != valTree->getRoot()->usedBit) {
    valTree->getRoot()->usedBit = usedBit;
    valTree->getRoot()->passWidthToChild();
  }
  return;
}

void graph::usedBits() {
  std::set<Node*> visitedNodes;
  /* add all sink nodes in topological order */
  for (Node* reg : regsrc) checkNodes.push_back(reg->getDst());
  for (Node* out : output) checkNodes.push_back(out);
  for (Node* mem : memory) { // all memory input
    for (Node* port : mem->member) {
      if (port->type == NODE_READER) {
        checkNodes.push_back(port->get_member(READER_ADDR));
        checkNodes.push_back(port->get_member(READER_EN));
        // checkNodes.push(port->get_member(READER_CLK));
      } else if (port->type == NODE_WRITER) {
        checkNodes.push_back(port->get_member(WRITER_ADDR));
        checkNodes.push_back(port->get_member(WRITER_EN));
        checkNodes.push_back(port->get_member(WRITER_MASK));
        checkNodes.push_back(port->get_member(WRITER_DATA));
      } else if (port->type == NODE_READWRITER) {
        checkNodes.push_back(port->get_member(READWRITER_ADDR));
        checkNodes.push_back(port->get_member(READWRITER_EN));
        checkNodes.push_back(port->get_member(READWRITER_WDATA));
        checkNodes.push_back(port->get_member(READWRITER_WMASK));
        checkNodes.push_back(port->get_member(READWRITER_WMODE));
      }
    }
  }
  for (Node* node: checkNodes) {
    node->usedBit = node->width;
  }

  while (!checkNodes.empty()) {
    Node* top = checkNodes.back();
    visitedNodes.insert(top);
    checkNodes.pop_back();
    top->passWidthToPrev();
  }

/* NOTE: reset cond & reset val tree*/

  for (Node* node : visitedNodes) {
    node->width = node->usedBit;
    if (node->valTree) node->valTree->getRoot()->updateWidth();
  }

}
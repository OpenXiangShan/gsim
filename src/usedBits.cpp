/*
 Compute the usedBits for every nodes and enodes
 can merged into inferWidth
*/

#include "common.h"
#include <map>
#include <stack>
#include <set>

#define Child(id, name) getChild(id)->name

/* all nodes that may affect their previous ndoes*/
static std::vector<Node*> checkNodes;

void ENode::passWidthToChild() {
  std::vector<int>childBits;
  if (nodePtr) {
    Node* node = nodePtr;
    if (node->isArray() && node->arraySplitted()) {
        auto range = this->getIdx(nodePtr);
        if (range.first < 0) {
          range.first = 0;
          range.second = node->arrayMember.size() - 1;
        }
        for (int i = range.first; i <= range.second; i ++) {
          Node* member = node->getArrayMember(i);
          if (usedBit > member->usedBit) {
            member->update_usedBit(usedBit);
            if (member->type == NODE_REG_SRC) {
              if (member->usedBit != member->getDst()->usedBit) {
                checkNodes.push_back(member->getDst());
                member->getDst()->usedBit = member->usedBit;
              }
            }
            checkNodes.push_back(member);
          }
        }
    } else {
      if (usedBit > node->usedBit) checkNodes.push_back(node);
    }
    node->update_usedBit(usedBit);
    if (node->type == NODE_REG_SRC) {
      if (node->usedBit != node->getDst()->usedBit) {
        checkNodes.push_back(node->getDst());
        node->getDst()->usedBit = node->usedBit;
      }
    }
    for (ENode* childENode : child) {
      if (!childENode) continue;
      childENode->usedBit = childENode->width;
      childENode->passWidthToChild();
    }
    return;
  }
  if (child.size() == 0) return;
  switch (opType) {
    case OP_ADD: case OP_SUB: case OP_OR: case OP_XOR: case OP_AND:
      childBits.push_back(usedBit);
      childBits.push_back(usedBit);
      break;
    case OP_MUL:
      childBits.push_back(MIN(usedBit, Child(0, width)));
      childBits.push_back(MIN(usedBit, Child(1, width)));
      break;
    case OP_DIV: case OP_REM: case OP_DSHL: case OP_DSHR:
    case OP_LT: case OP_LEQ: case OP_GT: case OP_GEQ: case OP_EQ: case OP_NEQ:
      childBits.push_back(Child(0, width));
      childBits.push_back(Child(1, width));
      break;
    case OP_CAT:
      childBits.push_back(MAX(usedBit - Child(1, width), 0));
      childBits.push_back(MIN(Child(1, width), usedBit));      
      break;
    case OP_CVT:
      childBits.push_back(usedBit);
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
      childBits.push_back(usedBit + values[0]);
      break;
    case OP_BITS:
      childBits.push_back(MIN(usedBit + values[1], values[0] + 1));
      break;
    case OP_BITS_NOSHIFT:
      childBits.push_back(usedBit);
      break;
    case OP_SEXT:
      childBits.push_back(usedBit);
      break;
    case OP_MUX:
    case OP_WHEN:
      childBits.push_back(1);
      childBits.push_back(usedBit);
      childBits.push_back(usedBit);
      break;
    case OP_STMT:
      for (size_t i = 0; i < getChildNum(); i ++) childBits.push_back(usedBit);
      break;
    case OP_READ_MEM:
      childBits.push_back(Child(0, width));
      break;
    case OP_WRITE_MEM:
      childBits.push_back(Child(0, width));
      childBits.push_back(memoryNode->width);
      break;
    case OP_RESET:
      childBits.push_back(1);
      childBits.push_back(usedBit);
      break;
    case OP_GROUP:
    case OP_EXIT:
    case OP_PRINTF:
    case OP_ASSERT:
    case OP_EXT_FUNC:
      for (size_t i = 0; i < getChildNum(); i ++) {
        childBits.push_back(getChild(i)->width);
      }
      break;
    default:
      printf("invalid op %d\n", opType);
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
    if (!tree) continue;
    if (usedBit != tree->getRoot()->usedBit) {
      tree->getRoot()->usedBit = usedBit;
      tree->getRoot()->passWidthToChild();
    }
    if (tree->getlval() && tree->getlval()->usedBit != usedBit) {
      tree->getlval()->usedBit = usedBit;
      tree->getlval()->passWidthToChild();
    }
  }
  if (updateTree) {
    updateTree->getRoot()->usedBit = usedBit;
    updateTree->getRoot()->passWidthToChild();
  }
  if (resetTree) {
    resetTree->getRoot()->usedBit = usedBit;
    resetTree->getRoot()->passWidthToChild();
  }
  if (assignTree.size() == 0) return;
  Assert(usedBit >= 0, "invalid usedBit %d in node %s", usedBit, name.c_str());
  for (ExpTree* tree : assignTree) {
    if (usedBit != tree->getRoot()->usedBit) {
      tree->getRoot()->usedBit = usedBit;
      tree->getRoot()->passWidthToChild();
    }
    if (tree->getlval() && tree->getlval()->usedBit != usedBit) {
      tree->getlval()->usedBit = usedBit;
      tree->getlval()->passWidthToChild();
    }
  }
  return;
}

void graph::usedBits() {
  std::set<Node*> visitedNodes;
  /* add all sink nodes in topological order */
  for (Node* special : specialNodes) checkNodes.push_back(special);
  for (Node* reg : regsrc) checkNodes.push_back(reg->getDst());
  for (Node* out : output) checkNodes.push_back(out);
  for (Node* mem : memory) { // all memory input
    for (Node* port : mem->member) {
      if (port->type == NODE_READER) checkNodes.push_back(port);
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
    if (checkNodes.empty()) {
      for (Node* mem : memory) {
        int usedBit = 0;
        for (Node* port : mem->member) {
          if (port->type == NODE_READER || port->type == NODE_READWRITER) {
            usedBit = MAX(port->usedBit, usedBit);
          }
        }
        if (mem->usedBit != usedBit) {
          mem->usedBit = usedBit;
          for (Node* port : mem->member) {
            if (port->type == NODE_WRITER) {
              port->usedBit = usedBit;
              checkNodes.push_back(port);
            }
          }
        }
      }
    }
  }

/* NOTE: reset cond & reset val tree*/

  for (Node* node : visitedNodes) {
    node->width = node->usedBit;
    for (ExpTree* tree : node->assignTree) tree->getRoot()->updateWidth();
    for (ExpTree* tree : node->arrayVal) tree->getRoot()->updateWidth();
    if (node->updateTree) node->updateTree->getRoot()->updateWidth();
    if (node->resetTree) node->resetTree->getRoot()->updateWidth();
  }

  for (Node* node : splittedArray) {
    int width = 0;
    for (Node* member : node->arrayMember) width = MAX(width, member->width);
    node->width = width;
  }

  for (Node* mem : memory) mem->width = mem->usedBit;
  for (SuperNode* super : sortedSuper) {
    for (Node* node : super->member) node->updateTreeWithNewWIdth();
  }
}

void Node::updateTreeWithNewWIdth() {
  /* add ops to match tree width */
  for (ExpTree* tree : assignTree) tree->updateWithNewWidth();
  for (ExpTree* tree : arrayVal) tree->updateWithNewWidth();
  if (updateTree) updateTree->updateWithNewWidth();
  if (resetTree) resetTree->updateWithNewWidth();

  for (ExpTree* tree : assignTree) {
    tree->when2mux(width);
    tree->matchWidth(width);
  }
  for (ExpTree* tree : arrayVal) {
    tree->when2mux(width);
    tree->matchWidth(width);
  }
  if (updateTree) updateTree->matchWidth(width);
  if (resetTree) resetTree->matchWidth(width);
}

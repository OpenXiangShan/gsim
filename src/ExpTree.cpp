#include <stack>
#include <utility>
#include "common.h"

void ExpTree::when2mux(int width) {
  if (getRoot()->opType != OP_WHEN || width >= BASIC_WIDTH) return;
  std::stack<ENode*>s;
  s.push(getRoot());
  std::set<ENode*> whenNodes;
  bool update = true;
  while (!s.empty()) {
    ENode* top = s.top();
    s.pop();
    if (top->opType != OP_WHEN) continue;
    /* TODO: optimize invalid */
    if (top->getChild(1) && (top->getChild(1)->opType != OP_STMT && top->getChild(1)->opType != OP_WHEN && top->getChild(1)->opType != OP_INVALID)
      && top->getChild(2) && (top->getChild(2)->opType != OP_STMT && top->getChild(2)->opType != OP_WHEN && top->getChild(2)->opType != OP_INVALID)) {
      whenNodes.insert(top);
      for (ENode* child : top->child) s.push(child);
    } else {
      update = false;
      break;
    }
  }
  if (update) {
    for (ENode* when : whenNodes) when->opType = OP_MUX;
  }
}

/* add OP_CAT or OP_BITS to match expWidth with nodeWidth */
void ExpTree::matchWidth(int width) {
  std::vector<std::pair<ENode*, int>> allNodes;
  std::stack<ENode*>s;
  /* generate all nodes that need to be updated, passing down to when children */
  if (getRoot()->opType != OP_WHEN && getRoot()->opType != OP_RESET) {
    allNodes.push_back(std::make_pair(nullptr, -1));
  } else {
    s.push(getRoot());
    getRoot()->width = width;
  }
  while (!s.empty()) {
    ENode* top = s.top();
    s.pop();
    for (size_t i = top->opType == OP_STMT ? 0 : 1; i < top->getChildNum(); i ++) {
      ENode* child = top->getChild(i);
      if (child) {
        if (child->opType == OP_WHEN || child->opType == OP_STMT || child->opType == OP_RESET) {
          s.push(child);
          child->width = width;
        }
        else allNodes.push_back(std::make_pair(top, i));
      }
    }
  }
  /* update node width */
  for (auto iter : allNodes) {
    ENode* oldNode = iter.first ? iter.first->getChild(iter.second) : getRoot();
    if (width > oldNode->width) { // add OP_CAT
      if (oldNode->opType == OP_INT || oldNode->opType == OP_INVALID) {
        oldNode->width = width;
      } else if (oldNode->sign) {
          ENode* sext = new ENode(OP_SEXT);
          sext->width = width;
          sext->sign = true;
          sext->addVal(width);
          sext->addChild(oldNode);
          if (iter.first) iter.first->setChild(iter.second, sext);
          else setRoot(sext);
      } else {
        ENode* pad = new ENode(OP_PAD);
        pad->addVal(width);
        pad->width = width;
        pad->addChild(oldNode);
        if (iter.first) iter.first->setChild(iter.second, pad);
        else setRoot(pad);
      }
    } else if (width < oldNode->width) {
      if (oldNode->opType == OP_INVALID) {
        oldNode->width = width;
      } else {
        ENode* bits = new ENode(OP_BITS);
        bits->width = width;
        bits->sign = oldNode->sign;
        bits->addVal(width - 1);
        bits->addVal(0);
        bits->addChild(oldNode);
        if (iter.first) iter.first->setChild(iter.second, bits);
        else setRoot(bits);
      }
    }
  }
}

ENode* allocIntEnode(int width, std::string val) {
  ENode* ret = new ENode(OP_INT);
  ret->width = width;
  ret->strVal = val;
  return ret;
}

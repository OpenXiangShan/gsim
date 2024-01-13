#include "common.h"

int SuperNode::counter = 1;

bool SuperNode::notMerge() {
  bool ret = false;
  for (Node* memberNode : member) {
    if (memberNode->type == NODE_REG_SRC || memberNode->type == NODE_INP) {
      ret = true;
      break;
    }
  }
  return ret;
}

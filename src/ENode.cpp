#include "common.h"
#include <tuple>

#define w0 getChild(0)->width
#define w1 getChild(1)->width
#define w2 getChild(2)->width
#define s0 getChild(0)->sign
#define s1 getChild(1)->sign
#define s2 getChild(2)->sign

void ENode::inferWidth() {
  if (width != 0) return;
  for (ENode* enode : child) {
    if (enode) enode->inferWidth();
  }
  if (nodePtr) {
    nodePtr->inferWidth();
    setWidth(nodePtr->width, nodePtr->sign);
  } else {
    switch (opType) {
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
        setWidth(MIN(w0, w1), false);
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
        setWidth(MAX(w0-values[0], 1), s0);
        break;
      case OP_HEAD:
        Assert(getChildNum() == 1 && values.size() == 1, "invalid child");
        setWidth(values[0], false);
        values[0] = w0 - values[0];
        break;
      case OP_TAIL:
        Assert(w0 > values[0], "the width of argument(%d) in tail is no greater than %d", w0, values[0]);
        Assert(getChildNum() == 1 && values.size() == 1, "invalid child");
        setWidth(w0-values[0], false);
        values[0] = w0 - values[0];
        break;
      case OP_BITS:
        Assert(getChildNum() == 1 && values.size() == 2, "invalid child");
        Assert(w0 > values[0] && values[0] >= values[1], "invalid bits(%d, %d) with argument(%d) ", values[0], values[1], w0);
        setWidth(values[0]-values[1] + 1, false);
        break;
      case OP_MUX:
      case OP_WHEN:
        Assert(getChildNum() == 3 && (getChild(1) || getChild(2)), "invalid child");
        if (getChild(1) && getChild(2)) setWidth(MAX(w1, w2), s1);
        else if (getChild(1)) setWidth(w1, s1);
        else setWidth(w2, s2);
        break;
      case OP_INDEX_INT:
      case OP_INDEX:
      case OP_READ_MEM:
        break;
      default:
        Panic();
    }
  }
}

void ENode::updateWidth() {
 for (ENode* childENode : child) {
  if (childENode) childENode->updateWidth();
 }
 width = usedBit;
}
/*
return the index of lvalue(required to be array)
if the lvalue is also an array(e.g. a[1], a defined as a[2][2]), return the range of index
*/
std::pair<int, int> ENode::getIdx(Node* node) {
  Assert(node->isArray(), "%s is not array", node->name.c_str());
  std::vector<int>index;

  for (ENode* indexENode : child) {
    if (indexENode->opType != OP_INDEX_INT) return std::make_pair(-1, -1);
    index.push_back(indexENode->values[0]);
  }
  Assert(index.size() <= node->dimension.size(), "invalid index");
  int num = 1;
  for (int i = (int)node->dimension.size() - 1; i >= (int)index.size(); i --) {
    num *= node->dimension[i];
  }
  int base = 0;
  for (size_t i = 0; i < index.size(); i ++) {
    base = base * node->dimension[i] + index[i];
  }
  return std::make_pair(base * num, base * (num + 1) - 1);
}
/*
return right value of connected node
* for normal nodes and array nodes that are not splitted, return themselves
* for splitted array, return the corresponding member nodes if it represents a single member
                    or return the array itself if the index is not determined or it represent multiple members
*/
Node* ENode::getConnectNode() {
  /* not array */
  if (!nodePtr || !nodePtr->isArray()) {
    return nodePtr;
  }
  /* un-splitted array */
  if (!nodePtr->arraySplitted()) return nodePtr;
  /* splitted array */
  int begin, end;
  std::tie(begin, end) = getIdx(nodePtr);
  if (begin < 0 || begin != end) return nodePtr;
  return nodePtr->getArrayMember(begin);
}
#include <graph.h>
#include <common.h>
int p_stoi(const char* str);
static std::vector<int>bits;
static bool popOperand = true;

#define CHILD_BITS -1

int popBits() {
  Assert(bits.size() > 0, "bits empty\n");
  int ret = bits.back();
  bits.pop_back();
  return ret;
}
/*
compute child bits
parentBits: The used-Bits for current op
>pad|shl|shr|head|tail
*/
int bits_1expr1int(Node* node, int parentBits, int opIdx) {
  PNode* op = node->workingVal->ops[opIdx];
  int n = p_stoi(op->getExtra(0).c_str());

  int usedBits = parentBits;
  if (op->name == "shl") usedBits = parentBits - n;
  else if (op->name == "shr") usedBits = parentBits + n;
  else if (op->name == "head") usedBits = n;
  else if (op->name == "pad") usedBits = MIN(op->getChild(0)->width, n);

  Assert(usedBits > 0 && usedBits <= op->getChild(0)->width, "usedBit %d op->child(0)->width %d parent %d in node %s <- %s\n", usedBits, op->getChild(0)->width, parentBits, node->name.c_str(), op->getChild(0)->name.c_str());
  // node->opBits.push_back(usedBits);
  bits.push_back(usedBits);
  return usedBits;
}
/*
>bits
*/
int bits_1expr2int(Node* node, int parentBits, int opIdx) {
  PNode* op = node->workingVal->ops[opIdx];
  int l = p_stoi(op->getExtra(0).c_str());
  int r = p_stoi(op->getExtra(1).c_str());
  int usedBits = MIN(parentBits + r, l + 1);
  bits.push_back(usedBits);
  return usedBits;
}
/*
add|sub|mul|div|rem|lt|leq|gt|geq|eq|neq|dshl|dshr|and|or|xor|cat
*/
void bits_2expr(Node* node, int parentBits, int opIdx) {
  PNode* op = node->workingVal->ops[opIdx];
  int usedBits1 = CHILD_BITS, usedBits2 = CHILD_BITS;
  if (op->name == "and" || op->name == "or" || op->name == "xor") {
    usedBits1 = usedBits2 = parentBits;
  } else if (op->name == "cat") {
    // TODO
  }
  bits.push_back(usedBits2);
  bits.push_back(usedBits1);
}
/*
asUInt|asSInt|asClock|asAsyncReset|cvt|neg|not|andr|orr|xorr
*/
void bits_1expr(Node* node, int parentBits, int opIdx) {
  PNode* op = node->workingVal->ops[opIdx];
  int usedBits = CHILD_BITS;
  if (op->name == "asUInt" || op->name == "asSInt" || op->name == "cvt" || op->name == "not") {
    usedBits = parentBits;
  } else if (op->name == "neg") {
    usedBits = parentBits - 1;
  }
  bits.push_back(usedBits);
}

void setUsedBits(Node* node, int bits) {
  if (bits == CHILD_BITS) bits = node->width;
  node->usedBits = MIN(node->width, MAX(node->usedBits, bits));
}

void getBits(Node* node) {
  if (node->status == CONSTANT_NODE && node->dimension.size() == 0) return;
  if (node->workingVal->ops.size() == 0 && node->workingVal->operands.size() == 0) {
    return;
  }
  int operandIdx = 0;
  if (node->usedBits == -1) node->usedBits = node->width;
  bits.push_back(node->usedBits);
  for (int opIdx = 0; opIdx < (int)node->workingVal->ops.size(); opIdx ++) {
    if (!node->workingVal->ops[opIdx]) { // update operands
      if (popOperand) setUsedBits(node->workingVal->operands[operandIdx ++], popBits());
      node->opBits.push_back(CHILD_BITS);
      popOperand = true;
      continue;
    }
    PNode* op = node->workingVal->ops[opIdx];
    int opBits = popBits();
    node->opBits.push_back(opBits == CHILD_BITS ? op->width : opBits);
    switch(op->type) {
      case P_1EXPR1INT: bits_1expr1int(node, opBits, opIdx); break;
      case P_1EXPR2INT: bits_1expr2int(node, opBits, opIdx); break;
      case P_2EXPR: bits_2expr(node, opBits, opIdx); break;
      case P_1EXPR: bits_1expr(node, opBits, opIdx); break;
      case P_EXPR_MUX:
      case P_WHEN:
        bits.push_back(opBits); bits.push_back(opBits); bits.push_back(1);
        break;
      case P_EXPR_INT_INIT:
        popOperand = false;
        break;
      case P_PRINTF:
        bits.push_back(1);
        for (int i = 0; i < op->getChild(2)->getChildNum(); i ++) {
          bits.push_back(CHILD_BITS);
        }
        break;
      case P_ASSERT:
        bits.push_back(1);
        bits.push_back(1);
        break;
      case P_INDEX:
        if (popOperand) {
          setUsedBits(node->workingVal->operands[operandIdx ++], opBits);
          popOperand = true;
        }
        bits.push_back(CHILD_BITS);
        break;
      case P_CONS_INDEX:
        bits.push_back(opBits);
        break;
      case P_L_CONS_INDEX:
        bits.push_back(opBits);
        break;
      case P_L_INDEX:
        bits.push_back(opBits);
        bits.push_back(CHILD_BITS);
        break;
      default:
        std::cout << node->name << " " << op->type << std::endl;
        Panic();
    }
  }

  if (operandIdx != (int)node->workingVal->operands.size()) {
    Assert(node->workingVal->operands.size() - operandIdx == bits.size(), "operandIdx %d size %ld in node %s\n", operandIdx, node->workingVal->operands.size(), node->name.c_str());
    for (size_t i = 0; i < bits.size(); i ++)
      setUsedBits(node->workingVal->operands[operandIdx ++], popBits());
  }
  popOperand = true;
  Assert(bits.size() == 0, "bits is not empty after %s\n", node->name.c_str());
  Assert(node->opBits.size() == node->workingVal->ops.size(), "opBits size %ld opsize %ld in node %s\n", node->opBits.size(), node->workingVal->ops.size(), node->name.c_str());
}

void usedBits(graph* g) {
  for (int i = g->sorted.size() - 1; i >= 0; i --) {
    if (g->sorted[i]->status == CONSTANT_NODE) continue;
    switch (g->sorted[i]->type) {
      case NODE_READER:
      case NODE_WRITER:
        for (Node* node : g->sorted[i]->member) {
          if (node->status == CONSTANT_NODE) continue;
          getBits(node);
        }
        break;
      case NODE_REG_DST:
        g->sorted[i]->usedBits = g->sorted[i]->width;
        if (g->sorted[i]->dimension.size() != 0) {
          for (Node* member : g->sorted[i]->member) {
            if (member->iValue)
              member->workingVal = member->iValue;
            getBits(member);
            member->workingVal = member->value;
          }
        }
        g->sorted[i]->workingVal = g->sorted[i]->iValue;
        getBits(g->sorted[i]);
        g->sorted[i]->workingVal = g->sorted[i]->value;
        break;
      default:
        if (g->sorted[i]->dimension.size() != 0) {
          for (Node* member : g->sorted[i]->member) getBits(member);
        }
        getBits(g->sorted[i]);
    }
  }
  for (Node* active : g->active) {
    getBits(active);
  }
  for (Node* n : g->sorted) {
    switch (n->type) {
      case NODE_REG_DST:
      case NODE_REG_SRC:
        continue;
      case NODE_OTHERS:
        n->width = n->usedBits;
        
    }
  }
}
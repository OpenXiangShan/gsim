#include <graph.h>
#include <common.h>
#include <set>

#define IS_RW(node) (node->type == NODE_READER || node->type == NODE_WRITER || node->type == NODE_READWRITER)

void updateOperand(Node* oldNode, Node* newNode, Node* dst) {
  auto ptr = find(dst->workingVal->operands.begin(), dst->workingVal->operands.end(), oldNode);
  if (ptr == dst->workingVal->operands.end()) {
    Assert((dst->type == NODE_MEMBER || dst->type == NODE_L1_RDATA)|| IS_RW(dst) || dst->type == NODE_ARRAY_MEMBER || dst->dimension.size() != 0, "%s not found in %s %d\n", oldNode->name.c_str(), dst->name.c_str(), dst->type);
    return;
  }
  while (ptr != dst->workingVal->operands.end()) {
    *ptr = newNode;
    ptr = find(dst->workingVal->operands.begin(), dst->workingVal->operands.end(), oldNode);
  }
}

void updateConnect(Node* oldNode, Node* newNode, Node* dst) {
  if(dst->prev.find(oldNode) != dst->prev.end()) {
    dst->prev.erase(oldNode);
    dst->prev.insert(newNode);
    // oldNode->next.erase(dst);
    newNode->next.insert(dst);
  }
}

void replacePrevNode(Node* oldNode, Node* newNode, Node* dst) {
  auto ptr = dst->next.end();
  if((ptr = find(dst->next.begin(), dst->next.end(), oldNode)) != dst->next.end()) {
    dst->next.erase(ptr);
  }
}
/*
A -> |         | -> E
     | C  -> D |
B -> |         | -> F
      new   old
*/

void markAlias(Node* node) {
  Node* oldNode = node;
  Node* newNode = node->workingVal->operands[0];

  if (oldNode->width != newNode->width) return; // TODO
  oldNode->type = NODE_INVALID;
  // delete edge prev->old
  for (Node* prev : oldNode->prev) {
    replacePrevNode(oldNode, newNode, prev);
    if (oldNode->dimension.size() != 0 || IS_RW(prev)) {
      for (Node* member : prev->member) replacePrevNode(oldNode, newNode, member);
    }
  }
  // update prev & operands in old.next
  newNode->next.erase(oldNode);
  newNode->master->next.erase(oldNode->master);
  for (Node* next : oldNode->next) {
    updateOperand(oldNode, newNode, next);
    updateConnect(oldNode, newNode, next);
    if (next->dimension.size() != 0 || IS_RW(next)) {
      for (Node* member : next->member) {
        updateOperand(oldNode, newNode, member);
      }
    }
    newNode->master->next.insert(next->master);
  }

 }

bool aliasArray(Node* node) {
  if (node->workingVal->ops.size() != 0 || node->workingVal->operands.size() != 1) return false;
  for (Node* member : node->member) {
    if (member->workingVal->ops.size() != 0 || member->workingVal->operands.size() != 0) return false;
  }
  return true;
}

void aliasAnalysis(graph* g) {
  for (size_t i = 0; i < g->sorted.size(); i ++) {
    Node* node = g->sorted[i];
    switch(node->type) {
      case NODE_OTHERS:
        if (node->dimension.size() != 0 && !aliasArray(node)) continue;
        if (node->workingVal->ops.size() == 0 && node->workingVal->operands.size() == 1) {
          markAlias(node);
        }
        break;
      default:
        if (node->workingVal->ops.size() == 0 && node->workingVal->operands.size() == 1) {
          std::cout << "Node " << node->name << " potential alias" << std::endl;
        }
    }
  }
  g->sorted.erase(std::remove_if(g->sorted.begin(), g->sorted.end(), [](const Node* n) { return n->type == NODE_INVALID; }),
                  g->sorted.end());
#if 0
  for (Node* superNode : g->superNodes) {
    std::cout << superNode->name << " " << superNode->id << ": ";
    for (Node* member : superNode->setOrder) std::cout << member->name <<  "(" << member->id << ", " << (member->type == NODE_INVALID ? "invalid " : "valid ") << member->prev.size()<< " " << member->next.size() << ") ";
    std::cout << "\nprev: " << std::endl;
    for (Node* prevSuper : superNode->prev) std::cout << "  " <<prevSuper->name << " " << prevSuper->id << std::endl;
    std::cout << "next: " << std::endl;
    for (Node* nextSuper : superNode->next) std::cout << "  " <<nextSuper->name << " " << nextSuper->id << std::endl;
  }
#endif
}
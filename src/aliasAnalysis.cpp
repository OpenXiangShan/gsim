#include <graph.h>
#include <common.h>
#include <set>

#define IS_RW(node) (node->type == NODE_READER || node->type == NODE_WRITER || node->type == NODE_READWRITER)

void replaceNode(Node* oldNode, Node* newNode, Node* dst) {
  auto ptr = find(dst->workingVal->operands.begin(), dst->workingVal->operands.end(), oldNode);
  if (ptr == dst->workingVal->operands.end()) {
    Assert(dst->type == NODE_MEMBER || IS_RW(dst) || dst->type == NODE_ARRAY_MEMBER || dst->dimension.size() != 0, "%s not found in %s\n", oldNode->name.c_str(), dst->name.c_str());
    return;
  }
  while (ptr != dst->workingVal->operands.end()) {
    *ptr = newNode;
    ptr = find(dst->workingVal->operands.begin(), dst->workingVal->operands.end(), oldNode);
  }
  auto prevPtr = dst->prev.end();
  while ((prevPtr = find(dst->prev.begin(), dst->prev.end(), oldNode)) != dst->prev.end()) {
    *prevPtr = newNode;
  }
}

void replacePrevNode(Node* oldNode, Node* dst) {
  auto ptr = dst->next.end();
  while((ptr = find(dst->next.begin(), dst->next.end(), oldNode)) != dst->next.end()) {
    dst->next.erase(ptr);
  }
}

void markAlias(Node* node) {
  Node* replace = node->workingVal->operands[0];
  if (node->width != replace->width) return; // TODO
  node->type = NODE_INVALID;
  for (Node* prev : node->prev) {
    replacePrevNode(node, prev);
    if (node->dimension.size() != 0 || IS_RW(prev)) {
      for (Node* member : prev->member) replacePrevNode(node, member);
    }
    prev->next.insert(prev->next.end(), node->next.begin(), node->next.end());
  }
  std::set<Node*> nextNodes;
  for (Node* next : node->next) {
    if (nextNodes.find(next) != nextNodes.end()) continue;
    replaceNode(node, replace, next);
    nextNodes.insert(next);
    if (node->dimension.size() != 0 || IS_RW(next)) {
      for (Node* member : next->member) {
        if (nextNodes.find(member) != nextNodes.end()) continue;
        replaceNode(node, replace, member);
        nextNodes.insert(member);
      }
    }
  }
}

void aliasAnalysis(graph* g) {
  for (size_t i = 0; i < g->sorted.size(); i ++) {
    Node* node = g->sorted[i];
    switch(node->type) {
      case NODE_OTHERS:
        if (node->dimension.size() != 0) continue; // TODO
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
}
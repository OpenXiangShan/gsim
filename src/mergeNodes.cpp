/**
 * @file mergeNodes.cpp
 * @brief merge Nodes
 */

#include "common.h"
#include "graph.h"
/* node -> nextNode */
void mergeToNext(Node* node) {
  Node* nextNode = *node->next.begin();
  node->clusId   = nextNode->clusId;
  if (nextNode->type != NODE_ACTIVE) {
    nextNode->prev.erase(node);
    nextNode->master->prev.erase(node->master);
    for (Node* prev : node->prev) {
      prev->next.erase(node);
      prev->next.insert(nextNode);
      prev->master->next.erase(node->master);
      prev->master->next.insert(nextNode->master);
      nextNode->master->prev.insert(prev->master);
      nextNode->prev.insert(prev);
    }
    node->master = nextNode->master;
  }
  Assert(nextNode->clusId == nextNode->id || nextNode->clusNodes.size() == 1,
          "Merge error! (%s, %d %d -> %s %d %d)\n", node->name.c_str(), node->id, node->clusId, nextNode->name.c_str(), nextNode->id, nextNode->clusId);

  Node* masterNode = nextNode->clusId == nextNode->id ? nextNode : nextNode->clusNodes[0];
  masterNode->clusNodes.push_back(node);
  node->clusNodes.push_back(masterNode);
}

void mergeToPrev(Node* node) {
  Node* prevNode = *node->prev.begin();
  prevNode->master->setOrder.push_back(node);
  Assert(std::find(node->master->setOrder.begin(), node->master->setOrder.end(), node) != node->master->setOrder.end(), "node %s -> %s %s \n", prevNode->name.c_str(), node->name.c_str(), node->master->name.c_str());
  node->master->setOrder.erase(std::find(node->master->setOrder.begin(), node->master->setOrder.end(), node));
  node->master = prevNode->master;
}

void mergeNodes(graph* g) {
  int num = 0;

  for (int i = g->sorted.size() - 1; i >= 0; i--) {
    switch (g->sorted[i]->type) {
      case NODE_OTHERS:
      case NODE_ACTIVE:
        if (g->sorted[i]->next.size() == 1 && g->sorted[i]->dimension.size() == 0 && (*g->sorted[i]->next.begin())->type != NODE_ARRAY_MEMBER) { // TODO: enable array combine
          mergeToNext(g->sorted[i]);
          num++;
        }
        else if (g->sorted[i]->prev.size() == 1) {
          std::cout << "inEdge1: " << g->sorted[i]->name << " " << g->sorted[i]->workingVal->ops.size() << " " << g->sorted[i]->workingVal->operands.size() << std::endl;
        }
        break;
      default:
        if(g->sorted[i]->next.size() == 1) {
          std::cout << "Node " << g->sorted[i]->name << " with single next. Type: " << g->sorted[i]->type << std::endl;
        }
      break;
    }
  }

  for (size_t i = 0; i < g->sorted.size(); i++) {
    if (g->sorted[i]->clusId != g->sorted[i]->id) continue;
    switch (g->sorted[i]->type) {
      case NODE_OTHERS:
        if (g->sorted[i]->prev.size() == 1 && (*g->sorted[i]->prev.begin())->type == NODE_OTHERS && g->sorted[i]->dimension.size() == 0 && (*g->sorted[i]->next.begin())->type != NODE_ARRAY_MEMBER) { // TODO: enable array combine
          mergeToPrev(g->sorted[i]);
          num++;
        }
    }
  }

  std::cout << "merge " << num << " nodes (" << g->sorted.size() << ")\n";

  g->sorted.erase(std::remove_if(g->sorted.begin(), g->sorted.end(), [](const Node* n) { return n->clusId != n->id; }),
                  g->sorted.end());
#if 0
  for (Node* superNode : g->superNodes) {
    std::cout << superNode->name << " " << superNode->id << ": ";
    for (Node* member : superNode->setOrder) std::cout << member->name <<  "(" << member->id << ", " << (member->clusId == member->id ? "valid" : "invalid") << ") ";
    std::cout << "\nprev: " << std::endl;
    for (Node* prevSuper : superNode->prev) std::cout << "  " <<prevSuper->name << " " << prevSuper->id << std::endl;
    std::cout << "next: " << std::endl;
    for (Node* nextSuper : superNode->next) std::cout << "  " <<nextSuper->name << " " << nextSuper->id << std::endl;
  }
  for (Node* node : g->sorted) {
    std::cout << node->name << " " << node->id << " " << node->master->name << " " << node->master->id << ": (" << node->next.size() << ")" << std::endl;
    for (Node* next : node->next) {
      std::cout << "   next " << next->name << " " << next->id << " ";
      if (next->master) std::cout << next->master->name << " " << next->master->id << std::endl;
      else std::cout << "NULL " << std::endl;
    }
  }
#endif
}

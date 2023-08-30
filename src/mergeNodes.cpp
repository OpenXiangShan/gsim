/**
 * @file mergeNodes.cpp
 * @brief merge Nodes
 */

#include "common.h"
#include "graph.h"

void mergeToNext(Node* node) {
  Node* nextNode = node->next[0];
  node->clusId   = nextNode->clusId;
  Assert(nextNode->clusId == nextNode->id || nextNode->clusNodes.size() == 1,
          "Merge error! (%s, %d %d -> %s %d %d)\n", node->name.c_str(), node->id, node->clusId, nextNode->name.c_str(), nextNode->id, nextNode->clusId);

  Node* masterNode = nextNode->clusId == nextNode->id ? nextNode : nextNode->clusNodes[0];
  masterNode->clusNodes.push_back(node);
  node->clusNodes.push_back(masterNode);
}

void mergeNodes(graph* g) {
  int num = 0;

  for (int i = g->sorted.size() - 1; i >= 0; i--) {
    switch (g->sorted[i]->type) {
      case NODE_OTHERS:
      case NODE_ACTIVE:
        if (g->sorted[i]->next.size() == 1 && g->sorted[i]->dimension.size() == 0 && g->sorted[i]->next[0]->type != NODE_ARRAY_MEMBER) { // TODO: enable array combine
          mergeToNext(g->sorted[i]);
          num++;
        }
        break;

      default:
        if(g->sorted[i]->next.size() == 1) {
          std::cout << "Node " << g->sorted[i]->name << " with single next. Type: " << g->sorted[i]->type << std::endl;
        }
      break;
    }
  }

  std::cout << "merge " << num << " nodes (" << g->sorted.size() << ")\n";

  g->sorted.erase(std::remove_if(g->sorted.begin(), g->sorted.end(), [](const Node* n) { return n->clusId != n->id; }),
                  g->sorted.end());
}

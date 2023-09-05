#include "graph.h"
#include "common.h"
#include <set>
#include <map>

static std::set<Node*> allNodes;

// (pseudo-)toposort & merge array
void sortMergeArray(graph* g) {
  std::vector<Node*> s(g->sources);
  s.insert(s.end(), g->input.begin(), g->input.end());
  s.insert(s.end(), g->constant.begin(), g->constant.end());
  s.insert(s.end(), g->memRdata1.begin(), g->memRdata1.end());

  allNodes.insert(s.begin(), s.end());
  for (Node* n : allNodes) n->scratch = n->inEdge;

  std::map<Node*, std::set<Node*>> prevArray;  // prevArray[n]: arrays that can reach n

  while (!s.empty()) {
    Node* top = s.back();
    s.pop_back();
    for (Node* next : top->next) {
      if (allNodes.find(next) == allNodes.end()) {
        next->scratch = next->inEdge;
        allNodes.insert(next);
      }
      next->scratch --;
      if (!next->scratch && (next->type != NODE_REG_DST && next->type != NODE_ACTIVE)) s.push_back(next);
      if (next->type == NODE_REG_DST || next->type == NODE_ACTIVE) continue;
      if (top->type == NODE_ARRAY_MEMBER) prevArray[next].insert(top->parent);
      else if (top->dimension.size() != 0) prevArray[next].insert(top);
      for (Node* prev : next->prev) {
        prevArray[next].insert(prevArray[prev].begin(), prevArray[prev].end());
      }
    }
  }

  for (auto iter : prevArray) {
    Node* realNode = iter.first;
    if (realNode->type != NODE_ARRAY_MEMBER && realNode->dimension.size() == 0) continue;
    if (realNode->type == NODE_ARRAY_MEMBER) realNode = realNode->parent;
    if (iter.second.find(realNode) != iter.second.end()) realNode->arraySplit = true;
  }

  int spiltNum = 0;
  for (Node* array : g->array) {
    if (array->arraySplit) {
      for (Node* member :array->member) member->type = NODE_OTHERS;
      spiltNum ++;
    } else {
      for (Node* member : array->member) {
        member->name = array->name;
        array->next.insert(array->next.end(), member->next.begin(), member->next.end());
        // for (Node* next : array->next) {
        //   *(find(next->prev.begin(), next->prev.end(), member)) = array;
        // }
      }
    }
  }

  std::cout << "split(" << spiltNum << "/" << g->array.size() << ") arrys" << std::endl;
}

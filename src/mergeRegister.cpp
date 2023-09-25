#include "graph.h"
#include "common.h"
#include <set>
#include <map>


void mergeRegister(graph* g) {
  std::vector<Node*> mergedRegs;
  int num = 0;
  for (Node* reg : g->sources) {
    if (!reg->regSplit) continue;
    bool split = false;
    for (Node* next : reg->next) {
      if (next->type != NODE_REG_DST) continue;
      if (next->master->id > reg->master->id) {
        split = true;
        break;
      }
    }
    if (!split) {
      reg->regSplit = false;
      reg->status = DEAD_NODE;
      reg->regNext->name = reg->name;
      if (reg->dimension.size() != 0) {
        for (size_t i = 0; i < reg->member.size(); i ++) {
          reg->regNext->member[i]->name = reg->member[i]->name;
        }
      }
      for (Node* next : reg->next) {
        next->prev.erase(reg);
        next->prev.insert(reg->regNext);
        auto ptr = next->workingVal->operands.end();
        while ((ptr = std::find(next->workingVal->operands.begin(), next->workingVal->operands.end(), reg)) != next->workingVal->operands.end()) {
          *ptr = reg->regNext;
        }
        next->master->prev.erase(reg->master);
        next->master->prev.insert(reg->regNext->master);
        reg->regNext->master->next.insert(next->master);
      }
      reg->regNext->next.insert(reg->next.begin(), reg->next.end());
      reg->next.erase(reg->next.begin(), reg->next.end());
    } else {
      num ++;
    }
  }
  g->sorted.erase(
      std::remove_if(g->sorted.begin(), g->sorted.end(), [](const Node* n) { return n->status == DEAD_NODE; }),
      g->sorted.end());
  std::cout << "find " << num << " splitted registers" << std::endl;
}
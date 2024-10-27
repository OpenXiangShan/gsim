/*
  using dfs to detect if there is any loop
*/

#include <stack>
#include <map>
#include "common.h"

#define NOT_VISIT 0
#define EXPANDED 1
#define VISITED 2

void graph::detectLoop() {
  std::stack<SuperNode*> s;
  std::map<SuperNode*, int> states;

  for (SuperNode* node : supersrc) {
    s.push(node);
    states[node] = NOT_VISIT;
  }

  while (!s.empty()) {
    SuperNode* top = s.top();
    if (states[top] == NOT_VISIT) {
      states[top] = EXPANDED;

      for (SuperNode* next : top->next) {
        /* enable self-loop for memory reader port with latency = 0 */
        if (next == top) continue;
        if (states.find(next) == states.end()) {
          s.push(next);
          states[next] = NOT_VISIT;
        } else if (states[next] == EXPANDED) { // find an expanded loop
          printf("Detect Loop:\n");
          next->display();
          while (!s.empty()) {
            SuperNode* n = s.top();
            s.pop();
            // display all expanded nodes until n
            if (states[n] == EXPANDED) n->display();
            if (n == next) Assert(0, "-------");
          }
        }
    
      }
    } else if (states[top] == EXPANDED) {
      states[top] = VISITED;
      s.pop();
    } 
  }
  std::cout << "NO Loop!\n";
}

void graph::detectSortedSuperLoop() {
  std::stack<SuperNode*> s;
  std::map<SuperNode*, int> states;

  for (SuperNode* node : sortedSuper) {
    s.push(node);
    states[node] = NOT_VISIT;
  }

  while (!s.empty()) {
    SuperNode* top = s.top();
    if (states[top] == NOT_VISIT) {
      states[top] = EXPANDED;

      for (SuperNode* next : top->next) {
        /* enable self-loop for memory reader port with latency = 0 */
        if (next == top) continue;
        if (states.find(next) == states.end()) {
          s.push(next);
          states[next] = NOT_VISIT;
        } else if (states[next] == EXPANDED) { // find an expanded loop
          printf("Detect Loop:\n");
          next->display();
          while (!s.empty()) {
            SuperNode* n = s.top();
            s.pop();
            // display all expanded nodes until n
            if (states[n] == EXPANDED) n->display();
            if (n == next) Assert(0, "-------");
          }
        }
      }
    } else if (states[top] == EXPANDED) {
      states[top] = VISITED;
      s.pop();
    }
  }
  std::cout << "NO Loop!\n";
}

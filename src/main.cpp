#include <stdio.h>
#include "common.h"
#include "Node.h"
#include "graph.h"

void generator(graph* g, std::string header, std::string src);
void randomGraph(graph* g);

int main() {
  graph g;
  randomGraph(&g);
  generator(&g, "top", "top");
}
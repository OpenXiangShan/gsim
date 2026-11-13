#include "common.h"
#include "NodeEvolution.h"

#include <stdexcept>

int Node::counter = 0;
int ENode::counter = 0;

int main(int argc, char** argv) {
  if (argc != 3) return 2;
  const std::string mode = argv[1];
  try {
    Node first(NODE_OUT), second(NODE_OUT);
    first.name = "first";
    second.name = "second";
    first.width = second.width = 8;
    ENode shared(OP_INT);
    shared.width = 8;
    shared.strVal = "1";
    ExpTree firstTree(&shared), secondTree(&shared);
    first.assignTree.push_back(&firstTree);
    second.assignTree.push_back(&secondTree);
    NodeEvolutionTrace trace(argv[2], "ReplayNegative", "synthetic");
    trace.inventory("before", {&first, &second});
    shared.strVal = "2";
    if (mode != "missing-scalar-event")
      trace.event("rewrite", "synthetic_scalar_expression_change", {&first}, {&first});
    if (mode == "valid-shared-change")
      trace.event("rewrite", "synthetic_shared_expression_change", {&second}, {&second});
    if (mode == "retain-cannot-explain-mutation")
      trace.event("retain", "synthetic_retention", {&second}, {&second});
    trace.inventory("after", {&first, &second});
    trace.finish();
    return mode == "valid-shared-change" ? 0 : 2;
  } catch (const std::runtime_error& error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
  }
}

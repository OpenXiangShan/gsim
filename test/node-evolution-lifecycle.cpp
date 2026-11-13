#include "NodeEvolution.h"

#include <cstdio>
#include <string>

int main(int argc, char** argv) {
  if (argc != 3) return 2;
  const std::string mode = argv[1];
  NodeEvolutionTrace trace(argv[2], "Lifecycle", "synthetic");
  if (mode == "invalid-stage") trace.inventory("during", {});
  if (mode == "null-stage") trace.inventory(nullptr, {});
  if (mode == "after-before") trace.inventory("after", {});
  if (mode == "event-before") trace.event("rewrite", "synthetic", {}, {});
  if (mode == "finish-before") trace.finish();
  trace.inventory("before", {});
  if (mode == "duplicate-before") trace.inventory("before", {});
  if (mode == "finish-without-after") trace.finish();
  trace.inventory("after", {});
  if (mode == "duplicate-after") trace.inventory("after", {});
  if (mode == "event-after") trace.event("rewrite", "synthetic", {}, {});
  trace.finish();
  if (mode == "duplicate-finish") trace.finish();
  if (mode == "inventory-after-finish") trace.inventory("before", {});
  if (mode == "event-after-finish") trace.event("rewrite", "synthetic", {}, {});
  if (mode != "valid") {
    std::fprintf(stderr, "Invalid lifecycle case was accepted: %s\n", mode.c_str());
    return 2;
  }
  return 0;
}

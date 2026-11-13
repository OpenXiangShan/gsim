#ifndef NODE_EVOLUTION_H
#define NODE_EVOLUTION_H

#include <fstream>
#include <cstdint>
#include <string>
#include <vector>

class Node;
class graph;

// Optional streaming diagnostics. IDs refer to native graph entities, never names.
class NodeEvolutionTrace {
 public:
  NodeEvolutionTrace(const std::string& path, const std::string& graphName,
                     const std::string& passName, const std::string& inputSha256 = "");
  ~NodeEvolutionTrace();
  void inventory(const char* stage, const std::vector<Node*>& nodes);
  void event(const char* action, const char* reason,
             const std::vector<Node*>& before, const std::vector<Node*>& after,
             const std::string& geometry = "");
  void finish();

 private:
  enum class State { BeforeInventory, Events, AfterInventory, Finished };
  struct Step {
    std::string action;
    std::string reason;
    std::vector<int> before;
    std::vector<int> after;
    std::string geometry;
  };
  std::ofstream out;
  // Boundary bodies are spooled instead of retaining millions of expressions.
  std::fstream beforeSpool;
  std::fstream mutationSpool;
  std::string beforeSpoolPath;
  std::string mutationSpoolPath;
  std::vector<uint64_t> beforeOffsets;
  std::vector<Step> steps;
  std::vector<unsigned char> boundary;
  size_t beforeCount = 0;
  size_t afterCount = 0;
  size_t eventCount = 0;
  size_t mutationCount = 0;
  State state = State::BeforeInventory;
  void check();
  void requireState(State expected, const char* operation);
};

#endif

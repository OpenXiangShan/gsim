#ifndef CPP_EMITTER_MT_H
#define CPP_EMITTER_MT_H

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

class graph;
class Node;
class SuperNode;

// Adds a dense, fixed-owner executor to the C++ emitted by the original
// single-threaded backend. Planning data stays in this object and never leaks
// into Node or SuperNode.
class CppEmitterMt {
 public:
  explicit CppEmitterMt(graph& graph);
  CppEmitterMt(const CppEmitterMt&) = delete;
  CppEmitterMt& operator=(const CppEmitterMt&) = delete;

  bool enabled() const;
  void prepare();
  void emitHeaderPreamble(FILE* header) const;
  void emitClassMembers(FILE* header) const;
  void emitConstructorInit();
  void emitConstructorStart();
  void emitDefinitions();
  void emitStep();

 private:
  struct Task {
    std::vector<int> cppIds;
    std::vector<int> predecessors;
    std::vector<int> successors;
    std::vector<int> waits;
    std::vector<int> stores;
    int cost = 0;
    int owner = 0;
    uint32_t waitBegin = 0;
    uint32_t waitEnd = 0;
    uint32_t storeBegin = 0;
    uint32_t storeEnd = 0;
  };

  struct Reset {
    struct Instruction {
      uint8_t type = 0;
      std::string text;
    };

    struct Worker {
      int id = -1;
      int chunkCount = 0;
      std::vector<Instruction> body;
    };

    SuperNode* super = nullptr;
    int id = -1;
    bool asynchronous = false;
    int chunkCount = 0;
    int triggerTask = -1;
    int triggerOwner = -1;
    std::vector<Instruction> body;
    std::vector<Worker> workers;
    std::vector<int> participants;
  };

  struct ResetJoin {
    int resetId = -1;
    size_t beforePosition = 0;
  };

  void emitText(int indent, bool canStartFile, const std::string& text);
  void emitResetBodyFunction(const std::string& name, const std::string& condition,
                             const std::vector<Reset::Instruction>& body, int chunkCount);
  void emitResetFunction(const Reset& reset);
  void emitAsyncResetWorkerFunction(const Reset& reset, const Reset::Worker& worker);
  void emitSuperNode(SuperNode* super, int indent);

  graph& graph_;
  bool enabled_ = false;
  int workerCount_ = 1;
  int maxTasks_ = 1600;
  int resetChunk_ = 4096;
  int readySlotCount_ = 1;
  std::vector<SuperNode*> byCppId_;
  std::vector<int> topologicalCppIds_;
  std::vector<Task> tasks_;
  std::vector<std::vector<int>> workerTasks_;
  std::vector<int> waitSlots_;
  std::vector<int> storeSlots_;
  std::vector<Reset> resets_;
  std::vector<std::vector<ResetJoin>> resetJoins_;
  std::map<Node*, int> asyncResetIds_;
};

#endif

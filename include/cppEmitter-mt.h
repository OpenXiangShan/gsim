#ifndef CPP_EMITTER_MT_H
#define CPP_EMITTER_MT_H

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "mtTaskPartition.h"
#include "mtTaskSchedule.h"

class graph;
class Node;
class SuperNode;

// Adds a dense, fixed-owner executor to the C++ emitted by the original
// single-threaded backend. Planning data stays in this object and never leaks
// into Node or SuperNode.
class CppEmitterMt : private MtTaskPlan, private MtWorkerPlan {
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
  void emitRegisterInitialization();
  void emitDefinitions();
  void emitStep();
  bool isTaskLocal(Node* node) const;
  bool isWorkerLocal(Node* node) const;
  int workerLocalOwner(Node* node) const;
  bool isPackedRegister(Node* node) const;
  std::string packedRegisterName(Node* node) const;
  const std::vector<Node*>& emissionNodes() const;

 private:
  using Task = MtTask;
  using RegisterUpdate = MtRegisterUpdate;
  using Reset = MtReset;
  using ResetJoin = MtResetJoin;

  void emitText(int indent, bool canStartFile, const std::string& text);
  void emitResetBodyFunction(const std::string& name, const std::string& condition,
                             const std::vector<Reset::Instruction>& body, int chunkCount);
  void emitResetFunction(const Reset& reset);
  void emitAsyncResetWorkerFunction(const Reset& reset, const Reset::Worker& worker);
  void emitRegisterUpdateFunction(const RegisterUpdate& update);
  void emitInstructions(const std::vector<InstInfo>& instructions, int indent);
  void emitTask(const Task& task, int indent);
  void buildRegisterStorageNames();
  std::string mapRegisterNames(const std::string& text) const;

  graph& graph_;
  bool enabled_ = false;
  int workerCount_ = 1;
  int targetTasks_ = 1600;
  int resetChunk_ = 4096;
  std::map<Node*, std::string> packedRegisterNames_;
  std::unordered_map<std::string, std::string> packedRegisterNamesByText_;
};

#endif

#ifndef EXECUTABLE_GRH_EFFECTS_H
#define EXECUTABLE_GRH_EFFECTS_H

#include <cstdint>
#include <string>
#include <vector>

class ENode;
class Node;

namespace executable_grh {

enum class EffectKind {
  Printf,
  Assert,
  Exit,
};

enum class EffectPlanStatus {
  Supported,
  OptimizerElided,
  Unsupported,
};

struct EffectGuardTerm {
  const ENode* expression = nullptr;
  bool expectedValue = true;
};

struct EffectPlan {
  EffectPlanStatus status = EffectPlanStatus::Unsupported;
  EffectKind kind = EffectKind::Printf;
  std::string systemTaskName;
  const ENode* eventClock = nullptr;
  const Node* baseClock = nullptr;
  std::vector<EffectGuardTerm> guards;
  std::vector<const ENode*> arguments;
  std::string formatLiteral;
  std::string formatText;
  bool literalizedUnboundFormatConversions = false;
  bool prependStderrHandle = false;
  bool hasExitCode = false;
  int64_t exitCode = 0;
  std::string reason;
};

// Resolve the exact NODE_SPECIAL shapes present at the pre-coarsen boundary.
// A supported plan is ready to become a posedge-triggered native GRH
// kSystemTask; an optimizer-elided plan has no remaining executable effect.
EffectPlan resolveExecutableGrhEffect(const Node& node);

const char* effectKindName(EffectKind kind);

}  // namespace executable_grh

#endif

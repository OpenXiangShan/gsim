#ifndef EXECUTABLE_GRH_EXT_REGISTRY_H
#define EXECUTABLE_GRH_EXT_REGISTRY_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace executable_grh {

enum class ExternalParamKind {
  Integer,
  String,
};

struct ExternalParamAbi {
  ExternalParamKind kind = ExternalParamKind::Integer;
  std::string value;
};

enum class ExternalMemberDirection {
  Input,
  Output,
};

struct ExternalMemberAbi {
  std::string name;
  ExternalMemberDirection direction = ExternalMemberDirection::Input;
  int64_t width = 0;
  bool isSigned = false;
  std::vector<int64_t> dimensions;
};

struct ExternalModuleAbi {
  std::string defname;
  std::vector<ExternalParamAbi> parameters;
  std::vector<ExternalMemberAbi> members;
  bool hasClock = false;
  std::string clockName;
};

enum class DpiArgDirection {
  Input,
  Output,
};

enum class DpiValueSourceKind {
  Parameter,
  Member,
  Literal,
};

struct DpiValueSource {
  DpiValueSourceKind kind = DpiValueSourceKind::Member;
  std::size_t index = 0;
  std::size_t element = 0;
  std::string literal;
};

struct DpiArgumentPlan {
  std::string name;
  DpiArgDirection direction = DpiArgDirection::Input;
  int64_t width = 0;
  bool isSigned = false;
  std::string type = "logic";
  DpiValueSource source;
};

enum class DpiEventEdge {
  None,
  Posedge,
  Negedge,
};

inline constexpr std::size_t kNoExternalMember = static_cast<std::size_t>(-1);

struct DpiCallPlan {
  std::string importSymbol;
  std::vector<DpiArgumentPlan> arguments;
  // All listed members must be true. An empty list means constant true.
  std::vector<std::size_t> conditionMembers;
  DpiEventEdge eventEdge = DpiEventEdge::None;
  bool useModuleClock = false;
  bool hasReturn = false;
  int64_t returnWidth = 0;
  bool returnSigned = false;
  std::string returnType = "logic";
  std::size_t returnMember = kNoExternalMember;
  // Empty means the target keeps its prior value while the condition is false.
  std::string inactiveReturnLiteral;
};

struct ExternalOutputConstant {
  std::size_t member = kNoExternalMember;
  std::string literal;
};

enum class ExternalModulePlanStatus {
  Supported,
  RequiresWrapper,
  Unsupported,
};

enum class ExternalModuleExecutionProfile {
  // Preserve the behavior of the original external RTL / DPI implementation.
  FullFidelity,
  // Match testcase/xiangshan/difftest/src/test/csrc/gsim/unimpl-blackbox.cpp.
  // This is intentionally opt-in: it disables SimJTAG and drops the one-shot
  // PrintCommitIDModule diagnostic used by the RTL simulation.
  XiangShanGsimCoremarkStub,
};

struct ExternalModuleDpiPlan {
  ExternalModulePlanStatus status = ExternalModulePlanStatus::Unsupported;
  std::vector<DpiCallPlan> calls;
  std::vector<ExternalOutputConstant> outputConstants;
  std::vector<std::size_t> ignoredParameters;
  std::vector<std::size_t> ignoredMembers;
  // Non-empty only when status == RequiresWrapper. The executable graph must
  // retain or replace this wrapper before export can be considered lossless.
  std::string requiredWrapper;
  std::string reason;
};

// Resolve only ABIs whose actual XiangShan DPI implementation has been audited.
// Unknown definitions fail closed instead of assuming defname is a C link symbol.
ExternalModuleDpiPlan resolveKnownXiangShanExternalModule(
    const ExternalModuleAbi& module,
    ExternalModuleExecutionProfile profile = ExternalModuleExecutionProfile::FullFidelity);

const char* dpiEventEdgeName(DpiEventEdge edge);

}  // namespace executable_grh

#endif

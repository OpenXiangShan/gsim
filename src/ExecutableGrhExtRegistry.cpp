#include "ExecutableGrhExtRegistry.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <sstream>

namespace executable_grh {
namespace {

ExternalModuleDpiPlan unsupported(std::string reason) {
  ExternalModuleDpiPlan plan;
  plan.reason = std::move(reason);
  return plan;
}

ExternalModuleDpiPlan supported() {
  ExternalModuleDpiPlan plan;
  plan.status = ExternalModulePlanStatus::Supported;
  return plan;
}

ExternalModuleDpiPlan requiresWrapper(std::string wrapper, std::string reason) {
  ExternalModuleDpiPlan plan;
  plan.status = ExternalModulePlanStatus::RequiresWrapper;
  plan.requiredWrapper = std::move(wrapper);
  plan.reason = std::move(reason);
  return plan;
}

std::string normalizePath(const std::string& name) {
  std::string normalized;
  normalized.reserve(name.size());
  for (std::size_t i = 0; i < name.size();) {
    if (name.compare(i, 7, "__DOT__") == 0) {
      normalized.push_back('.');
      i += 7;
    } else if (name.compare(i, 2, "$$") == 0) {
      normalized.push_back('.');
      i += 2;
    } else if (name[i] == '$') {
      normalized.push_back('.');
      ++i;
    } else {
      normalized.push_back(name[i]);
      ++i;
    }
  }
  normalized.erase(std::unique(normalized.begin(), normalized.end(),
                               [](char lhs, char rhs) { return lhs == '.' && rhs == '.'; }),
                   normalized.end());
  return normalized;
}

bool hasPathSuffix(const std::string& name, const std::string& suffix) {
  const std::string normalized = normalizePath(name);
  if (normalized == suffix) return true;
  if (normalized.size() <= suffix.size()) return false;
  const std::size_t start = normalized.size() - suffix.size();
  return normalized[start - 1] == '.' && normalized.compare(start, suffix.size(), suffix) == 0;
}

bool isScalar(const ExternalMemberAbi& member) {
  if (member.dimensions.empty()) return true;
  return std::all_of(member.dimensions.begin(), member.dimensions.end(),
                     [](int64_t dimension) { return dimension == 1; });
}

std::string describeMember(const ExternalMemberAbi& member, std::size_t index) {
  std::ostringstream stream;
  stream << "member[" << index << "] '" << member.name << "'";
  return stream.str();
}

std::optional<std::string> validateMember(const ExternalModuleAbi& module,
                                          std::size_t index,
                                          const std::string& suffix,
                                          ExternalMemberDirection direction,
                                          int64_t width) {
  if (index >= module.members.size()) return "missing member '" + suffix + "'";
  const ExternalMemberAbi& member = module.members[index];
  if (!hasPathSuffix(member.name, suffix)) {
    return describeMember(member, index) + " does not end in '" + suffix + "'";
  }
  if (member.direction != direction) {
    return describeMember(member, index) + " has the wrong direction";
  }
  if (member.width != width) {
    return describeMember(member, index) + " has width " + std::to_string(member.width) +
           ", expected " + std::to_string(width);
  }
  if (!isScalar(member)) {
    return describeMember(member, index) + " is not scalar (only unit dimensions are accepted)";
  }
  return std::nullopt;
}

DpiValueSource memberSource(std::size_t index) {
  DpiValueSource source;
  source.kind = DpiValueSourceKind::Member;
  source.index = index;
  return source;
}

DpiArgumentPlan inputArg(std::string name,
                         std::string type,
                         int64_t width,
                         bool isSigned,
                         std::size_t member) {
  DpiArgumentPlan argument;
  argument.name = std::move(name);
  argument.direction = DpiArgDirection::Input;
  argument.width = width;
  argument.isSigned = isSigned;
  argument.type = std::move(type);
  argument.source = memberSource(member);
  return argument;
}

DpiArgumentPlan outputArg(std::string name,
                          std::string type,
                          int64_t width,
                          bool isSigned,
                          std::size_t member) {
  DpiArgumentPlan argument = inputArg(std::move(name), std::move(type), width, isSigned, member);
  argument.direction = DpiArgDirection::Output;
  return argument;
}

bool requireClock(const ExternalModuleAbi& module, ExternalModuleDpiPlan& plan) {
  if (module.hasClock && !module.clockName.empty()) return true;
  plan = unsupported(module.defname + " requires a named captured Clock port");
  return false;
}

ExternalModuleDpiPlan resolveFlash(const ExternalModuleAbi& module) {
  if (!module.parameters.empty()) return unsupported("FlashHelper must not have parameters");
  if (module.members.size() != 3) return unsupported("FlashHelper must have exactly 3 non-clock members");
  ExternalModuleDpiPlan plan = supported();
  if (!requireClock(module, plan)) return plan;
  if (auto error = validateMember(module, 0, "r.en", ExternalMemberDirection::Input, 1))
    return unsupported(*error);
  if (auto error = validateMember(module, 1, "r.addr", ExternalMemberDirection::Input, 32))
    return unsupported(*error);
  if (auto error = validateMember(module, 2, "r.data", ExternalMemberDirection::Output, 64))
    return unsupported(*error);

  DpiCallPlan call;
  call.importSymbol = "flash_read";
  call.arguments.push_back(inputArg("addr", "int", 32, false, 1));
  call.arguments.push_back(outputArg("data", "longint", 64, false, 2));
  call.conditionMembers = {0};
  call.eventEdge = DpiEventEdge::Posedge;
  call.useModuleClock = true;
  plan.calls.push_back(std::move(call));
  return plan;
}

ExternalModuleDpiPlan resolveSdCard(const ExternalModuleAbi& module) {
  if (!module.parameters.empty()) return unsupported("SDCardHelper must not have parameters");
  if (module.members.size() != 4) return unsupported("SDCardHelper must have exactly 4 non-clock members");
  ExternalModuleDpiPlan plan = supported();
  if (!requireClock(module, plan)) return plan;
  if (auto error = validateMember(module, 0, "io.setAddr", ExternalMemberDirection::Input, 1))
    return unsupported(*error);
  if (auto error = validateMember(module, 1, "io.addr", ExternalMemberDirection::Input, 32))
    return unsupported(*error);
  if (auto error = validateMember(module, 2, "io.ren", ExternalMemberDirection::Input, 1))
    return unsupported(*error);
  if (auto error = validateMember(module, 3, "io.data", ExternalMemberDirection::Output, 32))
    return unsupported(*error);

  DpiCallPlan setAddress;
  setAddress.importSymbol = "sd_setaddr";
  setAddress.arguments.push_back(inputArg("addr", "int", 32, true, 1));
  setAddress.conditionMembers = {0};
  setAddress.eventEdge = DpiEventEdge::Posedge;
  setAddress.useModuleClock = true;
  plan.calls.push_back(std::move(setAddress));

  DpiCallPlan read;
  read.importSymbol = "sd_read";
  read.arguments.push_back(outputArg("data", "int", 32, true, 3));
  read.conditionMembers = {2};
  read.eventEdge = DpiEventEdge::Negedge;
  read.useModuleClock = true;
  plan.calls.push_back(std::move(read));
  return plan;
}

ExternalModuleDpiPlan resolveMemoryHelper(const ExternalModuleAbi& module) {
  if (module.parameters.size() != 1 || module.parameters[0].kind != ExternalParamKind::Integer) {
    return unsupported("Mem1R1WHelper requires one integer RAM_SIZE parameter");
  }
  if (module.members.size() != 8) {
    return unsupported("Mem1R1WHelper must have exactly 8 non-clock leaf members");
  }
  ExternalModuleDpiPlan plan = supported();
  if (!requireClock(module, plan)) return plan;
  const struct {
    const char* suffix;
    ExternalMemberDirection direction;
    int64_t width;
  } expected[] = {
      {"r.enable", ExternalMemberDirection::Input, 1},
      {"r.index", ExternalMemberDirection::Input, 64},
      {"r.data", ExternalMemberDirection::Output, 64},
      {"r.async", ExternalMemberDirection::Output, 1},
      {"w.enable", ExternalMemberDirection::Input, 1},
      {"w.index", ExternalMemberDirection::Input, 64},
      {"w.data", ExternalMemberDirection::Input, 64},
      {"w.mask", ExternalMemberDirection::Input, 64},
  };
  for (std::size_t i = 0; i < module.members.size(); ++i) {
    if (auto error = validateMember(module, i, expected[i].suffix,
                                    expected[i].direction, expected[i].width)) {
      return unsupported(*error);
    }
  }

  DpiCallPlan read;
  read.importSymbol = "difftest_ram_read";
  read.arguments.push_back(inputArg("rIdx", "longint", 64, true, 1));
  read.conditionMembers = {0};
  read.hasReturn = true;
  read.returnWidth = 64;
  read.returnSigned = true;
  read.returnType = "longint";
  read.returnMember = 2;
  read.inactiveReturnLiteral = "64'b0";
  plan.calls.push_back(std::move(read));

  DpiCallPlan write;
  write.importSymbol = "difftest_ram_write";
  write.arguments.push_back(inputArg("index", "longint", 64, true, 5));
  write.arguments.push_back(inputArg("data", "longint", 64, true, 6));
  write.arguments.push_back(inputArg("mask", "longint", 64, true, 7));
  write.conditionMembers = {4};
  write.eventEdge = DpiEventEdge::Posedge;
  write.useModuleClock = true;
  plan.calls.push_back(std::move(write));

  plan.outputConstants.push_back({3, "1'b1"});
  plan.ignoredParameters.push_back(0);
  return plan;
}

DpiArgumentPlan difftestArgument(std::size_t ordinal, std::size_t member,
                                 std::size_t element,
                                 const ExternalMemberAbi& abi) {
  std::string type = "logic";
  int64_t width = abi.width;
  bool isSigned = false;
  if (abi.width == 1) {
    // Keep scalar bits as bool-compatible DPI arguments.
  } else if (abi.width <= 8) {
    type = "byte";
    width = 8;
    isSigned = true;
  } else if (abi.width <= 16) {
    type = "shortint";
    width = 16;
    isSigned = true;
  } else if (abi.width <= 32) {
    type = "int";
    width = 32;
    isSigned = true;
  } else {
    type = "longint";
    width = 64;
    isSigned = true;
  }
  DpiArgumentPlan argument =
      inputArg("arg" + std::to_string(ordinal), type, width, isSigned, member);
  argument.source.element = element;
  return argument;
}

bool difftestOmitsValidMember(const std::string& defname) {
  // These audited snapshot/state adapters have no io.valid member. Their
  // enclosing DummyDPICWrapper has already folded wrapper valid/reset into
  // the common external `enable` input.
  return defname == "DiffExtTrapEvent" ||
         defname == "DiffExtCSRState" ||
         defname == "DiffExtDebugMode" ||
         defname == "DiffExtTriggerCSRState" ||
         defname == "DiffExtVecCSRState" ||
         defname == "DiffExtFpCSRState" ||
         defname == "DiffExtHCSRState" ||
         defname == "DiffExtArchIntRegState" ||
         defname == "DiffExtArchFpRegState" ||
         defname == "DiffExtArchVecRegState";
}

bool isAuditedDifftestDefname(const std::string& defname) {
  return defname == "DiffExtRefillEvent" ||
         defname == "DiffExtL1TLBEvent" ||
         defname == "DiffExtInstrCommit" ||
         defname == "DiffExtTrapEvent" ||
         defname == "DiffExtArchEvent" ||
         defname == "DiffExtCriticalErrorEvent" ||
         defname == "DiffExtCSRState" ||
         defname == "DiffExtDebugMode" ||
         defname == "DiffExtTriggerCSRState" ||
         defname == "DiffExtVecCSRState" ||
         defname == "DiffExtFpCSRState" ||
         defname == "DiffExtHCSRState" ||
         defname == "DiffExtNonRegInterruptPendingEvent" ||
         defname == "DiffExtMhpmeventOverflowEvent" ||
         defname == "DiffExtSyncAIAEvent" ||
         defname == "DiffExtSyncCustomMflushpwrEvent" ||
         defname == "DiffExtUncacheMMStoreEvent" ||
         defname == "DiffExtL2TLBEvent" ||
         defname == "DiffExtAtomicEvent" ||
         defname == "DiffExtLrScEvent" ||
         defname == "DiffExtCMOInvalEvent" ||
         defname == "DiffExtSbufferEvent" ||
         defname == "DiffExtStoreEvent" ||
         defname == "DiffExtArchIntRegState" ||
         defname == "DiffExtArchFpRegState" ||
         defname == "DiffExtArchVecRegState" ||
         defname == "DiffExtCommitData";
}

ExternalModuleDpiPlan resolveDifftest(const ExternalModuleAbi& module) {
  if (!isAuditedDifftestDefname(module.defname)) {
    return unsupported("no audited XiangShan DPI mapping for external defname '" +
                       module.defname + "'");
  }
  if (!module.parameters.empty()) return unsupported(module.defname + " must not have parameters");
  const bool omitsValid = difftestOmitsValidMember(module.defname);
  const std::size_t payloadBegin = omitsValid ? 1 : 2;
  if (module.members.size() <= payloadBegin) {
    return unsupported(
        module.defname + (omitsValid
            ? " must contain enable and payload members"
            : " must contain enable, io.valid, and payload members"));
  }
  ExternalModuleDpiPlan plan = supported();
  if (!requireClock(module, plan)) return plan;
  if (auto error = validateMember(module, 0, "enable", ExternalMemberDirection::Input, 1))
    return unsupported(*error);

  std::size_t archRegisterCount = 0;
  if (module.defname == "DiffExtArchIntRegState" ||
      module.defname == "DiffExtArchFpRegState") {
    archRegisterCount = 32;
  } else if (module.defname == "DiffExtArchVecRegState") {
    archRegisterCount = 64;
  }
  if (archRegisterCount != 0) {
    if (module.members.size() != 3) {
      return unsupported(module.defname + " must have exactly enable, io.value, and io.coreid");
    }
    const ExternalMemberAbi& value = module.members[1];
    if (!hasPathSuffix(value.name, "io.value") ||
        value.direction != ExternalMemberDirection::Input || value.width != 64 ||
        value.dimensions != std::vector<int64_t>{static_cast<int64_t>(archRegisterCount)}) {
      return unsupported(describeMember(value, 1) +
                         " must be the audited UInt<64> io.value register array");
    }
    if (auto error = validateMember(module, 2, "io.coreid",
                                    ExternalMemberDirection::Input, 8)) {
      return unsupported(*error);
    }
  }
  if (omitsValid) {
    if (hasPathSuffix(module.members[1].name, "io.valid")) {
      return unsupported(module.defname + " audited adapter must not contain io.valid");
    }
  } else {
    if (auto error = validateMember(module, 1, "io.valid", ExternalMemberDirection::Input, 1))
      return unsupported(*error);
  }

  DpiCallPlan call;
  call.importSymbol = "v_difftest_" + module.defname.substr(std::string("DiffExt").size());
  call.conditionMembers = {0};
  call.eventEdge = DpiEventEdge::Posedge;
  call.useModuleClock = true;
  std::size_t argumentOrdinal = 0;
  for (std::size_t i = payloadBegin; i < module.members.size(); ++i) {
    const ExternalMemberAbi& member = module.members[i];
    if (member.direction != ExternalMemberDirection::Input) {
      return unsupported(describeMember(member, i) + " is an output; audited DiffExt calls are input-only");
    }
    if (member.width <= 0 || member.width > 64) {
      return unsupported(describeMember(member, i) + " is not a supported 1..64-bit payload");
    }

    std::size_t elementCount = 1;
    for (int64_t dimension : member.dimensions) {
      if (dimension <= 0) {
        return unsupported(describeMember(member, i) + " has a non-positive array dimension");
      }
      const std::size_t size = static_cast<std::size_t>(dimension);
      if (elementCount > std::numeric_limits<std::size_t>::max() / size) {
        return unsupported(describeMember(member, i) + " has an overflowing array shape");
      }
      elementCount *= size;
    }
    for (std::size_t element = 0; element < elementCount; ++element) {
      call.arguments.push_back(difftestArgument(argumentOrdinal++, i, element, member));
    }
  }
  plan.calls.push_back(std::move(call));
  if (!omitsValid) plan.ignoredMembers.push_back(1);
  return plan;
}

ExternalModuleDpiPlan resolveSimJtag(const ExternalModuleAbi& module,
                                     ExternalModuleExecutionProfile profile) {
  if (module.parameters.size() != 1 ||
      module.parameters[0].kind != ExternalParamKind::Integer) {
    return unsupported("SimJTAG requires one integer TICK_DELAY parameter");
  }
  if (module.members.size() != 9) {
    return unsupported("SimJTAG must have exactly 9 GSim-visible non-clock members");
  }
  ExternalModuleDpiPlan clockCheck = supported();
  if (!requireClock(module, clockCheck)) return clockCheck;

  const struct {
    const char* suffix;
    ExternalMemberDirection direction;
    int64_t width;
  } expected[] = {
      {"reset", ExternalMemberDirection::Input, 1},
      {"jtag.TRSTn", ExternalMemberDirection::Output, 1},
      {"jtag.TMS", ExternalMemberDirection::Output, 1},
      {"jtag.TDI", ExternalMemberDirection::Output, 1},
      {"jtag.TDO.data", ExternalMemberDirection::Input, 1},
      {"jtag.TDO.driven", ExternalMemberDirection::Input, 1},
      {"enable", ExternalMemberDirection::Input, 1},
      {"init_done", ExternalMemberDirection::Input, 1},
      {"exit", ExternalMemberDirection::Output, 32},
  };
  for (std::size_t i = 0; i < module.members.size(); ++i) {
    if (auto error = validateMember(module, i, expected[i].suffix,
                                    expected[i].direction, expected[i].width)) {
      return unsupported(*error);
    }
  }

  if (profile == ExternalModuleExecutionProfile::FullFidelity) {
    return requiresWrapper(
        "SimJTAG.v",
        "GSim excludes the output Clock member jtag.TCK from the external ABI; the remaining "
        "members cannot reproduce the reset delay, sticky init, tick counter, jtag_tick state, "
        "and returned exit value without retaining the audited SimJTAG wrapper");
  }

  // Exact contract of the XiangShan GSim CoreMark link stub. This is not the
  // full SimJTAG RTL behavior and therefore remains behind an explicit profile.
  ExternalModuleDpiPlan plan = supported();
  plan.outputConstants = {
      {1, "1'b1"},
      {2, "1'b0"},
      {3, "1'b0"},
      {8, "32'b0"},
  };
  plan.ignoredParameters = {0};
  plan.ignoredMembers = {0, 4, 5, 6, 7};
  plan.reason = "matches the explicit XiangShan GSim CoreMark disabled-JTAG stub";
  return plan;
}

ExternalModuleDpiPlan resolvePrintCommitId(
    const ExternalModuleAbi& module,
    ExternalModuleExecutionProfile profile) {
  if (!module.parameters.empty()) {
    return unsupported("PrintCommitIDModule must not have parameters");
  }
  if (module.hasClock) {
    return unsupported("PrintCommitIDModule must not capture a Clock port");
  }
  if (module.members.size() != 3) {
    return unsupported("PrintCommitIDModule must have exactly 3 input members");
  }
  const struct {
    const char* suffix;
    int64_t width;
  } expected[] = {
      {"hartID", 6},
      {"commitID", 40},
      {"dirty", 1},
  };
  for (std::size_t i = 0; i < module.members.size(); ++i) {
    if (auto error = validateMember(module, i, expected[i].suffix,
                                    ExternalMemberDirection::Input, expected[i].width)) {
      return unsupported(*error);
    }
  }

  if (profile == ExternalModuleExecutionProfile::FullFidelity) {
    return requiresWrapper(
        "PrintCommitIDModule.v",
        "PrintCommitIDModule performs one initial $fwrite; an ordinary GSim external call has no "
        "initial-event contract and would either drop or repeat that side effect");
  }

  ExternalModuleDpiPlan plan = supported();
  plan.ignoredMembers = {0, 1, 2};
  plan.reason = "matches the explicit no-op PrintCommitIDModule in the XiangShan GSim CoreMark stub";
  return plan;
}

}  // namespace

ExternalModuleDpiPlan resolveKnownXiangShanExternalModule(
    const ExternalModuleAbi& module,
    ExternalModuleExecutionProfile profile) {
  if (module.defname == "FlashHelper") return resolveFlash(module);
  if (module.defname == "SDCardHelper") return resolveSdCard(module);
  if (module.defname == "Mem1R1WHelper") return resolveMemoryHelper(module);
  if (module.defname.compare(0, 7, "DiffExt") == 0) return resolveDifftest(module);
  if (module.defname == "SimJTAG") return resolveSimJtag(module, profile);
  if (module.defname == "PrintCommitIDModule") return resolvePrintCommitId(module, profile);
  if (module.defname == "ClockGate") {
    return unsupported("ClockGate is lowered to combinational logic by GSim and is not a DPI call");
  }
  if (module.defname == "imsic_csr_top") {
    return unsupported(
        "imsic_csr_top is only a legacy GSim stub; the audited SimTop contains synthesizable IMSIC RTL");
  }
  return unsupported("no audited XiangShan DPI mapping for external defname '" + module.defname + "'");
}

const char* dpiEventEdgeName(DpiEventEdge edge) {
  switch (edge) {
    case DpiEventEdge::None: return "none";
    case DpiEventEdge::Posedge: return "posedge";
    case DpiEventEdge::Negedge: return "negedge";
  }
  return "unknown";
}

}  // namespace executable_grh

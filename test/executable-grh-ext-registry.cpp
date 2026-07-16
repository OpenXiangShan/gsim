#include "ExecutableGrhExtRegistry.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

using executable_grh::DpiArgDirection;
using executable_grh::DpiEventEdge;
using executable_grh::DpiValueSourceKind;
using executable_grh::ExternalMemberAbi;
using executable_grh::ExternalMemberDirection;
using executable_grh::ExternalModuleAbi;
using executable_grh::ExternalModuleExecutionProfile;
using executable_grh::ExternalModulePlanStatus;
using executable_grh::ExternalParamAbi;
using executable_grh::ExternalParamKind;

namespace {

[[noreturn]] void fail(const std::string& message) {
  std::cerr << "FAIL: " << message << '\n';
  std::exit(1);
}

void expect(bool condition, const std::string& message) {
  if (!condition) fail(message);
}

ExternalMemberAbi member(std::string name, ExternalMemberDirection direction,
                         int64_t width, std::vector<int64_t> dimensions = {}) {
  ExternalMemberAbi abi;
  abi.name = std::move(name);
  abi.direction = direction;
  abi.width = width;
  abi.dimensions = std::move(dimensions);
  return abi;
}

ExternalModuleAbi module(std::string defname, std::vector<ExternalMemberAbi> members) {
  ExternalModuleAbi abi;
  abi.defname = std::move(defname);
  abi.members = std::move(members);
  abi.hasClock = true;
  abi.clockName = "top__DOT__clock";
  return abi;
}

ExternalModuleAbi moduleWithoutClock(std::string defname,
                                     std::vector<ExternalMemberAbi> members) {
  ExternalModuleAbi abi;
  abi.defname = std::move(defname);
  abi.members = std::move(members);
  return abi;
}

void testFlash() {
  ExternalModuleAbi abi = module("FlashHelper", {
      member("top__DOT__helper__DOT__r__DOT__en", ExternalMemberDirection::Input, 1),
      member("top__DOT__helper__DOT__r__DOT__addr", ExternalMemberDirection::Input, 32),
      member("top__DOT__helper__DOT__r__DOT__data", ExternalMemberDirection::Output, 64),
  });
  auto plan = executable_grh::resolveKnownXiangShanExternalModule(abi);
  expect(plan.status == ExternalModulePlanStatus::Supported, plan.reason);
  expect(plan.calls.size() == 1, "FlashHelper call count");
  expect(plan.calls[0].importSymbol == "flash_read", "FlashHelper link symbol");
  expect(plan.calls[0].conditionMembers == std::vector<std::size_t>{0}, "FlashHelper condition");
  expect(plan.calls[0].eventEdge == DpiEventEdge::Posedge, "FlashHelper edge");
  expect(plan.calls[0].arguments.size() == 2, "FlashHelper argument count");
  expect(plan.calls[0].arguments[1].direction == DpiArgDirection::Output,
         "FlashHelper data direction");
}

void testSdCard() {
  ExternalModuleAbi abi = module("SDCardHelper", {
      member("top$helper$io$setAddr", ExternalMemberDirection::Input, 1),
      member("top$helper$io$addr", ExternalMemberDirection::Input, 32),
      member("top$helper$io$ren", ExternalMemberDirection::Input, 1),
      member("top$helper$io$data", ExternalMemberDirection::Output, 32),
  });
  auto plan = executable_grh::resolveKnownXiangShanExternalModule(abi);
  expect(plan.status == ExternalModulePlanStatus::Supported, plan.reason);
  expect(plan.calls.size() == 2, "SDCardHelper call count");
  expect(plan.calls[0].importSymbol == "sd_setaddr", "SDCardHelper setaddr symbol");
  expect(plan.calls[0].eventEdge == DpiEventEdge::Posedge, "SDCardHelper setaddr edge");
  expect(plan.calls[1].importSymbol == "sd_read", "SDCardHelper read symbol");
  expect(plan.calls[1].eventEdge == DpiEventEdge::Negedge, "SDCardHelper read edge");
  expect(plan.calls[1].arguments[0].direction == DpiArgDirection::Output,
         "SDCardHelper read output");
}

void testMemoryHelper() {
  ExternalModuleAbi abi = module("Mem1R1WHelper", {
      member("top$helper$r$enable", ExternalMemberDirection::Input, 1, {1}),
      member("top$helper$r$index", ExternalMemberDirection::Input, 64, {1}),
      member("top$helper$r$data", ExternalMemberDirection::Output, 64, {1}),
      member("top$helper$r$async", ExternalMemberDirection::Output, 1, {1}),
      member("top$helper$w$enable", ExternalMemberDirection::Input, 1, {1}),
      member("top$helper$w$index", ExternalMemberDirection::Input, 64, {1}),
      member("top$helper$w$data", ExternalMemberDirection::Input, 64, {1}),
      member("top$helper$w$mask", ExternalMemberDirection::Input, 64, {1}),
  });
  abi.parameters.push_back({ExternalParamKind::Integer, "8793945538560"});
  auto plan = executable_grh::resolveKnownXiangShanExternalModule(abi);
  expect(plan.status == ExternalModulePlanStatus::Supported, plan.reason);
  expect(plan.calls.size() == 2, "Mem1R1WHelper call count");
  expect(plan.calls[0].importSymbol == "difftest_ram_read", "RAM read symbol");
  expect(plan.calls[0].hasReturn && plan.calls[0].returnMember == 2, "RAM read return");
  expect(plan.calls[0].inactiveReturnLiteral == "64'b0", "RAM disabled-read value");
  expect(plan.calls[0].eventEdge == DpiEventEdge::None, "RAM read must be asynchronous");
  expect(plan.calls[1].importSymbol == "difftest_ram_write", "RAM write symbol");
  expect(plan.calls[1].eventEdge == DpiEventEdge::Posedge, "RAM write edge");
  expect(plan.outputConstants.size() == 1 && plan.outputConstants[0].member == 3 &&
             plan.outputConstants[0].literal == "1'b1",
         "RAM async output");
  expect(plan.ignoredParameters == std::vector<std::size_t>{0}, "RAM_SIZE handling");
}

void testDifftest() {
  ExternalModuleAbi abi = module("DiffExtInstrCommit", {
      member("top$dpic$enable", ExternalMemberDirection::Input, 1),
      member("top$dpic$io$valid", ExternalMemberDirection::Input, 1),
      member("top$dpic$io$skip", ExternalMemberDirection::Input, 1),
      member("top$dpic$io$wpdest", ExternalMemberDirection::Input, 9),
      member("top$dpic$io$coreid", ExternalMemberDirection::Input, 8),
  });
  auto plan = executable_grh::resolveKnownXiangShanExternalModule(abi);
  expect(plan.status == ExternalModulePlanStatus::Supported, plan.reason);
  expect(plan.calls.size() == 1, "DiffExt call count");
  expect(plan.calls[0].importSymbol == "v_difftest_InstrCommit", "DiffExt link symbol");
  expect(plan.calls[0].conditionMembers == std::vector<std::size_t>({0}),
         "DiffExt enable condition");
  expect(plan.ignoredMembers == std::vector<std::size_t>({1}),
         "DiffExt io.valid handling");
  expect(plan.calls[0].eventEdge == DpiEventEdge::Posedge, "DiffExt edge");
  expect(plan.calls[0].arguments.size() == 3, "DiffExt payload count");
  expect(plan.calls[0].arguments[0].type == "logic", "1-bit DiffExt type");
  expect(plan.calls[0].arguments[1].type == "shortint", "9-bit DiffExt type");
  expect(plan.calls[0].arguments[2].type == "byte", "8-bit DiffExt type");
}

void testDifftestArrayFlattening() {
  ExternalModuleAbi abi = module("DiffExtRefillEvent", {
      member("top$dpic$enable", ExternalMemberDirection::Input, 1),
      member("top$dpic$io$valid", ExternalMemberDirection::Input, 1),
      member("top$dpic$io$addr", ExternalMemberDirection::Input, 64),
      member("top$dpic$io$data", ExternalMemberDirection::Input, 64, {8}),
      member("top$dpic$io$mask", ExternalMemberDirection::Input, 8),
  });
  auto plan = executable_grh::resolveKnownXiangShanExternalModule(abi);
  expect(plan.status == ExternalModulePlanStatus::Supported, plan.reason);
  expect(plan.calls.size() == 1, "array DiffExt call count");
  expect(plan.calls[0].arguments.size() == 10, "array DiffExt flattened payload count");
  for (std::size_t element = 0; element < 8; ++element) {
    const auto& source = plan.calls[0].arguments[element + 1].source;
    expect(source.kind == DpiValueSourceKind::Member && source.index == 3 &&
               source.element == element,
           "array DiffExt source order");
  }
  expect(plan.calls[0].arguments[9].source.index == 4,
         "array DiffExt member after array");
}

void testDifftestWithoutValidMember() {
  const std::vector<std::pair<std::string, int64_t>> cases = {
      {"DiffExtArchIntRegState", 32},
      {"DiffExtArchFpRegState", 32},
      {"DiffExtArchVecRegState", 64},
  };
  for (const auto& [defname, registerCount] : cases) {
    ExternalModuleAbi abi = module(defname, {
        member("top$dpic$enable", ExternalMemberDirection::Input, 1),
        member("top$dpic$io$value", ExternalMemberDirection::Input, 64,
               {registerCount}),
        member("top$dpic$io$coreid", ExternalMemberDirection::Input, 8),
    });
    auto plan = executable_grh::resolveKnownXiangShanExternalModule(abi);
    expect(plan.status == ExternalModulePlanStatus::Supported, plan.reason);
    expect(plan.calls.size() == 1, "snapshot DiffExt call count");
    expect(plan.calls[0].importSymbol ==
               "v_difftest_" + defname.substr(std::string("DiffExt").size()),
           "snapshot DiffExt link symbol");
    expect(plan.ignoredMembers.empty(), "snapshot DiffExt must not invent io.valid");
    expect(plan.calls[0].arguments.size() ==
               static_cast<std::size_t>(registerCount + 1),
           "snapshot DiffExt flattened payload count");
    for (int64_t element = 0; element < registerCount; ++element) {
      const auto& source = plan.calls[0].arguments[static_cast<std::size_t>(element)].source;
      expect(source.kind == DpiValueSourceKind::Member && source.index == 1 &&
                 source.element == static_cast<std::size_t>(element),
             "snapshot DiffExt array source order");
    }
    expect(plan.calls[0].arguments[static_cast<std::size_t>(registerCount)].source.index == 2,
           "snapshot DiffExt coreid order");
  }
}

ExternalModuleAbi simJtagAbi() {
  ExternalModuleAbi abi = module("SimJTAG", {
      member("top$jtag$reset", ExternalMemberDirection::Input, 1),
      member("top$jtag$jtag$TRSTn", ExternalMemberDirection::Output, 1),
      member("top$jtag$jtag$TMS", ExternalMemberDirection::Output, 1),
      member("top$jtag$jtag$TDI", ExternalMemberDirection::Output, 1),
      member("top$jtag$jtag$TDO$data", ExternalMemberDirection::Input, 1),
      member("top$jtag$jtag$TDO$driven", ExternalMemberDirection::Input, 1),
      member("top$jtag$enable", ExternalMemberDirection::Input, 1),
      member("top$jtag$init_done", ExternalMemberDirection::Input, 1),
      member("top$jtag$exit", ExternalMemberDirection::Output, 32),
  });
  abi.parameters.push_back({ExternalParamKind::Integer, "3"});
  return abi;
}

ExternalModuleAbi printCommitAbi() {
  return moduleWithoutClock("PrintCommitIDModule", {
      member("top$printCommitIDMod$hartID", ExternalMemberDirection::Input, 6),
      member("top$printCommitIDMod$commitID", ExternalMemberDirection::Input, 40),
      member("top$printCommitIDMod$dirty", ExternalMemberDirection::Input, 1),
  });
}

void testWrapperRequirements() {
  auto simJtagPlan = executable_grh::resolveKnownXiangShanExternalModule(simJtagAbi());
  expect(simJtagPlan.status == ExternalModulePlanStatus::RequiresWrapper,
         "full-fidelity SimJTAG must require its wrapper");
  expect(simJtagPlan.requiredWrapper == "SimJTAG.v", "SimJTAG wrapper identity");
  expect(simJtagPlan.reason.find("jtag.TCK") != std::string::npos,
         "SimJTAG diagnostic must expose the missing output clock");

  auto printPlan = executable_grh::resolveKnownXiangShanExternalModule(printCommitAbi());
  expect(printPlan.status == ExternalModulePlanStatus::RequiresWrapper,
         "full-fidelity PrintCommitIDModule must require its wrapper");
  expect(printPlan.requiredWrapper == "PrintCommitIDModule.v",
         "PrintCommitIDModule wrapper identity");
  expect(printPlan.reason.find("initial $fwrite") != std::string::npos,
         "PrintCommitIDModule diagnostic must expose one-shot timing");
}

void testCoremarkStubProfile() {
  auto simJtagPlan = executable_grh::resolveKnownXiangShanExternalModule(
      simJtagAbi(), ExternalModuleExecutionProfile::XiangShanGsimCoremarkStub);
  expect(simJtagPlan.status == ExternalModulePlanStatus::Supported, simJtagPlan.reason);
  expect(simJtagPlan.calls.empty(), "disabled SimJTAG stub must not call jtag_tick");
  expect(simJtagPlan.outputConstants.size() == 4, "disabled SimJTAG output count");
  expect(simJtagPlan.outputConstants[0].member == 1 &&
             simJtagPlan.outputConstants[0].literal == "1'b1",
         "disabled SimJTAG TRSTn");
  expect(simJtagPlan.outputConstants[3].member == 8 &&
             simJtagPlan.outputConstants[3].literal == "32'b0",
         "disabled SimJTAG exit");
  expect(simJtagPlan.ignoredParameters == std::vector<std::size_t>{0},
         "disabled SimJTAG TICK_DELAY");
  expect(simJtagPlan.ignoredMembers == std::vector<std::size_t>({0, 4, 5, 6, 7}),
         "disabled SimJTAG inputs");

  auto printPlan = executable_grh::resolveKnownXiangShanExternalModule(
      printCommitAbi(), ExternalModuleExecutionProfile::XiangShanGsimCoremarkStub);
  expect(printPlan.status == ExternalModulePlanStatus::Supported, printPlan.reason);
  expect(printPlan.calls.empty() && printPlan.outputConstants.empty(),
         "PrintCommitIDModule GSim stub must be a no-op");
  expect(printPlan.ignoredMembers == std::vector<std::size_t>({0, 1, 2}),
         "PrintCommitIDModule GSim stub inputs");
}

void testStrictFailures() {
  ExternalModuleAbi malformedSimJtag = simJtagAbi();
  malformedSimJtag.members[1].direction = ExternalMemberDirection::Input;
  auto simJtagPlan = executable_grh::resolveKnownXiangShanExternalModule(malformedSimJtag);
  expect(simJtagPlan.status == ExternalModulePlanStatus::Unsupported,
         "malformed SimJTAG must fail closed before requesting a wrapper");

  ExternalModuleAbi malformedPrint = printCommitAbi();
  malformedPrint.hasClock = true;
  malformedPrint.clockName = "top$clock";
  auto printPlan = executable_grh::resolveKnownXiangShanExternalModule(malformedPrint);
  expect(printPlan.status == ExternalModulePlanStatus::Unsupported,
         "clocked PrintCommitIDModule must fail closed");

  ExternalModuleAbi unknown = module("UnknownHelper", {});
  auto unknownPlan = executable_grh::resolveKnownXiangShanExternalModule(unknown);
  expect(unknownPlan.status == ExternalModulePlanStatus::Unsupported,
         "unknown helpers must fail closed");

  ExternalModuleAbi malformed = module("FlashHelper", {
      member("top$r$en", ExternalMemberDirection::Output, 1),
      member("top$r$addr", ExternalMemberDirection::Input, 32),
      member("top$r$data", ExternalMemberDirection::Output, 64),
  });
  auto malformedPlan = executable_grh::resolveKnownXiangShanExternalModule(malformed);
  expect(malformedPlan.status == ExternalModulePlanStatus::Unsupported,
         "malformed helper ABI must fail closed");

  ExternalModuleAbi unnamedClock = module("FlashHelper", {
      member("top$r$en", ExternalMemberDirection::Input, 1),
      member("top$r$addr", ExternalMemberDirection::Input, 32),
      member("top$r$data", ExternalMemberDirection::Output, 64),
  });
  unnamedClock.clockName.clear();
  auto unnamedClockPlan = executable_grh::resolveKnownXiangShanExternalModule(unnamedClock);
  expect(unnamedClockPlan.status == ExternalModulePlanStatus::Unsupported,
         "unnamed captured clock must fail closed");

  ExternalModuleAbi malformedArray = module("DiffExtRefillEvent", {
      member("top$dpic$enable", ExternalMemberDirection::Input, 1),
      member("top$dpic$io$valid", ExternalMemberDirection::Input, 1),
      member("top$dpic$io$data", ExternalMemberDirection::Input, 64, {0}),
  });
  auto malformedArrayPlan =
      executable_grh::resolveKnownXiangShanExternalModule(malformedArray);
  expect(malformedArrayPlan.status == ExternalModulePlanStatus::Unsupported,
         "non-positive DiffExt array dimension must fail closed");

  ExternalModuleAbi missingValid = module("DiffExtInstrCommit", {
      member("top$dpic$enable", ExternalMemberDirection::Input, 1),
      member("top$dpic$io$skip", ExternalMemberDirection::Input, 1),
  });
  auto missingValidPlan =
      executable_grh::resolveKnownXiangShanExternalModule(missingValid);
  expect(missingValidPlan.status == ExternalModulePlanStatus::Unsupported,
         "event DiffExt missing io.valid must fail closed");

  ExternalModuleAbi unexpectedValid = module("DiffExtArchIntRegState", {
      member("top$dpic$enable", ExternalMemberDirection::Input, 1),
      member("top$dpic$io$valid", ExternalMemberDirection::Input, 1),
      member("top$dpic$io$value", ExternalMemberDirection::Input, 64, {32}),
  });
  auto unexpectedValidPlan =
      executable_grh::resolveKnownXiangShanExternalModule(unexpectedValid);
  expect(unexpectedValidPlan.status == ExternalModulePlanStatus::Unsupported,
         "snapshot DiffExt with io.valid must fail closed");

  ExternalModuleAbi wrongArchShape = module("DiffExtArchVecRegState", {
      member("top$dpic$enable", ExternalMemberDirection::Input, 1),
      member("top$dpic$io$value", ExternalMemberDirection::Input, 64, {32}),
      member("top$dpic$io$coreid", ExternalMemberDirection::Input, 8),
  });
  auto wrongArchShapePlan =
      executable_grh::resolveKnownXiangShanExternalModule(wrongArchShape);
  expect(wrongArchShapePlan.status == ExternalModulePlanStatus::Unsupported,
         "snapshot DiffExt wrong register count must fail closed");

  ExternalModuleAbi unauditedDiff = module("DiffExtUnauditedProbe", {
      member("top$dpic$enable", ExternalMemberDirection::Input, 1),
      member("top$dpic$io$valid", ExternalMemberDirection::Input, 1),
      member("top$dpic$io$data", ExternalMemberDirection::Input, 64),
  });
  auto unauditedDiffPlan =
      executable_grh::resolveKnownXiangShanExternalModule(unauditedDiff);
  expect(unauditedDiffPlan.status == ExternalModulePlanStatus::Unsupported,
         "unaudited DiffExt defname must fail closed");
}

}  // namespace

int main() {
  testFlash();
  testSdCard();
  testMemoryHelper();
  testDifftest();
  testDifftestArrayFlattening();
  testDifftestWithoutValidMember();
  testWrapperRequirements();
  testCoremarkStubProfile();
  testStrictFailures();
  std::cout << "executable GRH ext registry PASS\n";
  return 0;
}

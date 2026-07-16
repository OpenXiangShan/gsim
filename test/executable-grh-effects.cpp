#include "common.h"
#include "ExecutableGrhEffects.h"

#include <cstdlib>
#include <iostream>
#include <string>

Config::Config() = default;
Config globalConfig;

namespace {

using executable_grh::EffectKind;
using executable_grh::EffectPlan;
using executable_grh::EffectPlanStatus;

[[noreturn]] void fail(const std::string& message) {
  std::cerr << "FAIL: " << message << '\n';
  std::exit(1);
}

void expect(bool condition, const std::string& message) {
  if (!condition) fail(message);
}

ENode* ref(Node* node) {
  ENode* expression = new ENode(node);
  expression->width = node->width;
  expression->sign = node->sign;
  expression->isClock = node->isClock;
  return expression;
}

ENode* when(ENode* condition, ENode* trueBranch, ENode* falseBranch) {
  ENode* expression = new ENode(OP_WHEN);
  expression->addChild(condition);
  expression->addChild(trueBranch);
  expression->addChild(falseBranch);
  return expression;
}

Node* signal(const std::string& name, int width, bool isClock = false,
             NodeType type = NODE_OTHERS) {
  Node* node = new Node(type);
  node->name = name;
  node->width = width;
  node->isClock = isClock;
  return node;
}

Node* effect(const std::string& name, ENode* root, ENode* clock) {
  Node* node = signal(name, 1, false, NODE_SPECIAL);
  node->effectClock = clock;
  node->assignTree.push_back(new ExpTree(root, new ENode(node)));
  return node;
}

void expectUnsupported(const EffectPlan& plan, const std::string& context) {
  if (plan.status != EffectPlanStatus::Unsupported || plan.reason.empty()) {
    fail(context + " must fail closed with a reason");
  }
}

void testOptimizerElided(Node* clock) {
  Node* node = signal("optimizer_elided", 1, false, NODE_SPECIAL);
  node->effectClock = ref(clock);
  EffectPlan plan = executable_grh::resolveExecutableGrhEffect(*node);
  expect(plan.status == EffectPlanStatus::OptimizerElided && !plan.reason.empty(),
         "empty optimized effect must be classified as optimizer-elided");
}

void testPrintf(Node* clock, Node* cond, Node* data) {
  ENode* print = new ENode(OP_PRINTF);
  print->strVal = "\"d=%d x=%x b=%b c=%c\\n\"";
  print->addChild(ref(data));
  print->addChild(ref(data));
  print->addChild(ref(data));
  print->addChild(ref(data));

  Node* outer = signal("outer", 1);
  Node* node = effect("print", when(ref(outer), nullptr,
                                    when(ref(cond), print, nullptr)), ref(clock));
  EffectPlan plan = executable_grh::resolveExecutableGrhEffect(*node);
  expect(plan.status == EffectPlanStatus::Supported, "printf plan rejected: " + plan.reason);
  expect(plan.kind == EffectKind::Printf, "printf kind mismatch");
  expect(plan.systemTaskName == "fwrite" && plan.prependStderrHandle,
         "printf must map to stderr fwrite without implicit newline");
  expect(plan.baseClock == clock && plan.eventClock == node->effectClock,
         "printf clock mismatch");
  expect(plan.guards.size() == 2 && !plan.guards[0].expectedValue &&
             plan.guards[1].expectedValue,
         "nested printf guard polarity mismatch");
  expect(plan.arguments.size() == 4, "printf argument count mismatch");
  expect(plan.formatLiteral == print->strVal, "printf format literal mismatch");
  expect(plan.formatText == "d=%d x=%x b=%b c=%c\n", "printf format decode mismatch");
}

void testWidePrintf(Node* clock, Node* cond) {
  Node* wide = signal("wide", 128, false, NODE_INP);
  ENode* print = new ENode(OP_PRINTF);
  print->strVal = "\"wide=%d\\n\"";
  print->addChild(ref(wide));
  EffectPlan plan = executable_grh::resolveExecutableGrhEffect(
      *effect("wide_print", when(ref(cond), print, nullptr), ref(clock)));
  expect(plan.status == EffectPlanStatus::Supported &&
             plan.arguments.size() == 1 && plan.arguments[0]->nodePtr == wide,
         "wide scalar printf argument rejected: " + plan.reason);
}

void testAssert(Node* clock, Node* cond, Node* pred) {
  ENode* assertion = new ENode(OP_ASSERT);
  assertion->strVal = "\"assert failed\\n\"";
  assertion->addChild(ref(pred));
  assertion->addChild(ref(cond));
  Node* node = effect("assert", assertion, ref(clock));

  EffectPlan plan = executable_grh::resolveExecutableGrhEffect(*node);
  expect(plan.status == EffectPlanStatus::Supported, "assert plan rejected: " + plan.reason);
  expect(plan.kind == EffectKind::Assert && plan.systemTaskName == "fatal",
         "assert must map to fatal");
  expect(plan.hasExitCode && plan.exitCode == 1, "assert exit code mismatch");
  expect(plan.guards.size() == 2 && plan.guards[0].expectedValue &&
             !plan.guards[1].expectedValue,
         "assert must fire on enable && !predicate");
  expect(!plan.literalizedUnboundFormatConversions,
         "plain assert message was unexpectedly rewritten");
}

void testAssertUnboundFormats(Node* clock, Node* cond, Node* pred) {
  ENode* assertion = new ENode(OP_ASSERT);
  assertion->strVal = "\"bad d=%d x=%x b=%b c=%c percent=%%\\n\"";
  assertion->addChild(ref(pred));
  assertion->addChild(ref(cond));
  EffectPlan plan = executable_grh::resolveExecutableGrhEffect(
      *effect("assert_formats", assertion, ref(clock)));
  expect(plan.status == EffectPlanStatus::Supported &&
             plan.literalizedUnboundFormatConversions,
         "unbound assert conversions were not literalized: " + plan.reason);
  expect(plan.formatText ==
             "bad d=%%d x=%%x b=%%b c=%%c percent=%%\n",
         "literalized assert format mismatch");
}

void testExit(Node* clock, Node* cond) {
  ENode* exit = new ENode(OP_EXIT);
  exit->strVal = "7";
  exit->addChild(ref(cond));
  Node* node = effect("exit", exit, ref(clock));

  EffectPlan plan = executable_grh::resolveExecutableGrhEffect(*node);
  expect(plan.status == EffectPlanStatus::Supported, "exit plan rejected: " + plan.reason);
  expect(plan.kind == EffectKind::Exit && plan.systemTaskName == "finish",
         "exit must map to finish");
  expect(plan.hasExitCode && plan.exitCode == 7, "exit code mismatch");
  expect(plan.guards.size() == 1 && plan.guards[0].expectedValue,
         "exit condition mismatch");
}

void testClockAlias(Node* baseClock, Node* cond) {
  Node* alias = signal("clock_alias", 1, true);
  alias->assignTree.push_back(new ExpTree(ref(baseClock), new ENode(alias)));
  ENode* exit = new ENode(OP_EXIT);
  exit->strVal = "0";
  exit->addChild(ref(cond));
  Node* node = effect("aliased_clock_exit", exit, ref(alias));
  EffectPlan plan = executable_grh::resolveExecutableGrhEffect(*node);
  expect(plan.status == EffectPlanStatus::Supported && plan.baseClock == baseClock,
         "clock alias did not reduce to the base input clock: " + plan.reason);
}

void testStrictFailures(Node* clock, Node* cond, Node* pred, Node* data) {
  ENode* duplicateExit = new ENode(OP_EXIT);
  duplicateExit->strVal = "0";
  duplicateExit->addChild(ref(cond));
  Node* duplicate = effect("duplicate_tree", duplicateExit, ref(clock));
  duplicate->assignTree.push_back(duplicate->assignTree[0]->dup());
  expectUnsupported(executable_grh::resolveExecutableGrhEffect(*duplicate),
                    "multiple effect assignment trees");

  ENode* missingEnablePrint = new ENode(OP_PRINTF);
  missingEnablePrint->strVal = "\"v=%d\"";
  missingEnablePrint->addChild(ref(data));
  expectUnsupported(executable_grh::resolveExecutableGrhEffect(
                        *effect("missing_enable", missingEnablePrint, ref(clock))),
                    "printf without enable guard");

  ENode* badFormatPrint = new ENode(OP_PRINTF);
  badFormatPrint->strVal = "\"v=%q\"";
  badFormatPrint->addChild(ref(data));
  expectUnsupported(executable_grh::resolveExecutableGrhEffect(
                        *effect("bad_format", when(ref(cond), badFormatPrint, nullptr),
                                ref(clock))),
                    "unknown printf conversion");

  ENode* missingArgPrint = new ENode(OP_PRINTF);
  missingArgPrint->strVal = "\"v=%d\"";
  expectUnsupported(executable_grh::resolveExecutableGrhEffect(
                        *effect("missing_arg", when(ref(cond), missingArgPrint, nullptr),
                                ref(clock))),
                    "printf conversion/argument mismatch");

  ENode* malformedAssert = new ENode(OP_ASSERT);
  malformedAssert->strVal = "\"bad\"";
  malformedAssert->addChild(ref(pred));
  expectUnsupported(executable_grh::resolveExecutableGrhEffect(
                        *effect("bad_assert", malformedAssert, ref(clock))),
                    "assert operand mismatch");

  ENode* unknownAssertFormat = new ENode(OP_ASSERT);
  unknownAssertFormat->strVal = "\"bad=%q\"";
  unknownAssertFormat->addChild(ref(pred));
  unknownAssertFormat->addChild(ref(cond));
  expectUnsupported(executable_grh::resolveExecutableGrhEffect(
                        *effect("unknown_assert_format", unknownAssertFormat,
                                ref(clock))),
                    "unknown assert conversion");

  ENode* malformedExit = new ENode(OP_EXIT);
  malformedExit->strVal = "not-an-int";
  malformedExit->addChild(ref(cond));
  expectUnsupported(executable_grh::resolveExecutableGrhEffect(
                        *effect("bad_exit", malformedExit, ref(clock))),
                    "invalid exit code");

  ENode* noClockExit = new ENode(OP_EXIT);
  noClockExit->strVal = "0";
  noClockExit->addChild(ref(cond));
  expectUnsupported(executable_grh::resolveExecutableGrhEffect(
                        *effect("no_clock", noClockExit, nullptr)),
                    "missing event clock");

  ENode* trueExit = new ENode(OP_EXIT);
  trueExit->strVal = "0";
  trueExit->addChild(ref(cond));
  ENode* falseExit = new ENode(OP_EXIT);
  falseExit->strVal = "0";
  falseExit->addChild(ref(cond));
  expectUnsupported(executable_grh::resolveExecutableGrhEffect(
                        *effect("two_effects", when(ref(cond), trueExit, falseExit), ref(clock))),
                    "two effect branches");

  Node* wideCond = signal("wide_cond", 2);
  ENode* guardedExit = new ENode(OP_EXIT);
  guardedExit->strVal = "0";
  guardedExit->addChild(ref(cond));
  expectUnsupported(executable_grh::resolveExecutableGrhEffect(
                        *effect("wide_guard", when(ref(wideCond), guardedExit, nullptr),
                                ref(clock))),
                    "non-one-bit effect guard");

  Node ordinary(NODE_OTHERS);
  expectUnsupported(executable_grh::resolveExecutableGrhEffect(ordinary),
                    "non-special node");
}

}  // namespace

int main() {
  Node* clock = signal("clock", 1, true, NODE_INP);
  Node* cond = signal("cond", 1, false, NODE_INP);
  Node* pred = signal("pred", 1, false, NODE_INP);
  Node* data = signal("data", 8, false, NODE_INP);

  testPrintf(clock, cond, data);
  testWidePrintf(clock, cond);
  testAssert(clock, cond, pred);
  testAssertUnboundFormats(clock, cond, pred);
  testExit(clock, cond);
  testClockAlias(clock, cond);
  testOptimizerElided(clock);
  testStrictFailures(clock, cond, pred, data);
  std::cout << "executable GRH effects PASS\n";
  return 0;
}

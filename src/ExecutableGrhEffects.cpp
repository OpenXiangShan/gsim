#include "common.h"
#include "ExecutableGrhEffects.h"

#include <charconv>
#include <limits>
#include <unordered_set>

namespace executable_grh {
namespace {

bool fail(EffectPlan& plan, const std::string& reason) {
  plan.status = EffectPlanStatus::Unsupported;
  if (plan.reason.empty()) plan.reason = reason;
  return false;
}

int expressionWidth(const ENode* expression) {
  if (!expression) return -1;
  if (expression->nodePtr) return expression->nodePtr->width;
  return expression->width;
}

bool isEffectOperator(OPType type) {
  return type == OP_PRINTF || type == OP_ASSERT || type == OP_EXIT;
}

bool expressionContainsEffect(const ENode* root,
                              std::unordered_set<const ENode*>& active) {
  if (!root) return false;
  if (!active.insert(root).second) return true;
  if (isEffectOperator(root->opType)) {
    active.erase(root);
    return true;
  }
  for (const ENode* child : root->child) {
    if (expressionContainsEffect(child, active)) {
      active.erase(root);
      return true;
    }
  }
  active.erase(root);
  return false;
}

bool validateLogicExpression(const ENode* expression,
                             const char* role,
                             EffectPlan& plan,
                             bool requireOneBit) {
  if (!expression) return fail(plan, std::string(role) + " expression is null");
  std::unordered_set<const ENode*> active;
  if (expressionContainsEffect(expression, active)) {
    return fail(plan, std::string(role) + " expression contains a side effect or a cycle");
  }
  const int width = expressionWidth(expression);
  if (width <= 0) return fail(plan, std::string(role) + " expression has no positive width");
  if (requireOneBit && width != 1) {
    return fail(plan, std::string(role) + " expression must be one bit");
  }
  return true;
}

bool isEmptyBranch(const ENode* branch) {
  return !branch ||
         (branch->opType == OP_EMPTY && !branch->nodePtr && branch->child.empty());
}

bool countEffectLeaves(const ENode* root,
                       std::unordered_set<const ENode*>& active,
                       size_t& count,
                       std::string& error) {
  if (isEmptyBranch(root)) return true;
  if (!active.insert(root).second) {
    error = "effect tree contains a cycle";
    return false;
  }
  if (isEffectOperator(root->opType)) {
    count++;
    active.erase(root);
    return true;
  }
  if (root->opType != OP_WHEN || root->child.size() != 3) {
    active.erase(root);
    return true;
  }
  if (!countEffectLeaves(root->child[1], active, count, error) ||
      !countEffectLeaves(root->child[2], active, count, error)) {
    active.erase(root);
    return false;
  }
  active.erase(root);
  return true;
}

bool locateEffectLeaf(const ENode* root,
                      EffectPlan& plan,
                      const ENode*& leaf,
                      std::unordered_set<const ENode*>& active) {
  if (!root) return fail(plan, "effect tree root is null");
  if (!active.insert(root).second) return fail(plan, "effect tree contains a cycle");
  if (isEffectOperator(root->opType)) {
    leaf = root;
    active.erase(root);
    return true;
  }
  if (root->opType != OP_WHEN || root->child.size() != 3 || !root->child[0]) {
    active.erase(root);
    return fail(plan, "effect tree must contain only OP_WHEN guards around one effect leaf");
  }
  if (!validateLogicExpression(root->child[0], "effect guard", plan, true)) {
    active.erase(root);
    return false;
  }

  size_t trueEffects = 0;
  size_t falseEffects = 0;
  std::string countError;
  std::unordered_set<const ENode*> countActive;
  if (!countEffectLeaves(root->child[1], countActive, trueEffects, countError)) {
    active.erase(root);
    return fail(plan, countError);
  }
  countActive.clear();
  if (!countEffectLeaves(root->child[2], countActive, falseEffects, countError)) {
    active.erase(root);
    return fail(plan, countError);
  }
  if ((trueEffects == 1) == (falseEffects == 1) ||
      trueEffects > 1 || falseEffects > 1) {
    active.erase(root);
    return fail(plan, "OP_WHEN must select exactly one effect-bearing branch");
  }

  const bool takeTrue = trueEffects == 1;
  const ENode* inactiveBranch = root->child[takeTrue ? 2 : 1];
  if (!isEmptyBranch(inactiveBranch)) {
    active.erase(root);
    return fail(plan, "inactive effect branch must be empty");
  }
  plan.guards.push_back(EffectGuardTerm{root->child[0], takeTrue});
  const bool ok = locateEffectLeaf(root->child[takeTrue ? 1 : 2], plan, leaf, active);
  active.erase(root);
  return ok;
}

enum class FormatConversionMode {
  MatchArguments,
  LiteralizeUnbound,
};

bool validateQuotedFormat(const std::string& literal,
                          size_t argumentCount,
                          EffectPlan& plan,
                          FormatConversionMode conversionMode,
                          std::string& decoded) {
  if (literal.size() < 2 || literal.front() != '"' || literal.back() != '"') {
    return fail(plan, "effect format is not a quoted FIRRTL string literal");
  }
  std::string unescaped;
  unescaped.reserve(literal.size() - 2);
  for (size_t i = 1; i + 1 < literal.size(); i++) {
    const char ch = literal[i];
    if (ch != '\\') {
      unescaped.push_back(ch);
      continue;
    }
    if (i + 1 >= literal.size() - 1) {
      return fail(plan, "effect format has a trailing escape");
    }
    switch (literal[++i]) {
      case 'n': unescaped.push_back('\n'); break;
      case 'r': unescaped.push_back('\r'); break;
      case 't': unescaped.push_back('\t'); break;
      case 'b': unescaped.push_back('\b'); break;
      case 'f': unescaped.push_back('\f'); break;
      case 'v': unescaped.push_back('\v'); break;
      case '\\': unescaped.push_back('\\'); break;
      case '"': unescaped.push_back('"'); break;
      default: return fail(plan, "effect format contains an unsupported escape");
    }
  }

  decoded.clear();
  decoded.reserve(unescaped.size());
  size_t conversions = 0;
  for (size_t i = 0; i < unescaped.size(); i++) {
    const char ch = unescaped[i];
    if (ch != '%') {
      decoded.push_back(ch);
      continue;
    }
    if (i + 1 >= unescaped.size()) {
      return fail(plan, "effect format ends with an incomplete conversion");
    }
    const char spec = unescaped[++i];
    if (spec == '%') {
      decoded += "%%";
      continue;
    }
    if (spec != 'd' && spec != 'x' && spec != 'b' && spec != 'c') {
      return fail(plan, std::string("unsupported GSim printf conversion %") + spec);
    }
    if (conversionMode == FormatConversionMode::MatchArguments) {
      decoded.push_back('%');
      decoded.push_back(spec);
      conversions++;
    } else {
      decoded += "%%";
      decoded.push_back(spec);
      plan.literalizedUnboundFormatConversions = true;
    }
  }
  if (conversionMode == FormatConversionMode::MatchArguments &&
      conversions != argumentCount) {
    return fail(plan, "printf format conversion count does not match argument count");
  }
  if (conversionMode == FormatConversionMode::LiteralizeUnbound &&
      argumentCount != 0) {
    return fail(plan, "literalized effect format unexpectedly has arguments");
  }
  return true;
}

bool parseExitCode(const std::string& text, int64_t& value) {
  if (text.empty()) return false;
  int64_t parsed = 0;
  const char* begin = text.data();
  const char* end = begin + text.size();
  const auto result = std::from_chars(begin, end, parsed, 10);
  if (result.ec != std::errc() || result.ptr != end) return false;
  if (parsed < std::numeric_limits<int32_t>::min() ||
      parsed > std::numeric_limits<int32_t>::max()) {
    return false;
  }
  value = parsed;
  return true;
}

const Node* resolveBaseClockNode(const ENode* expression,
                                 std::unordered_set<const ENode*>& activeExpressions,
                                 std::unordered_set<const Node*>& activeNodes,
                                 std::string& error) {
  if (!expression) {
    error = "effect clock expression is null";
    return nullptr;
  }
  if (!activeExpressions.insert(expression).second) {
    error = "effect clock expression contains a cycle";
    return nullptr;
  }
  if (expression->nodePtr) {
    const Node* node = expression->nodePtr;
    if (!node->isClock || node->width != 1) {
      error = "effect clock node is not a one-bit Clock";
      activeExpressions.erase(expression);
      return nullptr;
    }
    if (node->type == NODE_INP || node->type == NODE_REG_SRC) {
      activeExpressions.erase(expression);
      return node;
    }
    if (!activeNodes.insert(node).second) {
      error = "effect clock aliases contain a cycle";
      activeExpressions.erase(expression);
      return nullptr;
    }
    if (node->assignTree.size() != 1 || !node->assignTree[0] ||
        !node->assignTree[0]->getRoot()) {
      error = "effect clock alias does not have exactly one definition";
      activeNodes.erase(node);
      activeExpressions.erase(expression);
      return nullptr;
    }
    const Node* base = resolveBaseClockNode(node->assignTree[0]->getRoot(),
                                           activeExpressions, activeNodes, error);
    activeNodes.erase(node);
    activeExpressions.erase(expression);
    return base;
  }

  const bool transparentCast =
      expression->opType == OP_ASUINT || expression->opType == OP_ASSINT ||
      expression->opType == OP_ASCLOCK || expression->opType == OP_ASASYNCRESET;
  const bool transparentBit = expression->opType == OP_BITS &&
                              expression->values.size() == 2 &&
                              expression->values[0] == 0 && expression->values[1] == 0;
  if ((!transparentCast && !transparentBit) || expression->child.size() != 1 ||
      !expression->child[0]) {
    error = "effect clock is not reducible to one base clock";
    activeExpressions.erase(expression);
    return nullptr;
  }
  const Node* base = resolveBaseClockNode(expression->child[0], activeExpressions,
                                         activeNodes, error);
  activeExpressions.erase(expression);
  return base;
}

}  // namespace

const char* effectKindName(EffectKind kind) {
  switch (kind) {
    case EffectKind::Printf: return "printf";
    case EffectKind::Assert: return "assert";
    case EffectKind::Exit: return "exit";
  }
  return "unknown";
}

EffectPlan resolveExecutableGrhEffect(const Node& node) {
  EffectPlan plan;
  if (node.type != NODE_SPECIAL) {
    fail(plan, "effect resolver requires NODE_SPECIAL");
    return plan;
  }
  if (node.status != VALID_NODE) {
    fail(plan, "effect resolver requires a live node");
    return plan;
  }
  if (node.assignTree.empty()) {
    plan.status = EffectPlanStatus::OptimizerElided;
    plan.reason = "effect assignment was eliminated by pre-coarsen optimization";
    return plan;
  }
  if (node.assignTree.size() != 1 || !node.assignTree[0] ||
      !node.assignTree[0]->getRoot()) {
    fail(plan, "NODE_SPECIAL must have exactly one assignment tree");
    return plan;
  }
  const ENode* lvalue = node.assignTree[0]->getlval();
  if (!lvalue || lvalue->nodePtr != &node || !lvalue->child.empty()) {
    fail(plan, "NODE_SPECIAL assignment lvalue does not reference its owner");
    return plan;
  }
  if (!node.effectClock) {
    fail(plan, "NODE_SPECIAL is missing its event clock");
    return plan;
  }
  if (!validateLogicExpression(node.effectClock, "effect clock", plan, true)) return plan;

  std::string clockError;
  std::unordered_set<const ENode*> activeExpressions;
  std::unordered_set<const Node*> activeNodes;
  plan.baseClock = resolveBaseClockNode(node.effectClock, activeExpressions,
                                       activeNodes, clockError);
  if (!plan.baseClock) {
    fail(plan, clockError);
    return plan;
  }
  plan.eventClock = node.effectClock;

  const ENode* leaf = nullptr;
  std::unordered_set<const ENode*> active;
  if (!locateEffectLeaf(node.assignTree[0]->getRoot(), plan, leaf, active)) return plan;
  if (!leaf) {
    fail(plan, "NODE_SPECIAL has no effect leaf");
    return plan;
  }

  switch (leaf->opType) {
    case OP_PRINTF:
      plan.kind = EffectKind::Printf;
      plan.systemTaskName = "fwrite";
      plan.prependStderrHandle = true;
      if (plan.guards.empty()) {
        fail(plan, "OP_PRINTF is missing its enable guard");
        return plan;
      }
      for (const ENode* argument : leaf->child) {
        if (!validateLogicExpression(argument, "printf argument", plan, false)) return plan;
        plan.arguments.push_back(argument);
      }
      if (!validateQuotedFormat(leaf->strVal, plan.arguments.size(), plan,
                                FormatConversionMode::MatchArguments,
                                plan.formatText)) return plan;
      plan.formatLiteral = leaf->strVal;
      break;
    case OP_ASSERT:
      plan.kind = EffectKind::Assert;
      plan.systemTaskName = "fatal";
      plan.hasExitCode = true;
      plan.exitCode = 1;
      if (leaf->child.size() != 2 ||
          !validateLogicExpression(leaf->child[0], "assert predicate", plan, true) ||
          !validateLogicExpression(leaf->child[1], "assert enable", plan, true)) {
        if (plan.reason.empty()) fail(plan, "OP_ASSERT requires predicate and enable operands");
        return plan;
      }
      plan.guards.push_back(EffectGuardTerm{leaf->child[1], true});
      plan.guards.push_back(EffectGuardTerm{leaf->child[0], false});
      if (!validateQuotedFormat(leaf->strVal, 0, plan,
                                FormatConversionMode::LiteralizeUnbound,
                                plan.formatText)) return plan;
      plan.formatLiteral = leaf->strVal;
      break;
    case OP_EXIT:
      plan.kind = EffectKind::Exit;
      plan.systemTaskName = "finish";
      plan.hasExitCode = true;
      if (leaf->child.size() != 1 ||
          !validateLogicExpression(leaf->child[0], "exit condition", plan, true)) {
        if (plan.reason.empty()) fail(plan, "OP_EXIT requires one condition operand");
        return plan;
      }
      plan.guards.push_back(EffectGuardTerm{leaf->child[0], true});
      if (!parseExitCode(leaf->strVal, plan.exitCode)) {
        fail(plan, "OP_EXIT has an invalid integer exit code");
        return plan;
      }
      break;
    default:
      fail(plan, "unsupported effect leaf operator");
      return plan;
  }

  plan.status = EffectPlanStatus::Supported;
  plan.reason.clear();
  return plan;
}

}  // namespace executable_grh

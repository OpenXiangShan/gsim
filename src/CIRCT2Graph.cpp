/*
  Generate design graph (the intermediate representation of the input circuit) from AST
*/
#include "CIRCT2Graph.h"
#include "llvm/ADT/APInt.h"
#include "llvm/Support/Casting.h"
#include "mlir/IR/Block.h"

graph* CIRCT2Graph::generateGraph() {
  g = new graph();
  processInputPort();
  processOperations();
  return g;
}

void CIRCT2Graph::processInputPort() {
  size_t inputPortIdx = 0;
  for(size_t i = 0; i < topModule.getNumPorts(); i++) {
    auto portInfo = topModule.getPort(i);
    if(portInfo.isOutput()) continue;
    // 创建 typeInfo
    TypeInfo* typeInfo = new TypeInfo();
    typeInfo->set_sign(portInfo.type.isSignedInteger());
    typeInfo->set_width(portInfo.type.getIntOrFloatBitWidth());
    typeInfo->set_reset(UNCERTAIN);
    // 创建 node
    Node* node = new Node(NODE_INP); 
    node->name = topModule.getPortName(i).str();
    node->updateInfo(typeInfo);
    // 将 node 添加到图中
    g->input.push_back(node);
    // 将 value-node 关系添加到 map 中
    mlir::Value portValue = topModule.getBodyRegion().front().getArgument(inputPortIdx);
    valueNodeMap[portValue] = node;
    inputPortIdx++;
  }
}

void CIRCT2Graph::processOperations() {
  if (!topModule) return;
  Block* body = topModule.getBodyBlock();
  if (!body) return;

  for (auto& op : body->getOperations()) {
    // GRH: add more operation in this branch
    if (auto constantOp = llvm::dyn_cast<hw::ConstantOp>(op)) {
      processConstantOp(constantOp);
    } else {
      Assert(false, "Unsupported operation: %s", op.getName().getStringRef().str().c_str());
    }
  }
}

void CIRCT2Graph::processConstantOp(hw::ConstantOp constantOp) {
  auto intType = llvm::dyn_cast<IntegerType>(constantOp.getType());
  Assert(intType, "hw.constant expects integer result type");

  bool isSigned = intType.isSignedInteger();
  int width = intType.getWidth();

  Node* node = new Node(NODE_OTHERS);
  node->name = format("const_%d", node->id);
  node->setType(width, isSigned);
  node->status = CONSTANT_NODE;

  const llvm::APInt& value = constantOp.getValue();
  llvm::SmallVector<char> lc;
  value.toString(lc, 10, isSigned);
  std::string literal(lc.begin(), lc.end());
  if (literal.empty()) literal = "0";

  ENode* intENode = new ENode(OP_INT);
  intENode->strVal = literal;
  intENode->width = width;
  intENode->sign = isSigned;

  ExpTree* expTree = new ExpTree(intENode, node);
  node->valTree = expTree;
  node->assignTree.push_back(expTree);

  g->allNodes.push_back(node);
  valueNodeMap[constantOp.getResult()] = node;
}

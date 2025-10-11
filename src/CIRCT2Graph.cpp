/*
  Generate design graph (the intermediate representation of the input circuit) from AST
*/
#include "CIRCT2Graph.h"
#include "llvm/ADT/APInt.h"
#include "llvm/Support/Casting.h"
#include "mlir/IR/Block.h"

graph* CIRCT2Graph::generateGraph() {
  g = new graph();
  processIoPort();
  processOperations();
  return g;
}

/// For each Input and Output port of TopModule. Generate NODE_IND and NODE_OUT
void CIRCT2Graph::processIoPort() {
  auto portList = topModule.getPortList();
  for (auto& p : portList) {
    //std::cout<< p.getName().str() << " "<< std::endl;
    if (p.isInput()) {
      processInputPort(p);
    }else if (p.isOutput()) {
      processOutputPort(p);
    }else{ //InOut is not supproted!
      Panic();
    }
  }
}

void CIRCT2Graph::processInputPort(hw::PortInfo portInfo) {
    Assert(portInfo.isInput(),"");
    // 创建 typeInfo
    TypeInfo* typeInfo = new TypeInfo();
    typeInfo->set_sign(portInfo.type.isSignedInteger());
    typeInfo->set_width(portInfo.type.getIntOrFloatBitWidth());
    typeInfo->set_reset(UNCERTAIN);
    // 创建 node
    Node* node = new Node(NODE_INP); 
    node->name = portInfo.getName();
    node->updateInfo(typeInfo.get());
    // 将 node 添加到图中
    g->input.push_back(node);
    // 将 value-node 关系添加到 map 中
    mlir::Value portValue = topModule.getBodyRegion().front().getArgument(portInfo.argNum);
    valueNodeMap[portValue] = node;
}

void CIRCT2Graph::processOutputPort(hw::PortInfo portInfo) {
  Assert(portInfo.isOutput(),"");

  auto typeInfo = std::make_unique<TypeInfo>();
  typeInfo->set_sign(portInfo.type.isSignedInteger());
  typeInfo->set_width(portInfo.type.getIntOrFloatBitWidth());
  typeInfo->set_reset(UNCERTAIN);

  Node* node = new Node(NODE_OUT);
  node->name = portInfo.getName();
  node->updateInfo(typeInfo.get());

  g->output.push_back(node);
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

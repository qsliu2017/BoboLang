#include "Lex.h"
#include "Codegen.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
#include <map>
#include <memory>

static std::map<std::string, AllocaInst *> NamedValues;

std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;
static Function *getFunction(std::string Name)
{
  if (auto *F = TheModule->getFunction(Name))
    return F;

  auto FI = FunctionProtos.find(Name);
  if (FI != FunctionProtos.end())
    return FI->second->codegen();

  return nullptr;
}
static Function *CurrentFunction;

static Value *LogErrorV(const char *Str)
{
  fprintf(stderr, "Error: %s\n", Str);
  return nullptr;
}

static Function *LogErrorF(const char *Str)
{
  LogErrorV(Str);
  return nullptr;
}

Value *NumberDoubleExprAST::codegen()
{
  return ConstantFP::get(*TheContext, APFloat(Val));
}

Value *NumberIntExprAST::codegen()
{
  return ConstantInt::get(*TheContext, APInt(32, Val));
}

Value *VariableExprAST::codegen()
{
  Value *Ptr = NamedValues[Name];
  if (!Ptr)
    return LogErrorV("Unknown variable name");
  return Builder->CreateLoad(Ptr, Name);
}

Value *BinaryExprAST::codegen()
{
  auto L = LHS->codegen(), R = RHS->codegen();
  if (!L || !R)
    return nullptr;

  bool isLeftDoubleTy = L->getType()->isDoubleTy(),
       isRightDoubleTy = R->getType()->isDoubleTy();
  if (isLeftDoubleTy || isRightDoubleTy)
  {
    if (!isLeftDoubleTy)
      L = Builder->CreateUIToFP(L, Type::getDoubleTy(*TheContext));
    if (!isRightDoubleTy)
      R = Builder->CreateUIToFP(R, Type::getDoubleTy(*TheContext));

    switch (Op)
    {
    case '+':
      return Builder->CreateFAdd(L, R);
    case '-':
      return Builder->CreateFSub(L, R);
    case '*':
      return Builder->CreateFMul(L, R);
    case '<':
      return Builder->CreateFCmpULT(L, R);
    }
  }
  else
  {
    switch (Op)
    {
    case '+':
      return Builder->CreateAdd(L, R);
    case '-':
      return Builder->CreateSub(L, R);
    case '*':
      return Builder->CreateMul(L, R);
    case '<':
      return Builder->CreateICmpULT(L, R);
    }
  }
  return LogErrorV("binary op not sopport");
}

Value *CallExprAST::codegen()
{
  auto CalleeF = getFunction(Callee);
  if (!CalleeF)
    return LogErrorV("Unknown function");

  if (CalleeF->arg_size() != Args.size())
    return LogErrorV("Incorrect number of arguments");

  std::vector<Value *> ArgsValue;
  for (int i = 0, e = Args.size(); i < e; i++)
  {
    ArgsValue.push_back(Args[i]->codegen());
    if (!ArgsValue.back())
      return nullptr;
  }
  return Builder->CreateCall(CalleeF, ArgsValue);
}

Value *DeclStmtAST::codegen()
{
  Value *Last;
  for (auto &Name : Names)
  {
    if (NamedValues[Name])
      return LogErrorV("redeclare var");

    Type *Ty;
    if (ValType == type_int)
      Ty = Type::getInt32Ty(*TheContext);
    else if (ValType == type_double)
      Ty = Type::getDoubleTy(*TheContext);
    else
      return LogErrorV("unknown type");

    Last = NamedValues[Name] = Builder->CreateAlloca(Ty, 0, Name);
  }
  return Last;
}

Value *SimpStmtAST::codegen()
{
  Value *Ptr = NamedValues[Name];
  if (!Ptr)
    return LogErrorV("undeclared var");
  Value *V = Expr->codegen();

  bool isVarDouble = Ptr->getType()->isDoubleTy(),
       isValDouble = V->getType()->isDoubleTy();
  if (isVarDouble && !isValDouble)
    V = Builder->CreateUIToFP(V, Type::getDoubleTy(*TheContext));
  else if (!isVarDouble && isValDouble)
    V = Builder->CreateFPToUI(V, Type::getInt32Ty(*TheContext));

  Builder->CreateStore(V, Ptr);
  return V;
}

Value *ReturnStmtAST::codegen()
{
  Value *RetVal = Expr->codegen();
  Type *RetProtoTy = CurrentFunction->getReturnType();
  Type *RetValTy = RetVal->getType();

  bool isProtoDouble = RetProtoTy->isDoubleTy(),
       isValDouble = RetValTy->isDoubleTy();
  if (isProtoDouble && !isValDouble)
    RetVal = Builder->CreateUIToFP(RetVal, Type::getDoubleTy(*TheContext));
  else if (!isProtoDouble && isValDouble)
    RetVal = Builder->CreateFPToUI(RetVal, Type::getInt32Ty(*TheContext));

  return Builder->CreateRet(RetVal);
}

Value *getBoolValue(Value *Val)
{
  auto Type = Val->getType();
  if (Type->isDoubleTy())
    return Builder->CreateCmp(CmpInst::Predicate::FCMP_ONE, Val, ConstantFP::get(Type, 0.0));
  else if (Type->isIntegerTy())
    return Builder->CreateCmp(CmpInst::Predicate::ICMP_NE, Val, ConstantInt::get(Type, 0));
  return nullptr;
}

Value *IfElseStmtAST::codegen()
{
  Value *CondVal = Cond->codegen();
  if (!CondVal)
    return nullptr;
  CondVal = getBoolValue(CondVal);

  auto TheFunction = Builder->GetInsertBlock()->getParent();

  auto ThenBB = BasicBlock::Create(*TheContext, "then", TheFunction),
       ElseBB = BasicBlock::Create(*TheContext, "else", TheFunction),
       MergeBB = BasicBlock::Create(*TheContext, "ifcont", TheFunction);

  Builder->CreateCondBr(CondVal, ThenBB, ElseBB);

  Builder->SetInsertPoint(ThenBB);
  auto IfVal = Then->codegen();
  if (!IfVal)
    return nullptr;
  Builder->CreateBr(MergeBB);
  ThenBB = Builder->GetInsertBlock();

  Builder->SetInsertPoint(ElseBB);
  auto ElseVal = Else->codegen();
  if (!ElseVal)
    return nullptr;
  Builder->CreateBr(MergeBB);
  ElseBB = Builder->GetInsertBlock();

  Builder->SetInsertPoint(MergeBB);
  return MergeBB;
}

Value *WhileStmtAST::codegen()
{
  auto TheFunction = Builder->GetInsertBlock()->getParent();

  auto CondBB = BasicBlock::Create(*TheContext, "while", TheFunction),
       LoopBB = BasicBlock::Create(*TheContext, "loop", TheFunction),
       ContBB = BasicBlock::Create(*TheContext, "cont", TheFunction);

  Builder->CreateBr(CondBB);

  Builder->SetInsertPoint(CondBB);
  auto CondVal = Cond->codegen();
  if (!CondVal)
    return nullptr;
  CondVal = getBoolValue(CondVal);
  Builder->CreateCondBr(CondVal, LoopBB, ContBB);
  CondBB = Builder->GetInsertBlock();

  Builder->SetInsertPoint(LoopBB);
  auto LoopVal = Loop->codegen();
  if (!LoopVal)
    return nullptr;
  Builder->CreateBr(CondBB);
  LoopBB = Builder->GetInsertBlock();

  Builder->SetInsertPoint(ContBB);
  return ContBB;
}

Function *PrototypeAST::codegen()
{
  Type *ResultTy;
  if (FnType == type_int)
    ResultTy = Type::getInt32Ty(*TheContext);
  else if (FnType == type_double)
    ResultTy = Type::getDoubleTy(*TheContext);
  else
    return LogErrorF("unknown return type");

  std::vector<Type *> ArgsTy;
  for (int i = 0, e = Args.size(); i < e; i++)
  {
    if (ArgTypes[i] == type_int)
      ArgsTy.push_back(Type::getInt32Ty(*TheContext));
    else if (ArgTypes[i] == type_double)
      ArgsTy.push_back(Type::getDoubleTy(*TheContext));
    else
      return LogErrorF("unknown arg type");
  }

  FunctionType *FT = FunctionType::get(ResultTy, ArgsTy, false);

  Function *F = Function::Create(FT, Function::ExternalLinkage, Name, TheModule.get());

  unsigned Idx = 0;
  for (auto &Arg : F->args())
    Arg.setName(Args[Idx++]);

  return F;
}

Function *FunctionAST::codegen()
{
  auto &P = *Proto;
  FunctionProtos[Proto->getName()] = std::move(Proto);
  CurrentFunction = getFunction(P.getName());
  if (!CurrentFunction)
    return nullptr;

  BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", CurrentFunction);
  Builder->SetInsertPoint(BB);

  NamedValues.clear();

  for (auto &Arg : CurrentFunction->args())
  {
    Value *Ptr = NamedValues[std::string(Arg.getName())] = Builder->CreateAlloca(Arg.getType(), 0, Arg.getName());
    Builder->CreateStore(&Arg, Ptr);
  }

  for (auto &Stmt : Body)
  {
    if (!Stmt->codegen())
    {
      CurrentFunction->eraseFromParent();
      CurrentFunction = nullptr;
      return nullptr;
    }
  }

  verifyFunction(*CurrentFunction);
  Function *TheFunction = CurrentFunction;
  CurrentFunction = nullptr;
  return TheFunction;
}

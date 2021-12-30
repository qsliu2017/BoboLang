#include "Lex.h"
#include "Codegen.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
#include <map>
#include <memory>
#include <list>

/* Global flag indicates BlockAST::codegen() should copy args. */
static bool IsFunctionBlock = false;

static std::list<std::unique_ptr<std::map<std::string, AllocaInst *>>> VarScopes;
static AllocaInst *findVar(std::string Name)
{
  for (auto &Scope : VarScopes)
  {
    if (Scope->find(Name) != Scope->end())
      return (*Scope)[Name];
  }
  return nullptr;
}

static bool addVar(std::string Name, AllocaInst *Value)
{
  auto &Scope = VarScopes.front();
  if (Scope->find(Name) != Scope->end())
    return false;
  (*Scope)[Name] = Value;
  return true;
}

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
  Value *Ptr = findVar(Name);
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
  AllocaInst *Last;
  for (auto &Name : Names)
  {
    Type *Ty;
    if (ValType == type_int)
      Ty = Type::getInt32Ty(*TheContext);
    else if (ValType == type_double)
      Ty = Type::getDoubleTy(*TheContext);
    else
      return LogErrorV("unknown type");
    Last = Builder->CreateAlloca(Ty, 0, Name);
    if (!addVar(Name, Last))
      return LogErrorV("redeclare var");
  }
  return Last;
}

static Value *castValue(Value *V, Type *DestTy)
{
  if (V->getType() != DestTy)
  {
    auto castOp = DestTy->isFloatingPointTy() ? Instruction::CastOps::UIToFP : Instruction::CastOps::FPToUI;
    return Builder->CreateCast(castOp, V, DestTy);
  }
  return V;
}

Value *SimpStmtAST::codegen()
{
  auto Ptr = findVar(Name);
  if (!Ptr)
    return LogErrorV("undeclared var");
  Value *V = Expr->codegen();
  V = castValue(V, Ptr->getAllocatedType());
  Builder->CreateStore(V, Ptr);
  return V;
}

Value *BlockAST::codegen()
{
  VarScopes.push_front(std::make_unique<std::map<std::string, AllocaInst *>>());
  if (IsFunctionBlock)
  {
    auto TheFunction = Builder->GetInsertBlock()->getParent();
    for (auto &Arg : TheFunction->args())
    {
      auto Ptr = Builder->CreateAlloca(Arg.getType(), 0, Arg.getName());
      addVar(std::string(Arg.getName()), Ptr);
      Builder->CreateStore(&Arg, Ptr);
    }
    IsFunctionBlock = false;
  }

  Value *Last;
  for (auto &Stmt : Stmts)
  {
    if (!(Last = Stmt->codegen()))
      return nullptr;
  }

  VarScopes.pop_front();
  return Last;
}

Value *ReturnStmtAST::codegen()
{
  auto RetVal = Expr->codegen();
  RetVal = castValue(RetVal, Builder->GetInsertBlock()->getParent()->getReturnType());

  return Builder->CreateRet(RetVal);
}

static Value *getBoolValue(Value *Val)
{
  auto Type = Val->getType();
  if (Type->isFloatingPointTy())
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

  auto TheFunction = getFunction(P.getName());

  BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", TheFunction);
  Builder->SetInsertPoint(BB);

  IsFunctionBlock = true;
  if (!Body->codegen())
  {
    TheFunction->eraseFromParent();
    return nullptr;
  }

  verifyFunction(*TheFunction);
  return TheFunction;
}

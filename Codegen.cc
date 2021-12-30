#include "Lex.h"
#include "Codegen.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
#include <map>
#include <memory>
#include <list>

/* Global flag indicates BlockAST::codegen() should copy args. */
static bool IsFunctionBlock = false;

/* Variables of scope stack */
static std::list<std::unique_ptr<std::map<std::string, Value *>>> NamedValuesScope;

/* Variables on heap, GC heaper */
static std::list<std::unique_ptr<std::map<std::string, Value *>>> HeapValuesScope;

static Value *findVar(std::string Name)
{
  for (auto &Scope : NamedValuesScope)
  {
    if (Scope->find(Name) != Scope->end())
      return (*Scope)[Name];
  }
  return nullptr;
}

static bool addVar(std::string Name, Value *Value, bool onHeap = false)
{
  auto &Scope = NamedValuesScope.front();
  if (Scope->find(Name) != Scope->end())
    return false;

  (*Scope)[Name] = Value;
  if (onHeap)
    (*HeapValuesScope.front())[Name] = Value;
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

static Type *lowestCommonType(Type *Ty1, Type *Ty2)
{
  if (Ty1->isPointerTy())
    Ty1 = Ty1->getPointerElementType();
  if (Ty2->isPointerTy())
    Ty2 = Ty1->getPointerElementType();
  return (Ty1 == FPType || Ty2 == FPType) ? FPType : IntType;
}

static Value *getPointerElement(Value *Ptr)
{
  auto Type = Ptr->getType();
  if (Type->isPointerTy())
  {
    auto ElType = Type->getPointerElementType();
    return Builder->CreateLoad(ElType, Ptr);
  }
  return Ptr;
}

static Value *castValue(Value *V, Type *DestTy)
{
  auto Ty = V->getType();
  if (DestTy->isPointerTy())
  {
    if (!Ty->isPointerTy())
      return LogErrorV("cannot cast non-pointer to pointer");
    else if (Ty->getPointerElementType() != DestTy->getPointerElementType())
      return LogErrorV("cannot cast pointer to different type");
    else
      return V;
  }

  V = getPointerElement(V);
  if (Ty != DestTy)
  {
    auto castOp = DestTy->isFloatingPointTy() ? Instruction::CastOps::UIToFP : Instruction::CastOps::FPToUI;
    return Builder->CreateCast(castOp, V, DestTy);
  }
  return V;
}

Value *NumberDoubleExprAST::codegen()
{
  return ConstantFP::get(FPType, Val);
}

Value *NumberIntExprAST::codegen()
{
  return ConstantInt::get(IntType, Val);
}

Value *VariableExprAST::codegen()
{
  if (auto Ptr = findVar(Name))
    return Ptr;
  return LogErrorV("Unknown variable name");
}

Value *BinaryExprAST::codegen()
{
  auto L = LHS->codegen(), R = RHS->codegen();
  if (!L || !R)
    return nullptr;

  auto Ty = lowestCommonType(L->getType(), R->getType());
  L = castValue(L, Ty);
  R = castValue(R, Ty);

  if (Ty == FPType)
  {
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
    auto ArgTy = CalleeF->getArg(i)->getType();
    auto ArgVal = Args[i]->codegen();
    ArgsValue.push_back(castValue(ArgVal, ArgTy));
    if (!ArgsValue.back())
      return nullptr;
  }
  return Builder->CreateCall(CalleeF, ArgsValue);
}

Value *DeclStmtAST::codegen()
{
  Instruction *Last;
  for (auto &Name : Names)
  {
    Type *Ty, *PtrTy;
    switch (ValType)
    {
    case type_int:
      Ty = IntType;
      goto DeclStackVar;
    case type_double:
      Ty = FPType;
    DeclStackVar:
      Last = Builder->CreateAlloca(Ty, 0, Name);
      if (!addVar(Name, Last))
        return LogErrorV("redeclare var");
      break;

    case type_intptr:
      Ty = IntType;
      PtrTy = IntPtrType;
      goto DeclHeapVar;
    case type_doubleptr:
      Ty = FPType;
      PtrTy = FPPtrType;
    DeclHeapVar:
      Last = CallInst::CreateMalloc(Builder->GetInsertBlock(),
                                    PtrTy,
                                    Ty,
                                    ConstantExpr::getSizeOf(Ty), nullptr, nullptr);
      Builder->GetInsertBlock()->getInstList().push_back(Last);
      if (!addVar(Name, Last, true))
        return LogErrorV("redeclare var");
      break;

    default:
      return LogErrorV("unknown type");
    }
  }
  return Last;
}

Value *SimpStmtAST::codegen()
{
  auto Ptr = findVar(Name);
  if (!Ptr)
    return LogErrorV("undeclared var");
  Value *V = Expr->codegen();
  V = castValue(V, Ptr->getType()->getPointerElementType());
  Builder->CreateStore(V, Ptr);
  return V;
}

Value *BlockAST::codegen()
{
  NamedValuesScope.push_front(std::make_unique<std::map<std::string, Value *>>());
  HeapValuesScope.push_front(std::make_unique<std::map<std::string, Value *>>());
  if (IsFunctionBlock)
  {
    auto TheFunction = Builder->GetInsertBlock()->getParent();
    for (auto &Arg : TheFunction->args())
    {
      auto ArgTy = Arg.getType();
      if (ArgTy->isPointerTy())
        addVar(std::string(Arg.getName()), &Arg);
      else
      {
        auto Ptr = Builder->CreateAlloca(ArgTy, 0, Arg.getName());
        addVar(std::string(Arg.getName()), Ptr);
        Builder->CreateStore(&Arg, Ptr);
      }
    }
    IsFunctionBlock = false;
  }

  Value *Last;
  for (auto &Stmt : Stmts)
  {
    if (!(Last = Stmt->codegen()))
      return nullptr;
  }

  NamedValuesScope.pop_front();
  auto BB = Builder->GetInsertBlock();
  if (!isa<ReturnInst>(BB->getInstList().back()))
  {
    for (auto &Var : *HeapValuesScope.front())
    {
      auto GC = CallInst::CreateFree(Var.second, BB);
      BB->getInstList().push_back(GC);
    }
  }
  HeapValuesScope.pop_front();
  return Last;
}

Value *ReturnStmtAST::codegen()
{
  auto RetVal = Expr->codegen();
  auto BB = Builder->GetInsertBlock();
  auto RetTy = BB->getParent()->getReturnType();
  RetVal = castValue(RetVal, BB->getParent()->getReturnType());
  auto &Scope = HeapValuesScope.front();
  for (auto &Pair : *Scope)
  {
    if (RetTy->isPointerTy() && Pair.second == RetVal)
      continue;
    else
      BB->getInstList().push_back(CallInst::CreateFree(Pair.second, BB));
  }
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
  switch (FnType)
  {
  case type_int:
    ResultTy = IntType;
    break;
  case type_double:
    ResultTy = FPType;
    break;
  case type_intptr:
    ResultTy = IntPtrType;
    break;
  case type_doubleptr:
    ResultTy = FPPtrType;
    break;
  default:
    return LogErrorF("unknown return type");
  }

  std::vector<Type *> ArgsTy;
  for (int i = 0, e = Args.size(); i < e; i++)
  {
    switch (ArgTypes[i])
    {
    case type_int:
      ArgsTy.push_back(IntType);
      break;
    case type_double:
      ArgsTy.push_back(FPType);
      break;
    case type_intptr:
      ArgsTy.push_back(IntPtrType);
      break;
    case type_doubleptr:
      ArgsTy.push_back(FPPtrType);
      break;
    default:
      return LogErrorF("unknown arg type");
    }
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

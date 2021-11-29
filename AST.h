#ifndef AST_H
#define AST_H
#include "llvm/IR/Value.h"
#include <vector>

using namespace llvm;

class ExprAST
{
public:
  virtual ~ExprAST() = default;
#ifdef AST_CODEGEN
  virtual Value *codegen() = 0;
#endif
#ifdef AST_OUTPUT
  virtual void output() = 0;
#endif
};

class NumberDoubleExprAST : public ExprAST
{
  double Val;

public:
  NumberDoubleExprAST(double Val) : Val(Val) {}
#ifdef AST_CODEGEN
  Value *codegen() override;
#endif
#ifdef AST_OUTPUT
  void output() override
  {
    std::cout << "(Double Val: " << Val << ")";
  }
#endif
};

class NumberIntExprAST : public ExprAST
{
  int Val;

public:
  NumberIntExprAST(int Val) : Val(Val) {}
#ifdef AST_CODEGEN
  Value *codegen() override;
#endif
#ifdef AST_OUTPUT
  void output() override
  {
    std::cout << "(Int Val: " << Val << ")";
  }
#endif
};

class VariableExprAST : public ExprAST
{
  std::string Name;

public:
  VariableExprAST(const std::string &Name) : Name(Name) {}
#ifdef AST_CODEGEN
  Value *codegen() override;
#endif
#ifdef AST_OUTPUT
  void output() override
  {
    std::cout << "(Val: " << Name << ")";
  }
#endif
};

class BinaryExprAST : public ExprAST
{
  const char Op;
  std::unique_ptr<ExprAST> LHS, RHS;

public:
  BinaryExprAST(const char Op,
                std::unique_ptr<ExprAST> LHS,
                std::unique_ptr<ExprAST> RHS)
      : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
#ifdef AST_CODEGEN
  Value *codegen() override;
#endif
#ifdef AST_OUTPUT
  void output() override
  {
    std::cout << "(";
    LHS->output();
    std::cout << Op;
    RHS->output();
    std::cout << ")";
  }
#endif
};

class CallExprAST : public ExprAST
{
  std::string Callee;
  std::vector<std::unique_ptr<ExprAST>> Args;

public:
  CallExprAST(const std::string &Callee,
              std::vector<std::unique_ptr<ExprAST>> Args)
      : Callee(Callee), Args(std::move(Args)) {}
#ifdef AST_CODEGEN
  Value *codegen() override;
#endif
#ifdef AST_OUTPUT
  void output() override
  {
    std::cout << "(Call: " << Callee << " (Args: ";
    for (auto &Arg : Args)
      Arg->output();
    std::cout << "))";
  }
#endif
};

class StmtAST
{
public:
  virtual ~StmtAST() = default;
#ifdef AST_CODEGEN
  virtual Value *codegen() = 0;
#endif
#ifdef AST_OUTPUT
  virtual void output() = 0;
#endif
};

class DeclStmtAST : public StmtAST
{
  int ValType;
  std::vector<std::string> Names;

public:
  DeclStmtAST(int ValType, std::vector<std::string> Names)
      : ValType(ValType), Names(std::move(Names)) {}
#ifdef AST_CODEGEN
  Value *codegen() override;
#endif
#ifdef AST_OUTPUT
  void output() override
  {
    std::cout << "Declaration type " << ValType << " ( ";
    for (auto Name : Names)
      std::cout << Name << " ";
    std::cout << ");" << std::endl;
  }
#endif
};

class SimpStmtAST : public StmtAST
{
  std::string Name;
  std::unique_ptr<ExprAST> Expr;

public:
  SimpStmtAST(const std::string &Name, std::unique_ptr<ExprAST> Expr)
      : Name(Name), Expr(std::move(Expr)) {}
#ifdef AST_CODEGEN
  Value *codegen() override;
#endif
#ifdef AST_OUTPUT
  void output() override
  {
    std::cout << "Assignment: " << Name << " = ";
    Expr->output();
    std::cout << ";" << std::endl;
  }
#endif
};

class ReturnStmtAST : public StmtAST
{
  const std::unique_ptr<ExprAST> Expr;

public:
  ReturnStmtAST(std::unique_ptr<ExprAST> Expr)
      : Expr(std::move(Expr)) {}
#ifdef AST_CODEGEN
  Value *codegen() override;
#endif
#ifdef AST_OUTPUT
  void output() override
  {
    std::cout << "Return: ";
    Expr->output();
    std::cout << ";" << std::endl;
  }
#endif
};

class PrototypeAST
{
  std::string Name;
  std::vector<std::string> Args;
  std::vector<int> ArgTypes;
  int FnType;

public:
  PrototypeAST(const std::string &Name,
               std::vector<std::string> Args,
               std::vector<int> ArgTypes,
               const int FnType)
      : Name(Name),
        Args(std::move(Args)),
        ArgTypes(std::move(ArgTypes)),
        FnType(FnType) {}
#ifdef AST_CODEGEN
  Function *codegen();
#endif

  const std::string &getName() const
  {
    return Name;
  }
  const int getReturnType() { return FnType; }
  const std::vector<int> &getArgTypes() { return ArgTypes; }
#ifdef AST_OUTPUT
  void output()
  {
    std::cout << "(Function: " << Name << std::endl
              << "(Args:";
    for (auto &Arg : Args)
      std::cout << " " << Arg;
    std::cout << ")" << std::endl
              << "(ArgTypes:";
    for (auto ArgType : ArgTypes)
      std::cout << " " << ArgType;
    std::cout << ")" << std::endl
              << "(FnType: " << FnType << "))" << std::endl;
  }
#endif
};

class FunctionAST
{
  std::unique_ptr<PrototypeAST> Proto;
  std::vector<std::unique_ptr<StmtAST>> Body;

public:
  FunctionAST(std::unique_ptr<PrototypeAST> Proto,
              std::vector<std::unique_ptr<StmtAST>> Body)
      : Proto(std::move(Proto)), Body(std::move(Body)) {}
#ifdef AST_CODEGEN
  Function *codegen();
#endif
#ifdef AST_OUTPUT
  void output()
  {
    Proto->output();
    std::cout << "{" << std::endl;
    for (auto &Stmt : Body)
      Stmt->output();
    std::cout << "}" << std::endl;
  }
#endif
};

#endif

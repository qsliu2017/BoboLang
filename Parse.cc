#include "Lex.h"
#include "Parse.h"

static std::unique_ptr<ExprAST> LogErrorE(const char *Str)
{
  fprintf(stderr, "Error: %s\n", Str);
  return nullptr;
}

static std::unique_ptr<StmtAST> LogErrorS(const char *Str)
{
  LogErrorE(Str);
  return nullptr;
}

static std::unique_ptr<PrototypeAST> LogErrorP(const char *Str)
{
  LogErrorE(Str);
  return nullptr;
}

static std::unique_ptr<FunctionAST> LogErrorF(const char *Str)
{
  LogErrorE(Str);
  return nullptr;
}

std::unique_ptr<PrototypeAST> ParseExternFunctionDeclaration()
{
  if (getNextToken() != tok_def) // eat 'extern'
    return LogErrorP("Expected type declaration");
  auto Proto = ParsePrototype();
  if (!Proto)
    return nullptr;
  if (CurTok != ';')
    return LogErrorP("Expected ';' in global declaration");
  getNextToken(); // eat ';'
  return Proto;
}

std::unique_ptr<FunctionAST> ParseFunctionDefinition()
{
  auto Proto = ParsePrototype();
  if (!Proto)
    return nullptr;
  if (CurTok != '{')
    return LogErrorF("Expected '{' in function");
  getNextToken(); // eat '{'

  std::vector<std::unique_ptr<StmtAST>> Body;
  while (CurTok != '}')
  {
    auto Stmt = ParseStatement();
    if (!Stmt)
      return nullptr;
    Body.push_back(std::move(Stmt));
  }
  getNextToken(); // eat '}'
  return std::make_unique<FunctionAST>(std::move(Proto), std::move(Body));
}

std::unique_ptr<PrototypeAST> ParsePrototype()
{
  Types FnType = ValType;
  getNextToken(); // eat ValType

  if (CurTok != tok_identifier)
    return LogErrorP("Expected function name in prototype");
  std::string FnName = IdentifierStr;
  getNextToken(); // eat FnName

  if (CurTok != '(')
    return LogErrorP("Expected '(' in prototype");
  getNextToken(); // eat '('.

  std::vector<std::string> ArgNames;
  std::vector<int> ArgTypes;

  if (CurTok == ')')
    goto ParsePrototype_NoArg;
  else
    goto ParsePrototype_FirstArg;

  while (CurTok == ',')
  {
    getNextToken(); // eat ','

  ParsePrototype_FirstArg:
    if (CurTok != tok_def)
      return LogErrorP("Expected param type in prototype");
    ArgTypes.push_back(ValType);
    getNextToken();

    if (CurTok != tok_identifier)
      return LogErrorP("Expected param name in prototype");
    ArgNames.push_back(IdentifierStr);
    getNextToken();
  }

  if (CurTok != ')')
    return LogErrorP("Expected ')' in prototype");
ParsePrototype_NoArg:
  getNextToken(); // eat ')'.

  return std::make_unique<PrototypeAST>(FnName, std::move(ArgNames), std::move(ArgTypes), FnType);
}

std::unique_ptr<StmtAST> ParseStatement()
{
  std::unique_ptr<StmtAST> Stmt;
  if (CurTok == tok_identifier)
    Stmt = ParseSimpleAssignment();
  else if (CurTok == tok_def)
    Stmt = ParseVarDeclaration();
  else if (CurTok == tok_return)
    Stmt = ParseReturn();
  else
    return LogErrorS("Expected statement");

  if (CurTok != ';')
    return LogErrorS("Expected ';' after statement");
  getNextToken(); // eat ';'

  return std::move(Stmt);
}

std::unique_ptr<StmtAST> ParseVarDeclaration()
{
  int Type = ValType;
  std::vector<std::string> Names;

  do
  {
    if (getNextToken() != tok_identifier)
      return LogErrorS("Expected identifier in declaration");
    Names.push_back(IdentifierStr);
  } while (getNextToken() == ',');

  return std::make_unique<DeclStmtAST>(Type, std::move(Names));
}

std::unique_ptr<StmtAST> ParseSimpleAssignment()
{
  std::string Name = IdentifierStr;
  if (getNextToken() != '=')
    return LogErrorS("Expected '=' in simple statement");
  getNextToken(); // eat '='

  auto Expr = ParseExpression();
  if (!Expr)
    return nullptr;

  return std::make_unique<SimpStmtAST>(Name, std::move(Expr));
}

std::unique_ptr<StmtAST> ParseReturn()
{
  getNextToken(); // eat 'return'
  auto Expr = ParseExpression();
  if (!Expr)
    return nullptr;

  return std::make_unique<ReturnStmtAST>(std::move(Expr));
}

std::unique_ptr<ExprAST> ParseExpression()
{
  auto Value = ParseValue();
  if (!Value)
    return nullptr;

  if (CurTok == '<')
  {
    auto Op = CurTok;
    getNextToken();

    auto Expr = ParseExpression();
    if (!Expr)
      return nullptr;

    return std::make_unique<BinaryExprAST>(Op, std::move(Value), std::move(Expr));
  }

  return std::move(Value);
}

std::unique_ptr<ExprAST> ParseValue()
{
  auto Term = ParseTerm();
  if (!Term)
    return nullptr;

  if (CurTok == '+' || CurTok == '-')
  {
    auto Op = CurTok;
    getNextToken();

    auto Value = ParseValue();
    if (!Value)
      return nullptr;

    return std::make_unique<BinaryExprAST>(Op, std::move(Term), std::move(Value));
  }

  return std::move(Term);
}

std::unique_ptr<ExprAST> ParseTerm()
{
  auto Factor = ParseFactor();
  if (!Factor)
    return nullptr;

  if (CurTok == '*')
  {
    auto Op = CurTok;
    getNextToken();

    auto Term = ParseTerm();
    if (!Term)
      return nullptr;

    return std::make_unique<BinaryExprAST>(Op, std::move(Factor), std::move(Term));
  }

  return std::move(Factor);
}

std::unique_ptr<ExprAST> ParseFactor()
{
  if (CurTok == '(')
  {
    getNextToken(); // eat '('

    auto Expr = ParseExpression();
    if (!Expr)
      return nullptr;

    if (CurTok != ')')
      return LogErrorE("Expected ')'");
    getNextToken(); // eat ')'

    return Expr;
  }
  else if (CurTok == tok_identifier)
    return ParseIdentifierExpr();
  else if (CurTok == tok_number_int)
    return ParseNumberExpr(type_int);
  else if (CurTok == tok_number_double)
    return ParseNumberExpr(type_double);
  else
    return LogErrorE("Expected a factor");
}

std::unique_ptr<ExprAST> ParseNumberExpr(int NumberType)
{
  std::unique_ptr<ExprAST> NumExpr;
  if (NumberType == type_double)
  {
    NumExpr = std::make_unique<NumberDoubleExprAST>(NumVal.NumValD);
  }
  else if (NumberType == type_int)
  {
    NumExpr = std::make_unique<NumberIntExprAST>(NumVal.NumValI);
  }
  else
  {
    return LogErrorE("Unknown number type");
  }
  getNextToken(); // eat number
  return std::move(NumExpr);
}

std::unique_ptr<ExprAST> ParseIdentifierExpr()
{
  auto ident = IdentifierStr;
  getNextToken();

  if (CurTok != '(')
    return std::make_unique<VariableExprAST>(ident);
  getNextToken(); // eat '('

  std::vector<std::unique_ptr<ExprAST>> Args;

  if (CurTok == ')')
    goto ParseIdentifierExpr_NoArg;
  else
    goto ParseIdentifierExpr_FirstArg;

  while (CurTok == ',')
  {
    getNextToken(); // eat ','
  ParseIdentifierExpr_FirstArg:
    auto Expr = ParseExpression();
    if (!Expr)
      return nullptr;
    Args.push_back(std::move(Expr));
  }

  if (CurTok != ')')
    return LogErrorE("Expected ')' in callee");
ParseIdentifierExpr_NoArg:
  getNextToken(); // eat ')'

  return std::make_unique<CallExprAST>(ident, std::move(Args));
}

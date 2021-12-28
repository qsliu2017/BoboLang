#ifndef PARSE_H
#define PARSE_H
#include "AST.h"

std::unique_ptr<PrototypeAST> ParseExternFunctionDeclaration();
std::unique_ptr<FunctionAST> ParseFunctionDefinition();
std::unique_ptr<PrototypeAST> ParsePrototype();

std::unique_ptr<StmtAST> ParseStatement();
std::unique_ptr<StmtAST> ParseVarDeclaration();
std::unique_ptr<StmtAST> ParseSimpleAssignment();
std::unique_ptr<StmtAST> ParseReturn();
std::unique_ptr<StmtAST> ParseIfElse();
std::unique_ptr<StmtAST> ParseWhile();

std::unique_ptr<ExprAST> ParseExpression();
std::unique_ptr<ExprAST> ParseValue();
std::unique_ptr<ExprAST> ParseTerm();
std::unique_ptr<ExprAST> ParseFactor();
std::unique_ptr<ExprAST> ParseNumberExpr(int NumberType);
std::unique_ptr<ExprAST> ParseIdentifierExpr();

#endif

#ifndef CODEGEN_H
#define CODEGEN_H

#define AST_CODEGEN
#include "Parse.h"
#include "llvm/IR/IRBuilder.h"

extern std::unique_ptr<LLVMContext> TheContext;
extern std::unique_ptr<Module> TheModule;
extern std::unique_ptr<IRBuilder<>> Builder;
extern std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;

#undef AST_CODEGEN
#endif

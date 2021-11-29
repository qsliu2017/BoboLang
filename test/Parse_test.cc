#include <iostream>
#define AST_OUTPUT
#include "../Lex.h"
#include "../AST.h"
#include "../Parse.h"

void HandleDefinition()
{
  if (auto FnAST = ParseFunctionDefinition())
  {
    std::cout << "Parsed a function definition" << std::endl;
    FnAST->output();
  }
  else
  {
    // Skip token for error recovery.
    getNextToken();
  }
}

void HandleExtern()
{
  if (auto ProtoAST = ParseExternFunctionDeclaration())
  {
    std::cout << "Parsed an extern" << std::endl;
    ProtoAST->output();
  }
  else
  {
    // Skip token for error recovery.
    getNextToken();
  }
}

void MainLoop()
{
  while (true)
  {
    switch (CurTok)
    {
    case tok_eof:
      return;
    case tok_def:
      HandleDefinition();
      break;
    case tok_extern:
      HandleExtern();
      break;
    default:
      std::cout << "invalid input" << std::endl;
      getNextToken();
      break;
    }
  }
}

int main(int argc, char *argv[])
{

  if (argc < 2)
  {
    std::cout << "You need to specify the file to compile" << std::endl;
    return 1;
  }
  char *FileName = argv[1];
  fip = fopen(FileName, "r");
  if (fip == nullptr)
  {
    std::cout << "The file '" << FileName << "' is not existed" << std::endl;
    return 1;
  }

  getNextToken();

  MainLoop();

  return 0;
}

#undef AST_OUTPUT

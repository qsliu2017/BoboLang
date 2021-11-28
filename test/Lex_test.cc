#include "llvm/Support/raw_ostream.h"
#include "../Lex.h"
using namespace llvm;

int main(int argc, char* argv[]) {
  //file check
  if(argc < 2){
    std::cout << "You need to specify the file to compile" << std::endl;
    return 1;
  }
  char* FileName = argv[1];
  fip = fopen(FileName, "r");
  if(fip == nullptr){
    std::cout << "The file '" << FileName << "' is not existed" << std::endl;
    return 1;
  }

  //output
  while (true) {
    getNextToken();
    switch (CurTok){
      case tok_def:
        std::cout << CurTok << " " << ValType << std::endl;
        break;
      case tok_number_double:
        std::cout << CurTok << " " << NumVal.NumValD << std::endl;
        break;
      case tok_number_int:
        std::cout << CurTok << " " << NumVal.NumValI << std::endl;
        break;
      default:
        std::cout << CurTok << std::endl;
        break;
    }
    if(CurTok == -1){
      break;
    }
  }
  return 0;
}

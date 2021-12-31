#include "Lex.h"

FILE *fip;
int CurTok;
std::string IdentifierStr;
union NumVal NumVal;
Types ValType;

static std::map<std::string, Types> TypeValues{
    {"int", type_int},
    {"double", type_double},
    {"Int", type_intptr},
    {"Double", type_doubleptr},
};

static std::map<std::string, Token> ReservedValues{
    {"int", tok_def},
    {"double", tok_def},
    {"Int", tok_def},
    {"Double", tok_def},
    {"extern", tok_extern},
    {"return", tok_return},
    {"if", tok_if},
    {"else", tok_else},
    {"while", tok_while},
};

int gettok()
{
  static int LastChar = ' ';
  std::map<std::string, int>::iterator iter;

  while (isspace(LastChar))
    LastChar = fgetc(fip);

  if (isalpha(LastChar))
  {
    IdentifierStr = LastChar;
    while (isalnum(LastChar = fgetc(fip)))
      IdentifierStr += LastChar;

    auto ty = TypeValues.find(IdentifierStr);
    if (ty != TypeValues.end())
      ValType = ty->second;

    auto tok = ReservedValues.find(IdentifierStr);
    if (tok != ReservedValues.end())
      return tok->second;

    return tok_identifier;
  }

  if (isdigit(LastChar))
  {
    IdentifierStr = LastChar;
    bool isDouble = false;
    while (isdigit(LastChar = fgetc(fip)) || LastChar == '.')
    {
      isDouble |= LastChar == '.';
      IdentifierStr += LastChar;
    }
    if (isDouble)
    {
      NumVal.NumValD = atof(IdentifierStr.c_str());
      return tok_number_double;
    }
    else
    {
      NumVal.NumValI = atoi(IdentifierStr.c_str());
      return tok_number_int;
    }
  }

  if (LastChar == EOF)
    return tok_eof;

  int ThisChar = LastChar;
  LastChar = fgetc(fip);
  return ThisChar;
}

int getNextToken() { return CurTok = gettok(); }

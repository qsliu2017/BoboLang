#ifndef LEX_H
#define LEX_H
#include <iostream>
#include <map>

enum Token
{
	tok_eof = -1,
	tok_def = -2,
	tok_extern = -3,
	tok_identifier = -4,
	tok_number_int = -5,
	tok_number_double = -6,
	tok_return = -7,
	tok_if = -8,
	tok_else = -9,
	tok_while = -10,
};

enum Types
{
	type_int = 1,
	type_double = 2
};

union NumVal
{
	int NumValI;
	double NumValD;
};

int gettok();
extern int CurTok;
int getNextToken();
extern FILE *fip;
extern std::string IdentifierStr;
extern union NumVal NumVal;
extern Types ValType;

#endif

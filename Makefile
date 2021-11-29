CC=clang++
LLVMFLAG=$(shell llvm-config --cxxflags --ldflags --system-libs --libs all)
FLAG=$(LLVMFLAG) -std=c++17

Lex.o: Lex.cc Lex.h
	$(CC) $(FLAG) -c -o Lex.o Lex.cc

Parse.o: Parse.cc Parse.h AST.h Lex.o
	$(CC) $(FLAG) -c -o Parse.o Parse.cc Lex.o

Codegen.o : Codegen.cc Codegen.h Parse.o
	$(CC) $(FLAG) -c -o Codegen.o Codegen.cc Parse.o

Lex_test.o: test/Lex_test.cc Lex.o
	$(CC) $(FLAG) -o Lex_test.o test/Lex_test.cc Lex.o

Parse_test.o: test/Parse_test.cc Parse.o
	$(CC) $(FLAG) -o Parse_test.o Parse.o Lex.o test/Parse_test.cc

Codegen_test.o : test/Codegen_test.cc Codegen.o
	$(CC) $(FLAG) -o Codegen_test.o Codegen.o Parse.o Lex.o test/Codegen_test.cc

.PONNY: test_Lex test_Parse test_Codegen

test_Lex: Lex_test.o
	@echo "Expect:"
	@cat test/lex_test_output.data
	@echo "Actual:"
	@./Lex_test.o test/lex_test_input.data

test_Parse: Parse_test.o
	@echo "Expect:"
	@cat test/parse_output.data
	@echo "Actual:"
	@./Parse_test.o test/parse_input.data

test_Codegen: Codegen_test.o
	@echo "Expect:"
	@cat test/codegen_output.data
	@echo "Actual:"
	@./Codegen_test.o test/codegen_input.data

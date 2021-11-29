CC=clang++
LLVMFLAG=$(shell llvm-config --cxxflags --ldflags --system-libs --libs all)
FLAG=$(LLVMFLAG) -std=c++17

Lex.o: Lex.cc Lex.h
	$(CC) $(FLAG) -c -o Lex.o Lex.cc

Parse.o: Parse.cc Parse.h AST.h Lex.o
	$(CC) $(FLAG) -c -o Parse.o Parse.cc Lex.o

Lex_test.o: test/Lex_test.cc Lex.o
	$(CC) $(FLAG) -o Lex_test.o test/Lex_test.cc Lex.o

Parse_test.o: test/Parse_test.cc Parse.o Lex.o
	$(CC) $(FLAG) -o Parse_test.o Lex.o Parse.o test/Parse_test.cc

.PONNY: test_Lex

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

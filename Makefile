CC=clang++
LLVMFLAG=$(shell llvm-config --cxxflags --ldflags --system-libs --libs all)
FLAG=$(LLVMFLAG) -std=c++17

Lex.o: Lex.cc Lex.h
	$(CC) $(FLAG) -c -o Lex.o Lex.cc

Lex_test.o: test/Lex_test.cc Lex.o
	$(CC) $(FLAG) -o Lex_test.o test/Lex_test.cc Lex.o

.PONNY: test_Lex

test_Lex: Lex_test.o
	@echo "Expect:"
	@cat test/lex_test_output.data
	@echo "Actual:"
	@./Lex_test.o test/lex_test_input.data

#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "../Codegen.h"
#include "../Lex.h"
#include <memory>

std::unique_ptr<LLVMContext> TheContext;
std::unique_ptr<Module> TheModule;
std::unique_ptr<IRBuilder<>> Builder;

static void InitializeModuleAndPassManager()
{
	// Open a new context and module.
	TheContext = std::make_unique<LLVMContext>();
	TheModule = std::make_unique<Module>("my cool jit", *TheContext);

	// Create a new builder for the module.
	Builder = std::make_unique<IRBuilder<>>(*TheContext);
}

static void HandleDefinition()
{
	if (auto FnAST = ParseFunctionDefinition())
	{
		if (auto *FnIR = FnAST->codegen())
		{
			fprintf(stderr, "Read function definition:");
			FnIR->print(errs());
			fprintf(stderr, "\n");
		}
	}
	else
	{
		// Skip token for error recovery.
		getNextToken();
	}
}

static void HandleExtern()
{
	if (auto ProtoAST = ParseExternFunctionDeclaration())
	{
		if (auto *FnIR = ProtoAST->codegen())
		{
			fprintf(stderr, "Read extern: ");
			FnIR->print(errs());
			fprintf(stderr, "\n");
			FunctionProtos[ProtoAST->getName()] = std::move(ProtoAST);
		}
	}
	else
	{
		// Skip token for error recovery.
		getNextToken();
	}
}

/// top ::= definition | external | expression | ';'
static void MainLoop()
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
//===----------------------------------------------------------------------===//
// Main driver code.
//===----------------------------------------------------------------------===//
//don't modify this part

int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		errs() << "You need to specify the file to compile";
		return 1;
	}
	char *FileName = argv[1];
	fip = fopen(FileName, "r");
	if (fip == nullptr)
	{
		errs() << "The file '" << FileName << "' is not existed";
		return 1;
	}

	getNextToken();

	InitializeModuleAndPassManager();
	MainLoop();

	InitializeAllTargetInfos();
	InitializeAllTargets();
	InitializeAllTargetMCs();
	InitializeAllAsmParsers();
	InitializeAllAsmPrinters();

	auto TargetTriple = sys::getDefaultTargetTriple();
	TheModule->setTargetTriple(TargetTriple);

	std::string Error;
	auto Target = TargetRegistry::lookupTarget(TargetTriple, Error);

	// Print an error and exit if we couldn't find the requested target.
	// This generally occurs if we've forgotten to initialise the
	// TargetRegistry or we have a bogus target triple.
	if (!Target)
	{
		errs() << Error;
		return 1;
	}

	auto CPU = "generic";
	auto Features = "";

	TargetOptions opt;
	auto RM = Optional<Reloc::Model>();
	auto TheTargetMachine =
	    Target->createTargetMachine(TargetTriple, CPU, Features, opt, RM);
	TheModule->setDataLayout(TheTargetMachine->createDataLayout());

	auto Filename = "output.o";
	std::error_code EC;
	raw_fd_ostream dest(Filename, EC, sys::fs::OF_None);

	if (EC)
	{
		errs() << "Could not open file: " << EC.message();
		return 1;
	}

	legacy::PassManager pass;
	auto FileType = CGFT_ObjectFile;

	if (TheTargetMachine->addPassesToEmitFile(pass, dest, nullptr, FileType))
	{
		errs() << "TheTargetMachine can't emit a file of this type";
		return 1;
	}

	pass.run(*TheModule);
	dest.flush();

	outs() << "Wrote " << Filename << "\n";

	return 0;
}
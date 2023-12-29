#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <stack>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <llvm/IR/Attributes.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/Alignment.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>

#include "ds_utils.hpp"

static std::vector<Token> tokens = {};
static char next = 0;

std::vector<Token> Lex(const char *fileName) // TODO: convert to filepath
{
    std::ifstream fp(fileName);
    char c = 0, p = 0;
    int amount = 0;
    std::vector<Token> r;
    do {
        fp.get(c);
        if (!isvalidBF(c)) continue;

        switch (p) {
        case '>':
            if (c == '>' || c == '<') amount++;
            else {
                r.push_back({'m', ++amount});
                amount = 0;
            }
            break;
        case '<':
            if (c == '>' || c == '<') amount--;
            else {
                r.push_back({'m', --amount});
                amount = 0;
            }
            break;
        case '+':
            if (c == '+' || c == '-') amount++;
            else {
                r.push_back({'n', ++amount});
                amount = 0;
            }
            break;
        case '-':
            if (c == '+' || c == '-') amount--;
            else {
                r.push_back({'n', --amount});
                amount = 0;
            }
            break;
        case '[':
        case ']':
        case '.':
        case ',':
            r.push_back({p, 1});
            amount = 0;
            break;
        }
        p = c;
    } while (fp.good());
    return r;
}

std::vector<Token> Optimize(const std::vector<Token> &v)
{
    std::vector<Token> r;
    bool hc = false;
    for (size_t i = 0; i < v.size(); ++i) {
        // Subtitutes `[-]` for 'e' instruction
        if (i < v.size() - 3 && v.at(i).c == '[' &&
            v.at(i + 1) == Token{'n', -1} && v.at(i + 2).c == ']') {
            // Prevents generation of 'e' inst before any 'n' insts are made
            if(hc) r.push_back({'e', 1});
            i += 2;
        }
        else r.push_back(v.at(i));
        if (v.at(i).c == 'n') hc = true;
    }
    return r;
}

class AST {
public:
    AST() {}
    virtual ~AST();
    virtual llvm::Value *codegen() = 0;
    virtual void spew() = 0;
};

AST::~AST() {} //! DO NOT REMOVE, this is used so that no linker vtable error occur

class ExprAST : public AST {
    std::vector<Token> tok;

    public:
    ExprAST(std::vector<Token> &&vec) : tok(std::move(vec)) {}

    llvm::Value *codegen() override;
    void spew() override
    {
        std::cout << "(( ";
        for (auto &n : this->tok)
            std::cout << n.c << ", " << n.amount << "; ";
        std::cout << "))\n";
    }
};

class StmtAST : public AST {
    std::optional<std::vector<std::unique_ptr<AST>>> children;

    public:
    StmtAST(std::vector<std::unique_ptr<AST>> &&vptr) : children(std::move(vptr)) {}

    llvm::Value *codegen() override;
    void spew() override
    {
        if (this->children)
        {
            std::cout << "{{{\n";
            for (auto &n : this->children.value())
                n->spew();
            std::cout << "}}}\n";
        }
    }
    friend void StartCodeGen(const char *pname, std::unique_ptr<StmtAST> &&p);
};

static std::vector<Token> tokens;

std::unique_ptr<AST> ParseExpression(std::vector<Token>::const_iterator &nit)
{
    std::vector<Token> t;
    while (nit->c != '[' && nit->c != ']' && nit < tokens.cend())
        t.push_back(*(nit++));
    if (t.size() == 0)
        return nullptr;
    if (nit == tokens.cend())
        t.push_back(Token::None);
    return std::make_unique<ExprAST>(std::move(t));
}

std::unique_ptr<AST> ParseStatement(std::vector<Token>::const_iterator &nit)
{
    std::vector<std::unique_ptr<AST>> r;
    while (nit < tokens.cend()) {
        if (auto e = ParseExpression(nit); e != nullptr)
            r.push_back(std::move(e));
        if (nit->c == '[') r.push_back(std::move(ParseStatement(++nit)));
        if (nit->c == ']')
        {
            ++nit;
            return std::make_unique<StmtAST>(std::move(r));
        }
    }
    return std::make_unique<StmtAST>(std::move(r));
}

std::unique_ptr<AST> ParseTopStatement(std::vector<Token>::const_iterator &nit)
{
    std::vector<std::unique_ptr<AST>> r;
    do {
        if (auto e = ParseExpression(nit); e != nullptr)
            r.push_back(std::move(e));
        if (nit->c == '[')
        {
            r.push_back(std::move(ParseStatement(++nit)));
            --nit;
        }
    } while ((++nit) < tokens.cend());
    return std::make_unique<StmtAST>(std::move(r));
}

static std::unique_ptr<llvm::LLVMContext> TheContext;
static std::unique_ptr<llvm::Module> TheModule;
static std::unique_ptr<llvm::IRBuilder<>> Builder, MainFunction;
static llvm::AllocaInst *MainTape;
static llvm::Value *MainPointer;
static llvm::Function *MainFFunction, *FPutChar, *FGetChar;

llvm::LoadInst *getCurPointer()
{
    return MainFunction->CreateLoad(Builder->getInt8PtrTy(), MainPointer);
}

llvm::LoadInst *getCurPointerValue()
{
    return MainFunction->CreateLoad(Builder->getInt8Ty(), getCurPointer());
}

llvm::Value *TokenCodegen(Token &t) // TODO: Make this a class function
{
    switch (t.c) {
    case 'm': {
        auto cur = getCurPointer();
        return MainFunction->CreateStore(
            MainFunction->CreateGEP(Builder->getInt8Ty(), cur,
                                    Builder->getInt32(t.amount), "", true),
            MainPointer);
    }
    case 'n': {
        auto cur = getCurPointer();
        return MainFunction->CreateStore(
            MainFunction->CreateAdd(
                MainFunction->CreateLoad(Builder->getInt8Ty(), cur),
                Builder->getInt8(t.amount)),
            cur);
    }
    case 'e':
        return MainFunction->CreateStore(Builder->getInt8(0), getCurPointer());
    case ',':
        return MainFunction->CreateStore(
            MainFunction->CreateTrunc(MainFunction->CreateCall(FGetChar),
                                      Builder->getInt8Ty()),
            getCurPointer());
    case '.':
        return MainFunction->CreateCall(
            FPutChar, MainFunction->CreateZExt(getCurPointerValue(),
                                               Builder->getInt32Ty()));
    case '\0': return nullptr;
    default:
        throw std::logic_error("unexpected t.c value at token codegen");
        return nullptr;
    }
}

llvm::Value *ExprAST::codegen()
{
    for (auto &t : this->tok) {
        TokenCodegen(t);
    }
    return nullptr;
}

// One must remember that `Statement`s in brainfuck can only be loops
// So obviously this generates a loop in LLVM IR
llvm::Value *StmtAST::codegen()
{
    if (!this->children) return nullptr;
    llvm::BasicBlock *StartBB =
        llvm::BasicBlock::Create(*TheContext, "", MainFFunction);
    llvm::BasicBlock *BodyBB =
        llvm::BasicBlock::Create(*TheContext, "", MainFFunction);
    llvm::BasicBlock *MergeBB =
        llvm::BasicBlock::Create(*TheContext, "", MainFFunction);
    MainFunction->CreateBr(StartBB);
    MainFunction->SetInsertPoint(StartBB);

    MainFunction->CreateCondBr(
        MainFunction->CreateICmpNE(getCurPointerValue(), Builder->getInt8(0)),
        BodyBB, MergeBB);
    MainFunction->SetInsertPoint(BodyBB);
    for (auto &c : this->children.value()) {
        c->codegen();
    }
    MainFunction->CreateBr(StartBB);
    MainFunction->SetInsertPoint(MergeBB);
    return nullptr;
}

void InitModuleAndArray(const char *mname)
{
    TheContext = std::make_unique<llvm::LLVMContext>();
    TheModule = std::make_unique<llvm::Module>(mname, *TheContext);

    Builder = std::make_unique<llvm::IRBuilder<>>(*TheContext);
}

void InitMainFunction()
{
    llvm::FunctionType *FT =
        llvm::FunctionType::get(Builder->getInt32Ty(), false);
    MainFFunction = llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                                           "main", TheModule.get());
    MainFFunction->setDSOLocal(true);
    MainFFunction->addFnAttr(llvm::Attribute::NoFree);
    MainFFunction->addFnAttr(llvm::Attribute::NoUnwind);
    MainFFunction->addFnAttr(llvm::Attribute::StackProtectStrong);
    MainFFunction->addFnAttr(llvm::Attribute::UWTable);

    llvm::BasicBlock *BB =
        llvm::BasicBlock::Create(*TheContext, "", MainFFunction);
    Builder->SetInsertPoint(BB);
    MainFunction = std::make_unique<llvm::IRBuilder<>>(
        &MainFFunction->getEntryBlock(),
        MainFFunction->getEntryBlock().begin());
    MainTape = MainFunction->CreateAlloca(Builder->getInt8Ty(),
                                          Builder->getInt16(30000), "array");
    MainTape->setAlignment(llvm::Align::Constant<16>());
    MainPointer =
        MainFunction->CreateAlloca(Builder->getInt8PtrTy(), nullptr, "mnp");
    // MainPointer->setAlignment(llvm::Align::Constant<8>());
    MainFunction->CreateLifetimeStart(MainTape, Builder->getInt64(30000));
    MainFunction->CreateMemSet(MainTape, Builder->getInt8(0), 30000,
                               llvm::Align::Constant<16>());
    MainFunction->CreateStore(
        MainFunction->CreateGEP(
            llvm::ArrayType::get(Builder->getInt8Ty(), 30000), MainTape,
            {Builder->getInt64(0), Builder->getInt64(0)}, "", true),
        MainPointer);
}

void InitExternFunction()
{
    FPutChar = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getInt32Ty(*TheContext),
                                llvm::Type::getInt32Ty(*TheContext), false),
        llvm::Function::ExternalLinkage, "putchar", TheModule.get());
    FGetChar = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getInt32Ty(*TheContext), false),
        llvm::Function::ExternalLinkage, "getchar", TheModule.get());
}

void GenCleanup()
{
    llvm::BasicBlock *EndBB =
        llvm::BasicBlock::Create(*TheContext, "", MainFFunction);
    MainFunction->CreateBr(EndBB);
    MainFunction->SetInsertPoint(EndBB);
    MainFunction->CreateLifetimeEnd(MainTape, Builder->getInt64(30000));
    MainFunction->CreateRet(Builder->getInt32(EXIT_SUCCESS));
}

void StartCodeGen(const char *pname, std::unique_ptr<StmtAST> &&p)
{
    InitModuleAndArray(pname);
    InitExternFunction();
    InitMainFunction();
    for (auto &c : p->children.value())
        c->codegen();
    GenCleanup();
}

int main(const int argc, const char **argv)
{
    tokens = Optimize(Lex("/home/jaded/codes/Dust/hanoi.bf"));
    auto it = tokens.cbegin();
    auto p = ParseTopStatement(it);
    StartCodeGen("hanoi.bf", std::move(p));

    // Initialize the target registry etc.
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();

    auto TargetTriple = llvm::sys::getDefaultTargetTriple();
    TheModule->setTargetTriple(TargetTriple);

    // TODO: Make emitting llvm `.ll` codefile an option
    // TheModule->print(llvm::outs(), nullptr);

    std::string Error;
    auto Target = llvm::TargetRegistry::lookupTarget(TargetTriple, Error);

    // Print an error and exit if we couldn't find the requested target.
    // This generally occurs if we've forgotten to initialise the
    // TargetRegistry or we have a bogus target triple.
    if (!Target) {
        llvm::errs() << Error;
        return 1;
    }

    auto CPU = "generic";
    auto Features = "";

    llvm::TargetOptions opt;
    auto RM = std::optional<llvm::Reloc::Model>();
    auto TheTargetMachine =
        Target->createTargetMachine(TargetTriple, CPU, Features, opt, RM);

    TheModule->setDataLayout(TheTargetMachine->createDataLayout());

    auto Filename = "hanoi.o";
    std::error_code EC;
    llvm::raw_fd_ostream dest(Filename, EC, llvm::sys::fs::OF_None);

    if (EC) {
        llvm::errs() << "Could not open file: " << EC.message();
        return 1;
    }

    llvm::legacy::PassManager pass;
    auto FileType = llvm::CGFT_ObjectFile;

    if (TheTargetMachine->addPassesToEmitFile(pass, dest, nullptr, FileType)) {
        llvm::errs() << "TheTargetMachine can't emit a file of this type";
        return 1;
    }

    pass.run(*TheModule);
    dest.flush();

    llvm::outs() << "Wrote " << Filename << "\n";

    return 0;
}

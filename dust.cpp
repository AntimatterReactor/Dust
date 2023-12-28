#include <cstddef>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <stack>
#include <string>
#include <variant>
#include <vector>

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>

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
    while (nit < tokens.cend())
    {
        if (auto e = ParseExpression(nit); e != nullptr)
            r.push_back(std::move(e));
        if (nit->c == '[')
            r.push_back(std::move(ParseStatement(++nit)));
        if ((++nit)->c == ']')
            return std::make_unique<StmtAST>(std::move(r));
    }
    return std::make_unique<StmtAST>(std::move(r));
}

static std::unique_ptr<llvm::LLVMContext> TheContext;
static std::unique_ptr<llvm::Module> TheModule;
static std::unique_ptr<llvm::IRBuilder<>> Builder;

int main(const int argc, const char **argv) {}

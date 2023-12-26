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

class StmtAST {
    std::variant<std::unique_ptr<StmtAST>, std::vector<Token>> child;

    public:
    StmtAST() = delete;
    constexpr StmtAST(std::unique_ptr<StmtAST> &&ptr) : child(std::move(ptr)) {}
    constexpr StmtAST(std::vector<Token> &&vec) : child(std::move(vec)) {}

    ~StmtAST() = default;

    llvm::Value *codegen();
};

void Parser() {}

static std::unique_ptr<llvm::LLVMContext> TheContext;
static std::unique_ptr<llvm::Module> TheModule;
static std::unique_ptr<llvm::IRBuilder<>> Builder;

int main(const int argc, const char **argv) {}

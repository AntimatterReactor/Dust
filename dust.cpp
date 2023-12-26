#include <cstddef>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <stack>

struct Token {
    char c;
    int amount;
};

static std::vector<Token> tokens = {};

std::vector<Token> Lex(const char *fileName) // TODO: convert to filepath
{
    std::ifstream fp(fileName);
    char c = 0, p = 0;
    int amount = 0;
    std::vector<Token> r;
    do {
        fp.get(c);
        switch (c)
        {
        case '>':
        case '<':
        case '+':
        case '-':
        case '[':
        case ']':
        case '.':
        case ',': break;
        default: continue;
        }

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

};

void Parser ()
{

}

int main(const int argc, const char **argv)
{
    
}

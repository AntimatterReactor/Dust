#ifndef DS_UTILS_HPP_
#define DS_UTILS_HPP_

#include <fstream>

struct Token {
    char c;
    int amount;
};

Token getToken(std::istream &in); // TODO: implement
constexpr bool isvalidBF(const char ch)
{
    switch (ch) {
    case '>':
    case '<':
    case '+':
    case '-':
    case '[':
    case ']':
    case '.':
    case ',': return true;
    default: return false;
    }
}

#endif

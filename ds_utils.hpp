#ifndef DS_UTILS_HPP_
#define DS_UTILS_HPP_

#include <fstream>

struct Token {
    char c;
    int amount;
    friend bool operator==(const Token& lhs, const Token& rhs)
    {
        return lhs.c == rhs.c && lhs.amount == rhs.amount;
    }
    static Token None;
};

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

// DupliX 2 (design v5, 8.1): the lexer.
#include <cctype>

#include "dxg5/lang_ast.h"

namespace dx5 {

bool Lex(const std::string& s, std::vector<Token>* out, Diag* err)
{
    out->clear();
    int line = 1, col = 1, depth = 0;
    std::vector<int> braces;  // the ( depth outside each open {
    size_t i = 0;
    const size_t n = s.size();
    auto fail = [&](Pos p, const std::string& m) {
        if (err) *err = {p, m};
        return false;
    };
    auto adv = [&](size_t k) {
        for (size_t j = 0; j < k && i < n; j++, i++) {
            if (s[i] == '\n') line++, col = 1;
            else col++;
        }
    };
    auto push = [&](Token::Kind k, std::string t, Pos p) { out->push_back({k, std::move(t), p}); };
    while (i < n) {
        const char c = s[i];
        const Pos p{line, col};
        if (c == '\n') {
            if (depth == 0 && !out->empty() && out->back().kind != Token::Newline) push(Token::Newline, "\n", p);
            adv(1);
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\r') {
            adv(1);
            continue;
        }
        if (c == '/' && i + 1 < n && s[i + 1] == '/') {
            while (i < n && s[i] != '\n') adv(1);
            continue;
        }
        if (c == '/' && i + 1 < n && s[i + 1] == '*') {
            const size_t e = s.find("*/", i + 2);
            if (e == std::string::npos) return fail(p, "unclosed /* comment");
            adv(e + 2 - i);
            continue;
        }
        if (std::isdigit((unsigned char)c) || (c == '.' && i + 1 < n && std::isdigit((unsigned char)s[i + 1]))) {
            size_t j = i;
            if (c == '0' && j + 1 < n && (s[j + 1] == 'x' || s[j + 1] == 'X')) {
                j += 2;
                while (j < n && std::isxdigit((unsigned char)s[j])) j++;
            } else {
                while (j < n && (std::isdigit((unsigned char)s[j]) || s[j] == '.')) j++;
                if (j < n && (s[j] == 'e' || s[j] == 'E')) {
                    size_t k = j + 1;
                    if (k < n && (s[k] == '+' || s[k] == '-')) k++;
                    if (k < n && std::isdigit((unsigned char)s[k])) {
                        j = k;
                        while (j < n && std::isdigit((unsigned char)s[j])) j++;
                    }
                }
            }
            push(Token::Num, s.substr(i, j - i), p);
            adv(j - i);
            continue;
        }
        if (std::isalpha((unsigned char)c) || c == '_') {
            size_t j = i;
            while (j < n && (std::isalnum((unsigned char)s[j]) || s[j] == '_')) j++;
            push(Token::Ident, s.substr(i, j - i), p);
            adv(j - i);
            continue;
        }
        if (c == '"') {
            std::string t;
            size_t j = i + 1;
            for (;; j++) {
                if (j >= n || s[j] == '\n') return fail(p, "unclosed string");
                if (s[j] == '"') break;
                if (s[j] == '\\' && j + 1 < n && (s[j + 1] == '"' || s[j + 1] == '\\')) j++;
                t += s[j];
            }
            push(Token::Str, t, p);
            adv(j + 1 - i);
            continue;
        }
        static const char* ops[] = {"**", "==", "!=", "<=", ">=", "&&", "||", "+=", "-=", "*=", "/=", "=>", "+", "-", "*",
                                    "/",  "%",  "<",  ">",  "!",  "?",  ":",  "=",  "(",  ")",  "{",  "}",  ",", ".",  ";"};
        bool found = false;
        for (const char* o : ops) {
            const size_t len = std::char_traits<char>::length(o);
            if (s.compare(i, len, o) == 0) {
                push(Token::Op, o, p);
                if (o[0] == '(' && len == 1) depth++;
                if (o[0] == ')' && len == 1 && depth > 0) depth--;
                // a block's lines count again inside { } (expr(...) { ... } may sit in a call); an ( left open inside
                // it is dropped at its } (the formula's reading reports it on its own line)
                if (o[0] == '{' && len == 1) braces.push_back(depth), depth = 0;
                if (o[0] == '}' && len == 1) {
                    depth = braces.empty() ? 0 : braces.back();
                    if (!braces.empty()) braces.pop_back();
                }
                adv(len);
                found = true;
                break;
            }
        }
        if (!found) return fail(p, std::string("unexpected character '") + c + "'");
    }
    push(Token::End, "", {line, col});
    return true;
}

}  // namespace dx5

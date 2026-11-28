// DupliX 2 (design v5, 8.1): the parser (tokens -> syntax tree). Precedence, low to high:
//   ?:  ||  &&  == !=  < <= > >=  + -  * / %  unary - !  **  postfix (call, .name, .(expr))
#include "dxg5/lang_ast.h"

namespace dx5 {

namespace {

struct Parser {
    std::vector<Token> t;
    size_t i = 0;
    Diag err;
    bool failed = false;

    const Token& Peek(size_t k = 0) const { return t[std::min(i + k, t.size() - 1)]; }
    bool IsOp(const char* o, size_t k = 0) const { return Peek(k).kind == Token::Op && Peek(k).text == o; }
    bool IsWord(const char* w, size_t k = 0) const { return Peek(k).kind == Token::Ident && Peek(k).text == w; }
    bool Fail(const std::string& m)
    {
        if (!failed) {
            failed = true;
            err = {Peek().pos, m};
        }
        return false;
    }
    std::string What(const Token& k) const
    {
        switch (k.kind) {
        case Token::End: return "the end";
        case Token::Newline: return "the end of the line";
        default: return "'" + k.text + "'";
        }
    }
    bool Expect(const char* o)
    {
        if (IsOp(o)) {
            i++;
            return true;
        }
        return Fail(std::string("expected '") + o + "', found " + What(Peek()));
    }
    void SkipNewlines()
    {
        while (Peek().kind == Token::Newline || IsOp(";")) i++;
    }

    std::unique_ptr<Expr> Make(Expr::Kind k, Pos p, std::string text = "")
    {
        auto e = std::make_unique<Expr>();
        e->kind = k, e->pos = p, e->text = std::move(text);
        return e;
    }

    // ---- expressions ----
    std::unique_ptr<Expr> ParseE() { return Cond(); }

    std::unique_ptr<Expr> Cond()
    {
        auto c = Binary(0);
        if (!c) return nullptr;
        if (!IsOp("?")) return c;
        const Pos p = Peek().pos;
        i++;
        auto a = Cond();
        if (!a || !Expect(":")) return nullptr;
        auto b = Cond();
        if (!b) return nullptr;
        auto e = Make(Expr::Cond, p);
        e->args.push_back(std::move(c));
        e->args.push_back(std::move(a));
        e->args.push_back(std::move(b));
        return e;
    }

    static int Level(const std::string& op)
    {
        if (op == "||") return 1;
        if (op == "&&") return 2;
        if (op == "==" || op == "!=") return 3;
        if (op == "<" || op == "<=" || op == ">" || op == ">=") return 4;
        if (op == "+" || op == "-") return 5;
        if (op == "*" || op == "/" || op == "%") return 6;
        return 0;
    }

    std::unique_ptr<Expr> Binary(int minLevel)
    {
        auto a = UnaryE();
        if (!a) return nullptr;
        for (;;) {
            if (Peek().kind != Token::Op) break;
            const int lv = Level(Peek().text);
            if (!lv || lv <= minLevel) break;
            const Token op = Peek();
            i++;
            auto b = Binary(lv);
            if (!b) return nullptr;
            auto e = Make(Expr::Binary, op.pos, op.text);
            e->args.push_back(std::move(a));
            e->args.push_back(std::move(b));
            a = std::move(e);
        }
        return a;
    }

    std::unique_ptr<Expr> UnaryE()
    {
        if (IsOp("-") || IsOp("!")) {
            const Token op = Peek();
            i++;
            auto a = UnaryE();
            if (!a) return nullptr;
            auto e = Make(Expr::Unary, op.pos, op.text);
            e->args.push_back(std::move(a));
            return e;
        }
        return Power();
    }

    std::unique_ptr<Expr> Power()
    {
        auto a = Postfix();
        if (!a) return nullptr;
        if (!IsOp("**")) return a;
        const Pos p = Peek().pos;
        i++;
        auto b = UnaryE();  // right associative; -2 ** -1 reads the exponent's sign
        if (!b) return nullptr;
        auto e = Make(Expr::Binary, p, "**");
        e->args.push_back(std::move(a));
        e->args.push_back(std::move(b));
        return e;
    }

    std::unique_ptr<Expr> Postfix()
    {
        auto a = Primary();
        while (a && IsOp(".")) {
            const Pos p = Peek().pos;
            i++;
            if (IsOp("(")) {
                i++;
                auto x = ParseE();
                if (!x || !Expect(")")) return nullptr;
                auto e = Make(Expr::MemberExpr, p);
                e->args.push_back(std::move(a));
                e->args.push_back(std::move(x));
                a = std::move(e);
            } else if (Peek().kind == Token::Ident) {
                auto e = Make(Expr::Member, p, Peek().text);
                i++;
                e->args.push_back(std::move(a));
                a = std::move(e);
            } else {
                Fail("expected a name after '.', found " + What(Peek()));
                return nullptr;
            }
        }
        return a;
    }

    std::unique_ptr<Expr> Primary()
    {
        const Token k = Peek();
        if (k.kind == Token::Num) {
            i++;
            return Make(Expr::Num, k.pos, k.text);
        }
        if (k.kind == Token::Str) {
            i++;
            return Make(Expr::Str, k.pos, k.text);
        }
        if (k.kind == Token::Ident) {
            i++;
            if (k.text == "expr" && IsOp("{")) {
                auto e = Make(Expr::Call, k.pos, k.text);
                return FormulaBody(std::move(e));
            }
            if (!IsOp("(")) return Make(Expr::Name, k.pos, k.text);
            i++;
            auto e = Make(Expr::Call, k.pos, k.text);
            if (IsOp(")")) {
                i++;
                if (k.text == "expr" && IsOp("{")) return FormulaBody(std::move(e));
                return e;
            }
            for (;;) {
                if (Peek().kind == Token::Ident && IsOp(":", 1)) {
                    const std::string name = Peek().text;
                    i += 2;
                    auto v = ParseE();
                    if (!v) return nullptr;
                    e->named.emplace_back(name, std::move(v));
                } else {
                    if (!e->named.empty()) {
                        Fail("a positional argument after a named one");
                        return nullptr;
                    }
                    auto v = ParseE();
                    if (!v) return nullptr;
                    e->args.push_back(std::move(v));
                }
                if (IsOp(",")) {
                    i++;
                    continue;
                }
                if (!Expect(")")) return nullptr;
                if (k.text == "expr" && IsOp("{")) return FormulaBody(std::move(e));
                return e;
            }
        }
        if (IsOp("(")) {
            // (e) => body
            if (Peek(1).kind == Token::Ident && IsOp(")", 2) && IsOp("=>", 3)) {
                auto e = Make(Expr::Lambda, k.pos, Peek(1).text);
                i += 4;
                auto body = ParseE();
                if (!body) return nullptr;
                e->args.push_back(std::move(body));
                return e;
            }
            i++;
            auto e = ParseE();
            if (!e || !Expect(")")) return nullptr;
            return e;
        }
        Fail("expected a value, found " + What(k));
        return nullptr;
    }

    // expr(...) { lines }: the Expression node's formula, kept as text (design v5 8.3) in the named argument "{}".
    // The body's tokens are joined again (a newline or ; ends a line); its first token's position is kept.
    std::unique_ptr<Expr> FormulaBody(std::unique_ptr<Expr> e)
    {
        const Pos open = Peek().pos;
        i++;  // {
        std::string body, starts;
        int depth = 0;
        int line = -1;
        for (;;) {
            const Token& t = Peek();
            if (t.kind == Token::End) {
                Fail("expected '}' to close expr { ... }");
                return nullptr;
            }
            // a formula has no braces: the first } closes the block even inside an unclosed ( (the formula's own
            // reading then reports the missing ")" on its line)
            if (t.kind == Token::Op && t.text == "}") {
                i++;
                break;
            }
            if (t.kind == Token::Op && (t.text == "(" || t.text == "[")) depth++;
            if (t.kind == Token::Op && (t.text == ")" || t.text == "]") && depth > 0) depth--;
            if (t.kind == Token::Newline || (t.kind == Token::Op && t.text == ";")) {
                if (!body.empty() && body.back() != '\n') body += "\n";
            } else {
                if (line >= 0 && t.pos.line != line && depth == 0 && !body.empty() && body.back() != '\n') body += "\n";
                if (!body.empty() && body.back() != '\n') body += " ";
                // where each line of the body starts in the code (errors inside it are reported there)
                if (body.empty() || body.back() == '\n') starts += (starts.empty() ? "" : ",") + std::to_string(t.pos.line) + ":" + std::to_string(t.pos.col);
                body += t.kind == Token::Str ? "\"" + t.text + "\"" : t.text;
            }
            line = t.pos.line;
            i++;
        }
        auto s = Make(Expr::Str, open, body);
        e->named.emplace_back("{}", std::move(s));
        e->named.emplace_back("{}pos", Make(Expr::Str, open, starts));
        return e;
    }

    // ---- statements ----
    bool EndOfStatement()
    {
        if (Peek().kind == Token::Newline || IsOp(";")) {
            i++;
            return true;
        }
        if (Peek().kind == Token::End || IsOp("}")) return true;
        return Fail("expected the end of the line, found " + What(Peek()));
    }

    bool Block(std::vector<Stmt>* out)
    {
        if (!Expect("{")) return false;
        for (;;) {
            SkipNewlines();
            if (IsOp("}")) {
                i++;
                return true;
            }
            if (Peek().kind == Token::End) return Fail("expected '}', found the end");
            Stmt s;
            if (!Statement(&s)) return false;
            out->push_back(std::move(s));
        }
    }

    bool EachRest(Stmt* s)
    {
        // `each` has been read
        if (Peek().kind != Token::Ident) return Fail("expected the name of a geometry after 'each', found " + What(Peek()));
        s->kind = Stmt::Each;
        s->input = Peek().text;
        i++;
        return Block(&s->body);
    }

    bool Statement(Stmt* s)
    {
        s->pos = Peek().pos;
        if (IsWord("each")) {
            i++;
            return EachRest(s) && EndOfStatement();
        }
        if (IsWord("if")) {
            i++;
            s->kind = Stmt::If;
            s->value = ParseE();
            if (!s->value || !Block(&s->body)) return false;
            if (IsWord("else")) {
                i++;
                if (IsWord("if")) {
                    Stmt inner;
                    if (!Statement(&inner)) return false;
                    s->elseBody.push_back(std::move(inner));
                    return true;
                }
                if (!Block(&s->elseBody)) return false;
            }
            return EndOfStatement();
        }
        if (IsWord("group")) {
            i++;
            s->kind = Stmt::Group;
            if (Peek().kind != Token::Ident) return Fail("expected the group's name");
            s->names.push_back(Peek().text);
            i++;
            if (!Expect("(")) return false;
            while (!IsOp(")")) {
                Param p;
                p.pos = Peek().pos;
                if (Peek().kind != Token::Ident) return Fail("expected a parameter's name, found " + What(Peek()));
                p.name = Peek().text;
                i++;
                if (IsOp(":")) {
                    i++;
                    if (Peek().kind != Token::Ident) return Fail("expected a type after ':'");
                    p.type = Peek().text;
                    i++;
                    if (IsWord("structural")) {
                        p.structural = true;
                        i++;
                    }
                }
                if (IsOp("=")) {
                    i++;
                    p.dflt = ParseE();
                    if (!p.dflt) return false;
                }
                s->params.push_back(std::move(p));
                if (IsOp(",")) i++;
                else if (!IsOp(")")) return Fail("expected ',' or ')', found " + What(Peek()));
            }
            i++;
            return Block(&s->body) && EndOfStatement();
        }
        if (IsWord("return")) {
            i++;
            s->kind = Stmt::Return;
            if (IsOp("(") && Peek(1).kind == Token::Ident && IsOp(":", 2)) {
                // return (geometry: g, center: c): the group's outputs by name (design v5 12.1)
                auto e = Make(Expr::Call, Peek().pos, "()");
                i++;
                for (;;) {
                    if (Peek().kind != Token::Ident || !IsOp(":", 1)) return Fail("expected name: value in return ( )");
                    const std::string name = Peek().text;
                    i += 2;
                    auto v = ParseE();
                    if (!v) return false;
                    e->named.emplace_back(name, std::move(v));
                    if (IsOp(",")) {
                        i++;
                        continue;
                    }
                    if (!Expect(")")) return false;
                    break;
                }
                s->value = std::move(e);
                return EndOfStatement();
            }
            s->value = ParseE();
            return s->value && EndOfStatement();
        }
        // name = ..., a, b = ..., name += ...
        if (Peek().kind == Token::Ident && (IsOp("=", 1) || IsOp(",", 1) || IsOp("+=", 1) || IsOp("-=", 1) || IsOp("*=", 1) ||
                                            IsOp("/=", 1))) {
            s->names.push_back(Peek().text);
            i++;
            if (IsOp(",")) {
                i++;
                if (Peek().kind != Token::Ident || !IsOp("=", 1)) return Fail("expected 'name = ' after ','");
                s->names.push_back(Peek().text);
                i++;
            }
            const std::string op = Peek().text;
            i++;
            if (op != "=") {
                s->kind = Stmt::Compound;
                s->op = op;
            } else {
                s->kind = Stmt::Assign;
                if (IsWord("each") && s->names.size() == 1) {
                    i++;
                    return EachRest(s) && EndOfStatement();
                }
            }
            s->value = ParseE();
            return s->value && EndOfStatement();
        }
        s->kind = Stmt::ExprStmt;
        s->value = ParseE();
        return s->value && EndOfStatement();
    }
};

}  // namespace

bool ParseProgram(const std::string& src, Program* out, Diag* err)
{
    Parser p;
    if (!Lex(src, &p.t, err)) return false;
    Program prog;
    for (;;) {
        p.SkipNewlines();
        if (p.Peek().kind == Token::End) break;
        Stmt s;
        if (!p.Statement(&s)) {
            if (err) *err = p.err;
            return false;
        }
        prog.stmts.push_back(std::move(s));
    }
    *out = std::move(prog);
    return true;
}

bool ParseExpr(const std::string& src, std::unique_ptr<Expr>* out, Diag* err)
{
    Parser p;
    if (!Lex(src, &p.t, err)) return false;
    p.SkipNewlines();
    auto e = p.ParseE();
    if (e) {
        p.SkipNewlines();
        if (p.Peek().kind != Token::End) p.Fail("unexpected " + p.What(p.Peek()) + " after the expression");
    }
    if (!e || p.failed) {
        if (err) *err = p.err;
        return false;
    }
    *out = std::move(e);
    return true;
}

}  // namespace dx5

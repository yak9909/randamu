// DupliX 2 (design v5, 8): expressions as text, the math functions, constant folding.
#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <functional>
#include <set>

#include "dxg5/eval_internal.h"
#include "dxg5/lang.h"

namespace dx5 {

namespace {

int Level(const Expr& e)
{
    switch (e.kind) {
    case Expr::Cond: return 0;
    case Expr::Binary: {
        const std::string& o = e.text;
        if (o == "||") return 1;
        if (o == "&&") return 2;
        if (o == "==" || o == "!=") return 3;
        if (o == "<" || o == "<=" || o == ">" || o == ">=") return 4;
        if (o == "+" || o == "-") return 5;
        if (o == "**") return 8;
        return 6;
    }
    case Expr::Unary: return 7;
    case Expr::Lambda: return -1;
    default: return 9;
    }
}

std::string Wrap(const Expr& e, bool paren) { return paren ? "(" + PrintExpr(e) + ")" : PrintExpr(e); }

std::string Quote(const std::string& s)
{
    std::string o = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') o += '\\';
        o += c;
    }
    return o + "\"";
}

}  // namespace

std::string PrintExpr(const Expr& e)
{
    switch (e.kind) {
    case Expr::Num: return e.text;
    case Expr::Str: return Quote(e.text);
    case Expr::Name: return e.text;
    case Expr::Member: return Wrap(*e.args[0], Level(*e.args[0]) < 9) + "." + e.text;
    case Expr::MemberExpr: return Wrap(*e.args[0], Level(*e.args[0]) < 9) + ".(" + PrintExpr(*e.args[1]) + ")";
    case Expr::Unary: return e.text + Wrap(*e.args[0], Level(*e.args[0]) < 7);
    case Expr::Binary: {
        const int lv = Level(e);
        if (e.text == "**") return Wrap(*e.args[0], Level(*e.args[0]) < 9) + " ** " + Wrap(*e.args[1], Level(*e.args[1]) < 7);
        return Wrap(*e.args[0], Level(*e.args[0]) < lv) + " " + e.text + " " + Wrap(*e.args[1], Level(*e.args[1]) <= lv);
    }
    case Expr::Cond:
        return Wrap(*e.args[0], Level(*e.args[0]) <= 0) + " ? " + Wrap(*e.args[1], Level(*e.args[1]) < 0) + " : " +
               Wrap(*e.args[2], Level(*e.args[2]) < 0);
    case Expr::Lambda: return "(" + e.text + ") => " + PrintExpr(*e.args[0]);
    case Expr::Call: {
        std::string s = e.text + "(";
        bool first = true;
        for (const auto& a : e.args) s += (first ? "" : ", ") + PrintExpr(*a), first = false;
        for (const auto& a : e.named) s += (first ? "" : ", ") + a.first + ": " + PrintExpr(*a.second), first = false;
        return s + ")";
    }
    }
    return "";
}

bool IsMathFunction(const std::string& n)
{
    static const char* fns[] = {"sin",  "cos",   "tan", "asin",  "acos",   "atan2",     "sqrt",     "abs",     "floor",
                                "ceil", "round", "min", "max",   "clamp",  "mix",       "smoothstep", "len",   "normalize",
                                "dot",  "cross", "vec", "rot",   "inverse", "floordiv", "rot_mul",  "rot_apply", "sep_x",
                                "sep_y", "sep_z"};
    for (const char* f : fns)
        if (n == f) return true;
    return false;
}

std::string OpName(const std::string& s, bool unary)
{
    if (unary) return s == "-" ? "neg" : "not";
    static const struct {
        const char* sym;
        const char* op;
    } m[] = {{"+", "add"}, {"-", "sub"}, {"*", "mul"}, {"/", "div"}, {"%", "mod"}, {"**", "pow"}, {"==", "eq"}, {"!=", "ne"},
             {"<", "lt"},  {"<=", "le"}, {">", "gt"},  {">=", "ge"}, {"&&", "and"}, {"||", "or"}};
    for (const auto& x : m)
        if (s == x.sym) return x.op;
    return "";
}

Value NumberValue(const std::string& t)
{
    if (t.size() > 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) return Value::I((int64_t)std::strtoull(t.c_str() + 2, nullptr, 16));
    if (t.find_first_of(".eE") != std::string::npos) {
        double v = 0;
        std::from_chars(t.data(), t.data() + t.size(), v);
        return Value::F(v);
    }
    int64_t v = 0;
    std::from_chars(t.data(), t.data() + t.size(), v);
    return Value::I(v);
}

bool EvalConst(const Expr& e, Value* out)
{
    std::string err;
    switch (e.kind) {
    case Expr::Num: *out = NumberValue(e.text); return true;
    case Expr::Name:
        if (e.text == "true" || e.text == "false") {
            *out = Value::B(e.text == "true");
            return true;
        }
        return false;
    case Expr::Unary: {
        Value a;
        return EvalConst(*e.args[0], &a) && MathOp(OpName(e.text, true), &a, 1, out, &err);
    }
    case Expr::Binary: {
        Value a[2];
        return EvalConst(*e.args[0], &a[0]) && EvalConst(*e.args[1], &a[1]) && MathOp(OpName(e.text, false), a, 2, out, &err);
    }
    case Expr::Cond: {
        Value a[3];
        for (int k = 0; k < 3; k++)
            if (!EvalConst(*e.args[k], &a[k])) return false;
        return MathOp("switch", a, 3, out, &err);
    }
    case Expr::Member: {
        Value a;
        if (!EvalConst(*e.args[0], &a) || a.type != Type::Vector) return false;
        const std::string& m = e.text;
        if (m != "x" && m != "y" && m != "z") return false;
        *out = Value::F(m == "x" ? a.v.x : m == "y" ? a.v.y : a.v.z);
        return true;
    }
    case Expr::Call: {
        if (!IsMathFunction(e.text) || !e.named.empty() || e.args.size() > 3) return false;
        Value a[3];
        for (size_t k = 0; k < e.args.size(); k++)
            if (!EvalConst(*e.args[k], &a[k])) return false;
        return MathOp(e.text, a, (int)e.args.size(), out, &err);
    }
    default: return false;
    }
}

// ---- the Expression node's formula ----
namespace {

bool IdentStart(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
bool IdentChar(char c) { return IdentStart(c) || (c >= '0' && c <= '9'); }

bool IsMacroWord(const std::string& s)
{
    if (s.size() < 2 || (s[0] != 'm' && s[0] != 'i') || s[1] == '0') return false;
    int v = 0;
    for (size_t j = 1; j < s.size(); j++) {
        if (s[j] < '0' || s[j] > '9' || v > 100) return false;
        v = v * 10 + (s[j] - '0');
    }
    return v >= 1 && v <= 32;
}

}  // namespace

bool IsFormulaBuiltinName(const std::string& s)
{
    if (s == "true" || s == "false" || s == "index" || s == "id" || s == "count" || s == "time" || s == "seed") return true;
    if (FindBuiltin(s)) return true;
    return IsMacroWord(s);
}

bool ReservedName(const std::string& n)
{
    return FindDef(n) || IsMathFunction(n) || IsFormulaBuiltinName(n) || n == "attr" || n == "random" || n == "parent" ||
           n == "expr" || n == "noise" || n == "group" || n == "return" || n == "each" || n == "if" || n == "else";
}

bool ParseFormula(const std::string& text, std::vector<FormulaLine>* lines, Diag* err)
{
    lines->clear();
    struct Seg {
        size_t begin, end;
        int line, col;
    };
    std::vector<Seg> segs;
    int depth = 0, line = 1, col = 1;
    bool str = false;
    size_t start = 0;
    int sLine = 1, sCol = 1;
    for (size_t k = 0; k <= text.size(); k++) {
        const char c = k < text.size() ? text[k] : '\n';
        const bool end = k == text.size();
        if (str) {
            if (c == '"') str = false;
        } else if (c == '"') {
            str = true;
        } else if (c == '(' || c == '[') {
            depth++;
        } else if (c == ')' || c == ']') {
            depth--;
        }
        if (end || (!str && depth <= 0 && (c == '\n' || c == ';'))) {
            segs.push_back({start, k, sLine, sCol});
            start = k + 1;
            if (c == '\n') sLine = line + 1, sCol = 1;
            else sLine = line, sCol = col + 1;
        }
        if (c == '\n') line++, col = 1;
        else col++;
    }
    auto fail = [&](int l, int c, const std::string& m) {
        if (err) err->pos = {l, c}, err->msg = m;
        lines->clear();
        return false;
    };
    for (const Seg& s : segs) {
        std::string t = text.substr(s.begin, s.end - s.begin);
        if (!t.empty() && t.back() == '\r') t.pop_back();
        size_t p = 0;
        while (p < t.size() && (t[p] == ' ' || t[p] == '\t')) p++;
        if (p == t.size()) continue;  // a blank line
        FormulaLine fl;
        size_t q = p;
        if (IdentStart(t[q])) {
            while (q < t.size() && IdentChar(t[q])) q++;
            size_t r = q;
            while (r < t.size() && (t[r] == ' ' || t[r] == '\t')) r++;
            if (r < t.size() && t[r] == '=' && (r + 1 >= t.size() || t[r + 1] != '=')) {
                fl.name = t.substr(p, q - p);
                p = r + 1;
            }
        }
        std::unique_ptr<Expr> e;
        Diag d;
        if (!ParseExpr(t.substr(p), &e, &d))
            return fail(s.line + d.pos.line - 1, d.pos.line == 1 ? s.col + (int)p + d.pos.col - 1 : d.pos.col, d.msg);
        fl.expr = std::shared_ptr<const Expr>(std::move(e));
        for (const FormulaLine& o : *lines)
            if (!fl.name.empty() && o.name == fl.name) return fail(s.line, s.col, "the name " + fl.name + " is given twice");
        lines->push_back(std::move(fl));
        if (lines->size() > 1)
            for (const FormulaLine& o : *lines)
                if (o.name.empty()) return fail(s.line, s.col, "with several lines, each line needs a name: x = ...");
    }
    if (lines->empty()) return fail(1, 1, "an empty formula");
    if (lines->size() == 1 && lines->front().name.empty()) lines->front().name = "value";
    return true;
}

FormulaSockets ReadFormula(const std::string& text)
{
    FormulaSockets r;
    std::vector<FormulaLine> lines;
    if (!ParseFormula(text, &lines, &r.error)) return r;
    std::set<std::string> earlier;
    std::function<bool(const Expr&)> walk = [&](const Expr& x) -> bool {
        switch (x.kind) {
        case Expr::Name:
            if (!IsFormulaBuiltinName(x.text) && !earlier.count(x.text) &&
                std::find(r.inputs.begin(), r.inputs.end(), x.text) == r.inputs.end())
                r.inputs.push_back(x.text);
            return true;
        case Expr::Member:
            if (x.args[0]->kind == Expr::Name && x.args[0]->text == "parent") return true;
            return walk(*x.args[0]);
        case Expr::Call:
            if (x.text == "attr") {
                if (x.args.size() != 1 || x.args[0]->kind != Expr::Str || !x.named.empty()) {
                    r.error = {x.pos, "attr() takes the attribute's name in quotes: attr(\"ix\")"};
                    return false;
                }
                return true;
            }
            if (x.text == "random") {
                for (const auto& a : x.named)
                    if (a.first != "salt" || a.second->kind != Expr::Num) {
                        r.error = {x.pos, "random() takes only salt: a number"};
                        return false;
                    }
                if (!x.args.empty()) {
                    r.error = {x.pos, "random() takes only salt: a number"};
                    return false;
                }
                return true;
            }
            if (!IsMathFunction(x.text) || !x.named.empty()) {
                r.error = {x.pos, x.text + "() cannot be in a formula (use a node and wire it in)"};
                return false;
            }
            break;
        case Expr::Str:
        case Expr::Lambda:
        case Expr::MemberExpr:
            r.error = {x.pos, "not a formula"};
            return false;
        default: break;
        }
        for (const auto& a : x.args)
            if (!walk(*a)) return false;
        return true;
    };
    for (const FormulaLine& l : lines) {
        if (!walk(*l.expr)) {
            r.inputs.clear();
            return r;
        }
        earlier.insert(l.name);
        if (l.name[0] != '_') r.outputs.push_back(l.name);
    }
    r.ok = true;
    return r;
}

}  // namespace dx5

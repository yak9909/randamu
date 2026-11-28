// DupliX 2 (design v5, 7): the graph's stored form (version 2) and the conversion of version 1 graphs.
#include "dxg5/graph.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <set>

#include "dxg5/lang.h"
#include "graph/graph.h"

namespace dx5 {

namespace {

bool ParseNum(const std::string& s, double* v)
{
    auto r = std::from_chars(s.data(), s.data() + s.size(), *v);
    return r.ec == std::errc() && r.ptr == s.data() + s.size() && std::isfinite(*v);
}
template <class T>
bool ParseInt(const std::string& s, T* v, int base = 10)
{
    auto r = std::from_chars(s.data(), s.data() + s.size(), *v, base);
    return r.ec == std::errc() && r.ptr == s.data() + s.size();
}

std::vector<std::string> Split(const std::string& line, size_t restAfter = 0, std::string* rest = nullptr)
{
    std::vector<std::string> t;
    size_t i = 0;
    const size_t n = line.size();
    while (i < n) {
        while (i < n && line[i] == ' ') i++;
        if (i >= n) break;
        if (rest && restAfter && t.size() == restAfter) {
            *rest = line.substr(i);
            return t;
        }
        size_t j = i;
        while (j < n && line[j] != ' ') j++;
        t.push_back(line.substr(i, j - i));
        i = j;
    }
    if (rest) rest->clear();
    return t;
}

// A formula saved by P1.5 read a named attribute by its bare name (ix); now a bare unknown name is an input
// (design v5 8.3, 16 4), so on reading, a name the formula uses that has no input slot becomes attr("ix").
void MigrateFormula(Node& n)
{
    const FormulaSockets fs = ReadFormula(n.text);
    std::set<std::string> bare;
    for (const std::string& name : fs.inputs)
        if (!n.In(name)) bare.insert(name);
    if (bare.empty()) return;
    std::vector<FormulaLine> lines;
    Diag d;
    if (!ParseFormula(n.text, &lines, &d)) return;
    std::string text;
    for (const FormulaLine& l : lines) {
        std::unique_ptr<Expr> e;
        if (!ParseExpr(PrintExpr(*l.expr), &e, &d)) return;
        std::function<void(Expr&)> walk = [&](Expr& x) {
            if (x.kind == Expr::Name && bare.count(x.text)) {
                auto s = std::make_unique<Expr>();
                s->kind = Expr::Str, s->text = x.text;
                x.kind = Expr::Call, x.text = "attr";
                x.args.clear();
                x.args.push_back(std::move(s));
                return;
            }
            if (x.kind == Expr::Member && x.args[0]->kind == Expr::Name && x.args[0]->text == "parent") return;
            for (auto& a : x.args) walk(*a);
            for (auto& a : x.named) walk(*a.second);
        };
        walk(*e);
        if (!text.empty()) text += "\n";
        text += lines.size() == 1 && l.name == "value" ? PrintExpr(*e) : l.name + " = " + PrintExpr(*e);
    }
    n.text = text;
}

// a node's text on one line: a formula of several lines (design v5 8.3) keeps its line breaks as \n
std::string EscapeLine(const std::string& s)
{
    std::string o;
    for (char c : s) {
        if (c == '\\') o += "\\\\";
        else if (c == '\n') o += "\\n";
        else if (c != '\r') o += c;
    }
    return o;
}
std::string UnescapeLine(const std::string& s)
{
    std::string o;
    for (size_t k = 0; k < s.size(); k++) {
        if (s[k] == '\\' && k + 1 < s.size() && (s[k + 1] == 'n' || s[k + 1] == '\\')) {
            o += s[k + 1] == 'n' ? '\n' : '\\';
            k++;
        } else {
            o += s[k];
        }
    }
    return o;
}

std::string ValueText(const Value& v)
{
    switch (v.type) {
    case Type::Float: return "f:" + Num(v.f);
    case Type::Int: return "i:" + std::to_string(v.i);
    case Type::Bool: return std::string("b:") + (v.i ? "1" : "0");
    case Type::Vector: return "v:" + Num(v.v.x) + "," + Num(v.v.y) + "," + Num(v.v.z);
    default: return "";
    }
}

bool ParseValue(const std::string& s, Value* v)
{
    if (s.size() < 2 || s[1] != ':') return false;
    const std::string b = s.substr(2);
    switch (s[0]) {
    case 'f': {
        double x = 0;
        if (!ParseNum(b, &x)) return false;
        *v = Value::F(x);
        return true;
    }
    case 'i': {
        int64_t x = 0;
        if (!ParseInt(b, &x)) return false;
        *v = Value::I(x);
        return true;
    }
    case 'b':
        if (b != "0" && b != "1") return false;
        *v = Value::B(b == "1");
        return true;
    case 'v': {
        const size_t c1 = b.find(','), c2 = c1 == std::string::npos ? c1 : b.find(',', c1 + 1);
        Vec3 x;
        if (c2 == std::string::npos || !ParseNum(b.substr(0, c1), &x.x) || !ParseNum(b.substr(c1 + 1, c2 - c1 - 1), &x.y) ||
            !ParseNum(b.substr(c2 + 1), &x.z))
            return false;
        *v = Value::V(x);
        return true;
    }
    default: return false;
    }
}

void FillInputs(Node& n, const Graph& g)
{
    n.inputs.clear();
    const NodeDef* d = DefFor(g, n.type);
    if (!d) return;
    for (const InputDef& i : d->inputs) {
        InputSlot s;
        s.name = i.name;
        s.value = i.dflt;
        n.inputs.push_back(s);
    }
}

// ---- version 1 ----

struct KeyMap {
    const char* v1;
    const char* v5;
};

const char* MapKey(const std::string& v1Type, const std::string& key)
{
    static const struct {
        const char* type;
        KeyMap keys[10];
    } maps[] = {
        {"Grid", {{"countX", "cx"}, {"countY", "cy"}, {"countZ", "cz"}, {"spacingX", "sx"}, {"spacingY", "sy"}, {"spacingZ", "sz"}}},
        {"Circle", {{"count", "count"}, {"radius", "radius"}, {"startAngle", "start_angle"}, {"sweep", "sweep"}, {"align", "align"}}},
        {"Line", {{"count", "count"}, {"length", "length"}, {"angle", "angle"}}},
        {"Transform", {{"rotX", "rotation_x"}, {"rotY", "rotation_y"}, {"rotZ", "rotation_z"}, {"scale", "scale"}, {"pivot", "pivot"}}},
        {"Modify", {{"rotX", "rotation_x"}, {"rotY", "rotation_y"}, {"rotZ", "rotation_z"}, {"scale", "scale"}, {"opacity", "opacity"}}},
        {"Source", {{"first", "first"}, {"count", "layers"}, {"assign", "assign"}}},
        {"Output", {{"copyScale", "copy_scale"}}},
    };
    for (const auto& m : maps) {
        if (v1Type != m.type) continue;
        for (const KeyMap& k : m.keys)
            if (k.v1 && key == k.v1) return k.v5;
    }
    return nullptr;
}

std::string LowerName(const std::string& s)
{
    std::string o = s;
    for (char& c : o)
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    return o;
}

// base + depth x macro as a formula ("10 + 2*m2", "-0.5*m2 + m3", "m1")
std::string FormulaOf(const dxg::Input& in)
{
    std::vector<std::string> terms;
    std::vector<std::string> links;
    for (const dxg::Link& l : in.links) {
        const int k = dxg::MacroIndex(l.src);
        if (k < 0) continue;  // a source this build does not read (it read 0)
        const std::string m = "m" + std::to_string(k + 1);
        if (l.depth == 1) links.push_back(m);
        else if (l.depth == -1) links.push_back("-" + m);
        else links.push_back(Num(l.depth) + "*" + m);
    }
    if (in.base != 0 || links.empty()) terms.push_back(Num(in.base));
    for (auto& l : links) terms.push_back(l);
    std::string s;
    for (size_t i = 0; i < terms.size(); i++) {
        const std::string& t = terms[i];
        if (i == 0) s = t;
        else if (!t.empty() && t[0] == '-') s += " - " + t.substr(1);
        else s += " + " + t;
    }
    return s;
}

bool PlainMacro(const dxg::Input& in, int* k)
{
    if (in.base != 0 || in.links.size() != 1 || in.links[0].depth != 1) return false;
    *k = dxg::MacroIndex(in.links[0].src);
    return *k >= 0;
}

void FromV1(const dxg::Graph& v1, Graph* out)
{
    Graph g;
    g.revision = v1.revision;
    for (int k = 0; k < dxg::kNumMacros && k < kNumMacros; k++)
        g.macros[k] = {v1.macros[k].name, v1.macros[k].sliderMin, v1.macros[k].sliderMax};
    uint32_t maxId = 0;
    for (const dxg::Node& n : v1.nodes) maxId = std::max(maxId, n.id);
    g.nextId = std::max(v1.nextId, maxId + 1);
    // the nodes first (their ids stay: the wires and the element ids refer to them)
    for (const dxg::Node& n : v1.nodes) {
        Node m;
        m.id = n.id;
        m.legacyId = n.id;
        m.type = LowerName(n.typeName);
        if (m.type == "source") m.type = "source_layers";  // `source` reads the element's attribute
        m.key = m.type + std::to_string(n.id);
        m.x = n.x, m.y = n.y;
        FillInputs(m, g);
        g.nodes.push_back(m);
    }
    auto macroNode = [&](int k, const Node& near) {
        const std::string key = "m" + std::to_string(k + 1);
        if (const Node* m = g.FindKey(key)) return m->id;
        Node m;
        m.id = g.nextId++;
        m.type = "macro";
        m.key = key;
        m.setting = k + 1;
        m.x = near.x - 160, m.y = near.y + 40 * k;
        g.nodes.push_back(m);
        return m.id;
    };
    auto exprNode = [&](const std::string& key, const std::string& text, const Node& near) {
        Node e;
        e.id = g.nextId++;
        e.type = "expr";
        e.key = UniqueKey(g, key);
        e.text = text;
        e.x = near.x - 200, e.y = near.y;
        g.nodes.push_back(e);
        return e.id;
    };
    for (const dxg::Node& n : v1.nodes) {
        // positions of the vector are read before the node may move in the vector (push_back)
        const std::string type = n.typeName;
        const dxg::Param* pos[3] = {n.Find("posX"), n.Find("posY"), n.Find("posZ")};
        for (const dxg::Param& p : n.params) {
            const char* name = MapKey(type, p.key);
            if (!name) continue;
            Node near = *g.Find(n.id);
            const NodeDef* d = FindDef(near.type);
            const int idx = d ? d->Input(name) : -1;
            if (idx < 0) continue;
            const Type t = d->inputs[idx].type;
            int k = -1;
            Link link;
            Value v;
            if (PlainMacro(p.in, &k)) {
                link = {macroNode(k, near), 0};
            } else if (std::any_of(p.in.links.begin(), p.in.links.end(), [](const dxg::Link& l) { return dxg::MacroIndex(l.src) >= 0; })) {
                link = {exprNode(near.key + "." + name, FormulaOf(p.in), near), 0};
            } else if (!Convert(Value::F(p.in.base), t, &v)) {
                continue;
            }
            Node* m = g.Find(n.id);
            InputSlot* s = m->In(name);
            if (link.from) s->link = link;
            else s->value = v;
        }
        if (pos[0] || pos[1] || pos[2]) {
            Node near = *g.Find(n.id);
            if (!near.In("position")) continue;
            bool linked = false;
            Vec3 c;
            double* comp[3] = {&c.x, &c.y, &c.z};
            std::string parts[3];
            for (int a = 0; a < 3; a++) {
                const dxg::Input in = pos[a] ? pos[a]->in : dxg::Input{};
                *comp[a] = in.base;
                parts[a] = FormulaOf(in);
                for (const dxg::Link& l : in.links) linked |= dxg::MacroIndex(l.src) >= 0;
            }
            if (linked) {
                const uint32_t e = exprNode(near.key + ".position", "vec(" + parts[0] + ", " + parts[1] + ", " + parts[2] + ")", near);
                g.Find(n.id)->In("position")->link = {e, 0};
            } else {
                g.Find(n.id)->In("position")->value = Value::V(c);
            }
        }
    }
    for (const dxg::Wire& w : v1.wires) {
        Node* to = g.Find(w.to);
        if (!to || !g.Find(w.from)) continue;
        if (InputSlot* s = to->In("geometry")) s->link = {w.from, 0};
    }
    *out = g;
}

}  // namespace

InputSlot* Node::In(const std::string& name)
{
    for (InputSlot& s : inputs)
        if (s.name == name) return &s;
    return nullptr;
}
const InputSlot* Node::In(const std::string& name) const
{
    for (const InputSlot& s : inputs)
        if (s.name == name) return &s;
    return nullptr;
}

Node* Graph::Find(uint32_t id)
{
    for (Node& n : nodes)
        if (n.id == id) return &n;
    return nullptr;
}
const Node* Graph::Find(uint32_t id) const
{
    for (const Node& n : nodes)
        if (n.id == id) return &n;
    return nullptr;
}
const Node* Graph::FindKey(const std::string& key) const
{
    for (const Node& n : nodes)
        if (n.key == key) return &n;
    return nullptr;
}
Node* Graph::FindKey(const std::string& key)
{
    for (Node& n : nodes)
        if (n.key == key) return &n;
    return nullptr;
}
const Node* Graph::OutputNode() const
{
    for (const Node& n : nodes)
        if (n.type == "output") return &n;
    return nullptr;
}

std::string UniqueKey(const Graph& g, const std::string& want)
{
    if (!g.FindKey(want)) return want;
    for (int k = 2;; k++) {
        const std::string s = want + "_" + std::to_string(k);
        if (!g.FindKey(s)) return s;
    }
}

uint32_t AddNode(Graph& g, const std::string& type, const std::string& key, double x, double y)
{
    Node n;
    n.id = g.nextId++;
    n.type = type;
    if (key.empty()) {
        for (int k = 1;; k++) {
            const std::string s = type + std::to_string(k);
            if (!g.FindKey(s)) {
                n.key = s;
                break;
            }
        }
    } else {
        n.key = UniqueKey(g, key);
    }
    n.x = x, n.y = y;
    NormalizeMath(n);
    if (IsMathFamily(n.type) && n.text.empty()) n.text = DefaultMathOp(n.type);
    FillInputs(n, g);
    g.nodes.push_back(n);
    return n.id;
}

std::vector<OutputDef> OutputsOf(const Graph& g, const Node& n)
{
    if (n.type == "expr") {
        const FormulaSockets s = ReadFormula(n.text);
        std::vector<OutputDef> o;
        for (const std::string& name : s.outputs) o.push_back({name, Type::Any, true});
        if (o.empty()) o.push_back({"value", Type::Any, true});
        return o;
    }
    if (const MathOpDef* m = MathOpOf(n)) return {{"value", m->out, true}};
    const NodeDef* d = DefFor(g, n.type);
    return d ? d->outputs : std::vector<OutputDef>{};
}

const UserGroup* FindUserGroup(const Graph& g, const std::string& name)
{
    for (const UserGroup& u : g.groups)
        if (u.name == name) return &u;
    return nullptr;
}

const NodeDef* DefFor(const Graph& g, const std::string& type)
{
    if (const NodeDef* d = FindDef(type)) return d;
    const UserGroup* u = FindUserGroup(g, type);
    return u ? &u->def : nullptr;
}

const MathOpDef* MathOpOf(const Node& n) { return IsMathFamily(n.type) ? FindMathOp(n.type, n.text) : nullptr; }

Type OutputType(const Graph& g, const Node& n, int output)
{
    if (const MathOpDef* m = MathOpOf(n)) return m->out;
    const NodeDef* d = DefFor(g, n.type);
    return d && output >= 0 && output < (int)d->outputs.size() ? d->outputs[output].type : Type::Any;
}

bool InputVisible(const Graph&, const Node& n, int k)
{
    if (!IsMathFamily(n.type)) return true;
    const MathOpDef* m = MathOpOf(n);
    return m && k < m->arity;
}

Type MathInputType(const Graph&, const Node& n, int k)
{
    const MathOpDef* m = MathOpOf(n);
    return m && k >= 0 && k < m->arity ? m->inTypes[k] : Type::Any;
}

bool TypeFits(Type ot, Type it)
{
    if (it == Type::Any || ot == Type::Any || it == ot) return true;
    const bool scalar = ot == Type::Float || ot == Type::Int || ot == Type::Bool;
    if (it == Type::Float || it == Type::Int || it == Type::Bool) return scalar;
    if (it == Type::Vector) return ot == Type::Float || ot == Type::Int;
    return false;
}

bool LinkActive(const Graph& g, const Node& n, int k)
{
    if (k < 0 || k >= (int)n.inputs.size()) return false;
    const InputSlot& s = n.inputs[k];
    if (!s.link.from) return true;
    if (!IsMathFamily(n.type)) return true;
    const MathOpDef* m = MathOpOf(n);
    if (!m || k >= m->arity) return false;
    const Node* f = g.Find(s.link.from);
    if (!f) return false;
    const Type it = m->inTypes[k], ot = OutputType(g, *f, s.link.output);
    if (it == Type::Any || ot == Type::Any || it == ot) return true;
    const bool scalar = ot == Type::Float || ot == Type::Int || ot == Type::Bool;
    if (it == Type::Float || it == Type::Int || it == Type::Bool) return scalar;
    if (it == Type::Vector) return ot == Type::Float || ot == Type::Int;
    return false;
}

bool NormalizeMath(Node& n)
{
    if (IsMathFamily(n.type)) return true;
    const MathOpDef* m = FindMathOpByCodeName(n.type);
    if (!m) return false;
    n.type = m->family;
    n.text = m->op;
    return true;
}

std::string MacroLabel(const Graph& g, bool isInt, int k)
{
    const int n = isInt ? kNumIntMacros : kNumMacros;
    if (k < 0 || k >= n) return "";
    const MacroSetting& m = isInt ? g.intMacros[k] : g.macros[k];
    return m.name.empty() ? std::string(isInt ? "Int Macro " : "Macro ") + std::to_string(k + 1) : m.name;
}

std::string TypeWord(Type t)
{
    switch (t) {
    case Type::Geometry: return "geometry";
    case Type::Int: return "int";
    case Type::Bool: return "bool";
    case Type::Vector: return "vector";
    case Type::Rotation: return "rotation";
    case Type::Float: return "float";
    default: return "any";
    }
}
Type WordType(const std::string& w)
{
    if (w == "geometry") return Type::Geometry;
    if (w == "int") return Type::Int;
    if (w == "bool") return Type::Bool;
    if (w == "vector") return Type::Vector;
    if (w == "rotation") return Type::Rotation;
    if (w == "float") return Type::Float;
    return Type::Any;
}

std::string SerializeBody(const Graph& g, bool top);
bool ParseBody(const std::string& text, Graph* g);

std::string Serialize(const Graph& g) { return SerializeBody(g, true); }

std::string SerializeBody(const Graph& g, bool top)
{
    std::string s;
    if (top) {
        s = "DupliX graph " + std::to_string(kFormatVersion) + "\n";
        s += "revision " + std::to_string(g.revision) + "\n";
    }
    s += "nextid " + std::to_string(g.nextId) + "\n";
    if (top)
        for (const UserGroup& u : g.groups) {
            s += "group " + u.name + " 0\n";
            for (const InputDef& i : u.def.inputs)
                s += "gparam " + i.name + " " + TypeWord(i.type) + " " + (i.structural ? "1" : "0") + " " + (i.field ? "1" : "0") + " " +
                     (ValueText(i.dflt).empty() ? "-" : ValueText(i.dflt)) + "\n";
            for (const OutputDef& o : u.def.outputs) s += "gout " + o.name + " " + TypeWord(o.type) + " " + (o.field ? "1" : "0") + "\n";
            if (u.body) s += SerializeBody(*u.body, false);
            s += "end\n";
        }
    for (int pass = 0; pass < 2; pass++) {
        const MacroSetting* ms = pass ? g.intMacros : g.macros;
        for (int k = 0; k < 32; k++) {
            const MacroSetting& m = ms[k];
            if (m.name.empty() && m.sliderMin == 0 && m.sliderMax == 100) continue;
            s += std::string(pass ? "imacro " : "macro ") + std::to_string(k + 1) + " " + Num(m.sliderMin) + " " + Num(m.sliderMax);
            if (!m.name.empty()) s += " " + m.name;
            s += "\n";
        }
    }
    for (const Node& n : g.nodes) {
        s += "node " + std::to_string(n.id) + " " + n.type + " " + Num(n.x) + " " + Num(n.y) + " " + n.key;
        if (n.legacyId) s += " legacy=" + std::to_string(n.legacyId);
        if (n.zone) s += " zone=" + std::to_string(n.zone);
        if (n.salt) {
            char b[32];
            std::snprintf(b, sizeof(b), "%llx", (unsigned long long)n.salt);
            s += std::string(" salt=") + b;
        }
        if (n.setting) s += " setting=" + std::to_string(n.setting);
        const bool math = IsMathFamily(n.type);
        if (math) s += " op=" + n.text;
        s += "\n";
        for (const InputSlot& in : n.inputs) {
            if (in.link.from) {
                s += "in " + std::to_string(n.id) + " " + in.name + " link " + std::to_string(in.link.from) + " " +
                     std::to_string(in.link.output) + "\n";
            } else {
                const std::string v = ValueText(in.value);
                if (!v.empty()) s += "in " + std::to_string(n.id) + " " + in.name + " const " + v + "\n";
            }
        }
        if (!n.text.empty() && !math) s += "text " + std::to_string(n.id) + " " + EscapeLine(n.text) + "\n";
    }
    for (const std::string& l : g.unknownLines) s += l + "\n";
    return s;
}

NodeDef UserGroupDef(const std::string& name, std::vector<InputDef> in, std::vector<OutputDef> out)
{
    NodeDef d;
    d.name = name, d.label = name, d.category = "Group", d.group = true;
    d.inputs = std::move(in), d.outputs = std::move(out);
    return d;
}

bool Parse(const std::string& text, Graph* out, std::string* error)
{
    // the header decides the version
    {
        const size_t e = text.find('\n');
        std::string first = text.substr(0, e);
        if (!first.empty() && first.back() == '\r') first.pop_back();
        const auto t = Split(first);
        int ver = 0;
        if (t.size() < 3 || t[0] != "DupliX" || t[1] != "graph" || !ParseInt(t[2], &ver) || ver < 1) {
            if (error) *error = "not a DupliX graph";
            return false;
        }
        if (ver == 1) {
            dxg::Graph v1;
            if (!dxg::Parse(text, &v1, error)) return false;
            FromV1(v1, out);
            return true;
        }
    }
    Graph g;
    if (!ParseBody(text, &g)) return false;
    *out = g;
    return true;
}

// version 2's lines into g (which may already hold the groups a body may use)
bool ParseBody(const std::string& text, Graph* gp)
{
    Graph& g = *gp;
    size_t pos = 0;
    bool header = false;
    while (pos <= text.size()) {
        size_t e = text.find('\n', pos);
        if (e == std::string::npos) e = text.size();
        std::string line = text.substr(pos, e - pos);
        pos = e + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) {
            if (e >= text.size()) break;
            continue;
        }
        if (!header) {
            header = true;
            continue;
        }
        std::string rest;
        auto t = Split(line);
        const std::string w = t[0];  // a copy: t is split again below
        bool known = true;
        if (w == "group" && t.size() >= 3) {
            // a user's group: its table entry, then its body up to `end`
            UserGroup u;
            u.name = t[1];
            std::vector<InputDef> ins;
            std::vector<OutputDef> outs;
            std::string bodyText = "DupliX graph 2\n";
            while (pos <= text.size()) {
                size_t e2 = text.find('\n', pos);
                if (e2 == std::string::npos) e2 = text.size();
                std::string l = text.substr(pos, e2 - pos);
                pos = e2 + 1;
                if (!l.empty() && l.back() == '\r') l.pop_back();
                const auto lt = Split(l);
                if (!lt.empty() && lt[0] == "end") break;
                if (lt.size() >= 6 && lt[0] == "gparam") {
                    InputDef i;
                    i.name = lt[1], i.type = WordType(lt[2]), i.structural = lt[3] == "1", i.field = lt[4] == "1";
                    i.softMin = 0, i.softMax = 100;
                    if (lt[5] != "-") ParseValue(lt[5], &i.dflt);
                    if (i.type == Type::Geometry) i.context = "";
                    else if (i.field) i.context = "make";
                    ins.push_back(i);
                } else if (lt.size() >= 4 && lt[0] == "gout") {
                    outs.push_back({lt[1], WordType(lt[2]), lt[3] == "1"});
                } else {
                    bodyText += l + "\n";
                }
                if (e2 >= text.size()) break;
            }
            u.def = UserGroupDef(u.name, ins, outs);
            auto b = std::make_shared<Graph>();
            b->groups = g.groups;  // the groups before it
            if (!ParseBody(bodyText, b.get())) return false;
            u.body = b;
            if (!ReservedName(u.name) && !FindUserGroup(g, u.name)) g.groups.push_back(u);
            if (pos > text.size()) break;
            continue;
        }
        if (w == "revision" && t.size() >= 2) {
            known = ParseInt(t[1], &g.revision);
        } else if (w == "nextid" && t.size() >= 2) {
            known = ParseInt(t[1], &g.nextId);
        } else if ((w == "macro" || w == "imacro") && t.size() >= 4) {
            t = Split(line, 4, &rest);
            int k = 0;
            double lo = 0, hi = 0;
            if (ParseInt(t[1], &k) && k >= 1 && k <= 32 && ParseNum(t[2], &lo) && ParseNum(t[3], &hi))
                (w == "macro" ? g.macros : g.intMacros)[k - 1] = {rest, lo, hi};
            else known = false;
        } else if (w == "node" && t.size() >= 6) {
            Node n;
            n.type = t[2];
            n.key = t[5];
            if (ParseInt(t[1], &n.id) && n.id && ParseNum(t[3], &n.x) && ParseNum(t[4], &n.y) && !g.Find(n.id) && !g.FindKey(n.key)) {
                NormalizeMath(n);  // P1.5's math_add -> math, op add
                FillInputs(n, g);
                for (size_t k = 6; k < t.size(); k++) {
                    const std::string& o = t[k];
                    if (o.rfind("op=", 0) == 0 && IsMathFamily(n.type)) n.text = o.substr(3);
                    else if (o.rfind("legacy=", 0) == 0) ParseInt(o.substr(7), &n.legacyId);
                    else if (o.rfind("zone=", 0) == 0) ParseInt(o.substr(5), &n.zone);
                    else if (o.rfind("salt=", 0) == 0) ParseInt(o.substr(5), &n.salt, 16);
                    else if (o.rfind("setting=", 0) == 0) ParseInt(o.substr(8), &n.setting);
                }
                g.nodes.push_back(n);
            } else {
                known = false;
            }
        } else if (w == "in" && t.size() >= 5) {
            uint32_t id = 0;
            Node* n = ParseInt(t[1], &id) ? g.Find(id) : nullptr;
            InputSlot s;
            s.name = t[2];
            bool ok = n != nullptr;
            if (ok && t[3] == "const") ok = ParseValue(t[4], &s.value);
            else if (ok && t[3] == "link" && t.size() >= 6) ok = ParseInt(t[4], &s.link.from) && ParseInt(t[5], &s.link.output);
            else ok = false;
            if (ok) {
                if (InputSlot* have = n->In(s.name)) {
                    if (s.link.from) have->link = s.link;
                    else {
                        Value c;
                        // a stored value of another type than the input's (an older build): converted if it can be
                        have->value = Convert(s.value, have->value.type, &c) ? c : s.value;
                    }
                } else {
                    n->inputs.push_back(s);  // an input this build does not know: kept
                }
            } else {
                known = false;
            }
        } else if (w == "text" && t.size() >= 2) {
            t = Split(line, 2, &rest);
            uint32_t id = 0;
            Node* n = ParseInt(t[1], &id) ? g.Find(id) : nullptr;
            if (n) n->text = UnescapeLine(rest);
            else known = false;
        } else {
            known = false;
        }
        if (!known) g.unknownLines.push_back(line);
        if (e >= text.size()) break;
    }
    // wires from nodes that are not there are dropped
    for (Node& n : g.nodes)
        for (InputSlot& s : n.inputs)
            if (s.link.from && !g.Find(s.link.from)) s.link = {};
    for (const Node& n : g.nodes)
        if (n.id >= g.nextId) g.nextId = n.id + 1;
    for (Node& n : g.nodes)
        if (n.type == "expr") MigrateFormula(n);
    return true;
}

}  // namespace dx5

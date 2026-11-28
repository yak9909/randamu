// DupliX 2 (design v5 12, 12.1): the editor's operations on the user's groups: copy a built-in group for yourself,
// group a selection (Ctrl+G) and back, an Expression node to a group.
#include <algorithm>
#include <functional>
#include <map>
#include <set>

#include "dxg5/edit.h"
#include "dxg5/lang.h"

namespace dx5 {

namespace {

bool NameTaken(const Graph& g, const std::string& n) { return ReservedName(n) || FindUserGroup(g, n); }

std::string FreeName(const Graph& g, const std::string& want, bool numberFirst)
{
    if (!numberFirst && !NameTaken(g, want)) return want;
    for (int k = numberFirst ? 1 : 2;; k++) {
        const std::string s = want + (numberFirst ? "" : "_") + std::to_string(k);
        if (!NameTaken(g, s)) return s;
    }
}

std::string Unique(std::set<std::string>& used, const std::string& want)
{
    std::string s = want;
    for (int k = 2; used.count(s); k++) s = want + "_" + std::to_string(k);
    used.insert(s);
    return s;
}

bool IsGeoOut(const Graph& g, const Node& n, int output) { return OutputType(g, n, output) == Type::Geometry; }

// the group's table entry from its body: its parameters as given, structural where they reach a structural input,
// its outputs from the group_output node
NodeDef DefFromBody(const std::string& name, const Graph& body, std::vector<InputDef> params)
{
    std::set<uint32_t> structural;
    std::function<void(uint32_t)> up = [&](uint32_t id) {
        if (!structural.insert(id).second) return;
        const Node* n = body.Find(id);
        if (!n) return;
        for (const InputSlot& in : n->inputs)
            if (in.link.from) {
                const Node* f = body.Find(in.link.from);
                if (f && !IsGeoOut(body, *f, in.link.output)) up(in.link.from);
            }
    };
    for (const Node& n : body.nodes) {
        const NodeDef* d = DefFor(body, n.type);
        if (!d) continue;
        for (const InputSlot& in : n.inputs) {
            const int k = d->Input(in.name);
            if (k >= 0 && d->inputs[k].structural && in.link.from) up(in.link.from);
        }
    }
    std::string geoParam;
    for (const InputDef& p : params)
        if (p.type == Type::Geometry && geoParam.empty()) geoParam = p.name;
    for (InputDef& p : params) {
        if (p.type == Type::Geometry) continue;
        for (const Node& n : body.nodes)
            if (n.type == "param" && n.text == p.name && structural.count(n.id)) p.structural = true;
        p.field = !p.structural;
        p.context = p.structural ? "" : (geoParam.empty() ? "make" : geoParam);
    }
    std::vector<OutputDef> outs;
    for (const Node& n : body.nodes)
        if (n.type == "group_output")
            for (const InputSlot& in : n.inputs) {
                const Node* f = in.link.from ? body.Find(in.link.from) : nullptr;
                const Type t = f ? OutputType(body, *f, in.link.output) : Type::Float;
                outs.push_back({in.name, t == Type::Any ? Type::Float : t, t != Type::Geometry});
            }
    return UserGroupDef(name, params, outs);
}

bool Used(const Graph& g, const std::string& type)
{
    for (const Node& n : g.nodes)
        if (n.type == type) return true;
    for (const UserGroup& u : g.groups)
        if (u.body)
            for (const Node& n : u.body->nodes)
                if (n.type == type) return true;
    return false;
}

}  // namespace

bool CopyGroupForSelf(Graph& g, uint32_t node, std::string* newName)
{
    Node* n = g.Find(node);
    const NodeDef* d = n ? FindDef(n->type) : nullptr;
    const std::string* src = d && d->group ? GroupSource(n->type) : nullptr;
    if (!src) return false;
    auto body = std::make_shared<Graph>();
    std::string nm;
    std::vector<Diag> errs;
    if (!GroupToGraph(*src, body.get(), &nm, &errs)) return false;
    // the definition's Output is the group's output
    for (Node& x : body->nodes)
        if (x.type == "output") {
            x.type = "group_output";
            x.key = "return";  // as the code reader names it
            std::vector<InputSlot> keep;
            for (const InputSlot& s : x.inputs)
                if (s.name == "geometry") keep.push_back(s);
            x.inputs = keep;
        }
    body->groups = g.groups;
    UserGroup u;
    u.name = FreeName(g, n->type + "_copy", false);
    u.body = body;
    u.def = *d;
    u.def.name = u.def.label = u.name;
    u.def.category = "Group";
    g.groups.push_back(u);
    n = g.Find(node);
    n->type = u.name;
    if (newName) *newName = u.name;
    return true;
}

bool GroupSelection(Graph& g, const std::vector<uint32_t>& nodes, const std::string& name, uint32_t* groupNode)
{
    std::set<uint32_t> S(nodes.begin(), nodes.end());
    if (S.empty()) return false;
    double cx = 0, cy = 0;
    for (uint32_t id : S) {
        const Node* n = g.Find(id);
        if (!n || n->zone || n->type == "output" || n->type == "each" || n->type == "param" || n->type == "param_geo" ||
            n->type == "group_output")
            return false;
        for (const Node& z : g.nodes)
            if (z.zone == id) return false;
        cx += n->x, cy += n->y;
    }
    cx /= (double)S.size(), cy /= (double)S.size();
    const std::string gname = name.empty() ? FreeName(g, "group", true) : name;
    if (NameTaken(g, gname)) return false;
    // inputs: each wire from outside (one parameter per distinct source)
    std::map<std::pair<uint32_t, int>, std::string> paramOf;
    std::vector<InputDef> params;
    std::set<std::string> pnames;
    for (const Node& n : g.nodes) {
        if (!S.count(n.id)) continue;
        for (const InputSlot& s : n.inputs) {
            if (!s.link.from || S.count(s.link.from) || paramOf.count({s.link.from, s.link.output})) continue;
            const Node* f = g.Find(s.link.from);
            if (!f) continue;
            const bool geo = IsGeoOut(g, *f, s.link.output);
            InputDef p;
            p.name = Unique(pnames, geo ? "geometry" : s.name);
            const Type t = OutputType(g, *f, s.link.output);
            p.type = geo ? Type::Geometry : (t == Type::Any ? Type::Float : t);
            p.dflt = geo ? Value{} : Value::F(0);
            if (!geo) {
                Value c;
                if (Convert(Value::F(0), p.type, &c)) p.dflt = c;
            }
            p.softMin = 0, p.softMax = 100;
            params.push_back(p);
            paramOf[{s.link.from, s.link.output}] = p.name;
        }
    }
    // outputs: each output of the selection used outside, and who uses it
    std::vector<std::pair<uint32_t, int>> outs;
    std::vector<std::string> outNames;
    std::set<std::string> onames;
    struct Use {
        uint32_t node;
        std::string input;
        int output;
    };
    std::vector<Use> uses;
    for (const Node& n : g.nodes) {
        if (S.count(n.id)) continue;
        for (const InputSlot& s : n.inputs) {
            if (!s.link.from || !S.count(s.link.from)) continue;
            const std::pair<uint32_t, int> o = {s.link.from, s.link.output};
            auto it = std::find(outs.begin(), outs.end(), o);
            int k = (int)(it - outs.begin());
            if (it == outs.end()) {
                outs.push_back(o);
                outNames.push_back(Unique(onames, IsGeoOut(g, *g.Find(o.first), o.second) ? "geometry" : "value"));
            }
            uses.push_back({n.id, s.name, k});
        }
    }
    // the body: the nodes (same ids and keys), a parameter node per input, the group's output
    auto body = std::make_shared<Graph>();
    body->groups = g.groups;
    body->nextId = g.nextId;
    for (const Node& n : g.nodes)
        if (S.count(n.id)) body->nodes.push_back(n);
    std::map<std::string, uint32_t> pnode;
    for (const InputDef& p : params) {
        const uint32_t id = AddNode(*body, p.type == Type::Geometry ? "param_geo" : "param", p.name, cx - 300, cy);
        body->Find(id)->text = p.name;
        pnode[p.name] = id;
    }
    for (Node& n : body->nodes)
        for (InputSlot& s : n.inputs) {
            auto it = paramOf.find({s.link.from, s.link.output});
            if (s.link.from && !S.count(s.link.from) && it != paramOf.end()) s.link = {pnode[it->second], 0};
        }
    const uint32_t ret = AddNode(*body, "group_output", "return", cx + 300, cy);
    for (size_t k = 0; k < outs.size(); k++) {
        InputSlot s;
        s.name = outNames[k];
        s.link = {outs[k].first, outs[k].second};
        body->Find(ret)->inputs.push_back(s);
    }
    UserGroup u;
    u.name = gname;
    u.def = DefFromBody(gname, *body, params);
    u.body = body;
    // the graph: the selection goes, the group node takes its wires
    for (uint32_t id : S) RemoveNode(g, id);
    g.groups.push_back(u);
    const uint32_t gn = AddNode(g, gname, "", cx, cy);
    Node* G = g.Find(gn);
    for (const auto& kv : paramOf) {
        InputSlot* s = G->In(kv.second);
        if (s) s->link = {kv.first.first, kv.first.second};
    }
    for (const Use& x : uses)
        if (Node* m = g.Find(x.node))
            if (InputSlot* s = m->In(x.input)) s->link = {gn, x.output};
    if (groupNode) *groupNode = gn;
    return true;
}

bool UngroupNode(Graph& g, uint32_t node)
{
    const Node* n0 = g.Find(node);
    const UserGroup* u0 = n0 ? FindUserGroup(g, n0->type) : nullptr;
    if (!u0 || !u0->body) return false;
    const Node G = *n0;
    const UserGroup u = *u0;
    const Graph& body = *u.body;
    // who uses the group node's outputs
    struct Use {
        uint32_t node;
        std::string input;
        int output;
    };
    std::vector<Use> uses;
    for (const Node& m : g.nodes)
        for (const InputSlot& s : m.inputs)
            if (s.link.from == node) uses.push_back({m.id, s.name, s.link.output});
    RemoveNode(g, node);
    // the body's nodes come in with new ids (their keys kept when free)
    std::map<uint32_t, uint32_t> remap;
    for (const Node& b : body.nodes) {
        if (b.type == "param" || b.type == "param_geo" || b.type == "group_output") continue;
        Node c = b;
        c.id = g.nextId++;
        c.key = UniqueKey(g, b.key);
        remap[b.id] = c.id;
        g.nodes.push_back(c);
    }
    // a wire from a parameter is what fed the group node's input
    auto outer = [&](const Link& l, InputSlot* into) {
        const Node* p = body.Find(l.from);
        if (p && (p->type == "param" || p->type == "param_geo")) {
            const InputSlot* gs = G.In(p->text);
            if (gs && gs->link.from) into->link = gs->link;
            else {
                into->link = {};
                if (gs) into->value = gs->value;
            }
            return;
        }
        auto it = remap.find(l.from);
        into->link = it == remap.end() ? Link{} : Link{it->second, l.output};
    };
    for (Node& c : g.nodes) {
        if (!std::any_of(remap.begin(), remap.end(), [&](const std::pair<const uint32_t, uint32_t>& kv) { return kv.second == c.id; }))
            continue;
        for (InputSlot& s : c.inputs)
            if (s.link.from) outer(Link(s.link), &s);
        // a node inside an Each zone of the body stays in that zone (its new id)
        if (c.zone) {
            auto z = remap.find(c.zone);
            c.zone = z == remap.end() ? 0 : z->second;
        }
    }
    const Node* ret = nullptr;
    for (const Node& b : body.nodes)
        if (b.type == "group_output") ret = &b;
    for (const Use& x : uses) {
        Node* m = g.Find(x.node);
        InputSlot* s = m ? m->In(x.input) : nullptr;
        if (!s) continue;
        if (!ret || x.output < 0 || x.output >= (int)ret->inputs.size()) {
            s->link = {};
            continue;
        }
        const InputSlot& r = ret->inputs[x.output];
        if (r.link.from) outer(r.link, s);
        else s->link = {}, s->value = r.value;
    }
    if (!Used(g, u.name))
        g.groups.erase(std::remove_if(g.groups.begin(), g.groups.end(), [&](const UserGroup& x) { return x.name == u.name; }), g.groups.end());
    return true;
}

bool ExpressionToGroup(Graph& g, uint32_t exprNode, std::string* name)
{
    const Node* e = g.Find(exprNode);
    if (!e || e->type != "expr") return false;
    uint32_t gn = 0;
    if (!GroupSelection(g, {exprNode}, "", &gn)) return false;
    if (name) *name = g.Find(gn)->type;
    return true;
}

namespace {

bool FormulaNode(const std::string& t)
{
    return IsMathFamily(t) || t == "expr" || t == "attr" || t == "random" || t == "index" || t == "id" || t == "count" ||
           t == "time" || t == "seed" || t == "position" || t == "rotation" || t == "scale" || t == "opacity" || t == "source" ||
           t == "visible" || t == "weight" || t == "q" || t == "s";
}

std::string ConstFormula(const Value& v)
{
    switch (v.type) {
    case Type::Int: return std::to_string(v.i);
    case Type::Bool: return v.i ? "true" : "false";
    case Type::Vector: return "vec(" + Num(v.v.x) + ", " + Num(v.v.y) + ", " + Num(v.v.z) + ")";
    default: return Num(AsFloat(v));
    }
}

// the names in a formula's expression renamed (parent.* is left alone)
std::string Renamed(const Expr& e0, const std::map<std::string, std::string>& to)
{
    std::unique_ptr<Expr> e;
    Diag d;
    if (!ParseExpr(PrintExpr(e0), &e, &d)) return PrintExpr(e0);
    std::function<void(Expr&)> walk = [&](Expr& x) {
        if (x.kind == Expr::Name) {
            auto it = to.find(x.text);
            if (it != to.end()) x.text = it->second;
        }
        if (x.kind == Expr::Member && x.args[0]->kind == Expr::Name && x.args[0]->text == "parent") return;
        for (auto& a : x.args) walk(*a);
        for (auto& a : x.named) walk(*a.second);
    };
    walk(*e);
    return PrintExpr(*e);
}

// the groups each body may use (the ones before it), and the nodes of a changed group following its inputs by name
void RefreshGroups(Graph& g, const std::string& changed)
{
    auto follow = [&](Graph& x) {
        const UserGroup* u = FindUserGroup(g, changed);
        if (!u) return;
        for (Node& n : x.nodes) {
            if (n.type != changed) continue;
            std::vector<InputSlot> ins;
            for (const InputDef& d : u->def.inputs) {
                InputSlot s;
                s.name = d.name, s.value = d.dflt;
                if (const InputSlot* have = n.In(d.name)) s = *have;
                ins.push_back(s);
            }
            n.inputs = ins;
        }
        const int outs = (int)u->def.outputs.size();
        for (Node& n : x.nodes)
            for (InputSlot& s : n.inputs)
                if (s.link.from) {
                    const Node* f = x.Find(s.link.from);
                    if (f && f->type == changed && s.link.output >= outs) s.link = {};
                }
    };
    follow(g);
    for (size_t k = 0; k < g.groups.size(); k++) {
        if (!g.groups[k].body) continue;
        auto b = std::make_shared<Graph>(*g.groups[k].body);
        b->groups.assign(g.groups.begin(), g.groups.begin() + (long)k);
        follow(*b);
        g.groups[k].body = b;
    }
}

}  // namespace

bool OpenExpression(const Graph& g, uint32_t exprNode, Graph* inside)
{
    const Node* e = g.Find(exprNode);
    if (!e || e->type != "expr") return false;
    std::vector<FormulaLine> lines;
    Diag d;
    if (!ParseFormula(e->text, &lines, &d)) return false;
    Graph in;
    std::map<std::string, uint32_t> pn;
    double y = 0;
    for (const InputSlot& s : e->inputs) {
        const uint32_t id = AddNode(in, "param", s.name, -500, y);
        in.Find(id)->text = s.name;
        pn[s.name] = id;
        y += 70;
    }
    const uint32_t ret = AddNode(in, "group_output", "return", 400, 0);
    std::map<std::string, std::string> earlier;  // a line's name -> its formula (earlier lines are written in)
    double ly = 0;
    for (const FormulaLine& l : lines) {
        std::unique_ptr<Expr> x;
        if (!ParseExpr(PrintExpr(*l.expr), &x, &d)) return false;
        std::function<void(Expr&)> sub = [&](Expr& z) {
            if (z.kind == Expr::Name) {
                auto it = earlier.find(z.text);
                if (it != earlier.end()) {
                    std::unique_ptr<Expr> r;
                    if (ParseExpr(it->second, &r, &d)) z = std::move(*r);
                    return;
                }
            }
            if (z.kind == Expr::Member && z.args[0]->kind == Expr::Name && z.args[0]->text == "parent") return;
            for (auto& a : z.args) sub(*a);
            for (auto& a : z.named) sub(*a.second);
        };
        sub(*x);
        const std::string text = "(" + PrintExpr(*x) + ")";
        earlier[l.name] = text;
        if (l.name[0] == '_') continue;
        const uint32_t t = AddNode(in, "expr", "line_" + l.name, 0, ly);
        ly += 150;
        Node* tn = in.Find(t);
        tn->text = PrintExpr(*x);
        for (const std::string& name : ReadFormula(tn->text).inputs) {
            InputSlot s;
            s.name = name;
            auto it = pn.find(name);
            if (it != pn.end()) s.link = {it->second, 0};
            tn->inputs.push_back(s);
        }
        InputSlot o;
        o.name = l.name;
        o.link = {t, 0};
        in.Find(ret)->inputs.push_back(o);
        if (!ExpandExpression(in, t)) return false;
    }
    *inside = in;
    return true;
}

bool CloseExpression(Graph& g, uint32_t exprNode, const Graph& inside, std::string* why)
{
    auto no = [&](const std::string& w) {
        if (why) *why = w;
        return false;
    };
    const Node* e = g.Find(exprNode);
    if (!e || e->type != "expr") return no("not an Expression node");
    // an inside that only moved is the formula as written (its _ lines and its spacing stay)
    {
        Graph fresh;
        auto placeless = [](Graph x) {
            for (Node& n : x.nodes) n.x = n.y = 0;
            return Serialize(x);
        };
        if (OpenExpression(g, exprNode, &fresh) && placeless(fresh) == placeless(inside)) return true;
    }
    Graph in = inside;
    std::vector<uint32_t> S;
    const Node* ret0 = nullptr;
    for (const Node& n : in.nodes) {
        if (n.type == "param" || n.type == "group_output" || n.type == "macro" || n.type == "int_macro") {
            if (n.type == "group_output") ret0 = &n;
            continue;  // a macro is written by its name (m2)
        }
        if (!FormulaNode(n.type)) return no(n.type + " cannot be in a formula");
        S.push_back(n.id);
    }
    if (!ret0) return no("the Expression's outputs are gone");
    const uint32_t retId = ret0->id;
    // the nodes become one formula (one line per output used)
    uint32_t X = 0;
    bool used = false;
    for (const InputSlot& s : ret0->inputs)
        if (s.link.from && std::find(S.begin(), S.end(), s.link.from) != S.end()) used = true;
    if (used) {
        if (!CollapseToExpression(in, S)) return no("the nodes inside do not make a formula");
        for (const InputSlot& s : in.Find(retId)->inputs)
            if (s.link.from && in.Find(s.link.from) && in.Find(s.link.from)->type == "expr") X = s.link.from;
    }
    std::vector<FormulaLine> xl;
    std::map<std::string, std::string> to;  // the collapsed node's inputs -> the parameters they read
    if (X) {
        Diag d;
        if (!ParseFormula(in.Find(X)->text, &xl, &d)) return no(d.msg);
        for (const InputSlot& s : in.Find(X)->inputs) {
            const Node* p = s.link.from ? in.Find(s.link.from) : nullptr;
            if (p && p->type == "param") to[s.name] = p->text;
        }
    }
    const Node* ret = in.Find(retId);
    std::vector<std::pair<std::string, std::string>> outs;
    for (const InputSlot& s : ret->inputs) {
        std::string f;
        const Node* src = s.link.from ? in.Find(s.link.from) : nullptr;
        if (src && src->id == X && s.link.output < (int)xl.size()) f = Renamed(*xl[s.link.output].expr, to);
        else if (src && src->type == "param") f = src->text;
        else if (src && (src->type == "macro" || src->type == "int_macro")) f = std::string(src->type == "macro" ? "m" : "i") + std::to_string(src->setting);
        else f = ConstFormula(s.value);
        outs.push_back({s.name, f});
    }
    if (outs.empty()) return no("an Expression needs an output");
    std::string text;
    if (outs.size() == 1 && outs[0].first == "value") text = outs[0].second;
    else
        for (const auto& o : outs) text += (text.empty() ? "" : "\n") + o.first + " = " + o.second;
    return SetFormula(g, exprNode, text, why);
}

bool SetGroupBody(Graph& g, const std::string& name, const Graph& body, std::string* why)
{
    auto no = [&](const std::string& w) {
        if (why) *why = w;
        return false;
    };
    size_t i = 0;
    while (i < g.groups.size() && g.groups[i].name != name) i++;
    if (i == g.groups.size()) return no("a built-in group cannot be changed: Copy for Myself first");
    bool hasRet = false;
    for (const Node& n : body.nodes) hasRet |= n.type == "group_output";
    if (!hasRet) return no("a group needs its output");
    const NodeDef old = g.groups[i].def;
    std::vector<InputDef> params;
    for (const Node& n : body.nodes) {
        if (n.type != "param" && n.type != "param_geo") continue;
        const int k = old.Input(n.text);
        InputDef p;
        if (k >= 0) p = old.inputs[k];
        else {
            p.name = n.text, p.softMin = 0, p.softMax = 100;
            p.type = n.type == "param_geo" ? Type::Geometry : Type::Float;
            p.dflt = n.type == "param_geo" ? Value{} : Value::F(0);
        }
        params.push_back(p);
    }
    // the groups the body holds come back out: one changed inside it (an edit in a nested opened group) replaces the
    // top graph's, one made inside it (Ctrl+G, Copy for Myself there) goes in before this group (it uses it)
    for (const UserGroup& inner : body.groups) {
        if (inner.name == name) continue;
        size_t j = 0;
        while (j < g.groups.size() && g.groups[j].name != inner.name) j++;
        if (j < g.groups.size()) {
            if (j < i) g.groups[j].body = inner.body, g.groups[j].def = inner.def;
            else return no("the group " + inner.name + " comes after " + name + " and cannot be used in it");
        } else {
            if (ReservedName(inner.name)) return no("the group " + inner.name + " is named like a built-in node");
            g.groups.insert(g.groups.begin() + (long)i, inner);
            i++;
        }
    }
    auto b = std::make_shared<Graph>(body);
    b->groups.assign(g.groups.begin(), g.groups.begin() + (long)i);
    g.groups[i].def = DefFromBody(name, *b, params);
    g.groups[i].body = b;
    for (const UserGroup& inner : body.groups) RefreshGroups(g, inner.name);
    RefreshGroups(g, name);
    return true;
}

std::vector<int> UnusedNamedMacros(const Graph& g, bool isInt)
{
    const uint32_t used = isInt ? UsedIntMacros(g) : UsedMacros(g);
    const MacroSetting* ms = isInt ? g.intMacros : g.macros;
    std::vector<int> r;
    for (int k = 0; k < 32; k++)
        if (!(used >> k & 1) && !ms[k].name.empty()) r.push_back(k);
    return r;
}

std::string ExposeRefusal(const Graph& g, bool isInt)
{
    return FreeMacro(g, isInt) < 0 ? "No free macro (clear the name of an unused macro to use it)" : "";
}

}  // namespace dx5

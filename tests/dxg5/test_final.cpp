// Plan P1.5e, the final review's findings: each test reproduces one (C1, C2, I1-I5, M1).
#include <cstdio>
#include <string>

#include "dxg5/edit.h"
#include "dxg5/eval.h"
#include "dxg5/lang.h"
#include "test.h"

using namespace dx5;

namespace {

EvalContext CtxF()
{
    EvalContext c;
    c.macro = [](int k, double) { return k == 2 ? 4.0 : 0.0; };
    c.intMacro = [](int, double) { return (int64_t)0; };
    return c;
}

bool ReadF(const std::string& code, Graph* g, std::vector<Diag>* errsOut = nullptr)
{
    std::vector<Diag> errs, notes;
    if (ApplyCode(*g, code, &errs, &notes)) return true;
    if (errsOut) *errsOut = errs;
    else std::printf("  cannot read: %s (line %d)\n", errs.empty() ? "?" : errs[0].msg.c_str(), errs.empty() ? 0 : errs[0].pos.line);
    return false;
}

}  // namespace

// C1: a formula of several lines is saved and read back whole (also inside a group's body, where a line may start
// with "end")
TEST(final_c1_multiline_formula_saved)
{
    Graph g;
    CHECK(ReadF("g = points(3)\nw = expr { x = index * 2\n end1 = 3\n y = sin(time) }\nh = set_position(g, vec(w.x, w.y, w.end1))\noutput(h)\n", &g));
    const std::string s = Serialize(g);
    Graph back;
    CHECK(Parse(s, &back));
    std::string why;
    CHECK(SameGraph(g, back, &why));
    if (!why.empty()) std::printf("  %s\n", why.c_str());
    CHECK(back.unknownLines.empty());
    const auto r = Evaluate(back, CtxF());
    CHECK(r.elements.size() == 3 && r.elements[2].pos[0] == 4 && r.elements[2].pos[2] == 3);
    Graph gg;
    CHECK(ReadF("group two(geometry: geometry) {\n  w = expr { x = index\n end = 2 }\n  h = set_position(geometry, vec(w.x, w.end, 0))\n  return h\n}\n"
                "g = points(2)\nh = two(g)\noutput(h)\n",
                &gg));
    Graph gb;
    CHECK(Parse(Serialize(gg), &gb));
    CHECK(SameGraph(gg, gb, &why));
    if (!why.empty()) std::printf("  %s\n", why.c_str());
}

// C2: an edit inside a group opened inside another group reaches the top graph
TEST(final_c2_nested_group_edit_kept)
{
    Graph g;
    CHECK(ReadF("group inner(geometry: geometry) {\n  h = set_position(geometry, position * 2)\n  return h\n}\n"
                "group outer(geometry: geometry) {\n  h = inner(geometry)\n  return h\n}\n"
                "g = grid(cx: 3, cy: 1)\nh = outer(g)\noutput(h)\n",
                &g));
    // open outer, then inner inside it; edit inner (position * 3) and write back outward as the editor does
    Graph outerBody = *FindUserGroup(g, "outer")->body;
    Graph innerBody = *FindUserGroup(outerBody, "inner")->body;
    for (Node& n : innerBody.nodes)
        if (n.type == "expr") n.text = "position * 3";
    std::string why;
    CHECK(SetGroupBody(outerBody, "inner", innerBody, &why));
    CHECK(SetGroupBody(g, "outer", outerBody, &why));
    const auto r = Evaluate(g, CtxF());
    CHECK(r.elements.size() == 3 && r.elements[2].pos[0] == 300);
    // a group made inside a group's body reaches the top graph too
    Graph ob = *FindUserGroup(g, "outer")->body;
    std::vector<uint32_t> sel;
    for (const Node& n : ob.nodes)
        if (n.type == "inner") sel.push_back(n.id);
    uint32_t gn = 0;
    CHECK(GroupSelection(ob, sel, "", &gn));
    const std::string made = ob.Find(gn)->type;
    CHECK(SetGroupBody(g, "outer", ob, &why));
    CHECK(FindUserGroup(g, made) != nullptr);
    const auto r2 = Evaluate(g, CtxF());
    CHECK(r2.elements.size() == 3 && r2.elements[2].pos[0] == 300 && r2.warnings.empty());
}

// I1: a P1.5 formula reading a named attribute bare keeps reading it when its text is applied again
TEST(final_i1_p15_formula_migrates)
{
    const std::string text =
        "DupliX graph 2\nrevision 1\nnextid 5\nnode 1 grid 0 0 g\nin 1 cx const i:3\nin 1 cy const i:1\nin 1 sx const f:100\n"
        "node 2 set_position 0 0 h\nin 2 geometry link 1 0\nin 2 position link 3 0\nnode 3 expr 0 0 h.position\ntext 3 vec(ix * 10, 0, 0)\n"
        "node 4 output 0 0 out\nin 4 geometry link 2 0\n";
    Graph g;
    CHECK(Parse(text, &g));
    const auto before = Evaluate(g, CtxF());
    const Node* e = g.FindKey("h.position");
    CHECK(e != nullptr);
    if (!e) return;
    std::string why;
    CHECK(SetFormula(g, e->id, g.FindKey("h.position")->text, &why));
    const auto after = Evaluate(g, CtxF());
    CHECK(before.elements.size() == 3 && after.elements.size() == 3 && after.elements[2].pos[0] == 20 && before.elements[2].pos[0] == 20);
}

// I2: a group cannot be named like a math function or a formula's own name
TEST(final_i2_group_names)
{
    for (const char* n : {"sin", "vec", "attr", "random", "index", "expr", "position"}) {
        Graph g;
        std::vector<Diag> errs;
        const std::string code = std::string("group ") + n + "(geometry: geometry) {\n  return geometry\n}\ng = points(2)\noutput(g)\n";
        CHECK(!ReadF(code, &g, &errs));
    }
}

// I3: ungrouping a group that holds an each zone keeps the zone's nodes inside it
TEST(final_i3_ungroup_zone)
{
    Graph g;
    CHECK(ReadF("group z(geometry: geometry) {\n  h = each geometry {\n    position += vec(1, 0, 0)\n  }\n  return h\n}\n"
                "g = points(2)\nk = z(g)\noutput(k)\n",
                &g));
    const auto before = Evaluate(g, CtxF());
    CHECK(UngroupNode(g, g.FindKey("k")->id));
    for (const Node& n : g.nodes)
        if (n.zone) CHECK(g.Find(n.zone) && g.Find(n.zone)->type == "each");
    const auto after = Evaluate(g, CtxF());
    CHECK(after.elements.size() == 2 && after.elements[1].pos[0] == before.elements[1].pos[0]);
}

// I4: a typo inside expr { } on its third line is reported on that line
TEST(final_i4_expr_block_error_line)
{
    Graph g;
    std::vector<Diag> errs;
    CHECK(!ReadF("g = points(2)\nw = expr {\n  x = index\n  y = indx\n}\noutput(g)\n", &g, &errs));
    CHECK(!errs.empty() && errs[0].pos.line == 4);
    errs.clear();
    CHECK(!ReadF("g = points(2)\nw = expr {\n  x = index\n  y = (2\n}\noutput(g)\n", &g, &errs));
    CHECK(!errs.empty() && errs[0].pos.line == 4);
}

// I5: closing an opened Expression whose inside only moved leaves the formula as written
TEST(final_i5_close_unchanged_keeps_text)
{
    Graph g;
    CHECK(ReadF("g = points(2)\nw = expr {\n  _d = index + 1\n  x = _d * _d\n}\nh = set_position(g, vec(w.x, 0, 0))\noutput(h)\n", &g));
    const uint32_t w = g.FindKey("w")->id;
    const std::string text = g.Find(w)->text;
    Graph in;
    CHECK(OpenExpression(g, w, &in));
    for (Node& n : in.nodes) n.x += 50;
    std::string why;
    CHECK(CloseExpression(g, w, in, &why));
    CHECK(g.Find(w)->text == text);
}

// M1: an output wired straight from a macro inside an opened Expression stays the macro
TEST(final_m1_macro_output_kept)
{
    Graph g;
    CHECK(ReadF("g = points(2)\nw = expr { x = m3 }\nh = set_position(g, vec(w.x, 0, 0))\noutput(h)\n", &g));
    const uint32_t w = g.FindKey("w")->id;
    Graph in;
    CHECK(OpenExpression(g, w, &in));
    std::string why;
    CHECK(CloseExpression(g, w, in, &why));
    const auto r = Evaluate(g, CtxF());
    CHECK(r.elements.size() == 2 && r.elements[1].pos[0] == 4);
}

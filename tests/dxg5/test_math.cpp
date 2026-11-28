// Plan P1.5e Task 1: the math nodes are five families with an operator (design v5 6.2).
#include <cmath>
#include <string>

#include "dxg5/edit.h"
#include "dxg5/eval.h"
#include "dxg5/graph.h"
#include "test.h"

using namespace dx5;

namespace {

EvalContext Ctx()
{
    EvalContext c;
    c.macro = [](int, double) { return 0.0; };
    c.intMacro = [](int, double) { return (int64_t)0; };
    return c;
}

// points(3) -> set_position(vec(<value>, 0, 0)) -> output; the value node's result is element 2's x
struct XGraph {
    Graph g;
    uint32_t vec = 0;
    XGraph()
    {
        const uint32_t p = AddNode(g, "points", "pts", 0, 0);
        g.Find(p)->In("count")->value = Value::I(3);
        vec = AddNode(g, "math_vec", "v", 0, 0);
        const uint32_t sp = AddNode(g, "set_position", "sp", 0, 0);
        g.Find(sp)->In("geometry")->link = {p, 0};
        g.Find(sp)->In("position")->link = {vec, 0};
        const uint32_t o = AddNode(g, "output", "out", 0, 0);
        g.Find(o)->In("geometry")->link = {sp, 0};
    }
    double X()
    {
        const auto r = Evaluate(g, Ctx());
        return r.elements.size() == 3 ? r.elements[2].pos[0] : NAN;
    }
};

}  // namespace

TEST(math_family_ops)
{
    const MathOpDef* s = FindMathOp("math", "sin");
    CHECK(s && s->arity == 1);
    const MathOpDef* lt = FindMathOp("compare", "lt");
    CHECK(lt && lt->out == Type::Bool);
    const MathOpDef* a = FindMathOpByCodeName("math_add");
    CHECK(a && a->family == "math" && a->op == "add");
    const MathOpDef* d = FindMathOpByCodeName("vmath_dot");
    CHECK(d && d->family == "vmath" && d->op == "dot" && d->out == Type::Float);
    CHECK(FindMathOpByCodeName("math_vec") && FindMathOpByCodeName("math_vec")->family == "vmath");
    CHECK(!MathOps("boolmath").empty() && !MathOps("rotate").empty());
    // the add menu lists the five families, not one entry per operation
    int families = 0, perOp = 0;
    for (const NodeDef& n : AllDefs()) families += IsMathFamily(n.name), perOp += n.name.rfind("math_", 0) == 0;
    CHECK(families == 5 && perOp == 0);
}

TEST(math_node_eval_by_op)
{
    XGraph x;
    const uint32_t m = AddNode(x.g, "math", "m", 0, 0);
    CHECK(x.g.Find(m)->type == "math" && x.g.Find(m)->text == "add");
    std::string why;
    CHECK(SetMathOp(x.g, m, "mul", &why));
    x.g.Find(m)->In("a")->value = Value::F(3);
    x.g.Find(m)->In("b")->value = Value::F(4);
    x.g.Find(x.vec)->In("a")->link = {m, 0};
    CHECK_NEAR(x.X(), 12, 1e-12);
    CHECK(!SetMathOp(x.g, m, "nope", &why));
}

TEST(math_hidden_input_wire_kept)
{
    XGraph x;
    const uint32_t m = AddNode(x.g, "math", "m", 0, 0);
    const uint32_t i = AddNode(x.g, "index", "i", 0, 0);
    const uint32_t k = AddNode(x.g, "math", "k", 0, 0);
    x.g.Find(k)->In("a")->value = Value::F(5);
    x.g.Find(k)->In("b")->value = Value::F(0);
    x.g.Find(m)->In("a")->link = {i, 0};
    x.g.Find(m)->In("b")->link = {k, 0};
    x.g.Find(x.vec)->In("a")->link = {m, 0};
    CHECK_NEAR(x.X(), 7, 1e-12);  // 2 + 5
    std::string why;
    CHECK(SetMathOp(x.g, m, "sin", &why));
    const Node* n = x.g.Find(m);
    CHECK(n->In("b")->link.from == k);
    CHECK(InputVisible(x.g, *n, 0) && !InputVisible(x.g, *n, 1));
    CHECK(LinkActive(x.g, *n, 0) && !LinkActive(x.g, *n, 1));
    CHECK_NEAR(x.X(), std::sin(2.0), 1e-12);
    CHECK(SetMathOp(x.g, m, "add", &why));
    CHECK(LinkActive(x.g, *x.g.Find(m), 1));
    CHECK_NEAR(x.X(), 7, 1e-12);
}

TEST(math_type_mismatch_wire_inactive)
{
    Graph g;
    const uint32_t d = AddNode(g, "vmath", "d", 0, 0);
    std::string why;
    CHECK(SetMathOp(g, d, "dot", &why));
    const uint32_t r = AddNode(g, "rotate", "r", 0, 0);
    // wired under rot (three angles: the Float fits), then the operator changes to rot_mul (a Rotation): the wire
    // stays and stops counting; a new wire of that kind is refused
    CHECK(Connect(g, d, 0, r, "a", &why));
    CHECK(SetMathOp(g, r, "rot_mul", &why));
    CHECK(!LinkActive(g, *g.Find(r), 0));
    CHECK(!Connect(g, d, 0, r, "b", &why));
    CHECK(SetMathOp(g, r, "rot", &why));
    CHECK(LinkActive(g, *g.Find(r), 0));
}

TEST(math_legacy_types_read)
{
    const std::string text =
        "DupliX graph 2\nrevision 1\nnextid 6\nnode 5 math_add 0 0 k\nin 5 a const f:2\nnode 4 math_sin 0 0 s\n";
    Graph g;
    CHECK(Parse(text, &g));
    const Node* k = g.FindKey("k");
    CHECK(k && k->type == "math" && k->text == "add" && k->In("a") && k->In("a")->value.f == 2);
    const Node* s = g.FindKey("s");
    CHECK(s && s->type == "math" && s->text == "sin");
    const std::string out = Serialize(g);
    CHECK(out.find("node 5 math 0 0 k op=add") != std::string::npos);
    CHECK(out.find("text 5") == std::string::npos);
    Graph back;
    CHECK(Parse(out, &back) && back.FindKey("k")->text == "add");
}

TEST(math_disabled_wire_read_as_inactive)
{
    // a saved wire on a hidden input stays (it is not dropped on reading)
    const std::string text = "DupliX graph 2\nrevision 1\nnextid 4\nnode 1 index 0 0 i\nnode 2 math 0 0 m op=sin\nin 2 a link 1 0\nin 2 b link 1 0\n";
    Graph g;
    CHECK(Parse(text, &g));
    const Node* m = g.FindKey("m");
    CHECK(m && m->In("b")->link.from == 1 && !LinkActive(g, *m, 1));
}

TEST(math_link_refuses_wrong_type)
{
    // rot_apply: a is the rotation applied (Rotation), b the vector turned: a number (a macro) cannot feed a
    Graph g;
    const uint32_t r = AddNode(g, "rotate", "r", 0, 0);
    std::string why;
    CHECK(SetMathOp(g, r, "rot_apply", &why));
    CHECK(!LinkMacro(g, r, "a", true, 0));
    CHECK(LinkRefusal(g, r, "a", Type::Int).find("Rotation") != std::string::npos);
    CHECK(LinkMacro(g, r, "b", true, 0));  // an Int spreads to a Vector
    const uint32_t i = AddNode(g, "index", "i", 0, 0);
    CHECK(!Connect(g, i, 0, r, "a", &why) && why.find("Rotation") != std::string::npos);
    CHECK(MathInputType(g, *g.Find(r), 0) == Type::Rotation && MathInputType(g, *g.Find(r), 1) == Type::Vector);
}

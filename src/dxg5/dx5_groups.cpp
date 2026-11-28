// DupliX 2 (design v5, 6.1 and 12): the built-in groups: their inputs (names, types, defaults: P1's values, design
// v4 8.1), their definitions in code (the answer), and the C++ that stands in for an unmodified one whose inputs
// are single values (P1's src/graph/eval.cpp, the same arithmetic in the same order: the tests compare them).
#include <algorithm>
#include <cmath>
#include <map>
#include <memory>

#include "dxg5/eval_internal.h"
#include "dxg5/lang.h"
#include "dxg5/nodes.h"

namespace dx5 {

namespace {

constexpr double kDeg = 3.14159265358979323846 / 180.0;

// ---- the definitions ----
const std::map<std::string, std::string>& Sources()
{
    static const std::map<std::string, std::string> s = {
        {"grid", R"(group grid(cx: int structural = 5, cy: int structural = 5, cz: int structural = 1, sx = 100, sy = 100, sz = 100) {
    pts = points(cx * cy * cz)
    a = store(pts, "ix", index % cx)
    b = store(a, "iy", floordiv(index, cx) % cy)
    c = store(b, "iz", floordiv(index, cx * cy))
    d = store(c, "u", ix / max(cx - 1, 1))
    e = store(d, "v", iy / max(cy - 1, 1))
    f = store(e, "w", iz / max(cz - 1, 1))
    g = set_position(f, vec((ix - (cx - 1) / 2) * sx, (iy - (cy - 1) / 2) * sy, (iz - (cz - 1) / 2) * sz))
    return g
})"},
        {"circle", R"(group circle(count: int structural = 12, radius = 300, start_angle = 0, sweep = 360, align: int structural = 0) {
    pts = points(count)
    step = (e) => count <= 1 ? 0 : (abs(sweep) >= 360 - 1e-9 ? sweep / count : sweep / (count - 1))
    a = store(pts, "angle", start_angle + step * index)
    b = store(a, "t", count <= 1 ? 0 : (abs(sweep) >= 360 - 1e-9 ? index / count : index / (count - 1)))
    c = store(b, "normal", vec(sin(angle * 0.017453292519943295), -cos(angle * 0.017453292519943295), 0))
    d = set_position(c, vec(radius * sin(angle * 0.017453292519943295), -radius * cos(angle * 0.017453292519943295), 0))
    g = set_rotation(d, rot(0, 0, angle), where: align == 1)
    return g
})"},
        {"line", R"(group line(count: int structural = 10, length = 800, angle = 0) {
    pts = points(count)
    a = store(pts, "t", count <= 1 ? 0.5 : index / (count - 1))
    b = store(a, "normal", vec(-sin(angle * 0.017453292519943295), cos(angle * 0.017453292519943295), 0))
    g = set_position(b, vec((count <= 1 ? 0 : t - 0.5) * length * cos(angle * 0.017453292519943295), (count <= 1 ? 0 : t - 0.5) * length * sin(angle * 0.017453292519943295), 0))
    return g
})"},
        {"transform", R"(group transform(geometry: geometry, position: vector = vec(0, 0, 0), rotation_x = 0, rotation_y = 0, rotation_z = 0, scale = 100, pivot: int structural = 0) {
    ep = (e) => e.position
    er = (e) => e.rotation
    r = rot(rotation_x, rotation_y, rotation_z)
    c = pivot == 0 ? bounds(geometry, ep).center : vec(0, 0, 0)
    a = set_position(geometry, position + r * ((ep - c) * (scale / 100)))
    g = set_rotation(a, r * er)
    return g
})"},
        {"modify", R"(group modify(geometry: geometry, position: vector = vec(0, 0, 0), rotation_x = 0, rotation_y = 0, rotation_z = 0, scale = 0, opacity = 0) {
    w = (e) => e.weight
    ep = (e) => e.position
    er = (e) => e.rotation
    es = (e) => e.scale
    eo = (e) => e.opacity
    a = set_position(geometry, ep + position * w)
    b = set_rotation(a, er * rot(rotation_x * w, rotation_y * w, rotation_z * w), where: rotation_x != 0 || rotation_y != 0 || rotation_z != 0)
    c = set_scale(b, es * max(0, 1 + scale / 100 * w))
    g = set_opacity(c, clamp(eo * max(0, 1 + opacity / 100 * w), 0, 1))
    return g
})"},
        {"source_layers", R"(group source_layers(geometry: geometry, first: int structural = 1, layers: int structural = 1, assign: int structural = 0) {
    o0 = layers > 0 && layer_set(first)
    o1 = layers > 1 && layer_set(first + 1)
    o2 = layers > 2 && layer_set(first + 2)
    o3 = layers > 3 && layer_set(first + 3)
    o4 = layers > 4 && layer_set(first + 4)
    o5 = layers > 5 && layer_set(first + 5)
    o6 = layers > 6 && layer_set(first + 6)
    o7 = layers > 7 && layer_set(first + 7)
    c0 = o0 + 0
    c1 = c0 + o1
    c2 = c1 + o2
    c3 = c2 + o3
    c4 = c3 + o4
    c5 = c4 + o5
    c6 = c5 + o6
    c7 = c6 + o7
    rr = random(salt: 0x5352)
    pick = (e) => c7 == 0 ? 0 : (assign == 1 ? floor(rr * c7) % c7 : e.index % c7)
    src = (e) => c7 == 0 ? -1 : (pick < c0 ? first - 1 : pick < c1 ? first : pick < c2 ? first + 1 : pick < c3 ? first + 2 : pick < c4 ? first + 3 : pick < c5 ? first + 4 : pick < c6 ? first + 5 : first + 6)
    g = set_source(geometry, src)
    return g
})"},
    };
    return s;
}

// a group's definition read once (the render threads share it)
const Graph* Template(const std::string& name)
{
    static const std::map<std::string, std::shared_ptr<Graph>> all = [] {
        std::map<std::string, std::shared_ptr<Graph>> m;
        for (const auto& kv : Sources()) {
            auto g = std::make_shared<Graph>();
            std::string n;
            std::vector<Diag> errs;
            if (GroupToGraph(kv.second, g.get(), &n, &errs)) m[kv.first] = g;
            else std::fprintf(stderr, "dx5: built-in group %s does not read: line %d: %s\n", kv.first.c_str(),
                              errs.empty() ? 0 : errs[0].pos.line, errs.empty() ? "?" : errs[0].msg.c_str());
        }
        return m;
    }();
    auto it = all.find(name);
    return it == all.end() ? nullptr : it->second.get();
}

// ---- the C++ stand-ins ----
struct In {
    Evaluator& ev;
    const Node& n;
    bool ok = true;
    double F(const char* name)
    {
        static const GeoPtr none = std::make_shared<Geometry>(MakePoints(0));
        const Column c = ev.Input(n, name, none);
        if (c.size() != 1) ok = false;  // a field: the definition is evaluated instead
        return AsFloat(c.Get(0));
    }
    Vec3 V(const char* name)
    {
        static const GeoPtr none = std::make_shared<Geometry>(MakePoints(0));
        const Column c = ev.Input(n, name, none);
        if (c.size() != 1) ok = false;
        Value v;
        return Convert(c.Get(0), Type::Vector, &v) ? v.v : Vec3{};
    }
    int64_t I(const char* name, int64_t lo, int64_t hi) { return std::clamp<int64_t>(AsInt(ev.Single(n, name)), lo, hi); }
};

void Attr(Geometry& g, const char* name, Type t)
{
    if (!g.attrs.count(name)) g.attrs[name] = MakeColumn(t, g.count, Value::F(0));
}

bool Grid(Evaluator& ev, const Node& n, Geometry* out)
{
    In in{ev, n};
    const int64_t cap = (int64_t)std::min<size_t>(ev.Ctx().maxElements, 1000000);
    const int64_t cx = in.I("cx", 0, cap), cy = in.I("cy", 0, cap), cz = in.I("cz", 0, cap);
    const double sx = in.F("sx"), sy = in.F("sy"), sz = in.F("sz");
    if (!in.ok) return false;
    const double total = (double)cx * (double)cy * (double)cz;
    const size_t cnt = ev.Cap((size_t)std::min<double>(std::max(0.0, total), (double)ev.Ctx().maxElements + 1));
    Geometry g = MakePoints(cnt);
    for (const char* a : {"ix", "iy", "iz"}) Attr(g, a, Type::Int);
    for (const char* a : {"u", "v", "w"}) Attr(g, a, Type::Float);
    Column &pos = g.attrs["position"], &ix = g.attrs["ix"], &iy = g.attrs["iy"], &iz = g.attrs["iz"], &u = g.attrs["u"],
           &v = g.attrs["v"], &w = g.attrs["w"];
    size_t i = 0;
    for (int64_t c = 0; c < cz && i < cnt; c++)
        for (int64_t b = 0; b < cy && i < cnt; b++)
            for (int64_t a = 0; a < cx && i < cnt; a++, i++) {
                g.id[i] = ev.ElementId(n, i, "pts");
                pos.v[i] = {(a - (cx - 1) * 0.5) * sx, (b - (cy - 1) * 0.5) * sy, (c - (cz - 1) * 0.5) * sz};
                ix.i[i] = a, iy.i[i] = b, iz.i[i] = c;
                u.f[i] = (double)a / (double)std::max<int64_t>(cx - 1, 1);
                v.f[i] = (double)b / (double)std::max<int64_t>(cy - 1, 1);
                w.f[i] = (double)c / (double)std::max<int64_t>(cz - 1, 1);
            }
    *out = std::move(g);
    return true;
}

bool Circle(Evaluator& ev, const Node& n, Geometry* out)
{
    In in{ev, n};
    const int64_t cnt0 = in.I("count", 0, (int64_t)std::min<size_t>(ev.Ctx().maxElements + 1, 1000000));
    const double r = in.F("radius"), a0 = in.F("start_angle"), sweep = in.F("sweep");
    const bool align = in.I("align", 0, 1) == 1;
    if (!in.ok) return false;
    const size_t cnt = ev.Cap((size_t)cnt0);
    const bool closed = std::fabs(sweep) >= 360 - 1e-9;
    const double step = cnt0 <= 1 ? 0 : (closed ? sweep / (double)cnt0 : sweep / (double)(cnt0 - 1));
    Geometry g = MakePoints(cnt);
    Attr(g, "angle", Type::Float), Attr(g, "t", Type::Float), Attr(g, "normal", Type::Vector);
    Column &pos = g.attrs["position"], &rot = g.attrs["rotation"], &ang = g.attrs["angle"], &t = g.attrs["t"], &nrm = g.attrs["normal"];
    for (size_t i = 0; i < cnt; i++) {
        const double a = a0 + step * (double)i;
        g.id[i] = ev.ElementId(n, i, "pts");
        pos.v[i] = {r * std::sin(a * kDeg), -r * std::cos(a * kDeg), 0};
        if (align) rot.r[i] = RotXYZ(0, 0, a);
        ang.f[i] = a;
        t.f[i] = cnt0 <= 1 ? 0 : (closed ? (double)i / (double)cnt0 : (double)i / (double)(cnt0 - 1));
        nrm.v[i] = {std::sin(a * kDeg), -std::cos(a * kDeg), 0};
    }
    *out = std::move(g);
    return true;
}

bool Line(Evaluator& ev, const Node& n, Geometry* out)
{
    In in{ev, n};
    const int64_t cnt0 = in.I("count", 0, (int64_t)std::min<size_t>(ev.Ctx().maxElements + 1, 1000000));
    const double len = in.F("length"), angDeg = in.F("angle");
    if (!in.ok) return false;
    const double ang = angDeg * kDeg;
    const size_t cnt = ev.Cap((size_t)cnt0);
    Geometry g = MakePoints(cnt);
    Attr(g, "t", Type::Float), Attr(g, "normal", Type::Vector);
    Column &pos = g.attrs["position"], &t = g.attrs["t"], &nrm = g.attrs["normal"];
    for (size_t i = 0; i < cnt; i++) {
        const double tt = cnt0 <= 1 ? 0.5 : (double)i / (double)(cnt0 - 1);
        const double u = cnt0 <= 1 ? 0 : tt - 0.5;
        g.id[i] = ev.ElementId(n, i, "pts");
        pos.v[i] = {u * len * std::cos(ang), u * len * std::sin(ang), 0};
        t.f[i] = tt;
        nrm.v[i] = {-std::sin(ang), std::cos(ang), 0};
    }
    *out = std::move(g);
    return true;
}

bool TransformG(Evaluator& ev, const Node& n, Geometry* out)
{
    In in{ev, n};
    const Vec3 P = in.V("position");
    const Rot R = RotXYZ(in.F("rotation_x"), in.F("rotation_y"), in.F("rotation_z"));
    const double s = in.F("scale") / 100.0;
    const bool center = in.I("pivot", 0, 1) == 0;
    if (!in.ok) return false;
    Geometry g = *ev.InputGeo(n, "geometry");
    Column &pos = g.attrs["position"], &rot = g.attrs["rotation"];
    Vec3 piv{0, 0, 0};
    if (center && g.count) {
        Vec3 lo = pos.v[0], hi = pos.v[0];
        for (size_t i = 0; i < g.count; i++) {
            const Vec3 p = pos.v[i];
            lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
            hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
        }
        piv = (lo + hi) * 0.5;
    }
    for (size_t i = 0; i < g.count; i++) {
        const Vec3 d = R * ((pos.v[i] - piv) * s);
        pos.v[i] = P + d;
        rot.r[i] = R * rot.r[i];
    }
    *out = std::move(g);
    return true;
}

bool ModifyG(Evaluator& ev, const Node& n, Geometry* out)
{
    In in{ev, n};
    const Vec3 dp = in.V("position");
    const double dr[3] = {in.F("rotation_x"), in.F("rotation_y"), in.F("rotation_z")};
    const double ds = in.F("scale") / 100.0, dop = in.F("opacity") / 100.0;
    if (!in.ok) return false;
    Geometry g = *ev.InputGeo(n, "geometry");
    Column &pos = g.attrs["position"], &rot = g.attrs["rotation"], &scl = g.attrs["scale"], &op = g.attrs["opacity"], &wt = g.attrs["weight"];
    for (size_t i = 0; i < g.count; i++) {
        const double w = wt.f[i];
        pos.v[i] = pos.v[i] + dp * w;
        if (dr[0] != 0 || dr[1] != 0 || dr[2] != 0) rot.r[i] = rot.r[i] * RotXYZ(dr[0] * w, dr[1] * w, dr[2] * w);
        const double ks = std::max(0.0, 1 + ds * w);
        scl.v[i] = scl.v[i] * ks;
        op.f[i] = std::clamp(op.f[i] * std::max(0.0, 1 + dop * w), 0.0, 1.0);
    }
    *out = std::move(g);
    return true;
}

bool SourceG(Evaluator& ev, const Node& n, Geometry* out)
{
    In in{ev, n};
    const int64_t first = in.I("first", 1, kNumLayers), count = in.I("layers", 1, kNumLayers);
    const bool random = in.I("assign", 0, 1) == 1;
    Geometry g = *ev.InputGeo(n, "geometry");
    std::vector<int> slots;
    for (int64_t i = 0; i < count; i++) {
        const int64_t k = first - 1 + i;
        if (k < kNumLayers && (ev.Ctx().layersSet >> k & 1)) slots.push_back((int)k);
    }
    const size_t m = slots.size();
    Column& src = g.attrs["source"];
    for (size_t i = 0; i < g.count; i++) {
        if (!m) {
            src.i[i] = -1;
            continue;
        }
        const size_t j = random ? (size_t)(dxg::Rand01(g.id[i], ev.Ctx().seed, 0x5352u) * (double)m) % m : i % m;
        src.i[i] = slots[j];
    }
    *out = std::move(g);
    return true;
}

}  // namespace

const std::string* GroupSource(const std::string& name)
{
    auto it = Sources().find(name);
    return it == Sources().end() ? nullptr : &it->second;
}

bool NativeGroup(Evaluator& ev, const Node& n, Geometry* out)
{
    const std::string& t = n.type;
    if (t == "grid") return Grid(ev, n, out);
    if (t == "circle") return Circle(ev, n, out);
    if (t == "line") return Line(ev, n, out);
    if (t == "transform") return TransformG(ev, n, out);
    if (t == "modify") return ModifyG(ev, n, out);
    if (t == "source_layers") return SourceG(ev, n, out);
    return false;
}

bool GroupByDefinition(Evaluator& ev, const Node& n, Geometry* out)
{
    const Graph* tmpl = Template(n.type);
    if (!tmpl || !tmpl->OutputNode()) return false;
    Evaluator sub(*tmpl, ev.Ctx(), ev.Result());
    sub.BindGroup(&ev, &n);
    *out = *sub.InputGeo(*tmpl->OutputNode(), "geometry");
    return true;
}

// groups inside groups: a limit on the depth (a graph whose groups call round in a ring stops with a warning)
struct Depth {
    static int& N()
    {
        thread_local int n = 0;
        return n;
    }
    bool ok;
    Depth() : ok(++N() <= 64) {}
    ~Depth() { --N(); }
};

bool UserGroupGeo(Evaluator& ev, const Node& n, int output, Geometry* out)
{
    Depth depth;
    if (!depth.ok) {
        ev.Warn(n.key + ": groups call each other too deep");
        return false;
    }
    const UserGroup* u = ev.UserGroupOf(n.type);
    if (!u || !u->body || output < 0 || output >= (int)u->def.outputs.size()) return false;
    const Node* ret = nullptr;
    for (const Node& x : u->body->nodes)
        if (x.type == "group_output") ret = &x;
    if (!ret) return false;
    Evaluator sub(*u->body, ev.Ctx(), ev.Result());
    sub.BindGroup(&ev, &n);
    *out = *sub.InputGeo(*ret, u->def.outputs[output].name);
    return true;
}

bool UserGroupValue(Evaluator& ev, const Node& n, int output, const GeoPtr& ctx, Column* out)
{
    Depth depth;
    if (!depth.ok) {
        ev.Warn(n.key + ": groups call each other too deep");
        return false;
    }
    const UserGroup* u = ev.UserGroupOf(n.type);
    if (!u || !u->body || output < 0 || output >= (int)u->def.outputs.size()) return false;
    const Node* ret = nullptr;
    for (const Node& x : u->body->nodes)
        if (x.type == "group_output") ret = &x;
    if (!ret) return false;
    Evaluator sub(*u->body, ev.Ctx(), ev.Result());
    sub.BindGroup(&ev, &n);
    *out = sub.Input(*ret, u->def.outputs[output].name, ctx);
    return true;
}

namespace {

InputDef F(const char* n, double d, double lo, double hi, const char* ctx)
{
    InputDef i;
    i.name = n, i.type = Type::Float, i.dflt = Value::F(d), i.softMin = lo, i.softMax = hi, i.field = true, i.context = ctx;
    return i;
}
InputDef I(const char* n, int64_t d, double lo, double hi, const char* choices = "")
{
    InputDef i;
    i.name = n, i.type = Type::Int, i.dflt = Value::I(d), i.softMin = lo, i.softMax = hi, i.structural = true;
    i.choices = choices;
    return i;
}
InputDef V(const char* n, const char* ctx)
{
    InputDef i;
    i.name = n, i.type = Type::Vector, i.dflt = Value::V({0, 0, 0}), i.softMin = -1000, i.softMax = 1000, i.field = true;
    i.context = ctx;
    return i;
}
InputDef G()
{
    InputDef i;
    i.name = "geometry", i.type = Type::Geometry;
    return i;
}
NodeDef Group(const char* name, const char* label, const char* cat, std::vector<InputDef> in)
{
    NodeDef d;
    d.name = name, d.label = label, d.category = cat, d.inputs = std::move(in), d.group = true;
    d.outputs = {{"geometry", Type::Geometry, false}};
    return d;
}

}  // namespace

std::vector<NodeDef> GroupDefs()
{
    std::vector<NodeDef> d;
    d.push_back(Group("grid", "Grid", "Generate",
                      {I("cx", 5, 1, 50), I("cy", 5, 1, 50), I("cz", 1, 1, 20), F("sx", 100, 0, 400, "make"),
                       F("sy", 100, 0, 400, "make"), F("sz", 100, 0, 400, "make")}));
    d.push_back(Group("circle", "Circle", "Generate",
                      {I("count", 12, 1, 100), F("radius", 300, 0, 1000, "make"), F("start_angle", 0, -180, 180, "make"),
                       F("sweep", 360, 0, 360, "make"), I("align", 0, 0, 1, "Off|Outward")}));
    d.push_back(Group("line", "Line", "Generate",
                      {I("count", 10, 1, 100), F("length", 800, 0, 2000, "make"), F("angle", 0, -180, 180, "make")}));
    {
        // Transform moves the layout as a whole: single values (design v3 2.3)
        InputDef p = V("position", "");
        p.field = false;
        InputDef rx = F("rotation_x", 0, -180, 180, ""), ry = F("rotation_y", 0, -180, 180, ""), rz = F("rotation_z", 0, -180, 180, "");
        InputDef sc = F("scale", 100, 0, 200, "");
        for (InputDef* x : {&rx, &ry, &rz, &sc}) x->field = false;
        d.push_back(Group("transform", "Transform", "Set", {G(), p, rx, ry, rz, sc, I("pivot", 0, 0, 1, "Center|Origin")}));
    }
    d.push_back(Group("modify", "Modify", "Set",
                      {G(), V("position", "geometry"), F("rotation_x", 0, -180, 180, "geometry"),
                       F("rotation_y", 0, -180, 180, "geometry"), F("rotation_z", 0, -180, 180, "geometry"),
                       F("scale", 0, -100, 200, "geometry"), F("opacity", 0, -100, 100, "geometry")}));
    d.push_back(Group("source_layers", "Source", "Set",
                      {G(), I("first", 1, 1, 8), I("layers", 1, 1, 8), I("assign", 0, 0, 1, "Sequential|Random")}));
    return d;
}

}  // namespace dx5

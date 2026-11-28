// DupliX 2 (design v5, 15): the graph editor's window (Win32, GDI, drawn by itself). Every change goes through
// dxg5/edit.h and ends in ed::Commit (one AE undo step); while a value is dragged only the window changes, and the
// release writes once (design v4 7).
//
// A node: its title, its outputs (a socket on the right each), its inputs (a socket on the left each, a name, the
// value when nothing is wired). Geometry wires are green, value wires grey (dashed when they carry a field).
//   right-click empty space      add a node
//   drag a node's title          move it            Delete: remove the selected node
//   drag from an output          to an input to wire (it snaps to the nearest input that fits, 24 px); right-click
//                                a wired input to unwire; drag from a wired input to lift its wire off (Blender)
//   click a value                type a number (a formula of numbers is worked out); drag it left / right to change it
//   right-click a value          Expose to a macro, link to / unlink a macro, Connect Expression, reset
//   click an Expression's text   edit its formula (lines x = ..., Ctrl+Enter applies; inputs and outputs follow it)
//   click a math node's operator choose another (its unused inputs hide, their wires stay)
//   right-click a node           Open, Copy for Myself, Ungroup, Make Group, Expand / Collapse a formula, Group
//   Tab / Esc                    open the selected group or Expression / back;  Shift+click, Ctrl+G: group them
//   drag a macro (right) onto a value   link it;  double-click a macro: rename it
//   Ctrl+Z / Ctrl+Shift+Z        AE's Undo / Redo
//   middle-drag                  pan;  wheel: zoom about the cursor;  Home: zoom to fit
//   the ▸ of a group's title     open it (as Tab)
#include "v2/dx2_editor.h"

#include <windowsx.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace dx2 {
namespace ed {
namespace {

constexpr int kNodeW = 200, kTitleH = 22, kRowH = 20, kPort = 5, kPanelW = 230, kTabH = 26, kStatusH = 20;
constexpr COLORREF kBg = RGB(30, 30, 32), kGridC = RGB(40, 40, 43), kNodeC = RGB(52, 52, 56), kNodeSel = RGB(70, 70, 78),
                   kTitleGen = RGB(48, 110, 90), kTitleSet = RGB(64, 92, 140), kTitleVal = RGB(90, 78, 120),
                   kTitleOut = RGB(120, 84, 50), kText = RGB(222, 222, 222), kDim = RGB(150, 150, 150),
                   kLinkC = RGB(110, 190, 240), kPanelC = RGB(38, 38, 41), kWarn = RGB(240, 160, 80), kTabC = RGB(44, 44, 48);

COLORREF TypeColor(dx5::Type t)
{
    switch (t) {
    case dx5::Type::Geometry: return RGB(90, 200, 140);
    case dx5::Type::Int: return RGB(60, 140, 90);
    case dx5::Type::Bool: return RGB(200, 120, 160);
    case dx5::Type::Vector: return RGB(120, 110, 220);
    case dx5::Type::Rotation: return RGB(150, 120, 230);
    default: return RGB(160, 160, 160);
    }
}

enum class Hit { None, Canvas, Title, GroupOpen, OutPort, InPort, Value, ExprText, OpRow, Macro, UnusedToggle, Back, Panel };
struct HitInfo {
    Hit what = Hit::None;
    uint32_t node = 0;
    int index = 0;  // the output, the input (row in Node::inputs), or the macro
    bool isInt = false;
};

enum class Drag { None, Move, Wire, Scrub, Pan, MacroLink, Box };
enum class View { Nodes, Stack, Code };
constexpr int kApplyId = 9001;
constexpr UINT_PTR kProblemTimer = 1;
constexpr double kZoomMin = 0.3, kZoomMax = 3, kSnap = 24;

struct MacroRowInfo {
    bool isInt;
    int k;
    bool header = false;   // the "Int Macros" / "Macros" heading
    bool unused = false;   // the "unused" heading (click: fold / unfold), or a row under it (design v5 4.4)
};

// An opened Expression node or group (Tab, design v5 8.3 and 12.1): its inside is edited here and written back to the
// graph it is in on every change (one AE undo step). A built-in group is only shown.
struct Frame {
    bool expr = false, readOnly = false;
    uint32_t node = 0;
    std::string name, key;
    dx5::Graph graph, original;
    double panX = 30, panY = 30, zoom = 1;
};

struct Win {
    Session* s = nullptr;
    HWND wnd = nullptr, edit = nullptr;
    HFONT font = nullptr, bold = nullptr;
    // the node view: screen = world * zoom + pan (+ the tabs); its fonts are made again for each zoom
    double panX = 30, panY = 30, zoom = 1;
    HFONT zFont = nullptr, zBold = nullptr;
    double fontZoom = 0;
    // a wire being dragged: the input it snaps to; a wire lifted off an input (restored if the drag is cut short)
    uint32_t snapNode = 0;
    int snapInput = -1;
    bool lifted = false;
    uint32_t liftNode = 0;
    std::string liftInput;
    dx5::Link liftLink{};
    dx5::Graph liftBefore;
    // a macro dragged from the panel
    bool macroDragInt = false;
    int macroDragK = -1;
    // the add menu's search
    bool editSearch = false, enterPressed = false;
    POINT searchAt{};
    uint32_t selected = 0;
    // a drag
    Drag drag = Drag::None;
    POINT start{}, now{};
    uint32_t dragNode = 0;
    int dragIndex = 0;
    double startX = 0, startY = 0, startValue = 0;
    std::vector<std::pair<uint32_t, POINT>> moveFrom;  // the nodes a title drag moves, where they started (x100)
    bool moved = false;
    // the edit box: what it edits
    uint32_t editNode = 0;
    int editInput = -1, editMacro = -1;
    bool editMacroInt = false, editExprText = false;
    WNDPROC editProc = nullptr;
    std::vector<MacroRowInfo> macroRows;  // the macros listed in the panel, top to bottom
    std::string status;                   // the last refusal (shown at the bottom)
    // the views (design v5 8.4, 14)
    View view = View::Nodes;
    HWND code = nullptr, apply = nullptr, codeMsg = nullptr;
    HFONT mono = nullptr;
    WNDPROC codeProc = nullptr;
    bool codeError = false;
    // opened Expression nodes and groups, outermost first; the nodes picked with Shift (Ctrl+G groups them)
    std::vector<Frame> frames;
    std::vector<uint32_t> multi;
    bool unusedOpen = false;  // the macros panel's "unused" list unfolded
    bool editMultiline = false;
    std::string macroLog;     // the last "macros used / unused" logged
    std::vector<std::string> problems;  // what the last render of this graph found (the Overview shows one line)
} g_win;

// The graph the node view shows: the effect's, or the inside of what is opened.
dx5::Graph& G() { return g_win.frames.empty() ? g_win.s->graph : g_win.frames.back().graph; }

std::wstring W(const std::string& u8)
{
    if (u8.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), -1, nullptr, 0);
    std::wstring w((size_t)std::max(n - 1, 0), L'\0');
    if (n > 1) MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), -1, &w[0], n);
    return w;
}
std::string U8(const std::wstring& w)
{
    if (w.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s((size_t)std::max(n - 1, 0), '\0');
    if (n > 1) WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
}

// a Float always shows it is one: 100.0, not 100 (the user, 11/29)
std::string FloatText(double v)
{
    char b[64];
    snprintf(b, sizeof(b), "%.6g", v);
    std::string s = b;
    if (s.find_first_of(".eEn") == std::string::npos) s += ".0";
    return s;
}

std::string Num(double v, bool integer)
{
    char b[64];
    if (integer) snprintf(b, sizeof(b), "%lld", (long long)std::llround(v));
    else snprintf(b, sizeof(b), "%.6g", v);
    return b;
}

// a node's table entry here: a math node's is its operator's (its inputs a, b, c as the operator uses them), a user's
// group's comes from the graph
const dx5::NodeDef* DefOfNode(const dx5::Node& n)
{
    if (const dx5::MathOpDef* m = dx5::MathOpOf(n)) return dx5::FindDef(m->CodeName());
    return dx5::DefFor(G(), n.type);
}
const dx5::InputDef* InDef(const dx5::Node& n, const std::string& name)
{
    const dx5::NodeDef* d = DefOfNode(n);
    const int k = d ? d->Input(name) : -1;
    return k >= 0 ? &d->inputs[k] : nullptr;
}
int NumOutputs(const dx5::Node& n) { return (int)dx5::OutputsOf(G(), n).size(); }
bool IsMacroNode(const dx5::Node& n) { return n.type == "macro" || n.type == "int_macro"; }
// the rows under the outputs before the inputs: an Expression's formula (a row a line), a math node's operator
int TextRows(const dx5::Node& n)
{
    if (dx5::IsMathFamily(n.type)) return 1;
    if (n.type != "expr") return 0;
    return std::max(1, (int)std::count(n.text.begin(), n.text.end(), '\n') + 1);
}
bool HasTextRow(const dx5::Node& n) { return n.type == "expr"; }
// inputs drawn: a math node hides those its operator does not use (design v5 6.2)
bool Shown(const dx5::Node& n, int k) { return dx5::InputVisible(G(), n, k); }
int ShownIndex(const dx5::Node& n, int k)
{
    int r = 0;
    for (int i = 0; i < k; i++) r += Shown(n, i);
    return r;
}
int ShownCount(const dx5::Node& n) { return ShownIndex(n, (int)n.inputs.size()); }

// ---- layout ----
RECT CanvasRect()
{
    RECT c;
    GetClientRect(g_win.wnd, &c);
    c.top = kTabH;
    c.right = std::max(c.left, c.right - kPanelW);
    c.bottom = std::max(c.top, c.bottom - kStatusH);
    return c;
}
int FirstInputRow(const dx5::Node& n) { return NumOutputs(n) + TextRows(n); }
// a length of the node view on the screen
int Z(double v) { return (int)std::lround(v * g_win.zoom); }
int WorldH(const dx5::Node& n) { return kTitleH + (FirstInputRow(n) + ShownCount(n)) * kRowH + 4; }
RECT NodeRect(const dx5::Node& n)
{
    const int x = (int)std::lround(n.x * g_win.zoom + g_win.panX), y = (int)std::lround(n.y * g_win.zoom + g_win.panY) + kTabH;
    return {x, y, x + Z(kNodeW), y + Z(WorldH(n))};
}
int RowTop(const dx5::Node& n, int row) { return NodeRect(n).top + Z(kTitleH + row * kRowH); }
int RowMid(const dx5::Node& n, int row) { return NodeRect(n).top + Z(kTitleH + row * kRowH + kRowH / 2); }
POINT OutPort(const dx5::Node& n, int out) { return {NodeRect(n).right, RowMid(n, out)}; }
POINT InPort(const dx5::Node& n, int in) { return {NodeRect(n).left, RowMid(n, FirstInputRow(n) + ShownIndex(n, in))}; }
RECT ValueRect(const dx5::Node& n, int in)
{
    const RECT r = NodeRect(n);
    const int y = RowTop(n, FirstInputRow(n) + ShownIndex(n, in));
    return {r.left + Z(kNodeW / 2), y + 1, r.right - Z(6), y + Z(kRowH) - 1};
}
RECT TextRect(const dx5::Node& n)
{
    const RECT r = NodeRect(n);
    const int y = RowTop(n, NumOutputs(n));
    return {r.left + Z(8), y + 1, r.right - Z(8), y + Z(kRowH) * TextRows(n) - 1};
}
// the "‹ Back" bar over the canvas while something is opened
RECT BackRect()
{
    const RECT c = CanvasRect();
    return {c.left, c.top, c.left + 90, c.top + 24};
}
// the ▸ at the right of a group's title (design v5 6.1: opens its code)
RECT GroupOpenRect(const dx5::Node& n)
{
    const RECT r = NodeRect(n);
    return {r.right - Z(22), r.top, r.right, r.top + Z(kTitleH)};
}
bool IsGroup(const dx5::Node& n)
{
    const dx5::NodeDef* d = dx5::DefFor(G(), n.type);
    return d && d->group;
}
// what Tab opens: a group (built-in or the user's) or an Expression node
bool Openable(const dx5::Node& n) { return IsGroup(n) || n.type == "expr"; }
int PortR() { return std::max(2, Z(kPort)); }

void ZoomFonts()
{
    if (g_win.fontZoom == g_win.zoom && g_win.zFont) return;
    if (g_win.zFont) DeleteObject(g_win.zFont);
    if (g_win.zBold) DeleteObject(g_win.zBold);
    const int h = -std::max(4, Z(13));
    g_win.zFont = CreateFontW(h, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_win.zBold = CreateFontW(h, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_win.fontZoom = g_win.zoom;
}
RECT MacroRow(int row)
{
    const RECT c = CanvasRect();
    RECT r;
    GetClientRect(g_win.wnd, &r);
    const int y = kTabH + 34 + row * 22;
    return {c.right + 8, y, r.right - 8, y + 20};
}

bool In(const RECT& r, POINT p) { return p.x >= r.left && p.x < r.right && p.y >= r.top && p.y < r.bottom; }
bool Near(POINT a, POINT b, int d) { return std::abs(a.x - b.x) <= d && std::abs(a.y - b.y) <= d; }

HitInfo HitTest(POINT p)
{
    Session* s = g_win.s;
    HitInfo h;
    if (!s) return h;
    const RECT c = CanvasRect();
    if (!In(c, p)) {
        for (size_t row = 0; row < g_win.macroRows.size(); row++) {
            const MacroRowInfo& mr = g_win.macroRows[row];
            if (!In(MacroRow((int)row), p)) continue;
            if (mr.header && mr.unused) return {Hit::UnusedToggle, 0, 0};
            if (mr.header) continue;
            HitInfo m{Hit::Macro, 0, mr.k};
            m.isInt = mr.isInt;
            return m;
        }
        return {Hit::Panel, 0, 0};
    }
    if (!g_win.frames.empty() && In(BackRect(), p)) return {Hit::Back, 0, 0};
    dx5::Graph& g = G();
    for (auto it = g.nodes.rbegin(); it != g.nodes.rend(); ++it) {
        const dx5::Node& n = *it;
        for (int k = 0; k < NumOutputs(n); k++)
            if (Near(OutPort(n, k), p, PortR() + 3)) return {Hit::OutPort, n.id, k};
        for (int k = 0; k < (int)n.inputs.size(); k++)
            if (Shown(n, k) && Near(InPort(n, k), p, PortR() + 3)) return {Hit::InPort, n.id, k};
        const RECT r = NodeRect(n);
        if (!In(r, p)) continue;
        if (Openable(n) && In(GroupOpenRect(n), p)) return {Hit::GroupOpen, n.id, 0};
        if (p.y < r.top + Z(kTitleH)) return {Hit::Title, n.id, 0};
        if (TextRows(n) && In(TextRect(n), p)) return {dx5::IsMathFamily(n.type) ? Hit::OpRow : Hit::ExprText, n.id, 0};
        for (int i = 0; i < (int)n.inputs.size(); i++)
            if (Shown(n, i) && In(ValueRect(n, i), p)) return {Hit::Value, n.id, i};
        return {Hit::Title, n.id, 0};
    }
    return {Hit::Canvas, 0, 0};
}

// ---- what a value shows ----
std::string ConstText(const dx5::Node& n, const dx5::InputSlot& s)
{
    const dx5::InputDef* d = InDef(n, s.name);
    const dx5::Value& v = s.value;
    if (d && !d->choices.empty()) {
        const std::string& c = d->choices;
        int idx = (int)dx5::AsInt(v), i = 0;
        size_t a = 0;
        while (i < idx && a != std::string::npos) {
            a = c.find('|', a);
            if (a != std::string::npos) a++;
            i++;
        }
        if (a == std::string::npos) return Num((double)idx, true);
        const size_t b = c.find('|', a);
        return c.substr(a, b == std::string::npos ? std::string::npos : b - a);
    }
    switch (v.type) {
    case dx5::Type::Int: return Num((double)v.i, true);
    case dx5::Type::Bool: return v.i ? "true" : "false";
    case dx5::Type::Vector: return FloatText(v.v.x) + ", " + FloatText(v.v.y) + ", " + FloatText(v.v.z);
    case dx5::Type::Rotation: return "(rotation)";
    default: return FloatText(v.f);
    }
}

std::string LinkText(const dx5::Graph& g, const dx5::InputSlot& s)
{
    const dx5::Node* f = g.Find(s.link.from);
    if (!f) return "?";
    if (f->type == "macro") return dx5::MacroLabel(g, false, f->setting - 1);
    if (f->type == "int_macro") return dx5::MacroLabel(g, true, f->setting - 1);
    if (f->type == "expr") return dx5::ExprText(g, f->id, s.link.output);
    return "<- " + f->key;
}

// ---- drawing ----
void Text(HDC dc, const std::string& s, RECT r, COLORREF c, UINT fmt)
{
    SetTextColor(dc, c);
    const std::wstring w = W(s);
    DrawTextW(dc, w.c_str(), (int)w.size(), &r, fmt | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
}

void FillRect(HDC dc, const RECT& r, COLORREF c)
{
    HBRUSH b = CreateSolidBrush(c);
    ::FillRect(dc, &r, b);
    DeleteObject(b);
}

void Wire(HDC dc, POINT a, POINT b, COLORREF c, bool dashed)
{
    LOGBRUSH lb{BS_SOLID, c, 0};
    HPEN pen = dashed ? ExtCreatePen(PS_GEOMETRIC | PS_DASH, 2, &lb, 0, nullptr) : CreatePen(PS_SOLID, 2, c);
    HGDIOBJ old = SelectObject(dc, pen);
    const int dx = std::max(Z(40), (int)std::abs(b.x - a.x) / 2);
    POINT pts[4] = {a, {a.x + dx, a.y}, {b.x - dx, b.y}, b};
    PolyBezier(dc, pts, 4);
    SelectObject(dc, old);
    DeleteObject(pen);
}

void Port(HDC dc, POINT p, COLORREF c, bool filled)
{
    HBRUSH b = CreateSolidBrush(filled ? c : kNodeC);
    HPEN pen = CreatePen(PS_SOLID, 1, c);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, pen);
    const int k = PortR();
    Ellipse(dc, p.x - k, p.y - k, p.x + k + 1, p.y + k + 1);
    SelectObject(dc, ob), SelectObject(dc, op);
    DeleteObject(b), DeleteObject(pen);
}

COLORREF TitleColor(const dx5::Node& n)
{
    const dx5::NodeDef* d = dx5::DefFor(G(), n.type);
    if (!d) return kDim;
    if (d->category == "Output" || n.type == "group_output") return kTitleOut;
    if (d->category == "Generate") return kTitleGen;
    if (d->category == "Set" || d->category == "Geometry" || d->category == "Zone") return kTitleSet;
    if (d->category == "Group") return !d->outputs.empty() && d->outputs[0].type == dx5::Type::Geometry ? kTitleSet : kTitleVal;
    return kTitleVal;
}

dx5::Type OutType(const dx5::Graph& g, uint32_t node, int out)
{
    const dx5::Node* n = g.Find(node);
    if (!n) return dx5::Type::Float;
    const dx5::Type t = dx5::OutputType(g, *n, out);
    return t == dx5::Type::Any ? dx5::Type::Float : t;
}

// The input nearest to p (within kSnap px on the screen) that the wire being dragged can feed (Connect would take it).
bool FindSnap(POINT p, uint32_t* node, int* input)
{
    Session* s = g_win.s;
    if (!s) return false;
    double best = kSnap * kSnap + 1;
    for (const dx5::Node& n : G().nodes)
        for (int k = 0; k < (int)n.inputs.size(); k++) {
            if (!Shown(n, k)) continue;
            const POINT q = InPort(n, k);
            const double d2 = double(q.x - p.x) * (q.x - p.x) + double(q.y - p.y) * (q.y - p.y);
            if (d2 > kSnap * kSnap || d2 >= best) continue;
            dx5::Graph probe = G();
            if (!dx5::Connect(probe, g_win.dragNode, g_win.dragIndex, n.id, n.inputs[k].name, nullptr)) continue;
            best = d2, *node = n.id, *input = k;
        }
    return best <= kSnap * kSnap;
}

RECT TabRect(View v)
{
    const int i = (int)v;
    return {8 + i * 80, 0, 8 + i * 80 + 72, kTabH};
}

void PaintTabs(HDC dc, const RECT& client)
{
    RECT tabs = {client.left, client.top, client.right, client.top + kTabH};
    FillRect(dc, tabs, kTabC);
    const char* names[3] = {"Nodes", "Stack", "Code"};
    for (int i = 0; i < 3; i++) {
        RECT t = TabRect((View)i);
        const bool on = (int)g_win.view == i;
        if (on) FillRect(dc, {t.left, t.top + 3, t.right, t.bottom}, kBg);
        SelectObject(dc, on ? g_win.bold : g_win.font);
        Text(dc, names[i], t, on ? kText : kDim, DT_CENTER);
    }
    SelectObject(dc, g_win.font);
}

void PaintNodes(HDC dc, const RECT& client);
void PaintPanel(HDC dc, const RECT& client);

void Paint(HDC dc, const RECT& client)
{
    FillRect(dc, client, kBg);
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, g_win.font);
    if (g_win.view == View::Nodes) PaintNodes(dc, client);
    else if (g_win.view == View::Stack) {
        const RECT c = CanvasRect();
        RECT m = {c.left + 16, c.top + 16, c.right - 16, c.top + 40};
        Text(dc, "The stack view comes with the next build.", m, kDim, DT_LEFT);
    }
    PaintPanel(dc, client);
    PaintTabs(dc, client);
}

void PaintNodes(HDC dc, const RECT& client)
{
    Session* s = g_win.s;
    const RECT c = CanvasRect();
    // a grid that moves with the view
    HPEN gp = CreatePen(PS_SOLID, 1, kGridC);
    HGDIOBJ old = SelectObject(dc, gp);
    const double step = 40 * g_win.zoom;
    const double gx = std::fmod(g_win.panX, step), gy = std::fmod(g_win.panY, step);
    for (double x = gx < 0 ? gx + step : gx; x < c.right; x += step)
        MoveToEx(dc, (int)x, c.top, nullptr), LineTo(dc, (int)x, c.bottom);
    for (double y = c.top + (gy < 0 ? gy + step : gy); y < c.bottom; y += step)
        MoveToEx(dc, 0, (int)y, nullptr), LineTo(dc, c.right, (int)y);
    SelectObject(dc, old);
    DeleteObject(gp);
    SetBkMode(dc, TRANSPARENT);
    ZoomFonts();
    SelectObject(dc, g_win.zFont);
    if (!s) return;
    const dx5::Graph& g = G();
    // Each zones: a frame around the nodes inside (design v5 9)
    for (const dx5::Node& z : g.nodes) {
        if (z.type != "each") continue;
        RECT f = NodeRect(z);
        for (const dx5::Node& n : g.nodes)
            if (n.zone == z.id) {
                const RECT r = NodeRect(n);
                f.left = std::min(f.left, r.left), f.top = std::min(f.top, r.top);
                f.right = std::max(f.right, r.right), f.bottom = std::max(f.bottom, r.bottom);
            }
        InflateRect(&f, Z(14), Z(14));
        HPEN zp = CreatePen(PS_SOLID, 1, RGB(110, 100, 150));
        HGDIOBJ op = SelectObject(dc, zp), ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
        RoundRect(dc, f.left, f.top, f.right, f.bottom, Z(12), Z(12));
        SelectObject(dc, op), SelectObject(dc, ob);
        DeleteObject(zp);
    }
    for (const dx5::Node& n : g.nodes)
        for (int k = 0; k < (int)n.inputs.size(); k++) {
            const dx5::InputSlot& in = n.inputs[k];
            const dx5::Node* a = in.link.from ? g.Find(in.link.from) : nullptr;
            if (!a || !Shown(n, k)) continue;  // a hidden input keeps its wire unseen (design v5 6.2)
            const dx5::Type t = OutType(g, a->id, in.link.output);
            // a wire whose type does not fit the operator is kept, red, and not read
            const COLORREF col = dx5::LinkActive(g, n, k) ? TypeColor(t) : RGB(200, 60, 60);
            Wire(dc, OutPort(*a, in.link.output), InPort(n, k), col, t != dx5::Type::Geometry && dx5::IsField(g, a->id, in.link.output));
        }
    if (g_win.drag == Drag::Wire) {
        const dx5::Node* a = g.Find(g_win.dragNode);
        const dx5::Node* b = g_win.snapNode ? g.Find(g_win.snapNode) : nullptr;
        const POINT end = b && g_win.snapInput < (int)b->inputs.size() ? InPort(*b, g_win.snapInput) : g_win.now;
        if (a) Wire(dc, OutPort(*a, g_win.dragIndex), end, b ? TypeColor(OutType(g, a->id, g_win.dragIndex)) : kLinkC, false);
    }
    for (const dx5::Node& n : g.nodes) {
        const dx5::NodeDef* d = dx5::DefFor(g, n.type);
        const RECT r = NodeRect(n);
        if (r.right < c.left || r.left > c.right || r.bottom < c.top || r.top > c.bottom) continue;
        const bool picked = n.id == g_win.selected || std::find(g_win.multi.begin(), g_win.multi.end(), n.id) != g_win.multi.end();
        FillRect(dc, r, picked ? kNodeSel : kNodeC);
        RECT t = {r.left, r.top, r.right, r.top + Z(kTitleH)};
        FillRect(dc, t, TitleColor(n));
        SelectObject(dc, g_win.zBold);
        t.left += Z(10);
        if (Openable(n)) {
            t.right -= Z(22);
            Text(dc, "\xE2\x96\xB8", GroupOpenRect(n), kText, DT_CENTER);  // ▸ (Tab)
        }
        std::string title = d ? d->label : n.type + " (unknown)";
        if (IsMacroNode(n)) title = dx5::MacroLabel(g, n.type == "int_macro", n.setting - 1);
        if (n.type == "param" || n.type == "param_geo") title = "Input: " + n.text;
        if (n.type == "group_output") title = "Group Output";
        Text(dc, title, t, kText, DT_LEFT);
        SelectObject(dc, g_win.zFont);
        const std::vector<dx5::OutputDef> outs = dx5::OutputsOf(g, n);
        for (int k = 0; k < (int)outs.size(); k++) {
            RECT orow = {r.left + Z(10), RowTop(n, k), r.right - Z(12), RowTop(n, k) + Z(kRowH)};
            Text(dc, outs[k].name, orow, kDim, DT_RIGHT);
            Port(dc, OutPort(n, k), TypeColor(OutType(g, n.id, k)), false);
        }
        if (HasTextRow(n)) {
            // the formula, a row a line (click: edit it; Ctrl+Enter or leaving the box applies it)
            const RECT tr = TextRect(n);
            FillRect(dc, tr, RGB(40, 40, 44));
            size_t a = 0;
            for (int line = 0; line < TextRows(n); line++) {
                size_t b = n.text.find('\n', a);
                if (b == std::string::npos) b = n.text.size();
                RECT lr = {tr.left + Z(5), tr.top + Z(kRowH) * line, tr.right - Z(5), tr.top + Z(kRowH) * (line + 1)};
                Text(dc, n.text.substr(a, b - a), lr, kLinkC, DT_LEFT);
                a = b + 1;
            }
        } else if (dx5::IsMathFamily(n.type)) {
            // the operator in the values' column, its label in the names' (the rows line up: the user, 11/29)
            const RECT tr = TextRect(n);
            RECT lab = {r.left + Z(10), tr.top, r.left + Z(kNodeW / 2 - 4), tr.bottom};
            Text(dc, "op", lab, kDim, DT_LEFT);
            RECT box = {r.left + Z(kNodeW / 2), tr.top, r.right - Z(6), tr.bottom};
            FillRect(dc, box, RGB(40, 40, 44));
            box.left += Z(5), box.right -= Z(5);
            Text(dc, n.text, box, kText, DT_LEFT);
            Text(dc, "\xE2\x96\xBE", box, kDim, DT_RIGHT);  // ▾
        }
        for (int i = 0; i < (int)n.inputs.size(); i++) {
            if (!Shown(n, i)) continue;
            const dx5::InputSlot& in = n.inputs[i];
            const dx5::InputDef* id = InDef(n, in.name);
            RECT lr = {r.left + Z(10), RowTop(n, FirstInputRow(n) + ShownIndex(n, i)), r.left + Z(kNodeW / 2 - 4), 0};
            lr.bottom = lr.top + Z(kRowH);
            // a math node's input says what its operator takes (rot_apply: a is a Rotation, b a Vector)
            const dx5::Type mt = dx5::MathInputType(g, n, i);
            Text(dc, mt == dx5::Type::Any ? in.name : in.name + "  " + dx5::TypeName(mt), lr, kDim, DT_LEFT);
            if (!id || id->type != dx5::Type::Geometry) {
                RECT vr = ValueRect(n, i);
                if (!in.link.from) {
                    FillRect(dc, vr, RGB(40, 40, 44));
                    vr.left += Z(5), vr.right -= Z(5);
                    Text(dc, ConstText(n, in), vr, kText, DT_RIGHT);
                } else {
                    // a wired input has no value field; a formula or a macro shows its name, faint
                    const dx5::Node* f = g.Find(in.link.from);
                    vr.right -= Z(5);
                    if (f && (f->type == "expr" || IsMacroNode(*f))) Text(dc, LinkText(g, in), vr, RGB(120, 150, 175), DT_RIGHT);
                }
            }
            Port(dc, InPort(n, i), TypeColor(id ? id->type : dx5::Type::Float), in.link.from != 0);
        }
    }
    if (g_win.drag == Drag::Box && g_win.moved) {
        HPEN bp = CreatePen(PS_DOT, 1, RGB(170, 190, 230));
        HGDIOBJ op = SelectObject(dc, bp), ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
        Rectangle(dc, std::min(g_win.start.x, g_win.now.x), std::min(g_win.start.y, g_win.now.y), std::max(g_win.start.x, g_win.now.x),
                  std::max(g_win.start.y, g_win.now.y));
        SelectObject(dc, op), SelectObject(dc, ob);
        DeleteObject(bp);
    }
    if (g_win.drag == Drag::MacroLink && g_win.moved && g_win.macroDragK >= 0) {
        // the macro being dragged, under the cursor
        const std::string chip = (g_win.macroDragInt ? "I" : "M") + std::to_string(g_win.macroDragK + 1) + "  " +
                                 dx5::MacroLabel(g, g_win.macroDragInt, g_win.macroDragK);
        RECT cr = {g_win.now.x + 12, g_win.now.y - 10, g_win.now.x + 172, g_win.now.y + 10};
        FillRect(dc, cr, RGB(60, 80, 100));
        SelectObject(dc, g_win.font);
        cr.left += 6;
        Text(dc, chip, cr, kText, DT_LEFT);
    }
    SelectObject(dc, g_win.font);
    if (!g_win.frames.empty()) {
        // what is opened: ‹ Back, its name (Tab also goes back)
        const Frame& f = g_win.frames.back();
        RECT bar = {c.left, c.top, c.right, c.top + 24};
        FillRect(dc, bar, RGB(50, 50, 58));
        Text(dc, "\xE2\x80\xB9 Back", BackRect(), kText, DT_CENTER);  // ‹
        RECT nm = {BackRect().right + 8, c.top, c.right - 8, c.top + 24};
        Text(dc, (f.expr ? "Expression " : "Group ") + f.name + (f.readOnly ? "  (read only: right-click it, Copy for Myself)" : ""), nm,
             f.readOnly ? kWarn : kDim, DT_LEFT);
    } else if (!g.OutputNode()) {
        RECT m = {c.left + 10, c.bottom - 28, c.right - 10, c.bottom - 8};
        Text(dc, "No Output node: right-click to add one", m, kWarn, DT_LEFT);
    }

    // what the render found, over the bottom of the canvas (the Effect Controls show the first line only)
    if (!g_win.problems.empty()) {
        const int rows = std::min<int>(4, (int)g_win.problems.size());
        RECT pr = {c.left + 8, c.bottom - 10 - rows * 18, c.right - 8, c.bottom - 6};
        FillRect(dc, pr, RGB(56, 44, 36));
        for (int k = 0; k < rows; k++) {
            RECT lr = {pr.left + 8, pr.top + 2 + k * 18, pr.right - 8, pr.top + 2 + (k + 1) * 18};
            std::string t = "\xE2\x9A\xA0 " + g_win.problems[k];  // a warning sign
            if (k == rows - 1 && (int)g_win.problems.size() > rows) t += "   (+" + std::to_string(g_win.problems.size() - rows) + " more)";
            Text(dc, t, lr, kWarn, DT_LEFT);
        }
    }
    // the status line
    RECT st = {client.left + 8, client.bottom - kStatusH, client.right - kPanelW, client.bottom};
    FillRect(dc, {client.left, client.bottom - kStatusH, client.right - kPanelW, client.bottom}, kTabC);
    Text(dc, g_win.status, st, kWarn, DT_LEFT);
}

// the macros panel (right; every view)
void PaintPanel(HDC dc, const RECT& client)
{
    Session* s = g_win.s;
    if (!s) return;
    const dx5::Graph& g = s->graph;
    const RECT c = CanvasRect();
    RECT p = {c.right, kTabH, client.right, client.bottom};
    FillRect(dc, p, kPanelC);
    SelectObject(dc, g_win.bold);
    RECT h = {c.right + 10, kTabH + 6, client.right - 8, kTabH + 28};
    Text(dc, "Macros", h, kText, DT_LEFT);
    SelectObject(dc, g_win.font);
    g_win.macroRows.clear();
    const uint32_t usedI = dx5::UsedIntMacros(g), usedM = dx5::UsedMacros(g);
    // the macros in use under their headings, I (Int Macro) and M (Macro); the named ones no node reads folded under
    // "unused" (design v5 4.4: a macro only has a name and whether it is used)
    for (int pass = 0; pass < 2; pass++) {
        const bool isInt = pass == 0;
        const uint32_t used = isInt ? usedI : usedM;
        const size_t head = g_win.macroRows.size();
        for (int k = 0; k < 32; k++)
            if (used >> k & 1) {
                if (g_win.macroRows.size() == head) g_win.macroRows.push_back({isInt, -1, true});
                g_win.macroRows.push_back({isInt, k});
            }
    }
    const std::vector<int> unusedI = dx5::UnusedNamedMacros(g, true), unusedM = dx5::UnusedNamedMacros(g, false);
    if (!unusedI.empty() || !unusedM.empty()) {
        g_win.macroRows.push_back({false, -1, true, true});
        if (g_win.unusedOpen) {
            for (int k : unusedI) g_win.macroRows.push_back({true, k, false, true});
            for (int k : unusedM) g_win.macroRows.push_back({false, k, false, true});
        }
    }
    {
        int nused = 0;
        for (int k = 0; k < 32; k++) nused += (usedI >> k & 1) + (usedM >> k & 1);
        const std::string ml = "used " + std::to_string(nused) + " unused " + std::to_string(unusedI.size() + unusedM.size());
        if (ml != g_win.macroLog) Log("editor: macros %s", ml.c_str()), g_win.macroLog = ml;
    }
    for (size_t row = 0; row < g_win.macroRows.size(); row++) {
        const MacroRowInfo& m = g_win.macroRows[row];
        RECT r = MacroRow((int)row);
        if (m.header) {
            r.left += 2;
            if (m.unused)
                Text(dc, std::string(g_win.unusedOpen ? "\xE2\x96\xBE" : "\xE2\x96\xB8") + "  unused (" +
                             std::to_string(unusedI.size() + unusedM.size()) + ")",
                     r, kDim, DT_LEFT);
            else
                Text(dc, m.isInt ? "I  Int Macros" : "M  Macros", r, kDim, DT_LEFT);
            continue;
        }
        FillRect(dc, r, RGB(46, 46, 50));
        RECT a = {r.left + 6, r.top, r.left + 34, r.bottom};
        Text(dc, (m.isInt ? "I" : "M") + std::to_string(m.k + 1), a, kDim, DT_LEFT);
        RECT b = {r.left + 34, r.top, r.right - 80, r.bottom};
        const bool used = ((m.isInt ? usedI : usedM) >> m.k) & 1;
        Text(dc, dx5::MacroLabel(g, m.isInt, m.k), b, used ? kLinkC : kDim, DT_LEFT);
        const dx5::MacroSetting& ms = m.isInt ? g.intMacros[m.k] : g.macros[m.k];
        RECT rr = {r.right - 80, r.top, r.right - 6, r.bottom};
        Text(dc, Num(ms.sliderMin, false) + ".." + Num(ms.sliderMax, false), rr, kDim, DT_RIGHT);
    }
    // the help below the list, wrapped to the panel and cut at its bottom (it was cut off at the side, v4 1.3)
    const int listBottom = g_win.macroRows.empty() ? kTabH + 34 : MacroRow((int)g_win.macroRows.size() - 1).bottom + 12;
    RECT help = {c.right + 10, std::max(listBottom, (int)client.bottom - 200), client.right - 10, client.bottom - 8};
    SetTextColor(dc, kDim);
    const wchar_t* hint =
        L"Right-click: add a node, a value's or a node's menu.\n"
        L"Click a value to type a number; drag it to change it. Formulas go in an Expression node (Ctrl+Enter applies).\n"
        L"Tab: open a group or an Expression (Tab / Esc: back). Shift+click, Ctrl+G: group.\n"
        L"Drag from an output to an input to wire; drag from a wired input to lift its wire.\n"
        L"Wheel: zoom.  Middle-drag: pan.  Home: fit.\n"
        L"Drag a macro onto a value to link it; double-click it to rename.\n"
        L"Delete: remove the node.  Ctrl+Z: undo.\n"
        L"Keys go on Macro / Int Macro in the Effect Controls.";
    DrawTextW(dc, hint, -1, &help, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX | DT_END_ELLIPSIS);
}

void Redraw() { InvalidateRect(g_win.wnd, nullptr, FALSE); }
void Refused(const std::string& why)
{
    Log("editor: refused: %s", why.c_str());
    g_win.status = why;
    Redraw();
}

// A change in the node view, written as one AE undo step. Inside something opened, the inside is written back into
// the graph it is in (a formula, a group's body), out to the effect's graph.
void Save(const char* what)
{
    Session* s = g_win.s;
    if (!s) return;
    if (g_win.frames.empty()) return Commit(what);
    Frame& top = g_win.frames.back();
    if (top.readOnly) {
        top.graph = top.original;
        return Refused("A built-in group cannot be changed: right-click it, Copy for Myself");
    }
    for (int i = (int)g_win.frames.size() - 1; i >= 0; i--) {
        Frame& f = g_win.frames[i];
        dx5::Graph& parent = i ? g_win.frames[i - 1].graph : s->graph;
        const dx5::Graph parentBefore = parent;
        std::string why;
        const bool ok = f.expr ? dx5::CloseExpression(parent, f.node, f.graph, &why) : dx5::SetGroupBody(parent, f.name, f.graph, &why);
        if (!ok) {
            parent = parentBefore;
            for (Frame& x : g_win.frames) x.graph = x.original;
            return Refused(why.empty() ? "cannot write this back" : why);
        }
    }
    for (Frame& x : g_win.frames) x.original = x.graph;
    Commit(what);
}

void FitAll();

// Tab: opens an Expression node or a group (a built-in one is only shown)
void OpenNode(uint32_t id)
{
    const dx5::Node* n = G().Find(id);
    if (!n || !Openable(*n)) return;
    Frame f;
    f.node = id, f.key = n->key;
    f.panX = g_win.panX, f.panY = g_win.panY, f.zoom = g_win.zoom;
    if (n->type == "expr") {
        if (!dx5::OpenExpression(G(), id, &f.graph)) return Refused("this formula cannot be opened");
        f.expr = true, f.name = n->key;
    } else if (const dx5::UserGroup* u = dx5::FindUserGroup(G(), n->type)) {
        if (!u->body) return;
        f.graph = *u->body, f.name = n->type;
    } else {
        const std::string* src = dx5::GroupSource(n->type);
        std::string nm;
        std::vector<dx5::Diag> errs;
        if (!src || !dx5::GroupToGraph(*src, &f.graph, &nm, &errs)) return Refused("this group cannot be opened");
        f.readOnly = true, f.name = n->type;
    }
    f.original = f.graph;
    if (g_win.edit) SetFocus(g_win.wnd);
    g_win.frames.push_back(f);
    g_win.selected = 0, g_win.multi.clear();
    Log("editor: open %s%s", f.key.c_str(), f.readOnly ? " (read only)" : "");
    FitAll();
    Redraw();
}

void Back()
{
    if (g_win.frames.empty()) return;
    if (g_win.edit) SetFocus(g_win.wnd);
    const Frame f = g_win.frames.back();
    g_win.frames.pop_back();
    g_win.panX = f.panX, g_win.panY = f.panY, g_win.zoom = f.zoom;
    g_win.selected = G().Find(f.node) ? f.node : 0, g_win.multi.clear();
    if (g_win.frames.empty()) Log("editor: back to the graph");
    else Log("editor: back to %s", g_win.frames.back().key.c_str());
    Redraw();
}

// ---- the code view (design v5 8.4) ----
std::wstring ToEdit(const std::string& s)
{
    std::string t;
    for (char c : s) {
        if (c == '\n') t += '\r';
        t += c;
    }
    return W(t);
}

void CodeMessage(const std::string& m, bool error)
{
    g_win.codeError = error;
    if (g_win.codeMsg) SetWindowTextW(g_win.codeMsg, W(m).c_str());
}

void LayoutCode()
{
    if (!g_win.code) return;
    const RECT c = CanvasRect();
    const int barY = c.bottom - 30;
    MoveWindow(g_win.code, c.left + 4, c.top + 4, c.right - c.left - 8, barY - c.top - 8, TRUE);
    MoveWindow(g_win.apply, c.left + 4, barY, 150, 24, TRUE);
    MoveWindow(g_win.codeMsg, c.left + 162, barY + 4, c.right - c.left - 170, 20, TRUE);
}

void FillCode()
{
    if (!g_win.code || !g_win.s) return;
    SetWindowTextW(g_win.code, ToEdit(dx5::GraphToCode(g_win.s->graph)).c_str());
    SendMessageW(g_win.code, EM_SETMODIFY, FALSE, 0);
}

void SelectLine(int line)
{
    const LRESULT a = SendMessageW(g_win.code, EM_LINEINDEX, (WPARAM)std::max(0, line - 1), 0);
    if (a < 0) return;
    const LRESULT len = SendMessageW(g_win.code, EM_LINELENGTH, (WPARAM)a, 0);
    SendMessageW(g_win.code, EM_SETSEL, (WPARAM)a, (LPARAM)(a + len));
    SendMessageW(g_win.code, EM_SCROLLCARET, 0, 0);
    SetFocus(g_win.code);
}

void ApplyCodeNow()
{
    Session* s = g_win.s;
    if (!s || !g_win.code) return;
    const int n = GetWindowTextLengthW(g_win.code);
    std::wstring w((size_t)n + 1, L'\0');
    GetWindowTextW(g_win.code, &w[0], n + 1);
    w.resize((size_t)n);
    std::string code;
    for (char c : U8(w))
        if (c != '\r') code += c;
    std::vector<dx5::Diag> errs, notes;
    if (!dx5::ApplyCode(s->graph, code, &errs, &notes)) {
        const dx5::Diag d = errs.empty() ? dx5::Diag{} : errs[0];
        char b[64];
        snprintf(b, sizeof(b), "line %d col %d: ", d.pos.line, d.pos.col);
        const std::string m = std::string(b) + (errs.empty() ? "cannot read the code" : d.msg);
        Log("editor: code refused: %s", m.c_str());
        CodeMessage(m, true);
        SelectLine(d.pos.line);
        return;
    }
    Commit("apply code");
    std::string m = "Applied.";
    if (!notes.empty()) m = notes[0].msg;
    CodeMessage(m, false);
    FillCode();
}

LRESULT CALLBACK CodeProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_KEYDOWN && w == VK_RETURN && GetKeyState(VK_CONTROL) < 0) {
        ApplyCodeNow();
        return 0;
    }
    if (m == WM_CHAR && w == '\n') return 0;  // Ctrl+Enter's character
    if (m == WM_CHAR && w == '\t') {
        SendMessageW(h, EM_REPLACESEL, TRUE, (LPARAM)L"    ");
        return 0;
    }
    if (m == WM_KEYDOWN && w == 'A' && GetKeyState(VK_CONTROL) < 0) {
        SendMessageW(h, EM_SETSEL, 0, -1);
        return 0;
    }
    if (m == WM_KEYDOWN && w == 'R' && GetKeyState(VK_CONTROL) < 0) {  // take the graph as it is now
        FillCode();
        CodeMessage("", false);
        return 0;
    }
    if (m == WM_CHAR && (w == 1 || w == 18)) return 0;  // Ctrl+A / Ctrl+R: no beep
    return CallWindowProcW(g_win.codeProc, h, m, w, l);
}

void SetView(View v)
{
    if (g_win.edit) SetFocus(g_win.wnd);  // ends a value's edit box
    g_win.view = v;
    if (v != View::Nodes) {
        // the Code and Stack views show the effect's graph: what was opened is closed (its changes are written)
        while (!g_win.frames.empty()) Back();
    }
    if (v == View::Code) {
        if (!g_win.code) {
            g_win.code = CreateWindowExW(0, L"EDIT", L"",
                                         WS_CHILD | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL |
                                             ES_WANTRETURN | ES_NOHIDESEL,
                                         0, 0, 10, 10, g_win.wnd, nullptr, nullptr, nullptr);
            g_win.apply = CreateWindowExW(0, L"BUTTON", L"Apply (Ctrl+Enter)", WS_CHILD | BS_PUSHBUTTON, 0, 0, 10, 10, g_win.wnd,
                                          (HMENU)(INT_PTR)kApplyId, nullptr, nullptr);
            g_win.codeMsg = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | SS_LEFTNOWORDWRAP, 0, 0, 10, 10, g_win.wnd, nullptr, nullptr, nullptr);
            SendMessageW(g_win.code, WM_SETFONT, (WPARAM)g_win.mono, TRUE);
            SendMessageW(g_win.code, EM_SETLIMITTEXT, 0, 0);
            SendMessageW(g_win.apply, WM_SETFONT, (WPARAM)g_win.font, TRUE);
            SendMessageW(g_win.codeMsg, WM_SETFONT, (WPARAM)g_win.font, TRUE);
            g_win.codeProc = (WNDPROC)SetWindowLongPtrW(g_win.code, GWLP_WNDPROC, (LONG_PTR)CodeProc);
        }
        LayoutCode();
        FillCode();
        CodeMessage("", false);
        for (HWND c : {g_win.code, g_win.apply, g_win.codeMsg}) ShowWindow(c, SW_SHOW);
        SetFocus(g_win.code);
    } else {
        for (HWND c : {g_win.code, g_win.apply, g_win.codeMsg})
            if (c) ShowWindow(c, SW_HIDE);
        SetFocus(g_win.wnd);
    }
    Redraw();
}

// ---- the edit box ----
void EndEdit(bool commit);

LRESULT CALLBACK EditProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    // a formula's box takes several lines: Enter is a new line, Ctrl+Enter applies (design v5 8.3)
    const bool lineBreak = g_win.editMultiline && w == VK_RETURN && GetKeyState(VK_CONTROL) >= 0;
    if (m == WM_KEYDOWN && (w == VK_RETURN || w == VK_ESCAPE) && !lineBreak) {
        g_win.enterPressed = w == VK_RETURN;
        EndEdit(w == VK_RETURN);
        return 0;
    }
    if (m == WM_CHAR && (w == VK_ESCAPE || w == '\n' || (w == VK_RETURN && !lineBreak))) return 0;  // no beep
    if (m == WM_KILLFOCUS) {
        LRESULT r = CallWindowProcW(g_win.editProc, h, m, w, l);
        EndEdit(true);
        return r;
    }
    return CallWindowProcW(g_win.editProc, h, m, w, l);
}

void BeginEdit(const RECT& r, const std::string& text, HFONT font = nullptr, bool multiline = false)
{
    if (g_win.edit) EndEdit(false);
    g_win.enterPressed = false;
    g_win.editMultiline = multiline;
    std::string shown;
    for (char c : text) {
        if (c == '\n' && multiline) shown += '\r';
        shown += c;
    }
    const DWORD style = WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | (multiline ? ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL : 0);
    g_win.edit = CreateWindowExW(0, L"EDIT", W(shown).c_str(), style, r.left, r.top, r.right - r.left, r.bottom - r.top, g_win.wnd,
                                 nullptr, nullptr, nullptr);
    SendMessageW(g_win.edit, WM_SETFONT, (WPARAM)(font ? font : g_win.font), TRUE);
    g_win.editProc = (WNDPROC)SetWindowLongPtrW(g_win.edit, GWLP_WNDPROC, (LONG_PTR)EditProc);
    SendMessageW(g_win.edit, EM_SETSEL, 0, -1);
    SetFocus(g_win.edit);
}
void BeginValueEdit(const dx5::Node& n, int input)
{
    g_win.editNode = n.id, g_win.editInput = input, g_win.editMacro = -1, g_win.editExprText = false, g_win.editSearch = false;
    const dx5::InputSlot& s = n.inputs[input];
    std::string text = ConstText(n, s);
    if (s.link.from) {
        const dx5::Node* f = G().Find(s.link.from);
        text = f && f->type == "expr" ? dx5::ExprText(G(), f->id, 0) : LinkText(G(), s);
        if (f && (f->type == "macro" || f->type == "int_macro")) text = std::string(f->type == "macro" ? "m" : "i") + std::to_string(f->setting);
    }
    ZoomFonts();
    BeginEdit(ValueRect(n, input), text, g_win.zFont);
}


void EndEdit(bool commit)
{
    HWND e = g_win.edit;
    if (!e) return;
    g_win.edit = nullptr;  // first: DestroyWindow sends WM_KILLFOCUS again
    const int len = GetWindowTextLengthW(e);
    std::wstring buf((size_t)len + 1, L'\0');
    GetWindowTextW(e, &buf[0], len + 1);
    buf.resize((size_t)len);
    DestroyWindow(e);
    SetFocus(g_win.wnd);
    Session* s = g_win.s;
    if (!commit || !s) return Redraw();
    std::string text;
    for (char c : U8(buf))
        if (c != '\r') text += c;
    if (g_win.editMacro >= 0) {
        if (dx5::RenameMacro(s->graph, g_win.editMacroInt, g_win.editMacro, text)) Commit("rename macro");
    } else if (g_win.editExprText) {
        // the formula applies when it is committed: its inputs and outputs follow it, wires kept by name (8.3)
        dx5::Node* n = G().Find(g_win.editNode);
        if (!n) return Redraw();
        const dx5::Graph before = G();
        std::string why;
        if (!dx5::SetFormula(G(), n->id, text, &why)) return Refused("formula: " + why);
        n = G().Find(g_win.editNode);
        const int ins = (int)n->inputs.size(), outs = (int)dx5::OutputsOf(G(), *n).size();
        if (dx5::Serialize(G()) != dx5::Serialize(before)) {
            Log("editor: formula ok %d in %d out", ins, outs);
            g_win.status.clear();
            Save("formula");
        }
    } else {
        dx5::Node* n = G().Find(g_win.editNode);
        if (n && g_win.editInput >= 0 && g_win.editInput < (int)n->inputs.size()) {
            const std::string key = n->inputs[g_win.editInput].name;
            const dx5::Graph before = G();
            std::string why;
            bool isInt = false;
            int k = -1;
            // "m3" / "i2": link that macro
            if (text.size() >= 2 && (text[0] == 'm' || text[0] == 'i') && std::all_of(text.begin() + 1, text.end(), ::isdigit)) {
                isInt = text[0] == 'i';
                k = std::atoi(text.c_str() + 1) - 1;
            }
            Log("editor: %s.%s = '%s'", n->key.c_str(), key.c_str(), text.c_str());
            if (k >= 0 && k < 32 ? dx5::LinkMacro(G(), n->id, key, isInt, k) : dx5::SetConstText(G(), n->id, key, text, &why)) {
                g_win.status.clear();
                Save("set value");
            } else {
                G() = before;
                Refused(why.empty() ? "cannot set " + key : why);
            }
        }
    }
    Redraw();
}

// ---- menus: the editor's own (dark, one list, typing filters it: the user, 11/29) ----
struct MenuItem {
    std::string text;
    int id = 0;
    bool enabled = true, checked = false, header = false;
    std::string hint;  // dim, at the right (a category, a key)
};

struct MenuState {
    std::vector<MenuItem> items;
    std::vector<int> shown;  // indices of items in view (all, or those matching the filter)
    std::string filter;
    bool searchable = false;
    int hot = -1;            // index into shown
    int top = 0;             // first row of shown in view
    int result = 0;
    bool done = false;
    HWND wnd = nullptr;
};
MenuState* g_menu = nullptr;
constexpr int kMenuRow = 22, kMenuW = 280, kMenuRows = 20;

std::string Lower(std::string s)
{
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

bool Pickable(const MenuItem& it) { return it.enabled && !it.header; }

void MenuFilter(MenuState& m)
{
    m.shown.clear();
    const std::string q = Lower(m.filter);
    for (int i = 0; i < (int)m.items.size(); i++) {
        const MenuItem& it = m.items[i];
        if (!q.empty() && (it.header || (Lower(it.text) + " " + Lower(it.hint)).find(q) == std::string::npos)) continue;
        m.shown.push_back(i);
    }
    m.hot = -1, m.top = 0;
    for (int k = 0; k < (int)m.shown.size(); k++)
        if (Pickable(m.items[m.shown[k]])) {
            m.hot = q.empty() ? -1 : k;  // typing picks the first match (Enter takes it)
            break;
        }
}

int MenuHead(const MenuState& m) { return m.searchable ? kMenuRow : 0; }

void MenuScrollTo(MenuState& m, int k)
{
    if (k < m.top) m.top = k;
    if (k >= m.top + kMenuRows) m.top = k - kMenuRows + 1;
}

void MenuMove(MenuState& m, int dir)
{
    const int n = (int)m.shown.size();
    for (int step = 0, k = m.hot; step < n; step++) {
        k = k < 0 ? (dir > 0 ? 0 : n - 1) : (k + dir + n) % n;
        if (Pickable(m.items[m.shown[k]])) {
            m.hot = k;
            MenuScrollTo(m, k);
            return;
        }
    }
}

void MenuPick(MenuState& m, int k)
{
    if (k < 0 || k >= (int)m.shown.size() || !Pickable(m.items[m.shown[k]])) return;
    m.result = m.items[m.shown[k]].id;
    m.done = true;
}

LRESULT CALLBACK MenuProc(HWND h, UINT msg, WPARAM w, LPARAM l)
{
    MenuState* m = g_menu;
    if (!m) return DefWindowProcW(h, msg, w, l);
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT r;
        GetClientRect(h, &r);
        FillRect(dc, r, RGB(40, 40, 44));
        SetBkMode(dc, TRANSPARENT);
        SelectObject(dc, g_win.font);
        if (m->searchable) {
            RECT s = {6, 2, r.right - 6, kMenuRow - 2};
            FillRect(dc, s, RGB(30, 30, 33));
            s.left += 6;
            Text(dc, m->filter.empty() ? "Type to search" : m->filter + "_", s, m->filter.empty() ? kDim : kText, DT_LEFT);
        }
        for (int row = 0; row < kMenuRows && m->top + row < (int)m->shown.size(); row++) {
            const int k = m->top + row;
            const MenuItem& it = m->items[m->shown[k]];
            RECT ir = {0, MenuHead(*m) + row * kMenuRow, r.right, MenuHead(*m) + (row + 1) * kMenuRow};
            if (k == m->hot) FillRect(dc, ir, RGB(62, 86, 128));
            if (it.header) {
                SelectObject(dc, g_win.bold);
                RECT t = {ir.left + 8, ir.top, ir.right - 8, ir.bottom};
                Text(dc, it.text, t, kDim, DT_LEFT);
                SelectObject(dc, g_win.font);
                continue;
            }
            RECT t = {ir.left + 24, ir.top, ir.right - 90, ir.bottom};
            if (it.checked) Text(dc, "\xE2\x9C\x93", {ir.left + 6, ir.top, ir.left + 22, ir.bottom}, kText, DT_LEFT);  // ✓
            Text(dc, it.text, t, it.enabled ? kText : RGB(110, 110, 110), DT_LEFT);
            if (!it.hint.empty()) Text(dc, it.hint, {ir.right - 88, ir.top, ir.right - 8, ir.bottom}, kDim, DT_RIGHT);
        }
        if ((int)m->shown.size() > kMenuRows) {
            // where the view is in a longer list
            const int total = (int)m->shown.size(), hh = r.bottom - MenuHead(*m);
            const int y0 = MenuHead(*m) + hh * m->top / total, y1 = MenuHead(*m) + hh * (m->top + kMenuRows) / total;
            FillRect(dc, {r.right - 4, y0, r.right - 1, y1}, RGB(90, 90, 96));
        }
        EndPaint(h, &ps);
        return 0;
    }
    case WM_MOUSEMOVE: {
        const int y = GET_Y_LPARAM(l) - MenuHead(*m);
        const int k = y < 0 ? -1 : m->top + y / kMenuRow;
        const int hot = k >= 0 && k < (int)m->shown.size() && Pickable(m->items[m->shown[k]]) ? k : -1;
        if (hot != m->hot) m->hot = hot, InvalidateRect(h, nullptr, FALSE);
        return 0;
    }
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN: {
        // the menu holds the mouse: a press outside it closes it
        const POINT p = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
        RECT r;
        GetClientRect(h, &r);
        if (p.x < 0 || p.y < 0 || p.x >= r.right || p.y >= r.bottom) m->done = true;
        return 0;
    }
    case WM_LBUTTONUP:
    case WM_RBUTTONUP: {
        const int y = GET_Y_LPARAM(l) - MenuHead(*m);
        if (y >= 0 && GET_X_LPARAM(l) >= 0) MenuPick(*m, m->top + y / kMenuRow);
        return 0;
    }
    case WM_MOUSEWHEEL: {
        const int maxTop = std::max(0, (int)m->shown.size() - kMenuRows);
        m->top = std::clamp(m->top - GET_WHEEL_DELTA_WPARAM(w) / WHEEL_DELTA * 3, 0, maxTop);
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    }
    case WM_KEYDOWN:
        if (w == VK_ESCAPE) m->done = true;
        else if (w == VK_RETURN) MenuPick(*m, m->hot);
        else if (w == VK_DOWN) MenuMove(*m, 1);
        else if (w == VK_UP) MenuMove(*m, -1);
        else if (w == VK_BACK && m->searchable && !m->filter.empty()) m->filter.pop_back(), MenuFilter(*m);
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    case WM_CHAR:
        if (m->searchable && w >= 32 && w < 127) {
            m->filter += (char)w;
            MenuFilter(*m);
            InvalidateRect(h, nullptr, FALSE);
        }
        return 0;
    case WM_CAPTURECHANGED:
        if ((HWND)l != h) m->done = true;  // another window took the mouse
        return 0;
    }
    return DefWindowProcW(h, msg, w, l);
}

// Shows the menu at `client` (the editor's client point) and waits for a choice: its id, or 0 when it was closed.
int PickMenu(POINT client, std::vector<MenuItem> items, bool searchable)
{
    if (items.empty()) return 0;
    static bool registered = false;
    HINSTANCE inst = (HINSTANCE)GetWindowLongPtrW(g_win.wnd, GWLP_HINSTANCE);
    if (!registered) {
        WNDCLASSW wc{};
        wc.style = CS_DROPSHADOW;
        wc.lpfnWndProc = MenuProc;
        wc.hInstance = inst;
        wc.lpszClassName = L"DupliX2Menu";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        registered = RegisterClassW(&wc) != 0;
    }
    MenuState m;
    m.items = std::move(items);
    m.searchable = searchable;
    MenuFilter(m);
    const int rows = std::min(kMenuRows, (int)m.shown.size());
    int width = kMenuW;
    {
        HDC dc = GetDC(g_win.wnd);
        HGDIOBJ old = SelectObject(dc, g_win.font);
        for (const MenuItem& it : m.items) {
            SIZE sz{};
            const std::wstring t = W(it.text);
            GetTextExtentPoint32W(dc, t.c_str(), (int)t.size(), &sz);
            width = std::max(width, (int)sz.cx + 24 + 96);
        }
        SelectObject(dc, old);
        ReleaseDC(g_win.wnd, dc);
    }
    width = std::min(width, 520);
    const int height = MenuHead(m) + rows * kMenuRow + 2;
    POINT sp = client;
    ClientToScreen(g_win.wnd, &sp);
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(MonitorFromPoint(sp, MONITOR_DEFAULTTONEAREST), &mi);
    sp.x = std::min<long>(sp.x, mi.rcWork.right - width), sp.y = std::min<long>(sp.y, mi.rcWork.bottom - height);
    sp.x = std::max<long>(sp.x, mi.rcWork.left), sp.y = std::max<long>(sp.y, mi.rcWork.top);
    g_menu = &m;
    m.wnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, L"DupliX2Menu", L"", WS_POPUP | WS_BORDER, sp.x, sp.y, width, height,
                            g_win.wnd, nullptr, inst, nullptr);
    if (!m.wnd) {
        g_menu = nullptr;
        return 0;
    }
    ShowWindow(m.wnd, SW_SHOWNA);
    UpdateWindow(m.wnd);
    SetCapture(m.wnd);
    MSG msg;
    while (!m.done && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        // the keys go to the menu (the focus stays on the editor)
        if (msg.message >= WM_KEYFIRST && msg.message <= WM_KEYLAST) msg.hwnd = m.wnd;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (GetCapture() == m.wnd) ReleaseCapture();
    g_menu = nullptr;
    DestroyWindow(m.wnd);
    return m.result;
}

enum MenuId { M_ADD = 1000, M_EXPOSE = 200, M_RESET, M_UNLINK, M_EXPAND, M_CONNECTEXPR, M_LINKMENU, M_LINK = 300, M_LINKI = 400,
              M_CHOICE = 500, M_UNWIRE = 600 };

std::string MenuLabel(const dx5::NodeDef& d) { return d.label; }

// Adds a node of defs[index] (past the table: the user's groups, design v5 12.1) where the menu was opened.
void AddAt(POINT client, size_t index)
{
    const auto& defs = dx5::AllDefs();
    std::string type;
    if (index < defs.size()) type = defs[index].name;
    else if (index - defs.size() < G().groups.size()) type = G().groups[index - defs.size()].name;
    else return;
    const double x = (client.x - g_win.panX) / g_win.zoom, y = (client.y - kTabH - g_win.panY) / g_win.zoom;
    const uint32_t nid = dx5::AddNodeAt(G(), type, x, y);
    if (nid) {
        g_win.selected = nid, g_win.multi.clear();
        Save("add node");
    }
}

// The add menu (right-click on empty space, Ctrl+A): every node under its category; typing searches them.
void AddMenu(POINT client)
{
    std::vector<MenuItem> items;
    const char* order[] = {"Generate", "Set", "Geometry", "Input", "Math", "Utility", "Group", "Zone", "Output"};
    const auto& defs = dx5::AllDefs();
    // inside an Expression only formula nodes (design v5 8.3); inside a group no Output (its output is the group's)
    const bool inExpr = !g_win.frames.empty() && g_win.frames.back().expr;
    for (const char* cat : order) {
        const size_t head = items.size();
        for (size_t i = 0; i < defs.size(); i++) {
            const dx5::NodeDef& d = defs[i];
            if (d.category != cat || d.name == "each") continue;
            if (inExpr && d.category != std::string("Math") && d.category != std::string("Input")) continue;
            if (!g_win.frames.empty() && d.name == "output") continue;
            MenuItem it;
            it.text = MenuLabel(d), it.id = M_ADD + (int)i, it.hint = cat;
            it.enabled = !(d.name == "output" && G().OutputNode());
            items.push_back(it);
        }
        if (std::string(cat) == "Group" && !inExpr)
            for (size_t j = 0; j < G().groups.size(); j++) {
                MenuItem it;
                it.text = G().groups[j].name, it.id = M_ADD + (int)(defs.size() + j), it.hint = "Group (yours)";
                items.push_back(it);
            }
        if (items.size() > head) {
            MenuItem h;
            h.text = cat, h.header = true;
            items.insert(items.begin() + (long)head, h);
        }
    }
    const int id = PickMenu(client, items, true);
    if (id >= M_ADD && id < M_ADD + (int)(defs.size() + G().groups.size())) AddAt(client, (size_t)(id - M_ADD));
    Redraw();
}

void ValueMenu(POINT client, uint32_t node, int index)
{
    Session* s = g_win.s;
    dx5::Node* n = G().Find(node);
    if (!n || index < 0 || index >= (int)n->inputs.size()) return;
    const dx5::InputSlot slot = n->inputs[index];
    const dx5::InputDef* d = InDef(*n, slot.name);
    if (d && d->type == dx5::Type::Geometry) return;
    std::vector<MenuItem> items;
    auto add = [&](const std::string& t, int id, bool on = true, bool checked = false) {
        MenuItem it;
        it.text = t, it.id = id, it.enabled = on, it.checked = checked;
        items.push_back(it);
    };
    if (d && !d->choices.empty()) {
        const std::string& c = d->choices;
        size_t a = 0;
        for (int i = 0; a != std::string::npos; i++) {
            const size_t b = c.find('|', a);
            add(c.substr(a, b == std::string::npos ? std::string::npos : b - a), M_CHOICE + i, true, dx5::AsInt(slot.value) == i);
            a = b == std::string::npos ? b : b + 1;
        }
    } else {
        const bool isInt = d && (d->type == dx5::Type::Int || d->structural);
        // Expose writes the macro's value with the graph: at the effect's graph only; without a free macro it says why
        const std::string refusal = dx5::ExposeRefusal(s->graph, isInt);
        add(!refusal.empty() ? refusal : isInt ? "Expose to Int Macro" : "Expose to Macro", M_EXPOSE,
            refusal.empty() && !slot.link.from && g_win.frames.empty());
        add("Link to Macro...", M_LINKMENU);
        if (!slot.link.from) add("Connect Expression", M_CONNECTEXPR);
        if (slot.link.from) add("Unlink", M_UNLINK);
        const dx5::Node* f = slot.link.from ? G().Find(slot.link.from) : nullptr;
        if (f && f->type == "expr") add("Expand Formula into Nodes", M_EXPAND);
        add("Reset", M_RESET);
    }
    int id = PickMenu(client, items, false);
    if (id == M_LINKMENU) {
        // the macros, Int Macros first; one that cannot go in says why (design v5 6.2: a math node's typed input)
        std::vector<MenuItem> mm;
        for (int pass = 0; pass < 2; pass++) {
            const bool mi = pass == 0;
            MenuItem h;
            h.text = mi ? "Int Macros" : "Macros", h.header = true;
            mm.push_back(h);
            const std::string why = dx5::LinkRefusal(G(), node, slot.name, mi ? dx5::Type::Int : dx5::Type::Float);
            for (int k = 0; k < 32; k++) {
                MenuItem it;
                it.text = std::string(mi ? "I" : "M") + std::to_string(k + 1) + "  " + dx5::MacroLabel(s->graph, mi, k);
                it.id = (mi ? M_LINKI : M_LINK) + k, it.enabled = why.empty();
                if (!why.empty() && k == 0) it.hint = "not here";
                mm.push_back(it);
            }
        }
        id = PickMenu(client, mm, true);
    }
    if (id == M_EXPOSE) {
        dx5::MacroWrite w;
        if (dx5::Expose(s->graph, node, slot.name, &w)) Commit("expose", {w});
    } else if (id == M_RESET) {
        if (d) {
            dx5::Node* q = G().Find(node);
            dx5::InputSlot* qs = q ? q->In(slot.name) : nullptr;
            if (qs) qs->link = {}, qs->value = d->dflt;
            Save("reset value");
        }
    } else if (id == M_CONNECTEXPR) {
        if (dx5::ConnectExpression(G(), node, slot.name)) Save("connect expression");
    } else if (id == M_UNLINK) {
        if (dx5::Disconnect(G(), node, slot.name)) Save("unlink");
    } else if (id == M_EXPAND) {
        if (dx5::ExpandExpression(G(), slot.link.from)) Save("expand formula");
    } else if (id >= M_CHOICE && id < M_CHOICE + 64) {
        if (dx5::SetConst(G(), node, slot.name, id - M_CHOICE)) Save("set choice");
    } else if ((id >= M_LINK && id < M_LINK + 32) || (id >= M_LINKI && id < M_LINKI + 32)) {
        const bool mi = id >= M_LINKI;
        if (dx5::LinkMacro(G(), node, slot.name, mi, id - (mi ? M_LINKI : M_LINK))) Save("link macro");
        else Refused(dx5::LinkRefusal(G(), node, slot.name, mi ? dx5::Type::Int : dx5::Type::Float));
    }
    Redraw();
}

// Ctrl+G: the picked nodes (Shift-click, a box) and the selected one become a group (design v5 12.1)
void GroupPicked()
{
    std::vector<uint32_t> nodes = g_win.multi;
    if (g_win.selected && std::find(nodes.begin(), nodes.end(), g_win.selected) == nodes.end()) nodes.push_back(g_win.selected);
    if (nodes.empty()) return Refused("pick nodes first (drag a box, or Shift+click their titles), then Ctrl+G");
    const dx5::Graph before = G();
    uint32_t gn = 0;
    if (!dx5::GroupSelection(G(), nodes, "", &gn)) {
        G() = before;
        return Refused("these nodes cannot be grouped");
    }
    Log("editor: group %s", G().Find(gn)->type.c_str());
    g_win.multi.clear(), g_win.selected = gn;
    Save("group");
    Redraw();
}

// X / Delete: the selected nodes go
void DeletePicked()
{
    std::vector<uint32_t> nodes = g_win.multi;
    if (g_win.selected && std::find(nodes.begin(), nodes.end(), g_win.selected) == nodes.end()) nodes.push_back(g_win.selected);
    bool any = false;
    for (uint32_t id : nodes) {
        const dx5::Node* n = G().Find(id);
        if (!n || n->type == "param" || n->type == "param_geo" || n->type == "group_output") continue;  // a group's frame stays
        any |= dx5::RemoveNode(G(), id);
    }
    g_win.selected = 0, g_win.multi.clear();
    if (any) Save("remove node");
    Redraw();
}

// the nodes a "Collapse into Expression" from `node` takes: it and the formula nodes above it that only it uses
std::vector<uint32_t> FormulaTree(const dx5::Graph& g, uint32_t node)
{
    auto formula = [](const std::string& t) {
        return dx5::IsMathFamily(t) || t == "expr" || t == "attr" || t == "random" || t == "index" || t == "id" || t == "position" ||
               t == "rotation" || t == "scale" || t == "opacity" || t == "count" || t == "time" || t == "seed";
    };
    std::vector<uint32_t> set = {node};
    for (bool grew = true; grew;) {
        grew = false;
        for (const dx5::Node& m : g.nodes) {
            if (std::find(set.begin(), set.end(), m.id) != set.end() || !formula(m.type)) continue;
            bool feedsSet = false, feedsOther = false;
            for (const dx5::Node& u : g.nodes)
                for (const dx5::InputSlot& s : u.inputs)
                    if (s.link.from == m.id) (std::find(set.begin(), set.end(), u.id) != set.end() ? feedsSet : feedsOther) = true;
            if (feedsSet && !feedsOther) set.push_back(m.id), grew = true;
        }
    }
    return set;
}

// right-click on a node: open it, copy a built-in group for yourself, ungroup, make a group, expand / collapse a
// formula, group, delete
void NodeMenu(POINT client, uint32_t node)
{
    const dx5::Node* n = G().Find(node);
    if (!n) return;
    enum { N_OPEN = 800, N_COPY, N_UNGROUP, N_MAKE, N_EXPAND, N_COLLAPSE, N_GROUP, N_DELETE };
    const bool builtin = dx5::FindDef(n->type) && dx5::FindDef(n->type)->group;
    const bool user = dx5::FindUserGroup(G(), n->type) != nullptr;
    const bool formula = dx5::IsMathFamily(n->type) || n->type == "expr";
    std::vector<MenuItem> items;
    auto add = [&](const char* t, int id, const char* key = "") {
        MenuItem it;
        it.text = t, it.id = id, it.hint = key;
        items.push_back(it);
    };
    if (Openable(*n)) add("Open", N_OPEN, "Tab");
    if (builtin) add("Copy for Myself", N_COPY);
    if (user) add("Ungroup", N_UNGROUP);
    if (n->type == "expr") add("Make Group", N_MAKE), add("Expand Formula into Nodes", N_EXPAND);
    if (formula && n->type != "expr") add("Collapse into Expression", N_COLLAPSE);
    add("Group", N_GROUP, "Ctrl+G");
    add("Delete", N_DELETE, "X");
    const int id = PickMenu(client, items, false);
    const dx5::Graph before = G();
    std::string name;
    bool ok = false;
    const char* what = "";
    if (id == N_OPEN) return OpenNode(node);
    if (id == N_GROUP) return GroupPicked();
    if (id == N_DELETE) return DeletePicked();
    if (id == N_COPY) ok = dx5::CopyGroupForSelf(G(), node, &name), what = "copy group";
    else if (id == N_UNGROUP) ok = dx5::UngroupNode(G(), node), what = "ungroup";
    else if (id == N_MAKE) ok = dx5::ExpressionToGroup(G(), node, &name), what = "make group";
    else if (id == N_EXPAND) ok = dx5::ExpandExpression(G(), node), what = "expand formula";
    else if (id == N_COLLAPSE) ok = dx5::CollapseToExpression(G(), FormulaTree(G(), node)), what = "collapse formula";
    else return;
    if (!ok) {
        G() = before;
        return Refused(std::string(what) + ": not possible here");
    }
    Log("editor: %s%s%s", what, name.empty() ? "" : " ", name.c_str());
    Save(what);
    Redraw();
}

// a number that a drag can change (unwired, not a choice, a Float or Int)
bool Scrubbable(const dx5::Node& n, int input)
{
    const dx5::InputSlot& s = n.inputs[input];
    const dx5::InputDef* d = InDef(n, s.name);
    if (s.link.from || (d && !d->choices.empty())) return false;
    return s.value.type == dx5::Type::Float || s.value.type == dx5::Type::Int;
}

// ---- the view: zoom, fit, a group's code ----
// The zoom set to z (kept in range), the world point under `about` staying under it.
void ZoomAt(POINT about, double z)
{
    z = std::clamp(z, kZoomMin, kZoomMax);
    if (z == g_win.zoom) return;
    if (g_win.edit) SetFocus(g_win.wnd);  // a value's box would stay where the value was
    const double wx = (about.x - g_win.panX) / g_win.zoom, wy = (about.y - kTabH - g_win.panY) / g_win.zoom;
    g_win.zoom = z;
    g_win.panX = about.x - wx * z, g_win.panY = about.y - kTabH - wy * z;
    Log("editor: zoom %.3f", z);
    Redraw();
}

// Home: every node in view (not larger than 1:1)
void FitAll()
{
    Session* s = g_win.s;
    if (!s || G().nodes.empty()) return;
    double x0 = 1e18, y0 = 1e18, x1 = -1e18, y1 = -1e18;
    for (const dx5::Node& n : G().nodes) {
        x0 = std::min(x0, n.x), y0 = std::min(y0, n.y);
        x1 = std::max(x1, n.x + kNodeW), y1 = std::max(y1, n.y + WorldH(n));
    }
    const RECT c = CanvasRect();
    const double cw = std::max(1L, c.right - c.left - 60), ch = std::max(1L, c.bottom - c.top - 60);
    const double z = std::clamp(std::min({cw / std::max(1.0, x1 - x0), ch / std::max(1.0, y1 - y0), 1.0}), kZoomMin, kZoomMax);
    if (g_win.edit) SetFocus(g_win.wnd);
    g_win.zoom = z;
    g_win.panX = c.left + (c.right - c.left - (x1 - x0) * z) / 2 - x0 * z;
    g_win.panY = (c.bottom - c.top - (y1 - y0) * z) / 2 - y0 * z;
    Log("editor: zoom %.3f", z);
    Redraw();
}

// ---- the window ----
LRESULT CALLBACK Proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    Session* s = g_win.s;
    switch (m) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT r;
        GetClientRect(h, &r);
        HDC mem = CreateCompatibleDC(dc);
        HBITMAP bmp = CreateCompatibleBitmap(dc, std::max(1L, r.right), std::max(1L, r.bottom));
        HGDIOBJ old = SelectObject(mem, bmp);
        Paint(mem, r);
        BitBlt(dc, 0, 0, r.right, r.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old);
        DeleteObject(bmp);
        DeleteDC(mem);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_TIMER:
        if (w == kProblemTimer && s) {
            // the problems the render reported for this very graph (an older revision's are not shown)
            std::vector<std::string> p;
            if (!Problems(s->instance, s->graph.revision, &p)) p = g_win.problems;
            if (p != g_win.problems) g_win.problems = p, Redraw();
        }
        return 0;
    case WM_SIZE:
        LayoutCode();
        Redraw();
        return 0;
    case WM_COMMAND:
        if (LOWORD(w) == kApplyId && HIWORD(w) == BN_CLICKED) ApplyCodeNow();
        return 0;
    case WM_CTLCOLORSTATIC:
        if ((HWND)l == g_win.codeMsg) {
            static HBRUSH bg = CreateSolidBrush(kBg);
            SetTextColor((HDC)w, g_win.codeError ? RGB(250, 110, 90) : kDim);
            SetBkColor((HDC)w, kBg);
            return (LRESULT)bg;
        }
        break;
    case WM_LBUTTONDOWN: {
        if (!s) return 0;
        const POINT p = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
        if (p.y < kTabH) {
            for (int i = 0; i < 3; i++)
                if (In(TabRect((View)i), p) && g_win.view != (View)i) SetView((View)i);
            return 0;
        }
        if (g_win.view != View::Nodes) return 0;
        SetFocus(h);
        const HitInfo hi = HitTest(p);
        g_win.start = g_win.now = p;
        g_win.moved = false;
        g_win.dragNode = hi.node;
        g_win.dragIndex = hi.index;
        g_win.lifted = false;
        g_win.snapNode = 0, g_win.snapInput = -1;
        if (hi.what == Hit::Title) {
            const dx5::Node* n = G().Find(hi.node);
            if (GetKeyState(VK_SHIFT) < 0) {
                // Shift: pick more nodes (Ctrl+G groups them)
                if (g_win.selected && std::find(g_win.multi.begin(), g_win.multi.end(), g_win.selected) == g_win.multi.end())
                    g_win.multi.push_back(g_win.selected);
                auto it = std::find(g_win.multi.begin(), g_win.multi.end(), hi.node);
                if (it != g_win.multi.end()) g_win.multi.erase(it);
                else g_win.multi.push_back(hi.node);
            } else if (std::find(g_win.multi.begin(), g_win.multi.end(), hi.node) == g_win.multi.end()) {
                g_win.multi.clear();
            }
            g_win.selected = hi.node;
            g_win.drag = Drag::Move;
            g_win.startX = n->x, g_win.startY = n->y;
            // the picked nodes move together when one of them is dragged (11/29)
            g_win.moveFrom.clear();
            std::vector<uint32_t> moving = g_win.multi;
            if (std::find(moving.begin(), moving.end(), hi.node) == moving.end()) moving = {hi.node};
            for (uint32_t id : moving)
                if (const dx5::Node* m = G().Find(id)) g_win.moveFrom.push_back({id, {(long)std::lround(m->x * 100), (long)std::lround(m->y * 100)}});
        } else if (hi.what == Hit::GroupOpen) {
            g_win.selected = hi.node;
            OpenNode(hi.node);
            return 0;
        } else if (hi.what == Hit::Back) {
            Back();
            return 0;
        } else if (hi.what == Hit::UnusedToggle) {
            g_win.unusedOpen = !g_win.unusedOpen;
            Redraw();
            return 0;
        } else if (hi.what == Hit::OpRow) {
            // a math node's operator: the operators of its family (design v5 6.2)
            g_win.selected = hi.node;
            const dx5::Node* n = G().Find(hi.node);
            if (!n) return 0;
            const std::vector<const dx5::MathOpDef*> ops = dx5::MathOps(n->type);
            std::vector<MenuItem> items;
            for (size_t i = 0; i < ops.size(); i++) {
                MenuItem it;
                it.text = ops[i]->op, it.id = 700 + (int)i, it.checked = ops[i]->op == n->text;
                it.hint = std::to_string(ops[i]->arity) + (ops[i]->arity == 1 ? " input" : " inputs");
                items.push_back(it);
            }
            const int id = PickMenu(p, items, true);
            std::string why;
            if (id >= 700 && id < 700 + (int)ops.size() && ops[id - 700]->op != n->text) {
                const std::string op = ops[id - 700]->op;
                if (dx5::SetMathOp(G(), hi.node, op, &why)) {
                    Log("editor: op %s", op.c_str());
                    Save("operator");
                } else {
                    Refused(why);
                }
            }
            Redraw();
            return 0;
        } else if (hi.what == Hit::OutPort) {
            g_win.drag = Drag::Wire;
        } else if (hi.what == Hit::InPort) {
            // a wired input: its wire is lifted off and dragged from where it comes (Blender)
            const dx5::Node* n = G().Find(hi.node);
            if (n && n->inputs[hi.index].link.from) {
                g_win.lifted = true;
                g_win.liftBefore = G();
                g_win.liftNode = hi.node;
                g_win.liftInput = n->inputs[hi.index].name;
                g_win.liftLink = n->inputs[hi.index].link;
                g_win.dragNode = g_win.liftLink.from;
                g_win.dragIndex = g_win.liftLink.output;
                dx5::Disconnect(G(), hi.node, g_win.liftInput);
                g_win.drag = Drag::Wire;
            }
        } else if (hi.what == Hit::Macro) {
            g_win.drag = Drag::MacroLink;
            g_win.macroDragInt = hi.isInt, g_win.macroDragK = hi.index;
        } else if (hi.what == Hit::Value) {
            const dx5::Node* n = G().Find(hi.node);
            g_win.selected = hi.node;
            g_win.drag = Drag::Scrub;
            g_win.startValue = dx5::AsFloat(n->inputs[hi.index].value);
        } else if (hi.what == Hit::ExprText) {
            const dx5::Node* n = G().Find(hi.node);
            g_win.selected = hi.node;
            g_win.editNode = hi.node, g_win.editExprText = true, g_win.editMacro = -1, g_win.editInput = -1, g_win.editSearch = false;
            ZoomFonts();
            RECT er = TextRect(*n);
            er.bottom = std::max(er.bottom, er.top + Z(kRowH) * (TextRows(*n) + 2));  // room for a new line
            er.right = std::max(er.right, er.left + 320);
            BeginEdit(er, n->text, g_win.zFont, true);
        } else if (hi.what == Hit::Canvas) {
            // a box from here picks the nodes it touches (Shift adds to the picked ones)
            if (GetKeyState(VK_SHIFT) >= 0) g_win.selected = 0, g_win.multi.clear();
            g_win.drag = Drag::Box;
        }
        if (g_win.drag != Drag::None) SetCapture(h);
        Redraw();
        return 0;
    }
    case WM_MBUTTONDOWN:
        g_win.drag = Drag::Pan;
        g_win.start = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
        g_win.startX = g_win.panX, g_win.startY = g_win.panY;
        SetCapture(h);
        return 0;
    case WM_MOUSEMOVE: {
        if (!s || g_win.drag == Drag::None) return 0;
        const POINT p = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
        g_win.now = p;
        const int dx = p.x - g_win.start.x, dy = p.y - g_win.start.y;
        if (std::abs(dx) > 3 || std::abs(dy) > 3) g_win.moved = true;
        if (g_win.drag == Drag::Move) {
            for (const auto& m : g_win.moveFrom)
                dx5::MoveNode(G(), m.first, m.second.x / 100.0 + dx / g_win.zoom, m.second.y / 100.0 + dy / g_win.zoom);
        } else if (g_win.drag == Drag::Wire) {
            g_win.snapNode = 0, g_win.snapInput = -1;
            FindSnap(p, &g_win.snapNode, &g_win.snapInput);
        } else if (g_win.drag == Drag::Pan) {
            g_win.panX = g_win.startX + dx, g_win.panY = g_win.startY + dy;
        } else if (g_win.drag == Drag::Scrub && g_win.moved) {
            // only the window changes while dragging; the release writes once (design v4 7)
            dx5::Node* n = G().Find(g_win.dragNode);
            if (n && g_win.dragIndex < (int)n->inputs.size() && Scrubbable(*n, g_win.dragIndex)) {
                // a pixel moved: 1 (Shift 10, Ctrl 0.1); an Int with Ctrl moves 1 every 10 pixels (the user, 11/29)
                double step = 1.0;
                if (GetKeyState(VK_SHIFT) < 0) step = 10.0;
                if (GetKeyState(VK_CONTROL) < 0) step = 0.1;
                dx5::SetConst(G(), n->id, n->inputs[g_win.dragIndex].name, g_win.startValue + dx * step);
            }
        }
        Redraw();
        return 0;
    }
    case WM_LBUTTONUP: {
        if (!s) return 0;
        // the drag is taken and ended before ReleaseCapture: it sends WM_CAPTURECHANGED at once, which would read
        // a drag still going as one cut short and end it (11/28: no wire ever connected, a click opened no edit box)
        const Drag d = g_win.drag;
        g_win.drag = Drag::None;
        ReleaseCapture();
        const POINT p = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
        if (d == Drag::Move && g_win.moved) {
            Save("move node");
        } else if (d == Drag::Box) {
            if (g_win.moved) {
                const RECT box = {std::min(g_win.start.x, p.x), std::min(g_win.start.y, p.y), std::max(g_win.start.x, p.x),
                                  std::max(g_win.start.y, p.y)};
                for (const dx5::Node& n : G().nodes) {
                    const RECT r = NodeRect(n);
                    RECT x;
                    if (IntersectRect(&x, &r, &box) && std::find(g_win.multi.begin(), g_win.multi.end(), n.id) == g_win.multi.end())
                        g_win.multi.push_back(n.id);
                }
                g_win.selected = g_win.multi.empty() ? 0 : g_win.multi.back();
            }
        } else if (d == Drag::Wire) {
            // where it was let go: the input it snaps to, else an input under the cursor (refused with why)
            uint32_t toNode = 0;
            int toInput = -1;
            if (!FindSnap(p, &toNode, &toInput)) {
                const HitInfo hi = HitTest(p);
                if (hi.what == Hit::InPort) toNode = hi.node, toInput = hi.index;
            }
            g_win.snapNode = 0, g_win.snapInput = -1;
            const dx5::Node* to = toNode ? G().Find(toNode) : nullptr;
            std::string why;
            bool wired = false;
            if (to && toInput >= 0) {
                const std::string name = to->inputs[toInput].name;
                wired = dx5::Connect(G(), g_win.dragNode, g_win.dragIndex, toNode, name, &why);
                if (wired && g_win.lifted && toNode == g_win.liftNode && name == g_win.liftInput) {
                    G() = g_win.liftBefore;  // put back where it was: nothing changed
                    g_win.lifted = false;
                    Redraw();
                    return 0;
                }
                if (wired) {
                    g_win.status.clear();
                    Save("wire");  // with a lifted wire: its move, one step
                } else {
                    Refused(why);
                }
            }
            if (!wired && g_win.lifted) Save("unwire");  // a lifted wire let go elsewhere comes off
            g_win.lifted = false;
        } else if (d == Drag::MacroLink) {
            const HitInfo hi = HitTest(p);
            const dx5::Node* n = hi.what == Hit::Value ? G().Find(hi.node) : nullptr;
            if (g_win.moved && n && hi.index < (int)n->inputs.size()) {
                if (dx5::LinkMacro(G(), n->id, n->inputs[hi.index].name, g_win.macroDragInt, g_win.macroDragK)) {
                    g_win.status.clear();
                    Save("link macro");
                } else {
                    const std::string why = dx5::LinkRefusal(G(), n->id, n->inputs[hi.index].name,
                                                             g_win.macroDragInt ? dx5::Type::Int : dx5::Type::Float);
                    Refused(why.empty() ? "a macro cannot feed " + n->inputs[hi.index].name : why);
                }
            }
            g_win.macroDragK = -1;
        } else if (d == Drag::Scrub) {
            dx5::Node* n = G().Find(g_win.dragNode);
            if (n && g_win.dragIndex < (int)n->inputs.size()) {
                if (g_win.moved) {
                    if (Scrubbable(*n, g_win.dragIndex) && dx5::AsFloat(n->inputs[g_win.dragIndex].value) != g_win.startValue)
                        Save("drag value");
                } else {
                    const dx5::InputDef* id = InDef(*n, n->inputs[g_win.dragIndex].name);
                    if (id && !id->choices.empty()) ValueMenu(p, n->id, g_win.dragIndex);
                    else BeginValueEdit(*n, g_win.dragIndex);
                }
            }
        }
        Redraw();
        return 0;
    }
    case WM_MOUSEWHEEL: {
        if (!s || g_win.view != View::Nodes || g_win.drag != Drag::None) return 0;
        POINT p = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};  // screen coordinates
        ScreenToClient(h, &p);
        if (!In(CanvasRect(), p)) return 0;
        ZoomAt(p, g_win.zoom * std::pow(1.1, GET_WHEEL_DELTA_WPARAM(w) / (double)WHEEL_DELTA));
        return 0;
    }
    case WM_MBUTTONUP:
        g_win.drag = Drag::None;  // first (see WM_LBUTTONUP)
        ReleaseCapture();
        return 0;
    case WM_CAPTURECHANGED:
        if (g_win.drag != Drag::None && (HWND)l != h) {
            // a drag cut short (another window took the mouse): keep what it did as one step
            if ((g_win.drag == Drag::Move || g_win.drag == Drag::Scrub) && g_win.moved) Save("drag");
            if (g_win.drag == Drag::Wire && g_win.lifted && s) G() = g_win.liftBefore;  // the lifted wire goes back
            g_win.lifted = false, g_win.snapNode = 0, g_win.snapInput = -1, g_win.macroDragK = -1;
            g_win.drag = Drag::None;
            Redraw();
        }
        return 0;
    case WM_RBUTTONUP: {
        if (!s || g_win.view != View::Nodes) return 0;
        const POINT p = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
        const HitInfo hi = HitTest(p);
        if (hi.what == Hit::Canvas) AddMenu(p);
        else if (hi.what == Hit::Value) ValueMenu(p, hi.node, hi.index);
        else if (hi.what == Hit::Title || hi.what == Hit::GroupOpen || hi.what == Hit::OpRow || hi.what == Hit::ExprText) {
            g_win.selected = hi.node;
            NodeMenu(p, hi.node);
        }
        else if (hi.what == Hit::InPort) {
            const dx5::Node* n = G().Find(hi.node);
            if (n && n->inputs[hi.index].link.from) {
                MenuItem it;
                it.text = "Unwire", it.id = M_UNWIRE;
                const int id = PickMenu(p, {it}, false);
                if (id == M_UNWIRE && dx5::Disconnect(G(), hi.node, n->inputs[hi.index].name)) Save("unwire");
                Redraw();
            }
        }
        return 0;
    }
    case WM_LBUTTONDBLCLK: {
        if (!s) return 0;
        const POINT p = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
        const HitInfo hi = HitTest(p);
        // a second quick click on the "unused" heading folds it again (a double click is two clicks: 11/29)
        if (hi.what == Hit::UnusedToggle) {
            g_win.unusedOpen = !g_win.unusedOpen;
            Redraw();
            return 0;
        }
        if (hi.what == Hit::Macro) {
            for (size_t row = 0; row < g_win.macroRows.size(); row++)
                if (g_win.macroRows[row].k == hi.index && g_win.macroRows[row].isInt == hi.isInt) {
                    RECT r = MacroRow((int)row);
                    r.left += 30, r.right -= 80;
                    g_win.editMacro = hi.index, g_win.editMacroInt = hi.isInt, g_win.editExprText = false, g_win.editInput = -1,
                    g_win.editSearch = false;
                    BeginEdit(r, (hi.isInt ? s->graph.intMacros : s->graph.macros)[hi.index].name);
                }
        }
        return 0;
    }
    case WM_KEYDOWN:
        if (!s) return 0;
        if ((w == VK_DELETE || (w == 'X' && GetKeyState(VK_CONTROL) >= 0)) && g_win.view == View::Nodes) {
            DeletePicked();  // X or Delete (design: Blender's X)
        } else if (w == 'A' && GetKeyState(VK_CONTROL) < 0 && g_win.view == View::Nodes) {
            // Ctrl+A: the add menu at the cursor (typing searches it)
            POINT cp;
            GetCursorPos(&cp);
            ScreenToClient(h, &cp);
            if (!In(CanvasRect(), cp)) cp = {CanvasRect().left + 40, CanvasRect().top + 40};
            AddMenu(cp);
        } else if (w == VK_HOME && g_win.view == View::Nodes) {
            FitAll();
        } else if (w == VK_TAB && g_win.view == View::Nodes) {
            // Tab: open the selected Expression node or group; with nothing to open, back out (design v5 15)
            const dx5::Node* n = g_win.selected ? G().Find(g_win.selected) : nullptr;
            if (n && Openable(*n)) OpenNode(n->id);
            else Back();
        } else if (w == VK_ESCAPE && g_win.view == View::Nodes) {
            Back();
        } else if (w == 'G' && GetKeyState(VK_CONTROL) < 0 && g_win.view == View::Nodes) {
            GroupPicked();
        } else if (w == 'Z' && GetKeyState(VK_CONTROL) < 0) {
            RequestUndo(GetKeyState(VK_SHIFT) < 0);
        }
        return 0;
    case WM_CLOSE:
        DestroyWindow(h);
        return 0;
    case WM_DESTROY: {
        Session* gone = g_win.s;
        if (g_win.edit) g_win.edit = nullptr;
        if (g_win.zFont) DeleteObject(g_win.zFont);
        if (g_win.zBold) DeleteObject(g_win.zBold);
        g_win.zFont = g_win.zBold = nullptr, g_win.fontZoom = 0;
        g_win.s = nullptr;
        g_win.wnd = nullptr;
        WindowGone(gone);
        return 0;
    }
    }
    return DefWindowProcW(h, m, w, l);
}

}  // namespace

HWND CreateEditorWindow(Session* s)
{
    static bool registered = false;
    HINSTANCE inst = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCWSTR)&CreateEditorWindow, &inst);
    if (!registered) {
        WNDCLASSW wc{};
        wc.style = CS_DBLCLKS;
        wc.lpfnWndProc = Proc;
        wc.hInstance = inst;
        wc.lpszClassName = L"DupliX2Editor";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        registered = RegisterClassW(&wc) != 0;
    }
    static HFONT font = CreateFontW(-13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    static HFONT bold = CreateFontW(-13, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    static HFONT mono = CreateFontW(-14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");
    // owned by AE's main window: it stays above it and goes with it
    void* owner = nullptr;
    Suite<AEGP_UtilitySuite6> util(g_pica, kAEGPUtilitySuite, kAEGPUtilitySuiteVersion6);
    if (util) util->AEGP_GetMainHWND(&owner);
    g_win = Win{};
    g_win.s = s;
    g_win.font = font;
    g_win.bold = bold;
    g_win.mono = mono;
    HWND wnd = CreateWindowExW(0, L"DupliX2Editor", L"DupliX 2 Editor", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                               960, 600, (HWND)owner, nullptr, inst, nullptr);
    g_win.wnd = wnd;
    if (wnd) ShowWindow(wnd, SW_SHOWNORMAL);
    if (wnd) SetTimer(wnd, kProblemTimer, 500, nullptr);
    return wnd;
}

void GraphReloaded(HWND wnd)
{
    if (wnd != g_win.wnd) return;
    // the graph changed outside (AE's undo): what was opened may be gone, so the view goes back to the graph
    if (!g_win.frames.empty()) {
        const Frame outer = g_win.frames.front();
        g_win.frames.clear();
        g_win.panX = outer.panX, g_win.panY = outer.panY, g_win.zoom = outer.zoom;
    }
    g_win.multi.clear();
    if (g_win.selected && g_win.s && !g_win.s->graph.Find(g_win.selected)) g_win.selected = 0;
    if (g_win.view == View::Code && g_win.code) {
        // the code as the user left it stays; untouched code follows the graph
        if (SendMessageW(g_win.code, EM_GETMODIFY, 0, 0)) CodeMessage("The graph changed in After Effects: Ctrl+R takes it (your edit is lost), Apply replaces it.", true);
        else FillCode();
    }
    Redraw();
}

}  // namespace ed
}  // namespace dx2

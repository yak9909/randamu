// DupliX 2 (design v4, 1.1): the Overview in the Effect Controls.
//   left    a picture of the layout (the graph evaluated now, one dot per copy)
//   right   the number of copies and the macros in use ("1  Count")
//   bottom  Open Editor, and the presets (a click replaces the graph and sets its macros: one undo step)
#include "v2/dx2.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "AEFX_SuiteHelper.h"

namespace dx2 {
namespace {

struct RGBA {
    float r, g, b, a;
};

// one colour per Layer (the Overview's dots); this layer's copies are grey
const RGBA kLayerColors[kLayers] = {
    {0.93f, 0.42f, 0.40f, 1}, {0.96f, 0.69f, 0.30f, 1}, {0.93f, 0.86f, 0.38f, 1}, {0.48f, 0.80f, 0.42f, 1},
    {0.36f, 0.78f, 0.86f, 1}, {0.42f, 0.58f, 0.96f, 1}, {0.66f, 0.48f, 0.92f, 1}, {0.92f, 0.52f, 0.78f, 1},
};

struct Rect {
    float x, y, w, h;
    bool Has(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

// ---- drawing (Drawbot) ----
class Canvas {
public:
    Canvas(PF_InData* in_data, PF_OutData* out_data, PF_EventExtra* ev) : in_(in_data), out_(out_data)
    {
        if (AEFX_AcquireDrawbotSuites(in_data, out_data, &s_)) return;
        acquired_ = true;
        PF_EffectCustomUISuite1* cui = nullptr;
        if (AEFX_AcquireSuite(in_data, out_data, kPFEffectCustomUISuite, kPFEffectCustomUISuiteVersion1, nullptr,
                              (void**)&cui) == PF_Err_NONE &&
            cui) {
            cui->PF_GetDrawingReference(ev->contextH, &draw_);
            AEFX_ReleaseSuite(in_data, out_data, kPFEffectCustomUISuite, kPFEffectCustomUISuiteVersion1, nullptr);
        }
        if (!draw_) return;
        s_.drawbot_suiteP->GetSupplier(draw_, &sup_);
        s_.drawbot_suiteP->GetSurface(draw_, &surf_);
        s_.supplier_suiteP->GetDefaultFontSize(sup_, &fontSize_);
        s_.surface_suiteP->SetAntiAliasPolicy(surf_, kDRAWBOT_AntiAliasPolicy_High);
        PFAppSuite6* app = nullptr;
        if (in_data->pica_basicP->AcquireSuite(kPFAppSuite, kPFAppSuiteVersion6, (const void**)&app) == kSPNoError && app) {
            PF_App_Color c;
            if (!app->PF_AppGetColor(PF_App_Color_TEXT, &c)) text_ = {c.red / 65535.f, c.green / 65535.f, c.blue / 65535.f, 1};
            if (!app->PF_AppGetColor(PF_App_Color_PANEL_BACKGROUND, &c))
                panel_ = {c.red / 65535.f, c.green / 65535.f, c.blue / 65535.f, 1};
            in_data->pica_basicP->ReleaseSuite(kPFAppSuite, kPFAppSuiteVersion6);
        }
    }
    ~Canvas()
    {
        if (acquired_) AEFX_ReleaseDrawbotSuites(in_, out_);
    }
    bool Ok() const { return surf_ != nullptr; }
    float FontSize() const { return fontSize_; }
    RGBA Text() const { return text_; }
    RGBA Dim() const { return Mix(text_, panel_, 0.45f); }
    RGBA Shade(float t) const { return Mix(panel_, text_, t); }
    static RGBA Mix(RGBA a, RGBA b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, 1}; }

    void FillRound(const Rect& r, float radius, RGBA c)
    {
        Path([&](DRAWBOT_PathRef p) { RoundPath(p, r, radius); }, c, true);
    }
    void StrokeRound(const Rect& r, float radius, RGBA c, float width = 1)
    {
        Path([&](DRAWBOT_PathRef p) { RoundPath(p, {r.x + 0.5f, r.y + 0.5f, r.w - 1, r.h - 1}, radius); }, c, false, width);
    }
    void Circle(float cx, float cy, float radius, RGBA c)
    {
        Path(
            [&](DRAWBOT_PathRef p) {
                DRAWBOT_PointF32 ctr = {cx, cy};
                s_.path_suiteP->AddArc(p, &ctr, radius, 0, 360);
                s_.path_suiteP->Close(p);
            },
            c, true);
    }
    // text with its baseline at y; cut with "..." past maxWidth (0 = no limit)
    void Text(float x, float y, const std::wstring& s, RGBA c, float size = 0, float maxWidth = 0,
              DRAWBOT_TextAlignment align = kDRAWBOT_TextAlignment_Left)
    {
        if (!Ok() || s.empty()) return;
        DRAWBOT_FontRef font = nullptr;
        DRAWBOT_BrushRef brush = nullptr;
        DRAWBOT_ColorRGBA col = {c.r, c.g, c.b, c.a};
        if (!s_.supplier_suiteP->NewDefaultFont(sup_, size > 0 ? size : fontSize_, &font) &&
            !s_.supplier_suiteP->NewBrush(sup_, &col, &brush)) {
            DRAWBOT_PointF32 o = {x, y};
            s_.surface_suiteP->DrawString(surf_, brush, font, (const DRAWBOT_UTF16Char*)s.c_str(), &o, align,
                                          maxWidth > 0 ? kDRAWBOT_TextTruncation_EndEllipsis : kDRAWBOT_TextTruncation_None,
                                          maxWidth);
        }
        if (brush) s_.supplier_suiteP->ReleaseObject((DRAWBOT_ObjectRef)brush);
        if (font) s_.supplier_suiteP->ReleaseObject((DRAWBOT_ObjectRef)font);
    }

private:
    template <class F>
    void Path(F build, RGBA c, bool fill, float width = 1)
    {
        if (!Ok()) return;
        DRAWBOT_PathRef path = nullptr;
        if (s_.supplier_suiteP->NewPath(sup_, &path) || !path) return;
        build(path);
        DRAWBOT_ColorRGBA col = {c.r, c.g, c.b, c.a};
        if (fill) {
            DRAWBOT_BrushRef brush = nullptr;
            if (!s_.supplier_suiteP->NewBrush(sup_, &col, &brush) && brush) {
                s_.surface_suiteP->FillPath(surf_, brush, path, kDRAWBOT_FillType_Default);
                s_.supplier_suiteP->ReleaseObject((DRAWBOT_ObjectRef)brush);
            }
        } else {
            DRAWBOT_PenRef pen = nullptr;
            if (!s_.supplier_suiteP->NewPen(sup_, &col, width, &pen) && pen) {
                s_.surface_suiteP->StrokePath(surf_, pen, path);
                s_.supplier_suiteP->ReleaseObject((DRAWBOT_ObjectRef)pen);
            }
        }
        s_.supplier_suiteP->ReleaseObject((DRAWBOT_ObjectRef)path);
    }
    void RoundPath(DRAWBOT_PathRef p, const Rect& r, float rad)
    {
        rad = std::min(rad, std::min(r.w, r.h) * 0.5f);
        auto arc = [&](float cx, float cy, float start) {
            DRAWBOT_PointF32 c = {cx, cy};
            s_.path_suiteP->AddArc(p, &c, rad, start, 90);
        };
        s_.path_suiteP->MoveTo(p, r.x + rad, r.y);
        s_.path_suiteP->LineTo(p, r.x + r.w - rad, r.y);
        arc(r.x + r.w - rad, r.y + rad, 270);
        s_.path_suiteP->LineTo(p, r.x + r.w, r.y + r.h - rad);
        arc(r.x + r.w - rad, r.y + r.h - rad, 0);
        s_.path_suiteP->LineTo(p, r.x + rad, r.y + r.h);
        arc(r.x + rad, r.y + r.h - rad, 90);
        s_.path_suiteP->LineTo(p, r.x, r.y + rad);
        arc(r.x + rad, r.y + rad, 180);
        s_.path_suiteP->Close(p);
    }

    PF_InData* in_;
    PF_OutData* out_;
    DRAWBOT_Suites s_ = {};
    bool acquired_ = false;
    DRAWBOT_DrawRef draw_ = nullptr;
    DRAWBOT_SupplierRef sup_ = nullptr;
    DRAWBOT_SurfaceRef surf_ = nullptr;
    float fontSize_ = 11;
    RGBA text_ = {0.85f, 0.85f, 0.85f, 1};
    RGBA panel_ = {0.14f, 0.14f, 0.14f, 1};
};

Rect Frame(const PF_EventExtra* ev)
{
    const PF_UnionableRect& f = ev->effect_win.current_frame;
    return {(float)f.left, (float)f.top, (float)(f.right - f.left), (float)(f.bottom - f.top)};
}

std::wstring Wide(const std::string& u8)
{
    if (u8.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), -1, nullptr, 0);
    std::wstring w((size_t)std::max(n - 1, 0), L'\0');
    if (n > 1) MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), -1, &w[0], n);
    return w;
}

constexpr float kPad = 5, kButtonH = 20, kGap = 4;

// the buttons along the bottom (the same layout for drawing and for clicks): Open Editor, then one per preset
struct Button {
    Rect r;
    int preset;  // -1 = Open Editor
};
std::vector<Button> Buttons(const Rect& f)
{
    std::vector<Button> b;
    const auto& presets = dx5::Presets();
    const float y = f.y + f.h - kPad - kButtonH, inner = f.w - 2 * kPad;
    const float editW = std::min(110.0f, inner * 0.4f);
    b.push_back({{f.x + kPad, y, editW, kButtonH}, -1});
    const int n = (int)presets.size();
    const float pw = (inner - editW - kGap * n) / std::max(n, 1);
    for (int i = 0; i < n; i++) b.push_back({{f.x + kPad + editW + kGap + i * (pw + kGap), y, pw, kButtonH}, i});
    return b;
}

dxg::EvalResult EvaluateNow(PF_InData* in_data, PF_ParamDef* params[], const dx5::Graph& g)
{
    dx5::EvalContext c;
    c.time = in_data->time_scale ? (double)in_data->current_time / in_data->time_scale : 0;
    c.seed = params[P_SEED]->u.sd.value;
    double now[kMacros];
    for (int k = 0; k < kMacros; k++) now[k] = params[P_MACRO + k]->u.fs_d.value;
    c.macro = [&](int k, double) { return now[k]; };
    int64_t nowI[kIntMacros];
    for (int k = 0; k < kIntMacros; k++) nowI[k] = params[P_INT + k]->u.sd.value;
    c.intMacro = [&](int k, double) { return nowI[k]; };
    std::string names[kLayers];
    LayerNames(in_data, names);
    for (int k = 0; k < kLayers; k++)
        if (!names[k].empty()) c.layersSet |= 1u << k;
    c.maxElements = 20000;  // the picture: enough to show the shape
    return dx5::Evaluate(g, c);
}

void DrawOverview(Canvas& c, PF_InData* in_data, const Rect& f, PF_ParamDef* params[])
{
    const std::shared_ptr<const dx5::Graph> g = GraphOf(params[P_GRAPH]->u.arb_d.value);
    const dxg::EvalResult r = EvaluateNow(in_data, params, *g);

    // the picture: the layout fitted into the box, seen straight on
    const float bottom = f.y + f.h - kPad - kButtonH - kGap;
    // room for the problem line under the picture when there is one
    const float warnH = r.warnings.empty() ? 0.0f : c.FontSize() * 1.5f;
    const float box = std::max(20.0f, std::min(bottom - f.y - kPad - warnH, 96.0f));
    const Rect pic = {f.x + kPad, f.y + kPad, box, box};
    c.FillRound(pic, 6, c.Shade(0.08f));
    if (!r.elements.empty()) {
        double x0 = 1e300, y0 = 1e300, x1 = -1e300, y1 = -1e300;
        for (const dxg::Element& e : r.elements)
            x0 = std::min(x0, e.pos[0]), y0 = std::min(y0, e.pos[1]), x1 = std::max(x1, e.pos[0]), y1 = std::max(y1, e.pos[1]);
        const double span = std::max({x1 - x0, y1 - y0, 1e-9});
        const float inner = box - 14, cx = pic.x + box * 0.5f, cy = pic.y + box * 0.5f;
        const float scale = (float)(inner / span);
        const float dot = std::clamp(inner / std::sqrt((float)r.elements.size()) * 0.28f, 1.0f, 4.0f);
        const size_t step = std::max<size_t>(1, r.elements.size() / 2000);
        for (size_t i = 0; i < r.elements.size(); i += step) {
            const dxg::Element& e = r.elements[i];
            if (!e.visible) continue;
            RGBA col = e.source >= 0 && e.source < kLayers ? kLayerColors[e.source] : c.Shade(0.75f);
            col = Canvas::Mix(c.Shade(0.08f), col, (float)std::clamp(e.opacity, 0.15, 1.0));
            const float x = r.elements.size() == 1 ? cx : cx + (float)((e.pos[0] - (x0 + x1) * 0.5) * scale);
            const float y = r.elements.size() == 1 ? cy : cy + (float)((e.pos[1] - (y0 + y1) * 0.5) * scale);
            c.Circle(x, y, dot, col);
        }
    }

    // the summary and the macros in use
    const float tx = pic.x + box + 10, tw = f.x + f.w - tx - kPad, fs = c.FontSize(), lh = fs * 1.3f;
    float y = f.y + kPad + fs * 1.2f;
    c.Text(tx, y, L"DupliX 2", c.Text(), fs * 1.3f, tw);
    y += lh + 2;
    wchar_t line[160];
    const size_t n = r.elements.size();
    if (!r.hasOutput)
        swprintf(line, 160, L"No Output node");
    else
        swprintf(line, 160, L"%zu %ls", n, n == 1 ? L"copy" : L"copies");
    c.Text(tx, y, line, c.Text(), 0, tw);
    // the macros in use: Int Macros (I1 ...) then Macros (M1 ...)
    const uint32_t usedI = dx5::UsedIntMacros(*g), used = dx5::UsedMacros(*g);
    int shown = 0, total = 0;
    for (int k = 0; k < 32; k++) total += (used >> k & 1) + (usedI >> k & 1);
    for (int pass = 0; pass < 2; pass++) {
        const bool isInt = pass == 0;
        const uint32_t u = isInt ? usedI : used;
        for (int k = 0; k < 32 && y + lh * 2 < bottom; k++) {
            if (!(u >> k & 1)) continue;
            y += lh;
            c.Text(tx, y, (isInt ? L"I" : L"M") + std::to_wstring(k + 1), c.Dim(), 0, 0);
            c.Text(tx + fs * 2.2f, y, Wide(dx5::MacroLabel(*g, isInt, k)), c.Text(), 0, tw - fs * 2.2f);
            shown++;
        }
    }
    if (shown < total) {
        y += lh;
        swprintf(line, 160, L"+ %d more", total - shown);
        c.Text(tx, y, line, c.Dim(), 0, tw);
    }
    if (!r.warnings.empty()) {
        // the whole width under the picture (beside it, it was cut off), and where the rest is
        const float wx = f.x + kPad, ww = f.w - 2 * kPad;
        const std::wstring head = r.warnings.size() == 1 ? L"\x26A0 " : L"\x26A0 " + std::to_wstring(r.warnings.size()) + L" problems (all in the editor): ";
        c.Text(wx, bottom - 2, head + Wide(r.warnings[0]), {0.95f, 0.6f, 0.3f, 1}, 0, ww);
    }

    for (const Button& b : Buttons(f)) {
        const std::wstring label = b.preset < 0 ? L"Open Editor" : Wide(dx5::Presets()[b.preset].name);
        if (b.preset < 0) c.FillRound(b.r, 4, c.Shade(0.2f));
        c.StrokeRound(b.r, 4, c.Shade(0.45f), 1);
        c.Text(b.r.x + b.r.w * 0.5f, b.r.y + b.r.h * 0.5f + fs * 0.36f, label, c.Text(), 0, b.r.w - 4,
               kDRAWBOT_TextAlignment_Center);
    }
}

PF_Err Draw(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_EventExtra* ev)
{
    if (ev->effect_win.area != PF_EA_CONTROL || ev->effect_win.index != P_OVERVIEW) return PF_Err_NONE;
    Canvas c(in_data, out_data, ev);
    if (!c.Ok()) return PF_Err_NONE;
    DrawOverview(c, in_data, Frame(ev), params);
    ev->evt_out_flags = PF_EO_HANDLED_EVENT;
    return PF_Err_NONE;
}

PF_Err Click(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_EventExtra* ev)
{
    if (ev->effect_win.area != PF_EA_CONTROL || ev->effect_win.index != P_OVERVIEW) return PF_Err_NONE;
    const Rect f = Frame(ev);
    const float px = (float)ev->u.do_click.screen_point.h, py = (float)ev->u.do_click.screen_point.v;
    for (const Button& b : Buttons(f)) {
        if (!b.r.Has(px, py)) continue;
        if (b.preset < 0) OpenEditor(in_data);
        else QueuePreset(in_data, b.preset);  // the graph and its macros, written by the idle hook
        out_data->out_flags |= PF_OutFlag_REFRESH_UI;
        ev->evt_out_flags = PF_EO_HANDLED_EVENT;
        break;
    }
    return PF_Err_NONE;
}

}  // namespace

PF_Err HandleEvent(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output,
                   PF_EventExtra* extra)
{
    (void)output;
    switch (extra->e_type) {
    case PF_Event_DRAW:
        return Draw(in_data, out_data, params, extra);
    case PF_Event_DO_CLICK:
        return Click(in_data, out_data, params, extra);
    default:
        return PF_Err_NONE;
    }
}

}  // namespace dx2

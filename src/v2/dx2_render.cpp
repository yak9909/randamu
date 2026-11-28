// DupliX 2 (design v4): the render. PreRender evaluates the graph (src/graph) at the frame's time with the macro
// values and checks out the layers; SmartRender draws each element as its Source (Layer 1..8 or this layer) turned,
// scaled and faded, in list order, seen straight on (orthographic: z does not move a copy; a turned copy shows its
// turned outline). Render Mode 3D Layers draws the same for now (design v4 P6).
//
// Coordinates: the graph's positions are layer pixels from the layer's centre at full resolution. A copy's
// Source pixel at the Source layer's centre goes to the element's position.
//
// Tests: DUPLIX2_DUMP=<file> appends "t <seconds>" and the elements (graph::FormatElements) on every PreRender.
#include "v2/dx2.h"

#include <immintrin.h>
#include <ppl.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

namespace dx2 {
namespace {

constexpr int kInputId = 0;    // checkout ids: the input, then Layer k = 1 + k
constexpr int kOwn = kLayers;  // the Source slot of this layer

struct SourceRect {
    bool has = false;
    PF_LRect rect = {0, 0, 0, 0};  // the checked-out world's place in that layer's coordinates (downsampled)
    double pivX = 0, pivY = 0;     // the layer's centre in the world's pixels
    int w = 0, h = 0;
};

struct Copy {
    int src;                 // 0..7 = Layer, kOwn = this layer
    double px, py;           // where the Source's centre goes (output layer coordinates, downsampled)
    double ia, ib, ic, id;   // the inverse of the 2x2 map from Source px to output px
    double pivX, pivY;
    float opacity;
    bool translate;          // moved only (no turn, no scale): the same bilinear weights all along a row
    int x0, y0, x1, y1;      // its bounds in output layer coordinates
};

struct Pre {
    std::vector<Copy> copies;
    PF_LRect out = {0, 0, 0, 0};  // the result rect (output layer coordinates)
    bool has[kLayers + 1] = {};
};
void DeletePre(void* p) { delete (Pre*)p; }

bool RectEmpty(const PF_LRect& r) { return r.right <= r.left || r.bottom <= r.top; }

double Ratio(const PF_RationalScale& r) { return r.den ? (double)r.num / r.den : 1.0; }

std::mutex g_dumpMu;

// "t <seconds> layer <w> <h>": the layer's size tells the tests' comps apart
void Dump(double t, int w, int h, const dxg::EvalResult& r)
{
    char path[MAX_PATH] = {0};
    if (!GetEnvironmentVariableA("DUPLIX2_DUMP", path, sizeof(path))) return;
    std::lock_guard<std::mutex> lk(g_dumpMu);
    FILE* fp = nullptr;
    if (fopen_s(&fp, path, "a") || !fp) return;
    fprintf(fp, "t %.6f layer %d %d\n", t, w, h);
    fputs(dxg::FormatElements(r).c_str(), fp);
    fclose(fp);
}

}  // namespace

PF_Err PreRender(PF_InData* in_data, PF_OutData* out_data, PF_PreRenderExtra* extra)
{
    (void)out_data;
    auto* pre = new Pre();
    extra->output->pre_render_data = pre;
    extra->output->delete_pre_render_data_func = DeletePre;
    const double dsx = Ratio(in_data->downsample_x), dsy = Ratio(in_data->downsample_y);
    const double tSec = in_data->time_scale ? (double)in_data->current_time / in_data->time_scale : 0;

    // the graph and the seed (checked out at the frame's time)
    std::shared_ptr<const dx5::Graph> g;
    int seed = 0;
    PF_ParamDef d;
    AEFX_CLR_STRUCT(d);
    if (!PF_CHECKOUT_PARAM(in_data, P_GRAPH, in_data->current_time, in_data->time_step, in_data->time_scale, &d)) {
        g = GraphOf(d.u.arb_d.value);
        PF_CHECKIN_PARAM(in_data, &d);
    }
    if (!g) g = std::make_shared<dx5::Graph>();
    AEFX_CLR_STRUCT(d);
    if (!PF_CHECKOUT_PARAM(in_data, P_SEED, in_data->current_time, in_data->time_step, in_data->time_scale, &d)) {
        seed = d.u.sd.value;
        PF_CHECKIN_PARAM(in_data, &d);
    }
    // the macros the graph reads, at this frame's time; another time (later: births) is checked out when asked
    const uint32_t usedM = dx5::UsedMacros(*g), usedI = dx5::UsedIntMacros(*g);
    double now[kMacros] = {};
    for (int k = 0; k < kMacros; k++) {
        now[k] = dx5::MacroDefault(k);
        if (!(usedM >> k & 1)) continue;
        AEFX_CLR_STRUCT(d);
        if (!PF_CHECKOUT_PARAM(in_data, P_MACRO + k, in_data->current_time, in_data->time_step, in_data->time_scale, &d)) {
            now[k] = d.u.fs_d.value;
            PF_CHECKIN_PARAM(in_data, &d);
        }
    }
    int64_t nowI[kIntMacros] = {};
    for (int k = 0; k < kIntMacros; k++) {
        nowI[k] = dx5::IntMacroDefault(k);
        if (!(usedI >> k & 1)) continue;
        AEFX_CLR_STRUCT(d);
        if (!PF_CHECKOUT_PARAM(in_data, P_INT + k, in_data->current_time, in_data->time_step, in_data->time_scale, &d)) {
            nowI[k] = d.u.sd.value;
            PF_CHECKIN_PARAM(in_data, &d);
        }
    }
    auto intMacro = [&](int k, double t) -> int64_t {
        if (k < 0 || k >= kIntMacros) return 0;
        if (t == tSec) return nowI[k];
        const A_long at = (A_long)std::llround(t * in_data->time_scale);
        PF_ParamDef m;
        AEFX_CLR_STRUCT(m);
        if (PF_CHECKOUT_PARAM(in_data, P_INT + k, at, in_data->time_step, in_data->time_scale, &m)) return nowI[k];
        const int64_t v = m.u.sd.value;
        PF_CHECKIN_PARAM(in_data, &m);
        return v;
    };
    auto macro = [&](int k, double t) -> double {
        if (k < 0 || k >= kMacros) return 0;
        if (t == tSec) return now[k];
        const A_long at = (A_long)std::llround(t * in_data->time_scale);
        PF_ParamDef m;
        AEFX_CLR_STRUCT(m);
        if (PF_CHECKOUT_PARAM(in_data, P_MACRO + k, at, in_data->time_step, in_data->time_scale, &m)) return now[k];
        const double v = m.u.fs_d.value;
        PF_CHECKIN_PARAM(in_data, &m);
        return v;
    };

    // every layer whole, whatever part of the output is asked for
    PF_RenderRequest req = extra->input->output_request;
    req.rect.left = req.rect.top = -30000;
    req.rect.right = req.rect.bottom = 30000;
    req.preserve_rgb_of_zero_alpha = FALSE;
    SourceRect src[kLayers + 1];
    auto take = [&](const PF_CheckoutResult& r, SourceRect& s) {
        s.rect = r.result_rect;
        s.has = !RectEmpty(r.result_rect);
        s.w = r.result_rect.right - r.result_rect.left;
        s.h = r.result_rect.bottom - r.result_rect.top;
        s.pivX = r.ref_width * dsx * 0.5 - r.result_rect.left;
        s.pivY = r.ref_height * dsy * 0.5 - r.result_rect.top;
    };
    PF_CheckoutResult in_result;
    AEFX_CLR_STRUCT(in_result);
    PF_Err err = extra->cb->checkout_layer(in_data->effect_ref, P_INPUT, kInputId, &req, in_data->current_time,
                                           in_data->time_step, in_data->time_scale, &in_result);
    if (err) {
        Log("prerender: input checkout failed (%d)", (int)err);
        return err;
    }
    take(in_result, src[kOwn]);
    const double cx = in_result.ref_width * dsx * 0.5, cy = in_result.ref_height * dsy * 0.5;  // our layer's centre
    const uint32_t usedL = dx5::UsedLayers(*g);
    uint32_t layersSet = 0;
    for (int k = 0; k < kLayers; k++) {
        if (!(usedL >> k & 1)) continue;
        PF_CheckoutResult r;
        AEFX_CLR_STRUCT(r);
        // a Layer set to None checks out nothing (empty rects)
        if (extra->cb->checkout_layer(in_data->effect_ref, P_LAYER + k, 1 + k, &req, in_data->current_time,
                                      in_data->time_step, in_data->time_scale, &r) == PF_Err_NONE) {
            take(r, src[k]);
            if (src[k].has) layersSet |= 1u << k;
        }
    }

    dx5::EvalContext ctx;
    ctx.time = tSec;
    ctx.seed = seed;
    ctx.macro = macro;
    ctx.intMacro = intMacro;
    ctx.layersSet = layersSet;
    const dxg::EvalResult res = dx5::Evaluate(*g, ctx);
    Dump(tSec, (int)in_result.ref_width, (int)in_result.ref_height, res);
    // what went wrong, for the editor (the Overview has room for one line only)
    ReportProblems(InstanceId(in_data), g->revision, res.warnings);

    // the copies: the 2x2 map Source px -> output px is D M D^-1 (D = the downsampling, M at full resolution)
    double ux0 = 1e300, uy0 = 1e300, ux1 = -1e300, uy1 = -1e300;
    for (const dxg::Element& e : res.elements) {
        if (!e.visible || !(e.opacity > 0)) continue;
        const int si = (e.source >= 0 && e.source < kLayers) ? e.source : kOwn;
        const SourceRect& s = src[si];
        if (!s.has) continue;
        const double k = res.copyScale;
        const double m00 = e.basis[0] * e.scale[0] * k, m01 = e.basis[1] * e.scale[1] * k;
        const double m10 = e.basis[3] * e.scale[0] * k, m11 = e.basis[4] * e.scale[1] * k;
        const double a = m00, b = m01 * dsx / dsy, c = m10 * dsy / dsx, dd = m11;
        const double det = a * dd - b * c;
        if (!(std::fabs(det) > 1e-9)) continue;  // edge-on or no size
        Copy cp;
        cp.src = si;
        cp.px = cx + e.pos[0] * dsx;
        cp.py = cy + e.pos[1] * dsy;
        cp.ia = dd / det, cp.ib = -b / det, cp.ic = -c / det, cp.id = a / det;
        cp.pivX = s.pivX, cp.pivY = s.pivY;
        cp.opacity = (float)std::min(1.0, e.opacity);
        // one bilinear sample per output pixel, as DupliX's fast mode (src/core/raster.cpp) draws: n x n samples for a
        // copy smaller than its Source (up to 64) made Copy Scale 25% cost 7.5 times 100% (11/29, dx2_perf_compare.jsx:
        // 1013 ms against 135 ms for 25 copies of 1920 x 1080)
        cp.translate = std::fabs(cp.ia - 1) < 1e-9 && std::fabs(cp.id - 1) < 1e-9 && std::fabs(cp.ib) < 1e-9 &&
                       std::fabs(cp.ic) < 1e-9;
        double bx0 = 1e300, by0 = 1e300, bx1 = -1e300, by1 = -1e300;
        for (int j = 0; j < 4; j++) {
            const double sx = (j & 1 ? s.w : 0) - s.pivX, sy = (j & 2 ? s.h : 0) - s.pivY;
            const double ox = cp.px + a * sx + b * sy, oy = cp.py + c * sx + dd * sy;
            bx0 = std::min(bx0, ox), by0 = std::min(by0, oy), bx1 = std::max(bx1, ox), by1 = std::max(by1, oy);
        }
        constexpr double kLim = 1 << 20;
        cp.x0 = (int)std::floor(std::max(bx0, -kLim)) - 1, cp.y0 = (int)std::floor(std::max(by0, -kLim)) - 1;
        cp.x1 = (int)std::ceil(std::min(bx1, kLim)) + 1, cp.y1 = (int)std::ceil(std::min(by1, kLim)) + 1;
        if (cp.x1 <= cp.x0 || cp.y1 <= cp.y0) continue;
        ux0 = std::min(ux0, (double)cp.x0), uy0 = std::min(uy0, (double)cp.y0);
        ux1 = std::max(ux1, (double)cp.x1), uy1 = std::max(uy1, (double)cp.y1);
        pre->copies.push_back(cp);
        pre->has[si] = true;
    }

    PF_LRect max = {0, 0, 0, 0};
    if (!pre->copies.empty()) max = {(A_long)ux0, (A_long)uy0, (A_long)ux1, (A_long)uy1};
    const PF_LRect& want = extra->input->output_request.rect;
    PF_LRect res_rect = {std::max(max.left, want.left), std::max(max.top, want.top), std::min(max.right, want.right),
                         std::min(max.bottom, want.bottom)};
    if (RectEmpty(res_rect)) res_rect = {0, 0, 0, 0};
    pre->out = res_rect;
    extra->output->result_rect = res_rect;
    extra->output->max_result_rect = max;
    extra->output->solid = FALSE;
    extra->output->flags = PF_RenderOutputFlag_RETURNS_EXTRA_PIXELS;
    static std::atomic<int> logged{0};
    if (logged.fetch_add(1) < 200)
        Log("prerender: t %.3f, %zu elements, %zu copies drawn, out (%d,%d)-(%d,%d), request (%d,%d)-(%d,%d), ds %.3f/%.3f%s",
            tSec, res.elements.size(), pre->copies.size(), res_rect.left, res_rect.top, res_rect.right, res_rect.bottom,
            want.left, want.top, want.right, want.bottom, dsx, dsy, res.warnings.empty() ? "" : " (warnings)");
    return PF_Err_NONE;
}

namespace {

struct Px {
    float a, r, g, b;  // premultiplied, 0..1
};

template <class P>
struct Fmt;
template <>
struct Fmt<PF_Pixel8> {
    static constexpr float kMax = 255.0f;
};
template <>
struct Fmt<PF_Pixel16> {
    static constexpr float kMax = 32768.0f;
};
template <>
struct Fmt<PF_PixelFloat> {
    static constexpr float kMax = 1.0f;
};

// AE's effect worlds hold straight (unpremultiplied) alpha (src/core/raster.h): filtering and "over" are done
// premultiplied, and the result goes back to straight once per pixel.
template <class P>
inline Px Load(const P& p)
{
    const float s = 1.0f / Fmt<P>::kMax;
    const float a = p.alpha * s;
    return {a, p.red * s * a, p.green * s * a, p.blue * s * a};
}
template <class T>
inline T Clamp(float v, float mx)
{
    v = v * mx + 0.5f;
    if (v < 0) v = 0;
    if (v > mx) v = mx;
    return (T)v;
}
inline Px Straight(const Px& v)
{
    if (!(v.a > 0)) return {0, 0, 0, 0};
    const float k = 1.0f / v.a;
    return {v.a, v.r * k, v.g * k, v.b * k};
}
inline void Store(PF_Pixel8& p, const Px& pm)
{
    const Px v = Straight(pm);
    p.alpha = Clamp<A_u_char>(v.a, 255), p.red = Clamp<A_u_char>(v.r, 255), p.green = Clamp<A_u_char>(v.g, 255),
    p.blue = Clamp<A_u_char>(v.b, 255);
}
inline void Store(PF_Pixel16& p, const Px& pm)
{
    const Px v = Straight(pm);
    p.alpha = Clamp<A_u_short>(v.a, 32768), p.red = Clamp<A_u_short>(v.r, 32768),
    p.green = Clamp<A_u_short>(v.g, 32768), p.blue = Clamp<A_u_short>(v.b, 32768);
}
inline void Store(PF_PixelFloat& p, const Px& pm)
{
    const Px v = Straight(pm);
    p.alpha = v.a, p.red = v.r, p.green = v.g, p.blue = v.b;
}

// A Source converted once per render: premultiplied floats with a transparent border of one pixel all round, so
// bilinear sampling needs no bounds checks (11/28: converting and checking for each of 4 taps of every sample made
// a pixel cost ~280 ns; 25 copies of a 1920x1080 layer took 2.3 s).
struct Img {
    int w = 0, h = 0, stride = 0;  // the Source's size; stride = w + 2
    std::vector<Px> px;
    const Px* At(int x, int y) const { return &px[(size_t)(y + 1) * stride + (x + 1)]; }  // x, y from -1 to w / h
};

template <class P>
void Convert(const PF_EffectWorld* w, Img& img, int y0, int y1)
{
    for (int y = y0; y < y1; y++) {
        const P* row = (const P*)((const char*)w->data + (size_t)y * w->rowbytes);
        Px* d = &img.px[(size_t)(y + 1) * img.stride + 1];
        for (int x = 0; x < img.w; x++) d[x] = Load(row[x]);
    }
}

// a pixel (a, r, g, b) in one SSE register (lane 0 = alpha)
inline __m128 V(const Px* p) { return _mm_loadu_ps(&p->a); }

// bilinear in img (centres at +0.5): (x, y) must lie in [-0.5, w + 0.5) x [-0.5, h + 0.5)
inline __m128 Bilinear(const Img& img, double x, double y)
{
    x -= 0.5, y -= 0.5;
    const int x0 = (int)std::floor(x), y0 = (int)std::floor(y);
    const float fx = (float)(x - x0), fy = (float)(y - y0);
    const Px* p = img.At(x0, y0);
    const __m128 top = _mm_add_ps(_mm_mul_ps(V(p), _mm_set1_ps(1 - fx)), _mm_mul_ps(V(p + 1), _mm_set1_ps(fx)));
    const __m128 bot = _mm_add_ps(_mm_mul_ps(V(p + img.stride), _mm_set1_ps(1 - fx)),
                                  _mm_mul_ps(V(p + img.stride + 1), _mm_set1_ps(fx)));
    return _mm_add_ps(_mm_mul_ps(top, _mm_set1_ps(1 - fy)), _mm_mul_ps(bot, _mm_set1_ps(fy)));
}

// The copies are drawn nearest first (the list's last first), each UNDER what is there: d = d + s k (1 - d.a). The
// result is the same as "over" from the first; a pixel already opaque takes no more samples (DupliX's fast mode skips
// what an opaque copy hides the same way, src/core/raster.cpp: 11/29, 25 opaque copies at 300% took 0.7-1.9 s).
constexpr float kOpaque = 0.9999f;
inline void Under(Px* d, __m128 s, __m128 k)
{
    const __m128 dv = V(d);
    const __m128 t = _mm_sub_ps(_mm_set1_ps(1.0f), _mm_shuffle_ps(dv, dv, 0));
    _mm_storeu_ps(&d->a, _mm_add_ps(dv, _mm_mul_ps(_mm_mul_ps(s, k), t)));
}
inline bool Opaque(const Px* d) { return d->a >= kOpaque; }

// The part [x0, x1) of row Y (output layer coordinates, pixel centres at x + 0.5) whose sample of a copy falls
// inside its Source (with the border): solves lo <= p0 + q * X <= hi for X on both axes.
bool Span(const Copy& c, const Img& img, int Y, int* xa, int* xb)
{
    const double dy = Y + 0.5 - c.py;
    // s(X) = piv + inv * (X + 0.5 - px, dy): sx = ax + ia * X, sy = ay + ic * X
    const double ax = c.pivX + c.ia * (0.5 - c.px) + c.ib * dy, ay = c.pivY + c.ic * (0.5 - c.px) + c.id * dy;
    double lo = -1e18, hi = 1e18;
    auto clip = [&](double a, double q, double mn, double mx) {
        if (std::fabs(q) < 1e-12) {
            if (a < mn || a > mx) lo = 1, hi = 0;
            return;
        }
        double t0 = (mn - a) / q, t1 = (mx - a) / q;
        if (t0 > t1) std::swap(t0, t1);
        lo = std::max(lo, t0), hi = std::min(hi, t1);
    };
    // below w + 0.5 (not at it): the last tap must stay inside the border
    clip(ax, c.ia, -0.5, img.w + 0.4999);
    clip(ay, c.ic, -0.5, img.h + 0.4999);
    if (!(lo <= hi)) return false;
    *xa = std::max((int)std::ceil(lo - 1e-9), c.x0);
    *xb = std::min((int)std::floor(hi + 1e-9) + 1, c.x1);
    return *xa < *xb;
}

template <class P>
void DrawRows(PF_EffectWorld* out, const Pre& pre, const Img* const* imgs, int yBegin, int yEnd)
{
    const int ox = pre.out.left, oy = pre.out.top, W = (int)out->width;
    std::vector<Px> acc((size_t)W);  // the row, premultiplied
    for (int y = yBegin; y < yEnd; y++) {
        P* row = (P*)((char*)out->data + (size_t)y * out->rowbytes);
        std::fill(acc.begin(), acc.end(), Px{0, 0, 0, 0});
        bool any = false;
        int opaque = 0;  // pixels of the row already opaque: when all are, the copies behind are not drawn
        const int Y = y + oy;
        for (auto it = pre.copies.rbegin(); it != pre.copies.rend() && opaque < W; ++it) {
            const Copy& c = *it;
            if (Y < c.y0 || Y >= c.y1) continue;
            const Img* img = imgs[c.src];
            if (!img) continue;
            int xa = 0, xb = 0;
            if (!Span(c, *img, Y, &xa, &xb)) continue;
            xa = std::max(xa, ox), xb = std::min(xb, ox + W);
            if (xa >= xb) continue;
            any = true;
            const __m128 k = _mm_set1_ps(c.opacity);
            const double dy = Y + 0.5 - c.py;
            double sx = c.pivX + c.ia * (xa + 0.5 - c.px) + c.ib * dy;
            double sy = c.pivY + c.ic * (xa + 0.5 - c.px) + c.id * dy;
            Px* d = &acc[(size_t)(xa - ox)];
            const int n = xb - xa;
            auto put = [&](Px* q, __m128 s) {
                Under(q, s, k);
                if (Opaque(q)) opaque++;
            };
            if (c.translate) {
                // moved only: the same weights all along the row (one tap when the pixels line up)
                const double fx0 = sx - 0.5, fy0 = sy - 0.5;
                const int x0 = (int)std::floor(fx0), y0 = (int)std::floor(fy0);
                const float fx = (float)(fx0 - x0), fy = (float)(fy0 - y0);
                const Px* p = img->At(x0, y0);
                if (fx < 1e-4f && fy < 1e-4f) {
                    for (int i = 0; i < n; i++)
                        if (!Opaque(d + i)) put(d + i, V(p + i));
                } else {
                    const __m128 wa = _mm_set1_ps((1 - fx) * (1 - fy)), wb = _mm_set1_ps(fx * (1 - fy));
                    const __m128 wc = _mm_set1_ps((1 - fx) * fy), wd = _mm_set1_ps(fx * fy);
                    const Px* q = p + img->stride;
                    for (int i = 0; i < n; i++) {
                        if (Opaque(d + i)) continue;
                        const __m128 s = _mm_add_ps(_mm_add_ps(_mm_mul_ps(V(p + i), wa), _mm_mul_ps(V(p + i + 1), wb)),
                                                    _mm_add_ps(_mm_mul_ps(V(q + i), wc), _mm_mul_ps(V(q + i + 1), wd)));
                        put(d + i, s);
                    }
                }
            } else {
                for (int i = 0; i < n; i++, sx += c.ia, sy += c.ic)
                    if (!Opaque(d + i)) put(d + i, Bilinear(*img, sx, sy));
            }
        }
        if (any)
            for (int x = 0; x < W; x++) Store(row[x], acc[(size_t)x]);
    }
}

// runs f(a, b) over [0, n) split across the cores, on PPL's threads (kept between calls: a new std::thread in
// AE costs every loaded DLL's thread attach, ~13 ms a call for 7 threads, 11/28)
template <class F>
void Parallel(int n, F f)
{
    const int threads = std::clamp((int)std::thread::hardware_concurrency(), 1, 16);
    const int chunk = (n + threads - 1) / threads;
    concurrency::parallel_for(0, threads, [&](int t) {
        const int a = t * chunk, b = std::min(n, a + chunk);
        if (a < b) f(a, b);
    });
}

template <class P>
void Draw(PF_EffectWorld* out, const Pre& pre, const PF_EffectWorld* const* worlds)
{
    for (int y = 0; y < out->height; y++) memset((char*)out->data + (size_t)y * out->rowbytes, 0, (size_t)out->width * sizeof(P));
    if (pre.copies.empty()) return;
    const auto t0 = std::chrono::steady_clock::now();
    // kept per render thread: a new 33 MB buffer each frame costs its first-touch page faults (MFR: one per thread)
    thread_local Img store[kLayers + 1];
    const Img* imgs[kLayers + 1] = {};
    for (int k = 0; k <= kLayers; k++) {
        const PF_EffectWorld* w = worlds[k];
        if (!w || w->width <= 0 || w->height <= 0) continue;
        Img& img = store[k];
        img.w = w->width, img.h = w->height, img.stride = w->width + 2;
        img.px.resize((size_t)img.stride * (img.h + 2));
        // only the border must be clear (the inside is overwritten)
        std::fill(img.px.begin(), img.px.begin() + img.stride, Px{0, 0, 0, 0});
        std::fill(img.px.end() - img.stride, img.px.end(), Px{0, 0, 0, 0});
        for (int y = 1; y <= img.h; y++) {
            img.px[(size_t)y * img.stride] = Px{0, 0, 0, 0};
            img.px[(size_t)y * img.stride + img.stride - 1] = Px{0, 0, 0, 0};
        }
        Parallel(img.h, [&](int a, int b) { Convert<P>(w, img, a, b); });
        imgs[k] = &img;
    }
    const auto t1 = std::chrono::steady_clock::now();
    Parallel(out->height, [&](int a, int b) { DrawRows<P>(out, pre, imgs, a, b); });
    static std::atomic<int> logged{0};
    if (logged.fetch_add(1) < 50)
        Log("draw: convert %.1f ms, rows %.1f ms", std::chrono::duration<double, std::milli>(t1 - t0).count(),
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t1).count());
}

}  // namespace

PF_Err SmartRender(PF_InData* in_data, PF_OutData* out_data, PF_SmartRenderExtra* extra)
{
    (void)out_data;
    using Clock = std::chrono::steady_clock;
    const auto t0 = Clock::now();
    const Pre* pre = (const Pre*)extra->input->pre_render_data;
    PF_EffectWorld* outW = nullptr;
    PF_EffectWorld* worlds[kLayers + 1] = {};
    PF_Err err = PF_Err_NONE;
    // the layers' pixels before the output (the other way round, AE answered BAD_CALLBACK_PARAM for the input)
    if (pre) {
        if (pre->has[kOwn]) {
            const PF_Err e = extra->cb->checkout_layer_pixels(in_data->effect_ref, kInputId, &worlds[kOwn]);
            if (e || !worlds[kOwn]) {
                Log("render: this layer's pixels: err %d (world %p)", (int)e, (void*)worlds[kOwn]);
                worlds[kOwn] = nullptr;
            }
        }
        for (int k = 0; k < kLayers; k++)
            if (pre->has[k] && extra->cb->checkout_layer_pixels(in_data->effect_ref, 1 + k, &worlds[k])) worlds[k] = nullptr;
    }
    err = extra->cb->checkout_output(in_data->effect_ref, &outW);
    if (err) Log("render: output failed (%d)", (int)err);
    if (!err && outW && pre) {
        PF_WorldSuite2* ws = nullptr;
        PF_PixelFormat fmt = PF_PixelFormat_INVALID;
        if (in_data->pica_basicP->AcquireSuite(kPFWorldSuite, kPFWorldSuiteVersion2, (const void**)&ws) == kSPNoError && ws) {
            ws->PF_GetPixelFormat(outW, &fmt);
            in_data->pica_basicP->ReleaseSuite(kPFWorldSuite, kPFWorldSuiteVersion2);
        }
        const auto t1 = Clock::now();
        // every layer has the output's depth (AE converts layers to the project's depth)
        switch (fmt) {
        case PF_PixelFormat_ARGB32: Draw<PF_Pixel8>(outW, *pre, worlds); break;
        case PF_PixelFormat_ARGB64: Draw<PF_Pixel16>(outW, *pre, worlds); break;
        case PF_PixelFormat_ARGB128: Draw<PF_PixelFloat>(outW, *pre, worlds); break;
        default:
            Log("render: unknown pixel format %d", (int)fmt);
            err = PF_Err_BAD_CALLBACK_PARAM;
            break;
        }
        static std::atomic<int> logged{0};
        auto ms = [](Clock::duration d) { return std::chrono::duration<double, std::milli>(d).count(); };
        if (logged.fetch_add(1) < 200)
            Log("render: %zu copies, format %d, output %d x %d, own layer %d x %d: checkout %.1f ms, draw %.1f ms",
                pre->copies.size(), (int)fmt, (int)outW->width, (int)outW->height,
                worlds[kOwn] ? (int)worlds[kOwn]->width : 0, worlds[kOwn] ? (int)worlds[kOwn]->height : 0, ms(t1 - t0),
                ms(Clock::now() - t1));
    }
    if (err) Log("render: failed (%d)", (int)err);
    for (int k = 0; k < kLayers; k++)
        if (worlds[k]) extra->cb->checkin_layer_pixels(in_data->effect_ref, 1 + k);
    if (worlds[kOwn]) extra->cb->checkin_layer_pixels(in_data->effect_ref, kInputId);
    return err;
}

}  // namespace dx2

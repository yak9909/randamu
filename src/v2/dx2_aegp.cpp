// DupliX 2 (design v4): the parts that go through AEGP in UI commands (never in a render): the instance id, the
// rows shown in the Effect Controls, the macros' slider ranges, the layers' names.
#include "v2/dx2.h"

#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <map>
#include <mutex>
#include <vector>

namespace dx2 {

AEGP_PluginID g_pluginId = 0;

void Log(const char* fmt, ...)
{
    char path[MAX_PATH] = {0};
    // DUPLIX2_LOG, else the tests' DUPLIX_LOG (the same file as DupliX's, lines start with "dx2:")
    if (GetEnvironmentVariableA("DUPLIX2_LOG", path, sizeof(path)) == 0 &&
        GetEnvironmentVariableA("DUPLIX_LOG", path, sizeof(path)) == 0)
        return;
    FILE* fp = nullptr;
    if (fopen_s(&fp, path, "a") || !fp) return;
    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(fp, "[%02d:%02d:%02d.%03d][tid %lu] dx2: ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
            GetCurrentThreadId());
    va_list ap;
    va_start(ap, fmt);
    vfprintf(fp, fmt, ap);
    va_end(ap);
    fputc('\n', fp);
    fclose(fp);
}

namespace {

// per instance, keyed by the id in its sequence data (AE passes another effect_ref on every command)
struct Instance {
    std::vector<signed char> hidden;  // what this instance last set per stream (-1 = never set)
};
std::mutex g_mu;
std::map<A_u_longlong, Instance> g_inst;
std::atomic<A_u_longlong> g_nextId{1};

struct SeqData {
    A_u_longlong magic;
    A_u_longlong id;  // this session's id of the instance (a new one after loading or duplicating)
};
constexpr A_u_longlong kSeqMagic = 0x44583253455132ULL;  // "DX2SEQ2"

// id 0 (no sequence data): nothing is remembered, every row is set each time
Instance& Inst(PF_InData* in_data)
{
    const A_u_longlong id = InstanceId(in_data);
    Instance& s = g_inst[id];
    if (s.hidden.size() != (size_t)P_NUM || id == 0) s.hidden.assign(P_NUM, -1);
    return s;
}

// The effect's streams (one AEGP effect handle for the call).
struct Streams {
    SPBasicSuite* sp;
    Suite<AEGP_PFInterfaceSuite1> pfi;
    Suite<AEGP_EffectSuite4> es;
    Suite<AEGP_StreamSuite6> ss;
    Suite<AEGP_KeyframeSuite5> ks;
    AEGP_EffectRefH eff = nullptr;
    explicit Streams(PF_InData* in_data)
        : sp(in_data->pica_basicP), pfi(sp, kAEGPPFInterfaceSuite, kAEGPPFInterfaceSuiteVersion1),
          es(sp, kAEGPEffectSuite, kAEGPEffectSuiteVersion4), ss(sp, kAEGPStreamSuite, kAEGPStreamSuiteVersion6),
          ks(sp, kAEGPKeyframeSuite, kAEGPKeyframeSuiteVersion5)
    {
        if (pfi && es && ss && g_pluginId) pfi->AEGP_GetNewEffectForEffect(g_pluginId, in_data->effect_ref, &eff);
    }
    ~Streams()
    {
        if (eff) es->AEGP_DisposeEffect(eff);
    }
    explicit operator bool() const { return eff != nullptr; }
    AEGP_StreamRefH Get(int index) const
    {
        AEGP_StreamRefH s = nullptr;
        if (eff && ss->AEGP_GetNewEffectStreamByIndex(g_pluginId, eff, index, &s)) s = nullptr;
        return s;
    }
    void Dispose(AEGP_StreamRefH s) const
    {
        if (s) ss->AEGP_DisposeStream(s);
    }
    // keys or an expression: the row must stay shown (hiding it would hide the user's work)
    bool Animated(int index) const
    {
        AEGP_StreamRefH s = Get(index);
        if (!s) return false;
        A_long n = 0;
        A_Boolean expr = FALSE;
        if (ks) ks->AEGP_GetStreamNumKFs(s, &n);
        ss->AEGP_GetExpressionState(g_pluginId, s, &expr);
        Dispose(s);
        return n > 0 || expr;
    }
    A_long LayerId(PF_InData* in_data, int index) const
    {
        AEGP_StreamRefH s = Get(index);
        if (!s) return 0;
        A_Time t = {in_data->current_time, (A_u_long)(in_data->time_scale ? in_data->time_scale : 1)};
        AEGP_StreamValue2 v;
        AEFX_CLR_STRUCT(v);
        A_long id = 0;
        if (!ss->AEGP_GetNewStreamValue(g_pluginId, s, AEGP_LTimeMode_LayerTime, &t, TRUE, &v)) {
            id = v.val.layer_id;
            ss->AEGP_DisposeStreamValue(&v);
        }
        Dispose(s);
        return id;
    }
};

std::string Utf8(const A_UTF16Char* w)
{
    if (!w) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, (const wchar_t*)w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return {};
    std::string s((size_t)n - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, (const wchar_t*)w, -1, &s[0], n, nullptr, nullptr);
    return s;
}

// Hides / shows the rows whose state differs from what this instance last set. Two ways at once:
// AEGP's HIDDEN flag (the Effect Controls and the timeline) and PF_PUI_INVISIBLE through PF_UpdateParamUI.
bool ApplyHidden(PF_InData* in_data, PF_ParamDef* params[], const Streams& st, const std::vector<signed char>& want)
{
    std::vector<int> change;
    {
        std::lock_guard<std::mutex> lk(g_mu);
        Instance& s = Inst(in_data);
        for (int i = 1; i < P_NUM; i++)
            if (want[i] >= 0 && s.hidden[i] != want[i]) change.push_back(i);
    }
    if (change.empty()) return false;
    Suite<AEGP_DynamicStreamSuite4> ds(in_data->pica_basicP, kAEGPDynamicStreamSuite, kAEGPDynamicStreamSuiteVersion4);
    Suite<PF_ParamUtilsSuite3> pu(in_data->pica_basicP, kPFParamUtilsSuite, kPFParamUtilsSuiteVersion3);
    if (!ds) return false;
    std::vector<int> done;
    for (int i : change) {
        if (pu && params && params[i] && params[i]->param_type != PF_Param_GROUP_END && i != P_GRAPH) {
            PF_ParamDef d = *params[i];
            if (want[i]) d.ui_flags |= PF_PUI_INVISIBLE;
            else d.ui_flags &= ~PF_PUI_INVISIBLE;
            pu->PF_UpdateParamUI(in_data->effect_ref, i, &d);
            params[i]->ui_flags = d.ui_flags;
        }
        AEGP_StreamRefH s = st.Get(i);
        if (!s) continue;  // a group's end has no stream
        if (!ds->AEGP_SetDynamicStreamFlag(s, AEGP_DynStreamFlag_HIDDEN, FALSE, want[i] ? TRUE : FALSE)) done.push_back(i);
        st.Dispose(s);
    }
    std::lock_guard<std::mutex> lk(g_mu);
    Instance& s = Inst(in_data);
    for (int i : done) s.hidden[i] = want[i];
    return true;
}

}  // namespace

A_u_longlong InstanceId(PF_InData* in_data)
{
    if (!in_data || !in_data->sequence_data || PF_GET_HANDLE_SIZE(in_data->sequence_data) < sizeof(SeqData)) return 0;
    const SeqData* sd = (const SeqData*)PF_LOCK_HANDLE(in_data->sequence_data);
    const A_u_longlong id = sd->magic == kSeqMagic ? sd->id : 0;
    PF_UNLOCK_HANDLE(in_data->sequence_data);
    return id;
}

PF_Err SequenceSetup(PF_InData* in_data, PF_OutData* out_data)
{
    PF_Handle h = in_data->sequence_data;
    if (!h || PF_GET_HANDLE_SIZE(h) < sizeof(SeqData)) {
        if (h) PF_DISPOSE_HANDLE(h);
        h = PF_NEW_HANDLE(sizeof(SeqData));
        if (!h) return PF_Err_OUT_OF_MEMORY;
    }
    SeqData* sd = (SeqData*)PF_LOCK_HANDLE(h);
    sd->magic = kSeqMagic;
    sd->id = g_nextId.fetch_add(1);  // a loaded or duplicated instance starts with nothing remembered
    PF_UNLOCK_HANDLE(h);
    out_data->sequence_data = h;
    return PF_Err_NONE;
}

void SequenceSetdown(PF_InData* in_data, PF_OutData* out_data)
{
    {
        std::lock_guard<std::mutex> lk(g_mu);
        const A_u_longlong id = InstanceId(in_data);
        if (id) g_inst.erase(id);
    }
    if (in_data->sequence_data) {
        PF_DISPOSE_HANDLE(in_data->sequence_data);
        out_data->sequence_data = nullptr;
    }
}

bool UpdateParamsUI(PF_InData* in_data, PF_ParamDef* params[])
{
    if (!params || !params[P_GRAPH]) return false;
    const auto t0 = std::chrono::steady_clock::now();
    Streams st(in_data);
    if (!st) return false;
    const std::shared_ptr<const dx5::Graph> g = GraphOf(params[P_GRAPH]->u.arb_d.value);
    const uint32_t usedM = dx5::UsedMacros(*g), usedI = dx5::UsedIntMacros(*g), usedL = dx5::UsedLayers(*g);
    std::vector<signed char> want(P_NUM, 0);
    // tests only: DUPLIX2_SHOW_ALL=1 shows every row (a script cannot set a hidden parameter)
    const bool all = GetEnvironmentVariableA("DUPLIX2_SHOW_ALL", nullptr, 0) > 0;
    auto group = [&](int topic, int first, int count, auto shown) {
        bool any = false;
        for (int k = 0; k < count; k++) {
            const bool on = all || shown(k);
            want[first + k] = on ? 0 : 1;
            any |= on;
        }
        want[topic] = want[first + count] = any ? 0 : 1;
    };
    group(P_MACRO_TOPIC, P_MACRO, kMacros, [&](int k) { return (usedM >> k & 1) || st.Animated(P_MACRO + k); });
    group(P_INT_TOPIC, P_INT, kIntMacros, [&](int k) { return (usedI >> k & 1) || st.Animated(P_INT + k); });
    group(P_COLOR_TOPIC, P_COLOR, kColorMacros, [&](int k) { return st.Animated(P_COLOR + k); });
    group(P_POINT_TOPIC, P_POINT, kPointMacros, [&](int k) { return st.Animated(P_POINT + k); });
    group(P_LAYER_TOPIC, P_LAYER, kLayers, [&](int k) { return (usedL >> k & 1) || st.LayerId(in_data, P_LAYER + k) != 0; });
    want[P_GRAPH] = 1;  // never shown (PF_PUI_NO_ECW_UI; the flag keeps it out of the timeline too)
    // EXPERIMENT want[P_REVISION] = 1;
    bool changed = ApplyHidden(in_data, params, st, want);

    // the macros' slider ranges from the graph (PF_UpdateParamUI is not saved: applied again every time)
    Suite<PF_ParamUtilsSuite3> pu(in_data->pica_basicP, kPFParamUtilsSuite, kPFParamUtilsSuiteVersion3);
    int ranges = 0;
    for (int k = 0; pu && k < kMacros; k++) {
        PF_ParamDef* p = params[P_MACRO + k];
        if (!p || p->param_type != PF_Param_FLOAT_SLIDER) continue;
        const dx5::MacroSetting& m = g->macros[k];
        const PF_FpShort lo = (PF_FpShort)m.sliderMin, hi = (PF_FpShort)m.sliderMax;
        if (p->u.fs_d.slider_min == lo && p->u.fs_d.slider_max == hi) continue;
        PF_ParamDef d = *p;
        d.u.fs_d.slider_min = lo;
        d.u.fs_d.slider_max = hi;
        if (!pu->PF_UpdateParamUI(in_data->effect_ref, P_MACRO + k, &d)) {
            p->u.fs_d.slider_min = lo, p->u.fs_d.slider_max = hi;
            ranges++;
        }
    }
    for (int k = 0; pu && k < kIntMacros; k++) {
        PF_ParamDef* p = params[P_INT + k];
        if (!p || p->param_type != PF_Param_SLIDER) continue;
        const dx5::MacroSetting& m = g->intMacros[k];
        const PF_ParamValue lo = (PF_ParamValue)std::llround(m.sliderMin), hi = (PF_ParamValue)std::llround(m.sliderMax);
        if (p->u.sd.slider_min == lo && p->u.sd.slider_max == hi) continue;
        PF_ParamDef d = *p;
        d.u.sd.slider_min = lo;
        d.u.sd.slider_max = hi;
        if (!pu->PF_UpdateParamUI(in_data->effect_ref, P_INT + k, &d)) {
            p->u.sd.slider_min = lo, p->u.sd.slider_max = hi;
            ranges++;
        }
    }
    changed |= ranges > 0;
    Log("params ui: macros used %08x, int macros used %08x, layers used %02x, %d range(s) set, %.1f ms (instance %llu)", usedM, usedI, usedL, ranges,
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(),
        (unsigned long long)InstanceId(in_data));
    return changed;
}

void LayerNames(PF_InData* in_data, std::string out[kLayers])
{
    for (int k = 0; k < kLayers; k++) out[k].clear();
    Streams st(in_data);
    if (!st) return;
    Suite<AEGP_LayerSuite9> ls(st.sp, kAEGPLayerSuite, kAEGPLayerSuiteVersion9);
    Suite<AEGP_MemorySuite1> ms(st.sp, kAEGPMemorySuite, kAEGPMemorySuiteVersion1);
    AEGP_LayerH self = nullptr;
    AEGP_CompH comp = nullptr;
    if (!ls || !ms || st.pfi->AEGP_GetEffectLayer(in_data->effect_ref, &self) || ls->AEGP_GetLayerParentComp(self, &comp))
        return;
    for (int k = 0; k < kLayers; k++) {
        const A_long id = st.LayerId(in_data, P_LAYER + k);
        AEGP_LayerH L = nullptr;
        if (!id || ls->AEGP_GetLayerFromLayerID(comp, id, &L) || !L) continue;
        AEGP_MemHandle nameH = nullptr, srcH = nullptr;
        if (ls->AEGP_GetLayerName(g_pluginId, L, &nameH, &srcH)) continue;
        for (AEGP_MemHandle h : {nameH, srcH}) {
            if (!h) continue;
            A_UTF16Char* p = nullptr;
            if (out[k].empty() && !ms->AEGP_LockMemHandle(h, (void**)&p)) {
                out[k] = Utf8(p);
                ms->AEGP_UnlockMemHandle(h);
            }
        }
        if (nameH) ms->AEGP_FreeMemHandle(nameH);
        if (srcH) ms->AEGP_FreeMemHandle(srcH);
        if (out[k].empty()) out[k] = "Layer " + std::to_string(k + 1);
    }
}

}  // namespace dx2

namespace dx2 {
namespace {
struct ProblemReport {
    uint32_t revision = 0;
    std::vector<std::string> problems;
};
std::mutex g_problemMu;
std::map<A_u_longlong, ProblemReport> g_problems;
}  // namespace

void ReportProblems(A_u_longlong instance, uint32_t revision, const std::vector<std::string>& problems)
{
    if (!instance) return;
    std::lock_guard<std::mutex> lk(g_problemMu);
    ProblemReport& r = g_problems[instance];
    r.revision = revision;
    r.problems = problems;
}

bool Problems(A_u_longlong instance, uint32_t revision, std::vector<std::string>* out)
{
    std::lock_guard<std::mutex> lk(g_problemMu);
    auto it = g_problems.find(instance);
    if (it == g_problems.end() || it->second.revision != revision) return false;
    *out = it->second.problems;
    return true;
}

}  // namespace dx2

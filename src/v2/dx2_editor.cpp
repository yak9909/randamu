// DupliX 2 (design v4, 1.2 and 7): the editor's link to After Effects. What P0 settled (spike/P0Graph):
//   - a change is queued and written by AE's idle hook (a write from the window's own message handling never
//     returned from AEGP_SetStreamValue), as one undo group: the graph data, and the macros it set (Expose, presets)
//   - after the write, PF_RefreshAllWindows (else the Overview and UPDATE_PARAMS_UI wait for another action)
//   - the graph's revision: the idle hook compares it and reloads after AE's own undo
//   - the editor follows its instance: SEQUENCE_SETDOWN (any thread) marks it gone; its effect reference is never
//     used after that (AE crashed in P0 when it was)
// The window itself is dx2_editor_win.cpp.
//
// Tests: DUPLIX2_TEST_SCRIPT=<file> opens the editor on an instance when its Random Seed changes and runs that
// editor script (graph/edit.h RunScript) on the editor's graph, committed like any edit.
#include "v2/dx2_editor.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <mutex>
#include <set>
#include <sstream>

namespace dx2 {
namespace {

struct Write {
    AEGP_EffectRefH eff = nullptr;
    bool ownsEff = false;           // disposed after the write (a test's own reference)
    A_u_longlong instance = 0;
    std::string text;               // the new graph data
    std::vector<dx5::MacroWrite> macros;
    std::string why;
    std::chrono::steady_clock::time_point notBefore{};  // the idle hook waits until then
};

std::mutex g_mu;                 // the queue and g_gone (SEQUENCE_SETDOWN may come on a render thread)
std::vector<Write> g_queue;
std::set<A_u_longlong> g_gone;   // instances whose SEQUENCE_SETDOWN came
ed::Session* g_session = nullptr;  // main thread only
std::vector<ed::Session*> g_old;    // closed editors whose writes may still be queued (main thread only)
int g_undo = 0;                  // AE commands asked by the editor: +1 undo, -1 redo (main thread)

bool Gone(A_u_longlong inst)
{
    std::lock_guard<std::mutex> lk(g_mu);
    return g_gone.count(inst) != 0;
}

AEGP_StreamRefH Stream(AEGP_EffectRefH eff, int index)
{
    Suite<AEGP_StreamSuite6> ss(g_pica, kAEGPStreamSuite, kAEGPStreamSuiteVersion6);
    AEGP_StreamRefH st = nullptr;
    if (!ss || ss->AEGP_GetNewEffectStreamByIndex(g_pluginId, eff, index, &st)) return nullptr;
    return st;
}

// The graph data of an instance now (through AEGP).
bool ReadGraphText(AEGP_EffectRefH eff, std::string* out)
{
    Suite<AEGP_StreamSuite6> ss(g_pica, kAEGPStreamSuite, kAEGPStreamSuiteVersion6);
    AEGP_StreamRefH st = Stream(eff, P_GRAPH);
    if (!ss || !st) return false;
    AEGP_StreamValue2 v{};
    A_Time t{0, 1};
    bool ok = false;
    if (!ss->AEGP_GetNewStreamValue(g_pluginId, st, AEGP_LTimeMode_LayerTime, &t, FALSE, &v)) {
        *out = GraphText((PF_Handle)v.val.arbH);
        ok = true;
        ss->AEGP_DisposeStreamValue(&v);
    }
    ss->AEGP_DisposeStream(st);
    return ok;
}

void DisposeEffect(AEGP_EffectRefH eff)
{
    Suite<AEGP_EffectSuite4> es(g_pica, kAEGPEffectSuite, kAEGPEffectSuiteVersion4);
    if (es && eff) es->AEGP_DisposeEffect(eff);
}

// One undo step: the graph data and the macros that go with it.
void DoWrite(const Write& w)
{
    const auto t0 = std::chrono::steady_clock::now();
    Suite<AEGP_UtilitySuite6> util(g_pica, kAEGPUtilitySuite, kAEGPUtilitySuiteVersion6);
    Suite<AEGP_StreamSuite6> ss(g_pica, kAEGPStreamSuite, kAEGPStreamSuiteVersion6);
    Suite<AEGP_KeyframeSuite5> ks(g_pica, kAEGPKeyframeSuite, kAEGPKeyframeSuiteVersion5);
    if (!util || !ss) return;
    A_Err eStart = util->AEGP_StartUndoGroup("Edit DupliX Graph");
    A_Err eGraph = -1;
    AEGP_StreamRefH st = Stream(w.eff, P_GRAPH);
    AEGP_StreamValue2 v{};
    A_Time t{0, 1};
    if (st && !ss->AEGP_GetNewStreamValue(g_pluginId, st, AEGP_LTimeMode_LayerTime, &t, FALSE, &v)) {
        PF_Handle nh = NewGraphHandle(w.text);
        if (nh) {
            AEGP_StreamValue2 nv = v;
            nv.val.arbH = (AEGP_ArbBlockVal)nh;
            eGraph = ss->AEGP_SetStreamValue(g_pluginId, st, &nv);
            Suite<PF_HandleSuite1> hs(g_pica, kPFHandleSuite, kPFHandleSuiteVersion1);
            if (hs) hs->host_dispose_handle(nh);  // SetStreamValue copies it (COPY_FUNC), it does not adopt it
        }
        ss->AEGP_DisposeStreamValue(&v);
    }
    if (st) ss->AEGP_DisposeStream(st);
    // the hidden revision: its change makes AE draw the frame again (the graph data's does not)
    int revSet = 0;
    if (!eGraph) {
        AEGP_StreamRefH rs = Stream(w.eff, P_REVISION);
        AEGP_StreamValue2 rv{};
        if (rs && !ss->AEGP_GetNewStreamValue(g_pluginId, rs, AEGP_LTimeMode_LayerTime, &t, FALSE, &rv)) {
            AEGP_StreamValue2 nv = rv;
            nv.val.one_d = (double)(((long long)rv.val.one_d + 1) % 1000000);
            if (!ss->AEGP_SetStreamValue(g_pluginId, rs, &nv)) revSet = 1;
            ss->AEGP_DisposeStreamValue(&rv);
        }
        if (rs) ss->AEGP_DisposeStream(rs);
    }
    int macrosSet = 0;
    for (const dx5::MacroWrite& m : w.macros) {
        AEGP_StreamRefH ms = Stream(w.eff, (m.isInt ? P_INT : P_MACRO) + m.k);
        if (!ms) continue;
        A_long nk = 0;
        if (ks) ks->AEGP_GetStreamNumKFs(ms, &nk);
        if (nk > 0) {
            Log("write: %s %d has keys: its value is left as it is", m.isInt ? "Int Macro" : "Macro", m.k + 1);
        } else {
            AEGP_StreamValue2 mv{};
            if (!ss->AEGP_GetNewStreamValue(g_pluginId, ms, AEGP_LTimeMode_LayerTime, &t, FALSE, &mv)) {
                AEGP_StreamValue2 nv = mv;
                nv.val.one_d = m.value;
                if (!ss->AEGP_SetStreamValue(g_pluginId, ms, &nv)) macrosSet++;
                ss->AEGP_DisposeStreamValue(&mv);
            }
        }
        ss->AEGP_DisposeStream(ms);
    }
    // the macro rows the new graph reads are shown at once (UPDATE_PARAMS_UI comes only when the Effect Controls show
    // this layer, and a hidden row takes no keys: an Expose'd macro must be keyable now). Hiding stays with it.
    int shown = 0;
    if (!eGraph) {
        Suite<AEGP_DynamicStreamSuite4> ds(g_pica, kAEGPDynamicStreamSuite, kAEGPDynamicStreamSuiteVersion4);
        const std::shared_ptr<const dx5::Graph> ng = GraphOfText(w.text);
        const uint32_t usedM = dx5::UsedMacros(*ng), usedI = dx5::UsedIntMacros(*ng);
        auto show = [&](int index) {
            AEGP_StreamRefH s = Stream(w.eff, index);
            if (!s) return;
            if (ds && !ds->AEGP_SetDynamicStreamFlag(s, AEGP_DynStreamFlag_HIDDEN, FALSE, FALSE)) shown++;
            ss->AEGP_DisposeStream(s);
        };
        if (usedM) show(P_MACRO_TOPIC);
        if (usedI) show(P_INT_TOPIC);
        for (int k = 0; k < 32; k++) {
            if (usedM >> k & 1) show(P_MACRO + k);
            if (usedI >> k & 1) show(P_INT + k);
        }
    }
    A_Err eEnd = util->AEGP_EndUndoGroup();
    // the Overview and UPDATE_PARAMS_UI at once (P0)
    Suite<PF_AdvAppSuite2> adv(g_pica, kPFAdvAppSuite, kPFAdvAppSuiteVersion2);
    if (adv) adv->PF_RefreshAllWindows();
    Log("write (%s): graph %zu bytes (start %d set %d end %d), revision %s, %d of %zu macro(s) set, %d row(s) shown, %.2f ms",
        w.why.c_str(), w.text.size(), (int)eStart, (int)eGraph, (int)eEnd, revSet ? "set" : "NOT set", macrosSet, w.macros.size(), shown,
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
}

bool QueuedFor(AEGP_EffectRefH eff)
{
    std::lock_guard<std::mutex> lk(g_mu);
    for (const Write& w : g_queue)
        if (w.eff == eff) return true;
    return false;
}

// A closed session lets its reference go once its writes are done. Returns true if it was deleted.
bool Retire(ed::Session* s)
{
    if (QueuedFor(s->eff)) return false;
    const bool gone = Gone(s->instance);
    if (!gone) DisposeEffect(s->eff);  // the reference of an instance that went away is not touched (P0)
    Log("editor: session closed (instance %llu%s)", (unsigned long long)s->instance, gone ? ", gone" : "");
    delete s;
    return true;
}

// The editor's session after the writes: AE's undo noticed (by the revision), the reference let go when closed.
void TendSession()
{
    g_old.erase(std::remove_if(g_old.begin(), g_old.end(), Retire), g_old.end());
    ed::Session* s = g_session;
    if (!s) return;
    const bool gone = Gone(s->instance);
    if (s->closing || gone) {
        if (gone && s->wnd) PostMessageA(s->wnd, WM_CLOSE, 0, 0);
        if (!s->closing) return;
        g_session = nullptr;
        if (!Retire(s)) g_old.push_back(s);
        return;
    }
    if (QueuedFor(s->eff)) return;  // our own write comes first
    std::string text;
    dx5::Graph g;
    if (!ReadGraphText(s->eff, &text) || !dx5::Parse(text, &g)) return;
    if (g.revision != s->graph.revision) {
        Log("editor: the graph changed outside the editor: revision %u -> %u (reloaded)", (unsigned)s->graph.revision,
            (unsigned)g.revision);
        s->graph = g;
        if (s->wnd) ed::GraphReloaded(s->wnd);
    }
}

// Opens (or brings up) the editor on the instance of `in_data`. Returns the session, or nullptr.
ed::Session* Open(PF_InData* in_data)
{
    const A_u_longlong inst = InstanceId(in_data);
    if (g_session && g_session->instance == inst && !g_session->closing) {
        if (g_session->wnd) SetForegroundWindow(g_session->wnd);
        return g_session;
    }
    if (g_session) {
        // one editor at a time: the other instance's closes (its queued writes are still done, then it lets go)
        if (g_session->wnd) DestroyWindow(g_session->wnd);
        g_session->wnd = nullptr;
        g_session->closing = true;
        g_old.push_back(g_session);
        g_session = nullptr;
    }
    Suite<AEGP_PFInterfaceSuite1> pfi(in_data->pica_basicP, kAEGPPFInterfaceSuite, kAEGPPFInterfaceSuiteVersion1);
    AEGP_EffectRefH eff = nullptr;
    if (!pfi || pfi->AEGP_GetNewEffectForEffect(g_pluginId, in_data->effect_ref, &eff) || !eff) {
        Log("editor: no effect reference");
        return nullptr;
    }
    auto* s = new ed::Session();
    s->eff = eff;
    s->instance = inst;
    std::string text;
    if (!ReadGraphText(eff, &text) || !dx5::Parse(text, &s->graph)) s->graph = dx5::DefaultPreset().graph;
    g_session = s;
    s->wnd = ed::CreateEditorWindow(s);
    Log("editor: opened on instance %llu (revision %u, window %p)", (unsigned long long)inst, (unsigned)s->graph.revision,
        (void*)s->wnd);
    if (!s->wnd) s->closing = true;
    return s;
}

}  // namespace

namespace ed {

Session* Current() { return g_session && !g_session->closing ? g_session : nullptr; }

void Commit(const char* why, const std::vector<dx5::MacroWrite>& macros)
{
    Session* s = Current();
    if (!s) return;
    s->graph.revision++;
    Write w;
    w.eff = s->eff;
    w.instance = s->instance;
    w.text = dx5::Serialize(s->graph);
    w.macros = macros;
    w.why = why;
    std::lock_guard<std::mutex> lk(g_mu);
    g_queue.push_back(std::move(w));
}

void RequestUndo(bool redo) { g_undo += redo ? -1 : 1; }

void WindowGone(Session* s)
{
    if (!s) return;
    s->wnd = nullptr;
    s->closing = true;
}

}  // namespace ed

void EditorInstanceGone(A_u_longlong instance)
{
    if (!instance) return;
    Log("sequence setdown: instance %llu", (unsigned long long)instance);
    std::lock_guard<std::mutex> lk(g_mu);
    g_gone.insert(instance);
}

A_Err IdleHook(AEGP_GlobalRefcon, AEGP_IdleRefcon, A_long* max_sleep)
{
    std::vector<Write> todo;
    {
        std::lock_guard<std::mutex> lk(g_mu);
        const auto now = std::chrono::steady_clock::now();
        std::vector<Write> later;
        for (Write& w : g_queue) (w.notBefore <= now ? todo : later).push_back(std::move(w));
        g_queue.swap(later);
        // a write for an instance that went away is dropped, and its reference is not touched
        for (Write& w : todo)
            if (g_gone.count(w.instance)) {
                Log("write (%s): the instance went away, dropped", w.why.c_str());
                w.eff = nullptr;
            }
    }
    for (Write& w : todo) {
        if (!w.eff) continue;
        DoWrite(w);
        if (w.ownsEff) DisposeEffect(w.eff);
    }
    if (g_undo) {
        Suite<AEGP_CommandSuite1> cs(g_pica, kAEGPCommandSuite, kAEGPCommandSuiteVersion1);
        for (; cs && g_undo > 0; g_undo--) cs->AEGP_DoCommand(16);    // Edit > Undo
        for (; cs && g_undo < 0; g_undo++) cs->AEGP_DoCommand(2035);  // Edit > Redo
        g_undo = 0;
    }
    TendSession();
    // tests: DUPLIX2_IDLE_LOG=1 logs that AE's main loop is alive (once every 2 s)
    static auto lastBeat = std::chrono::steady_clock::now();
    static const bool beat = GetEnvironmentVariableA("DUPLIX2_IDLE_LOG", nullptr, 0) > 0;
    if (beat && std::chrono::steady_clock::now() - lastBeat > std::chrono::seconds(2)) {
        lastBeat = std::chrono::steady_clock::now();
        Log("idle: alive (editor %s)", g_session ? (g_session->wnd ? "open" : "closing") : "none");
    }
    if (g_session && max_sleep && *max_sleep > 6) *max_sleep = 6;  // the editor is open: come back soon (1/10 s)
    return A_Err_NONE;
}

void OpenEditor(PF_InData* in_data) { Open(in_data); }

void QueuePreset(PF_InData* in_data, int index, int delayMs)
{
    if (index < 0 || index >= (int)dx5::Presets().size()) return;
    const dx5::PresetInfo& info = dx5::Presets()[index];
    Suite<AEGP_PFInterfaceSuite1> pfi(in_data->pica_basicP, kAEGPPFInterfaceSuite, kAEGPPFInterfaceSuiteVersion1);
    AEGP_EffectRefH eff = nullptr;
    if (!pfi || pfi->AEGP_GetNewEffectForEffect(g_pluginId, in_data->effect_ref, &eff) || !eff) {
        Log("preset %s: no effect reference", info.name);
        return;
    }
    std::string text;
    dx5::Graph old;
    const uint32_t rev = ReadGraphText(eff, &text) && dx5::Parse(text, &old) ? old.revision : 0;
    dx5::Preset p = info.make();
    p.graph.revision = rev + 1;  // an open editor sees the change and reloads
    Write w;
    w.eff = eff;
    w.ownsEff = true;
    w.instance = InstanceId(in_data);
    w.text = dx5::Serialize(p.graph);
    w.macros = p.macros;
    w.why = std::string("preset ") + info.name;
    w.notBefore = std::chrono::steady_clock::now() + std::chrono::milliseconds(delayMs);
    Log("preset %s: queued (revision %u)", info.name, (unsigned)p.graph.revision);
    std::lock_guard<std::mutex> lk(g_mu);
    g_queue.push_back(std::move(w));
}

void MaybeRunTestScript(PF_InData* in_data, int changed, int seed)
{
    char path[MAX_PATH] = {0};
    if (changed != P_SEED || !GetEnvironmentVariableA("DUPLIX2_TEST_SCRIPT", path, sizeof(path))) return;
    std::ifstream f(path, std::ios::binary);
    if (!f) return;
    std::stringstream buf;
    buf << f.rdbuf();
    std::string script = buf.str();
    const std::string mark = "== seed " + std::to_string(seed) + " ==";
    if (script.find("== seed ") != std::string::npos) {  // one script per seed value
        const size_t a = script.find(mark);
        if (a == std::string::npos) return;
        const size_t from = script.find('\n', a);
        const size_t to = script.find("\n== seed ", from == std::string::npos ? a : from);
        script = from == std::string::npos ? "" : script.substr(from + 1, to == std::string::npos ? std::string::npos : to - from - 1);
    }
    const A_u_longlong inst = InstanceId(in_data);
    static std::set<std::pair<A_u_longlong, size_t>> done;  // main thread only
    const auto key = std::make_pair(inst, std::hash<std::string>()(script));
    if (!inst || done.count(key)) return;
    done.insert(key);
    // "uipreset <name>": what the Overview's preset button does (the same function), no editor
    const size_t up = script.find("uipreset ");
    if (up != std::string::npos) {
        std::string name = script.substr(up + 9);
        name = name.substr(0, name.find_first_of("\r\n"));
        for (int i = 0; i < (int)dx5::Presets().size(); i++)
            if (name == dx5::Presets()[i].name) QueuePreset(in_data, i, 1500);
        return;
    }
    ed::Session* s = Open(in_data);
    if (!s) {
        Log("test script: the editor did not open");
        return;
    }
    std::vector<dx5::MacroWrite> macros;
    std::string err;
    if (!dx5::RunScript(s->graph, script, &macros, &err)) {
        Log("test script: %s", err.c_str());
        return;
    }
    if (s->wnd) ed::GraphReloaded(s->wnd);
    ed::Commit("test script", macros);
    // the idle hook also runs while the script that set the seed is still running, and AEGP_SetStreamValue did
    // not return when it wrote then (11/28): wait until the script is over
    std::lock_guard<std::mutex> lk(g_mu);
    if (!g_queue.empty()) g_queue.back().notBefore = std::chrono::steady_clock::now() + std::chrono::milliseconds(1500);
    Log("test script: queued (revision %u, %zu macro(s))", (unsigned)s->graph.revision, macros.size());
}

}  // namespace dx2

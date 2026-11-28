// DupliX 2 (design v4): shared declarations of the effect's files.
#pragma once

#define PF_DEEP_COLOR_AWARE 1

#include "AEConfig.h"
#ifdef AE_OS_WIN
#include <Windows.h>
#endif

#include "entry.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "AE_EffectCBSuites.h"
#include "AE_EffectSuites.h"
#include "AE_AdvEffectSuites.h"
#include "AE_EffectUI.h"
#include "AE_GeneralPlug.h"

#include <memory>
#include <string>
#include <vector>

#include "dxg5/edit.h"
#include "dxg5/eval.h"
#include "dxg5/graph.h"
#include "dxg5/lang.h"
#include "v2/dx2_flags.h"
#include "v2/dx2_params.h"

namespace dx2 {

// one line to the file named by DUPLIX2_LOG or DUPLIX_LOG (no-op if neither is set)
void Log(const char* fmt, ...);

extern AEGP_PluginID g_pluginId;
extern SPBasicSuite* g_pica;  // set on every command (the idle hook and the editor use it)

template <class T>
struct Suite {
    SPBasicSuite* sp;
    T* s = nullptr;
    const char* name;
    int32 ver;
    Suite(SPBasicSuite* p, const char* n, int32 v) : sp(p), name(n), ver(v)
    {
        if (!sp || sp->AcquireSuite(n, v, (const void**)&s) != kSPNoError) s = nullptr;
    }
    ~Suite()
    {
        if (s) sp->ReleaseSuite(name, ver);
    }
    T* operator->() const { return s; }
    explicit operator bool() const { return s != nullptr; }
};

// ---- the graph data (dx2_arb.cpp) ----
// A new handle holding `text` (the graph's stored form); nullptr if out of memory.
PF_Handle NewGraphHandle(const std::string& text);
// The text in a graph handle ("" if none).
std::string GraphText(PF_Handle h);
// The graph in a handle, parsed once per distinct text (a cache shared by all threads, behind a mutex).
// Never null: a graph that cannot be read is an empty graph.
std::shared_ptr<const dx5::Graph> GraphOf(PF_Handle h);
std::shared_ptr<const dx5::Graph> GraphOfText(const std::string& text);
// Rewrites the handle (resized; it may move, so *h is updated) with `text`: for a change made in a UI event
// through params[].
bool SetGraphHandle(PF_Handle* h, const std::string& text);
PF_Err HandleArb(PF_InData* in_data, PF_OutData* out_data, PF_ArbParamsExtra* x);
std::string DefaultGraphText();

// ---- AEGP (dx2_aegp.cpp): UI commands and the idle hook only, never render ----
// Sequence data: an id per instance (what it remembers in the UI is keyed by it). SEQUENCE_SETUP and RESETUP.
PF_Err SequenceSetup(PF_InData* in_data, PF_OutData* out_data);
void SequenceSetdown(PF_InData* in_data, PF_OutData* out_data);
A_u_longlong InstanceId(PF_InData* in_data);
// Hides the macro and layer rows the graph does not use (rows with keys or expressions stay), and gives the
// macros their slider ranges from the graph. Returns true if a row changed (then PF_OutFlag_REFRESH_UI).
bool UpdateParamsUI(PF_InData* in_data, PF_ParamDef* params[]);
// The layer names of Layer 1..8 ("" if none), for the Overview and the editor.
void LayerNames(PF_InData* in_data, std::string out[kLayers]);
// The problems the last evaluation of an instance's graph found (any thread: PreRender reports, the editor reads).
// Kept with the graph's revision: the editor shows them only for the graph it has.
void ReportProblems(A_u_longlong instance, uint32_t revision, const std::vector<std::string>& problems);
bool Problems(A_u_longlong instance, uint32_t revision, std::vector<std::string>* out);

// ---- the editor (dx2_editor.cpp) ----
void OpenEditor(PF_InData* in_data);
// The Overview's preset buttons: the preset's graph and its macros written through the idle hook, one undo step
// (a UI event's in-place change of the Arbitrary Data was not taken by AE: only the macros changed, 11/28).
// `delayMs` holds the write back (tests: the script that asked is still running).
void QueuePreset(PF_InData* in_data, int index, int delayMs = 0);
// SEQUENCE_SETDOWN of an instance (any thread): its editor closes.
void EditorInstanceGone(A_u_longlong instance);
// The idle hook (registered at GLOBAL_SETUP).
A_Err IdleHook(AEGP_GlobalRefcon, AEGP_IdleRefcon, A_long* max_sleep);
// Tests: DUPLIX2_TEST_SCRIPT=<file> runs that editor script on this instance through the editor (once per script
// and instance) when its Random Seed changes (`changed` is the parameter's index, `seed` its new value). A file
// with lines "== seed <n> ==" holds one script per seed value.
void MaybeRunTestScript(PF_InData* in_data, int changed, int seed);

// ---- custom UI (dx2_ui.cpp) ----
PF_Err HandleEvent(PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output,
                   PF_EventExtra* extra);
constexpr int kOverviewHeight = 132;

// ---- render (dx2_render.cpp) ----
PF_Err PreRender(PF_InData* in_data, PF_OutData* out_data, PF_PreRenderExtra* extra);
PF_Err SmartRender(PF_InData* in_data, PF_OutData* out_data, PF_SmartRenderExtra* extra);

}  // namespace dx2

// DupliX 2 (design v4): the effect's entry point and its parameters (design v4 8.1: the table in dx2_params.h).
#include "v2/dx2.h"

#include <cstdio>
#include <string>

namespace dx2 {

SPBasicSuite* g_pica = nullptr;

// the PiPL's values (dx2_flags.h) against the SDK's: a mismatch is a dialog in AE, so it must not build
static_assert(DX2_EFFECT_VERSION ==
                  PF_VERSION(DX2_VERSION_MAJOR, DX2_VERSION_MINOR, DX2_VERSION_BUG, PF_Stage_DEVELOP, DX2_VERSION_BUILD),
              "dx2_flags.h: DX2_EFFECT_VERSION");
static_assert(DX2_OUT_FLAGS == (PF_OutFlag_CUSTOM_UI | PF_OutFlag_DEEP_COLOR_AWARE | PF_OutFlag_SEND_UPDATE_PARAMS_UI),
              "dx2_flags.h: DX2_OUT_FLAGS");
static_assert(DX2_OUT_FLAGS2 == (PF_OutFlag2_PARAM_GROUP_START_COLLAPSED_FLAG | PF_OutFlag2_SUPPORTS_SMART_RENDER |
                                 PF_OutFlag2_FLOAT_COLOR_AWARE | PF_OutFlag2_SUPPORTS_THREADED_RENDERING),
              "dx2_flags.h: DX2_OUT_FLAGS2");

namespace {

// Adds the parameters in order; the Disk ID of each is its index (dx2_params.h).
struct Builder {
    PF_InData* in;
    int next = 1;  // 0 is the input layer
    PF_Err err = PF_Err_NONE;

    PF_ParamDef Def(const char* name, PF_ParamType type, PF_ParamFlags flags = PF_ParamFlag_NONE)
    {
        PF_ParamDef d;
        AEFX_CLR_STRUCT(d);
        d.param_type = type;
        PF_STRNNCPY(d.PF_DEF_NAME, name, sizeof(d.PF_DEF_NAME));
        d.flags = flags;
        return d;
    }
    void Add(PF_ParamDef& d, int expect)
    {
        if (err) return;
        if (expect != next) {
            Log("ParamsSetup: \"%s\" added at %d, expected %d", d.PF_DEF_NAME, next, expect);
            err = PF_Err_INTERNAL_STRUCT_DAMAGED;
            return;
        }
        d.uu.id = next;
        err = PF_ADD_PARAM(in, -1, &d);
        next++;
    }
    void Topic(int at, const char* name, bool collapsed)
    {
        PF_ParamDef d = Def(name, PF_Param_GROUP_START, collapsed ? PF_ParamFlag_START_COLLAPSED : PF_ParamFlag_NONE);
        Add(d, at);
    }
    void End(int at)
    {
        PF_ParamDef d = Def("", PF_Param_GROUP_END);
        Add(d, at);
    }
    void Slider(int at, const char* name, int vmin, int vmax, int smin, int smax, int dflt, bool supervise = false)
    {
        PF_ParamDef d = Def(name, PF_Param_SLIDER, supervise ? PF_ParamFlag_SUPERVISE : PF_ParamFlag_NONE);
        d.u.sd.valid_min = vmin;
        d.u.sd.valid_max = vmax;
        d.u.sd.slider_min = smin;
        d.u.sd.slider_max = smax;
        d.u.sd.value = d.u.sd.dephault = dflt;
        Add(d, at);
    }
    void Float(int at, const char* name, double vmin, double vmax, double smin, double smax, double dflt)
    {
        PF_ParamDef d = Def(name, PF_Param_FLOAT_SLIDER);
        d.u.fs_d.valid_min = (PF_FpShort)vmin;
        d.u.fs_d.valid_max = (PF_FpShort)vmax;
        d.u.fs_d.slider_min = (PF_FpShort)smin;
        d.u.fs_d.slider_max = (PF_FpShort)smax;
        d.u.fs_d.value = (PF_FpShort)dflt;
        d.u.fs_d.dephault = (PF_FpShort)dflt;
        d.u.fs_d.precision = PF_Precision_TENTHS;
        d.u.fs_d.display_flags = PF_ValueDisplayFlag_NONE;
        d.u.fs_d.curve_tolerance = AEFX_DEFAULT_CURVE_TOLERANCE;
        Add(d, at);
    }
    void Popup(int at, const char* name, const char* items, int dflt, bool supervise = false)
    {
        int n = 1;
        for (const char* p = items; *p; p++)
            if (*p == '|') n++;
        PF_ParamDef d = Def(name, PF_Param_POPUP, supervise ? PF_ParamFlag_SUPERVISE : PF_ParamFlag_NONE);
        d.u.pd.num_choices = (A_short)n;
        d.u.pd.value = d.u.pd.dephault = (A_short)dflt;
        d.u.pd.u.namesptr = items;
        Add(d, at);
    }
    void Layer(int at, const char* name)
    {
        PF_ParamDef d = Def(name, PF_Param_LAYER, PF_ParamFlag_SUPERVISE);
        d.u.ld.dephault = PF_LayerDefault_NONE;
        Add(d, at);
    }
    void Point3(int at, const char* name)
    {
        PF_ParamDef d = Def(name, PF_Param_POINT_3D);
        d.u.point3d_d.x_value = d.u.point3d_d.x_dephault = 50;  // percent of the layer: its centre
        d.u.point3d_d.y_value = d.u.point3d_d.y_dephault = 50;
        d.u.point3d_d.z_value = d.u.point3d_d.z_dephault = 0;
        Add(d, at);
    }
    void Color(int at, const char* name)
    {
        PF_ParamDef d = Def(name, PF_Param_COLOR);
        d.u.cd.value.red = d.u.cd.value.green = d.u.cd.value.blue = d.u.cd.value.alpha = 255;
        d.u.cd.dephault = d.u.cd.value;
        Add(d, at);
    }
    void Custom(int at, const char* name, int height)
    {
        PF_ParamDef d = Def(name, PF_Param_NO_DATA);
        d.ui_flags = PF_PUI_CONTROL;
        d.ui_width = 260;
        d.ui_height = (A_short)height;
        Add(d, at);
    }
    void Graph(int at)
    {
        PF_ParamDef d = Def("Graph", PF_Param_ARBITRARY_DATA, PF_ParamFlag_CANNOT_TIME_VARY);
        d.ui_flags = PF_PUI_NO_ECW_UI;
        d.u.arb_d.id = (A_short)at;
        d.u.arb_d.refconPV = nullptr;
        d.u.arb_d.dephault = NewGraphHandle(DefaultGraphText());
        if (!d.u.arb_d.dephault) {
            err = PF_Err_OUT_OF_MEMORY;
            return;
        }
        Add(d, at);
    }
};

PF_Err ParamsSetup(PF_InData* in_data, PF_OutData* out_data)
{
    Builder b{in_data};
    char nm[32];

    b.Custom(P_OVERVIEW, "Overview", kOverviewHeight);
    b.Popup(P_RENDER_MODE, "Render Mode", kRenderModeItems, RM_FAST, true);
    b.Slider(P_SEED, "Random Seed", 0, 1000000, 0, 1000, 0, true);

    b.Topic(P_MACRO_TOPIC, "Macros", false);
    for (int k = 0; k < kMacros; k++) {
        snprintf(nm, sizeof(nm), "Macro %d", k + 1);
        // the default graph's Count, Spacing and Copy Scale (design v4 1.2): it draws without a write
        b.Float(P_MACRO + k, nm, -1000000, 1000000, 0, 100, dx5::MacroDefault(k));
    }
    b.End(P_MACRO_TOPIC_END);

    b.Topic(P_COLOR_TOPIC, "Color Macros", true);
    for (int k = 0; k < kColorMacros; k++) {
        snprintf(nm, sizeof(nm), "Color Macro %d", k + 1);
        b.Color(P_COLOR + k, nm);
    }
    b.End(P_COLOR_TOPIC_END);

    b.Topic(P_POINT_TOPIC, "Point Macros", true);
    for (int k = 0; k < kPointMacros; k++) {
        snprintf(nm, sizeof(nm), "Point Macro %d", k + 1);
        b.Point3(P_POINT + k, nm);
    }
    b.End(P_POINT_TOPIC_END);

    b.Topic(P_LAYER_TOPIC, "Layers", false);
    for (int k = 0; k < kLayers; k++) {
        snprintf(nm, sizeof(nm), "Layer %d", k + 1);
        b.Layer(P_LAYER + k, nm);
    }
    b.End(P_LAYER_TOPIC_END);

    b.Graph(P_GRAPH);

    // design v5 4.4: appended after the graph (the earlier Disk IDs stay)
    b.Topic(P_INT_TOPIC, "Int Macros", false);
    for (int k = 0; k < kIntMacros; k++) {
        snprintf(nm, sizeof(nm), "Int Macro %d", k + 1);
        b.Slider(P_INT + k, nm, -1000000, 1000000, 0, 100, (int)dx5::IntMacroDefault(k));
    }
    b.End(P_INT_TOPIC_END);
    b.Slider(P_REVISION, "Graph Revision", 0, 1000000, 0, 1000000, 0, false);

    if (!b.err && b.next != P_NUM) {
        Log("ParamsSetup: %d parameters, expected %d", b.next, (int)P_NUM);
        b.err = PF_Err_INTERNAL_STRUCT_DAMAGED;
    }
    if (!b.err) {
        PF_CustomUIInfo ci;
        AEFX_CLR_STRUCT(ci);
        ci.events = PF_CustomEFlag_EFFECT;
        b.err = (*in_data->inter.register_ui)(in_data->effect_ref, &ci);
    }
    out_data->num_params = P_NUM;
    return b.err;
}

PF_Err GlobalSetup(PF_InData* in_data, PF_OutData* out_data)
{
    out_data->my_version =
        PF_VERSION(DX2_VERSION_MAJOR, DX2_VERSION_MINOR, DX2_VERSION_BUG, PF_Stage_DEVELOP, DX2_VERSION_BUILD);
    out_data->out_flags = DX2_OUT_FLAGS;
    out_data->out_flags2 = DX2_OUT_FLAGS2;
    Suite<AEGP_UtilitySuite6> util(in_data->pica_basicP, kAEGPUtilitySuite, kAEGPUtilitySuiteVersion6);
    A_Err e = util ? util->AEGP_RegisterWithAEGP(nullptr, "DupliX2", &g_pluginId) : -1;
    Suite<AEGP_RegisterSuite5> reg(in_data->pica_basicP, kAEGPRegisterSuite, kAEGPRegisterSuiteVersion5);
    A_Err ei = (reg && !e) ? reg->AEGP_RegisterIdleHook(g_pluginId, IdleHook, nullptr) : -1;
    Log("GlobalSetup: %d parameters, plugin id %d (err %d), idle hook err %d", (int)P_NUM, (int)g_pluginId, (int)e,
        (int)ei);
    return PF_Err_NONE;
}

}  // namespace
}  // namespace dx2

extern "C" DllExport PF_Err EffectMain(PF_Cmd cmd, PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[],
                                       PF_LayerDef* output, void* extra)
{
    using namespace dx2;
    PF_Err err = PF_Err_NONE;
    if (in_data && in_data->pica_basicP) g_pica = in_data->pica_basicP;
    try {
        switch (cmd) {
        case PF_Cmd_GLOBAL_SETUP:
            err = GlobalSetup(in_data, out_data);
            break;
        case PF_Cmd_ABOUT:
            PF_SPRINTF(out_data->return_msg, "DupliX 2 %d.%d.%d (preview)\rCopies of layers laid out by a graph.\rhttps://github.com/yak9909/DupliX",
                       DX2_VERSION_MAJOR, DX2_VERSION_MINOR, DX2_VERSION_BUG);
            break;
        case PF_Cmd_PARAMS_SETUP:
            err = ParamsSetup(in_data, out_data);
            break;
        case PF_Cmd_SEQUENCE_SETUP:
        case PF_Cmd_SEQUENCE_RESETUP:
            err = SequenceSetup(in_data, out_data);
            break;
        case PF_Cmd_SEQUENCE_SETDOWN:
            EditorInstanceGone(InstanceId(in_data));
            SequenceSetdown(in_data, out_data);
            break;
        case PF_Cmd_UPDATE_PARAMS_UI:
            if (UpdateParamsUI(in_data, params)) out_data->out_flags |= PF_OutFlag_REFRESH_UI;
            break;
        case PF_Cmd_USER_CHANGED_PARAM:
            MaybeRunTestScript(in_data, ((PF_UserChangedParamExtra*)extra)->param_index,
                               params && params[P_SEED] ? params[P_SEED]->u.sd.value : 0);
            UpdateParamsUI(in_data, params);
            out_data->out_flags |= PF_OutFlag_REFRESH_UI;  // the Overview shows the layers and the seed
            break;
        case PF_Cmd_ARBITRARY_CALLBACK:
            err = HandleArb(in_data, out_data, (PF_ArbParamsExtra*)extra);
            break;
        case PF_Cmd_EVENT:
            err = HandleEvent(in_data, out_data, params, output, (PF_EventExtra*)extra);
            break;
        case PF_Cmd_SMART_PRE_RENDER:
            err = PreRender(in_data, out_data, (PF_PreRenderExtra*)extra);
            break;
        case PF_Cmd_SMART_RENDER:
            err = SmartRender(in_data, out_data, (PF_SmartRenderExtra*)extra);
            break;
        default:
            break;
        }
    } catch (const std::bad_alloc&) {
        err = PF_Err_OUT_OF_MEMORY;
    } catch (...) {
        err = PF_Err_INTERNAL_STRUCT_DAMAGED;
    }
    return err;
}

// DupliX 2 (design v4, 8.1): the Effect Controls. Everything else is in the graph data (the Graph parameter),
// edited in the editor; the macros are what After Effects keys and drives with expressions.
//
// The index of a parameter is also its Disk ID (uu.id) and its AEGP effect stream index. AE names a parameter's
// match name after its Disk ID ("DupliX2-0005" is Macro 1) and expressions refer to it: never insert, remove or
// reorder; only append before P_NUM.
#pragma once

#include "dxg5/graph.h"

namespace dx2 {

constexpr int kMacros = dx5::kNumMacros;            // 32
constexpr int kIntMacros = dx5::kNumIntMacros;      // 32
constexpr int kColorMacros = 8;
constexpr int kPointMacros = 8;
constexpr int kLayers = dx5::kNumLayers;            // 8

enum Param {
    P_INPUT = 0,
    P_OVERVIEW = 1,        // custom UI: Open Editor, a picture of the layout, the macros in use, the presets
    P_RENDER_MODE = 2,     // 3D Layers / Fast (2D)
    P_SEED = 3,
    P_MACRO_TOPIC = 4,
    P_MACRO = 5,           // Macro 1..32
    P_MACRO_TOPIC_END = P_MACRO + kMacros,                 // 37
    P_COLOR_TOPIC = P_MACRO_TOPIC_END + 1,                 // 38
    P_COLOR = P_COLOR_TOPIC + 1,                           // 39: Color Macro 1..8
    P_COLOR_TOPIC_END = P_COLOR + kColorMacros,            // 47
    P_POINT_TOPIC = P_COLOR_TOPIC_END + 1,                 // 48
    P_POINT = P_POINT_TOPIC + 1,                           // 49: Point Macro 1..8
    P_POINT_TOPIC_END = P_POINT + kPointMacros,            // 57
    P_LAYER_TOPIC = P_POINT_TOPIC_END + 1,                 // 58
    P_LAYER = P_LAYER_TOPIC + 1,                           // 59: Layer 1..8
    P_LAYER_TOPIC_END = P_LAYER + kLayers,                 // 67
    P_GRAPH = P_LAYER_TOPIC_END + 1,                       // 68: the graph data (Arbitrary Data)
    P_INT_TOPIC = P_GRAPH + 1,                             // 69 (design v5 4.4: appended)
    P_INT = P_INT_TOPIC + 1,                               // 70: Int Macro 1..32
    P_INT_TOPIC_END = P_INT + kIntMacros,                  // 102
    // the graph's revision, never shown: written with every graph write, because AE does not take a change of the
    // Arbitrary Data written by an AEGP as a reason to draw the frame again (a number's change it does: 11/29,
    // tests/ae/run_dx2_live.ps1)
    P_REVISION = P_INT_TOPIC_END + 1,                      // 103
    P_NUM
};
static_assert(P_MACRO == 5 && P_MACRO + kMacros - 1 == 36, "Macro 1..32 are DupliX2-0005..0036 (design v4 8.1)");
static_assert(P_GRAPH == 68, "the parameter table of design v4 8.1");
static_assert(P_INT == 70 && P_INT + kIntMacros - 1 == 101 && P_INT_TOPIC_END == 102 && P_REVISION == 103 && P_NUM == 104,
              "Int Macro 1..32 are DupliX2-0070..0101 (design v5 4.4)");

enum RenderMode { RM_3D_LAYERS = 1, RM_FAST = 2 };
constexpr const char* kRenderModeItems = "3D Layers|Fast (2D)";

}  // namespace dx2

#define DUPLIX2_MATCH_NAME "DupliX2"

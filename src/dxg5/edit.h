// DupliX 2 (design v5, 8.3, 14, 15): the editor's operations on a graph. The editor's views (nodes, stack, code)
// call only these, and so do the tests (RunScript), so what a test drives is what the user drives.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "dxg5/graph.h"
#include "dxg5/lang_ast.h"

namespace dx5 {

// A macro whose value must be set in After Effects together with the graph (in the same undo step).
struct MacroWrite {
    bool isInt = false;
    int k = -1;  // 0..31
    double value = 0;
};

uint32_t AddNodeAt(Graph& g, const std::string& type, double x, double y);
// Removes the node, the wires from it, and (an Each zone) the nodes inside it.
bool RemoveNode(Graph& g, uint32_t id);
bool MoveNode(Graph& g, uint32_t id, double x, double y);
// Feeds `input` of `to` from `output` of `from` (replacing what fed it). Refused (with why) for a type that does
// not fit, a field into an input that takes single values, or a loop.
bool Connect(Graph& g, uint32_t from, int output, uint32_t to, const std::string& input, std::string* why);
bool Disconnect(Graph& g, uint32_t to, const std::string& input);
// Sets an unwired value (the wire is taken off). Structural inputs are rounded, choices kept in range.
bool SetConst(Graph& g, uint32_t id, const std::string& input, double v);
// A value typed into an input field (design v5 8.3): a number or a formula of numbers only ("360 / 12") is set as
// the value; a formula with names is refused ("a formula goes in an Expression node").
bool SetConstText(Graph& g, uint32_t id, const std::string& input, const std::string& text, std::string* why);
// An Expression node's formula, applied on commit (design v5 8.3). Refused (graph untouched, why = "line L col C:
// ...") when it does not read. Inputs follow the formula's unknown names (kept by name with their wires and values,
// the others go); wires from its outputs follow the outputs' names (an output that goes takes its wires); a
// random() without a salt gets one written into the text.
bool SetFormula(Graph& g, uint32_t exprNode, const std::string& text, std::string* why);
// "Connect Expression": a new Expression node (formula 0) to the input's left, wired into it.
bool ConnectExpression(Graph& g, uint32_t id, const std::string& input);
// A math node's operator (design v5 6.2). Its wires stay; those on inputs the operator does not use stop counting.
bool SetMathOp(Graph& g, uint32_t id, const std::string& op, std::string* why);
// Why a value of type `from` cannot go into this input ("" when it can): a math node's input takes what its operator
// takes ("a of rot_apply is a Rotation: an Int cannot go in").
std::string LinkRefusal(const Graph& g, uint32_t id, const std::string& input, Type from);
// Wires Macro k+1 (or Int Macro k+1) into the input (one Macro node per macro, shared).
bool LinkMacro(Graph& g, uint32_t id, const std::string& input, bool isInt, int k);
// "Expose" (design v4 3): a free macro (an Int Macro for an Int input) takes the input's value now (returned in
// `write`), the input's name and its slider range, and is wired in. False for a choice, a wired input, or no free
// macro.
bool Expose(Graph& g, uint32_t id, const std::string& input, MacroWrite* write);
bool RenameMacro(Graph& g, bool isInt, int k, const std::string& name);
bool SetMacroRange(Graph& g, bool isInt, int k, double lo, double hi);
// The lowest macro no node reads and that has no name, or -1.
int FreeMacro(const Graph& g, bool isInt);
// Bits of the macros / layers the graph reads.
uint32_t UsedMacros(const Graph& g);
uint32_t UsedIntMacros(const Graph& g);
uint32_t UsedLayers(const Graph& g);
// Is this output a field (a value per element)?
bool IsField(const Graph& g, uint32_t node, int output);

// The macros no node reads that have a name (the Macros panel folds them under "unused": design v5 4.4).
std::vector<int> UnusedNamedMacros(const Graph& g, bool isInt);
// Why Expose cannot pick a macro ("" when it can).
std::string ExposeRefusal(const Graph& g, bool isInt);

// An Expression node made into one Math node per operation, and back (design v5 8.3).
bool ExpandExpression(Graph& g, uint32_t exprNode);
// The nodes (Math, Expression, attribute reads) become one Expression node; each of their outputs used outside them
// is one of its outputs (one line each).
bool CollapseToExpression(Graph& g, const std::vector<uint32_t>& nodes);

// ---- the user's groups (design v5 12, 12.1; dx5_edit_groups.cpp) ----
// "Copy for Myself": a built-in group's definition copied into the graph as grid_copy, grid_copy_2, ...; the node
// becomes that group (same key: the same elements and ids).
bool CopyGroupForSelf(Graph& g, uint32_t node, std::string* newName);
// Ctrl+G: the nodes become one group node (wires from outside are its inputs, wires to outside its outputs). An
// empty name gives group1, group2, ...
bool GroupSelection(Graph& g, const std::vector<uint32_t>& nodes, const std::string& name, uint32_t* groupNode);
// A user's group node back into its nodes (the definition goes when nothing else uses it).
bool UngroupNode(Graph& g, uint32_t node);
// "Make Group" on an Expression node: a group of its own holding it.
bool ExpressionToGroup(Graph& g, uint32_t exprNode, std::string* name);

// ---- editing inside (design v5 8.3, 12.1): Tab opens an Expression node or a group ----
// The inside of an Expression node: a param node per input, the formula's lines as math and input nodes, a
// group_output node whose inputs are its outputs (by name). Only formula nodes may be added inside.
bool OpenExpression(const Graph& g, uint32_t exprNode, Graph* inside);
// The inside written back as the node's formula (SetFormula: its wires kept by name). False (why) when the inside
// holds a node a formula cannot say.
bool CloseExpression(Graph& g, uint32_t exprNode, const Graph& inside, std::string* why);
// A user's group's body replaced (its inputs and outputs found again; its nodes' inputs follow by name). False for a
// built-in group or a body that has no group_output.
bool SetGroupBody(Graph& g, const std::string& name, const Graph& body, std::string* why);

// The code view's Apply: the graph read from `code` (keys kept from `g`). On failure `g` is untouched.
bool ApplyCode(Graph& g, const std::string& code, std::vector<Diag>* errors, std::vector<Diag>* notes);

// ---- presets ----
struct Preset {
    Graph graph;
    std::vector<MacroWrite> macros;  // the macro values that go with it
};
// The graph a new instance starts with: a 5 x 5 grid; Count (Int Macro 1), Spacing (Macro 2), Copy Scale (Macro 3).
// The effect's defaults (Int Macro 1 = 5, Macro 2 = 100, Macro 3 = 100) are its values, so it draws as it is.
Preset DefaultPreset();
Preset CirclePreset();
struct PresetInfo {
    const char* name;
    Preset (*make)();
};
const std::vector<PresetInfo>& Presets();
// The effect's defaults: Macro 1..32 (5, 100, 100, then 0: Macro 1-3 keep P1's defaults) and Int Macro 1..32 (5, 0...).
double MacroDefault(int k);
int64_t IntMacroDefault(int k);

// ---- scripts (tests, and the editor's test hook) ----
// One command per line:
//   clear | preset <name> | add <Type> <x> <y> [as <alias>] | remove <n> | move <n> <x> <y>
//   wire <from> <to> | unwire <to> | set <n> <input> <value> | expr <n> <input> <formula>
//   link <n> <input> m<k>|i<k> | unlink <n> <input> | expose <n> <input>
//   name m<k>|i<k> <name> | range m<k>|i<k> <lo> <hi> | value m<k>|i<k> <v>
//   code ... end (the lines between applied as code)
// <n> is an alias, a key or a node id; inputs may be P1's names (countX, rotZ, ...). '#' starts a comment.
bool RunScript(Graph& g, const std::string& script, std::vector<MacroWrite>* writes, std::string* error);

// ---- the stack view (design v5 14) ----
struct StackRow {
    uint32_t node = 0;
    int depth = 0;               // indent (branches of a node with more than one geometry input)
    bool collapsedGraph = false; // the part from here up is not a tree: shown as one row that opens the node view
};
// Output first going up its geometry inputs, listed upstream first (the top row acts first).
std::vector<StackRow> StackRows(const Graph& g);

}  // namespace dx5

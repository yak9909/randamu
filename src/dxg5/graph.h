// DupliX 2 (design v5, 2 and 7): the graph. Nodes joined by wires; a wire feeds one input from one output, whether
// it carries a geometry or a value. The stored form is version 2 of the line format (one statement per line);
// version 1 (P1's graphs) is read and converted.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "dxg5/nodes.h"
#include "dxg5/value.h"

namespace dx5 {

constexpr int kFormatVersion = 2;
constexpr int kNumMacros = 32;     // Macro 1..32 (Float)
constexpr int kNumIntMacros = 32;  // Int Macro 1..32
constexpr int kNumLayers = 8;

struct Link {
    uint32_t from = 0;  // 0: no wire
    int output = 0;
};

struct InputSlot {
    std::string name;
    Value value;  // used when no wire feeds it
    Link link;
};

struct Node {
    uint32_t id = 0;          // unique in the graph while the node lives (the stored wires refer to it)
    std::string key;          // unique, kept for life: element ids and the code's variable name (design v5 11)
    std::string type;         // a NodeDef name; an unknown name is kept and written back
    double x = 0, y = 0;      // the node view's position
    std::vector<InputSlot> inputs;
    std::string text;         // Expression's formula, Store / Attr's attribute name
    int setting = 0;          // Macro / Int Macro: 1..32; Reduce: the operation
    uint64_t salt = 0;        // Random's salt (0: from the key)
    uint32_t zone = 0;        // the Each zone it is inside (0: none)
    uint32_t legacyId = 0;    // from a version 1 graph: its element ids are P1's (design v5 11)

    InputSlot* In(const std::string& name);
    const InputSlot* In(const std::string& name) const;
};

struct MacroSetting {
    std::string name;  // "" = "Macro N" / "Int Macro N"
    double sliderMin = 0, sliderMax = 100;
};

struct Graph;

// A group the user defined or copied for themself (design v5 12.1): a function. Its body is a graph whose param /
// param_geo nodes are its inputs and whose group_output node's inputs are its outputs; `def` is its node's table entry
// (inputs, outputs, structural inputs found from the body). Saved as `group <name> 0` ... `end`.
struct UserGroup {
    std::string name;
    std::shared_ptr<const Graph> body;
    NodeDef def;
};

struct Graph {
    uint32_t revision = 0;  // +1 on every write from the editor (AE's undo is detected by it)
    uint32_t nextId = 1;
    std::vector<Node> nodes;
    // the user's groups, in the order they may use each other (a group uses only those before it). A group's body
    // holds the groups before it too, so it can be evaluated and written on its own.
    std::vector<UserGroup> groups;
    MacroSetting macros[kNumMacros];
    MacroSetting intMacros[kNumIntMacros];
    std::vector<std::string> unknownLines;

    Node* Find(uint32_t id);
    const Node* Find(uint32_t id) const;
    const Node* FindKey(const std::string& key) const;
    Node* FindKey(const std::string& key);
    const Node* OutputNode() const;
};

const UserGroup* FindUserGroup(const Graph& g, const std::string& name);
// A user's group's table entry (category Group, group = true).
NodeDef UserGroupDef(const std::string& name, std::vector<InputDef> in, std::vector<OutputDef> out);
// "geometry", "int", "float", "bool", "vector", "rotation", "any" (the code's and the stored form's type words)
std::string TypeWord(Type t);
Type WordType(const std::string& w);
// The table entry of a node type in this graph: a basic node or built-in group (FindDef), else the user's group.
const NodeDef* DefFor(const Graph& g, const std::string& type);

// `want` if no node has it, else want_2, want_3, ...
std::string UniqueKey(const Graph& g, const std::string& want);
// A node of `type` with its inputs at their defaults (an unknown type: no inputs). An empty key becomes
// <type><n> (the smallest n from 1 that is free). Returns its id.
// A math operation's code name ("math_add", "vmath_dot") is made a node of its family with the operator as text.
uint32_t AddNode(Graph& g, const std::string& type, const std::string& key, double x, double y);
// "math_add" (P1.5's stored type, or a code name) -> type "math", text "add". True if the node is a math node.
bool NormalizeMath(Node& n);
// The node's outputs: an Expression node's come from its formula (design v5 8.3; `value` when it does not read),
// the others' from the node table.
std::vector<OutputDef> OutputsOf(const Graph& g, const Node& n);
// A math node's operation (nullptr for other nodes or an unknown operator).
const MathOpDef* MathOpOf(const Node& n);
// The type an output gives (a math node's comes from its operator).
Type OutputType(const Graph& g, const Node& n, int output);
// The type a math node's input k takes under its operator (Any for other nodes and hidden inputs).
Type MathInputType(const Graph& g, const Node& n, int k);
// Can a value of type `from` go into an input of type `to` (Any fits all)?
bool TypeFits(Type from, Type to);
// Is input k drawn? (A math node hides the inputs its operator does not use: design v5 6.2.)
bool InputVisible(const Graph& g, const Node& n, int k);
// Does the wire into input k count? Not on a hidden input, nor when the output's type does not fit the operator's
// input (drawn red). Such a wire is kept; evaluation uses the input's own value instead. True when unwired.
bool LinkActive(const Graph& g, const Node& n, int k);

std::string Serialize(const Graph& g);
// Reads version 2, and version 1 (converted: design v5 7). False only when the text is not a DupliX graph.
bool Parse(const std::string& text, Graph* out, std::string* error = nullptr);

// "Macro 7" / "Int Macro 2" or the name the editor gave it.
std::string MacroLabel(const Graph& g, bool isInt, int k);

}  // namespace dx5

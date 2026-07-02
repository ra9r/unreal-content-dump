# Output format

The dump mirrors the project's `/Game` package tree under the output directory. One text file is
written per logic-bearing asset; pure-art assets (meshes, textures, materials, fonts, sounds) are
skipped.

| Asset type          | File suffix       | Contents |
|---------------------|-------------------|----------|
| Blueprint           | `.bp.txt`         | Parent class, implemented interfaces, variables (+ default values), components, Class Defaults (CDO overrides vs parent), and every graph (Event / Function / Macro). |
| User Defined Struct | `.struct.txt`     | Fields with C++ types. |
| User Defined Enum   | `.enum.txt`       | Values with display names. |
| Data Table          | `.datatable.txt`  | Row struct + all rows as JSON. |
| Data Asset          | `.dataasset.txt`  | All properties. |

Example path mapping:
`/Game/MyGame/Blueprints/BP_Player` → `<OutDir>/MyGame/Blueprints/BP_Player.bp.txt`

## Reading a `.bp.txt`

Layout, top to bottom:

```
# Blueprint: <Name>
Path: /Game/.../<Name>.<Name>
ParentClass: /Script/<Module>.<Class>

## Implemented Interfaces
- <interface paths>

## Variables
- <Name> : <category>[] = <default value>

## Components (SimpleConstructionScript)
- <Name> : <ComponentClass>

## Class Defaults (overrides vs parent)
- <InheritedProperty> (<CppType>) = <value>      # e.g. a GameMode's DefaultPawnClass/HUDClass

=== EventGraph: EventGraph ===
<engine T3D node text: Begin Object Class=/Script/BlueprintGraph.K2Node_... ... End Object>

=== Function: <Name> ===
<nodes>

=== Macro: <Name> ===
<nodes>
```

## Reading the graph (T3D node text)

Each node is a `Begin Object ... End Object` block. To follow logic:

- **Node type** is in the `Class=` line, e.g. `K2Node_Event` (events), `K2Node_CallFunction`
  (function calls — see `MemberName=`), `K2Node_VariableSet` / `K2Node_VariableGet`,
  `K2Node_IfThenElse` (branch), `K2Node_MacroInstance`, `K2Node_Timeline`, etc.
- **Connections** are on pins: `LinkedTo=(<OtherNodeName> <PinId>, ...)`. Follow `exec` pins for
  control flow and data pins for values. `K2Node_Knot` nodes are reroute pass-throughs.
- **Targets / members**: `FunctionReference=(MemberName="...")`, `VariableReference=(MemberName="...")`,
  `MemberParent="..."` tell you what is being called/read and on which class.

The text is complete and exact but verbose; read the specific graphs relevant to the question
rather than whole files.

## Caveats

- **The dump carries no reliable error signal — judge correctness from the wiring.** The dumper
  strips transient compiler-state fields (`ErrorType` / `bHasCompilerMessage` / `ErrorMsg`) from node
  text, because they are recomputed during the headless per-asset load and false-positive on any node
  that references another asset (other Blueprints, GameInstance, widget classes, enums, cross-Blueprint
  dispatchers). Whether a Blueprint actually compiles is the user's **in-editor Compile** status;
  whether the logic is correct is what the pins / `LinkedTo` / exec order say. Never infer bugs from a
  node's metadata.
- **Each node serializes to one very long line** (all pins inline). Prefer targeted `grep -oE` / `awk`
  over reading raw lines, and read by node block (`Begin Object` … `End Object`).

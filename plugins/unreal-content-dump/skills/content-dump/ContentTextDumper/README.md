# Content Text Dumper

An editor-only Unreal Engine plugin that exports a project's **logic-bearing Content assets to
plain text**, so they can be read, diffed, and reviewed outside the editor.

## Why

Blueprints, structs, enums, data tables, and data assets are stored as **binary `.uasset`
files**. They can't be read in a text editor, can't be meaningfully diffed in version control, and
can't be understood by tools (or AI assistants) that only see text. This plugin runs the engine
headlessly and writes a faithful **text representation** of each asset — including full Blueprint
graphs, using the engine's own node serialization (`FEdGraphUtilities::ExportNodesToText`, the same
data you get from copy/pasting nodes). The result:

- **AI legibility** — assistants can read how the project actually works.
- **Reviewable diffs** — committing the dump (or diffing it on demand) turns an opaque Blueprint
  edit into a readable change.

Because it's engine-backed, the output is exact for the installed engine version — there's no
third-party `.uasset` parser to drift out of date.

## What it dumps

Only logic-bearing asset types are exported (pure-art assets — meshes, textures, materials, fonts,
sounds — are skipped so the engine never loads them). One text file is written per asset, mirroring
the `Content/` folder structure.

| Asset type            | File suffix        | Contents |
|-----------------------|--------------------|----------|
| Blueprint             | `.bp.txt`          | Parent class, implemented interfaces, variables (+ default values), components, **Class Defaults** (CDO overrides vs parent), and every graph (Event/Function/Macro) |
| User Defined Struct   | `.struct.txt`      | Fields with C++ types |
| User Defined Enum     | `.enum.txt`        | Values with display names |
| Data Table            | `.datatable.txt`   | Row struct + all rows as JSON |
| Data Asset            | `.dataasset.txt`   | All properties |

## Requirements

- **Unreal Engine installed** with the project's editor target **built** — the dumper is a C++
  commandlet that ships in this plugin, so the editor binaries must be compiled before first use.
- Run it **headless** — pass `-NullRHI` (and `-unattended -nopause -nosplash`); no GUI editor
  session is needed.

## How to run

Run the commandlet with the engine's command-line editor binary, `UnrealEditor-Cmd`:

```sh
"<UE-root>/Engine/Binaries/<Platform>/UnrealEditor-Cmd" "<path-to>/<Project>.uproject" \
    -run=DumpContent \
    -OutDir="<output dir>" \
    -Root=/Game \
    -unattended -nopause -nosplash -NullRHI
```

Concrete example for this project (macOS, UE 5.7):

```sh
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor-Cmd" \
    "/Users/rodney/Workspace/D6scend/D6scend.uproject" \
    -run=DumpContent \
    -OutDir="/Users/rodney/Workspace/D6scend/Docs/ContentText" \
    -Root=/Game \
    -unattended -nopause -nosplash -NullRHI
```

(On Windows the binary is `Engine/Binaries/Win64/UnrealEditor-Cmd.exe`.)

### Flags

| Flag                               | Required                                       | Meaning |
|------------------------------------|------------------------------------------------|---------|
| `-run=DumpContent`                 | yes                                            | Selects this commandlet (the engine appends "Commandlet" → `UDumpContentCommandlet`). |
| `-OutDir=`                         | no — default `<ProjectDir>/Docs/ContentText`   | Output directory (created if missing). |
| `-Root=`                           | no — default `/Game`                           | Content root to dump (recursive). |
| `-unattended -nopause -nosplash`   | recommended                                    | Run non-interactively — no prompts or splash screen. |
| `-NullRHI`                         | recommended                                    | No rendering; runs headless with no GPU. |

## Output

- Files are written under `OutDir`, mirroring the `/Game` package path
  (e.g. `/Game/D6scend/Blueprints/Gameboard/BP_Cube` → `OutDir/D6scend/Blueprints/Gameboard/BP_Cube.bp.txt`).
- Re-running regenerates the files. The output is **regenerable**, so it's typically either
  committed (to get readable diffs on Blueprint edits) or git/Diversion-ignored — your choice.

## Configuration / installation notes

- The plugin is **auto-enabled** by virtue of living in `Plugins/`. It is **editor-only** (module
  `Type: Editor`) and contributes nothing to packaged/shipping builds.
- **Loading phase must stay `Default`.** Commandlet classes are resolved early in engine startup
  (right after `LoadStartupModules`, which only loads the `PreDefault`/`Default`/`PostDefault`
  phases). If this module's `LoadingPhase` is set later (e.g. `PostEngineInit`), the engine can't
  find the commandlet and fails with *"DumpContentCommandlet looked like a commandlet, but we could
  not find the class."* Keep it at `Default` if you copy this plugin into another project.
- To reuse in another project: drop the `ContentTextDumper/` folder into that project's `Plugins/`
  and rebuild its editor target. The commandlet has no project-specific code.

## Limitations / notes

- Blueprint graphs are emitted as raw engine node text (T3D): complete and exact, but verbose.
- Blueprint variable type categories are coarse (`object` / `struct` / `real`), not fully resolved
  type names.
- Output encoding is whatever the engine's string writer auto-detects; assets containing non-ASCII
  characters may be written as UTF-16 rather than UTF-8.

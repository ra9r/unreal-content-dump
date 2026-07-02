---
name: content-dump
description: >-
  Dump an Unreal Engine project's Blueprints and Content assets to readable text so they can be
  inspected, diffed, or understood. Use when working in a UE project and you need to read what a
  Blueprint / struct / enum / data asset actually does (its variables, components, class defaults,
  and graph logic) — because `.uasset` files are binary and can't be read directly — or when the
  user asks to export/dump/inspect Unreal Content as text. Installs and runs the ContentTextDumper
  editor commandlet bundled with this skill.
---

# Unreal Content Dump

Produce a faithful **text** representation of an Unreal Engine project's logic-bearing Content
(Blueprints, user structs/enums, data tables, data assets) by installing and running an editor
**commandlet** (`DumpContent`) bundled with this skill. The output lets you read and reason about
Blueprint logic that is otherwise locked inside binary `.uasset` files.

The dumper is **engine-backed**: it runs the real engine headless, so the text is exact for the
project's engine version (it uses the engine's own node serialization, the same data you get from
copy/pasting Blueprint nodes).

## When to use

- The user asks to understand, inspect, review, or explain a Blueprint / the project's logic.
- You need to read what an actor, widget, GameMode, struct, enum, or data asset does and only the
  `.uasset` is available.
- The user explicitly asks to dump/export Unreal Content to text.

## Requirements (state these if unmet)

- Unreal Engine installed, and the project's **editor target can be compiled** (a C++ toolchain is
  available). The commandlet is C++, so it must be built into the project once.
- Works on macOS / Windows / Linux. Commands below show macOS first.

## Caveats & first-run notes (surface these before running)

- **No prior editor session is needed.** This skill builds the C++ itself and the commandlet builds
  the asset registry headlessly — you do NOT have to open the project in the editor first. But the
  project **must be buildable on this machine**: the matching engine version installed (the
  `.uproject`'s `EngineAssociation`; a GUID means a registered source build) **and** a C++ toolchain
  (Xcode / VS build tools / clang). A missing engine or toolchain is the most common failure on a
  fresh checkout — the build fails before any dump runs.
- **First run is slow.** A fresh checkout has no `Binaries/`, so step 4 compiles from scratch
  (minutes) and the first dump does a full asset-registry scan. Subsequent runs are fast.
- **This mutates the working tree.** Installing copies `Plugins/ContentTextDumper/` in and edits the
  `.uproject` (the `TargetAllowList` entry). On a clean checkout those show up as changes — tell the
  user, and let them decide whether to commit the tool or keep it local (don't silently commit it
  into a shared game repo).
- **Close the editor first.** The dump reads the **last-saved `.uasset` on disk**, not unsaved
  in-editor state, and a second `UnrealEditor` process can contend with an open one. Save/close the
  project before running.

## Procedure

### 1. Locate the project
Find the `.uproject` (search the current directory and parents). Record:
- `PROJECT_DIR` — directory containing the `.uproject`
- `UPROJECT` — full path to the `.uproject`
- `PROJECT_NAME` — `.uproject` filename without extension (the editor target is `<PROJECT_NAME>Editor`)

If there is no `.uproject`, stop — this skill only applies to Unreal projects.

### 2. Resolve the engine
Read `EngineAssociation` from the `.uproject`.
- **Version string** (e.g. `"5.7"`): use the platform default install path:
  - macOS: `/Users/Shared/Epic Games/UE_<ver>`
  - Windows: `C:\Program Files\Epic Games\UE_<ver>`
  - Linux: ask the user, or use `$UE_ROOT`
- **GUID** (source/custom build): check `$UE_ROOT` first; otherwise look it up
  (macOS/Windows launcher registers installs) or ask the user for the engine root.
- Always allow `$UE_ROOT` to override.

Verify the command binary exists before continuing:
- macOS: `<ENGINE>/Engine/Binaries/Mac/UnrealEditor-Cmd`
- Windows: `<ENGINE>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe`
- Linux: `<ENGINE>/Engine/Binaries/Linux/UnrealEditor-Cmd`

### 3. Install the plugin (idempotent)
If `<PROJECT_DIR>/Plugins/ContentTextDumper/` does **not** exist, copy this skill's bundled source
into it. The source lives next to this SKILL.md in `./ContentTextDumper/`:

```sh
cp -R "<THIS_SKILL_DIR>/ContentTextDumper" "<PROJECT_DIR>/Plugins/ContentTextDumper"
```

If it already exists, confirm it's this dumper (not a name clash) and skip the copy.

**Restrict it to editor targets** so it never breaks packaging/cooking of the game. In the project's
`.uproject`, ensure the `Plugins` array has an entry limiting ContentTextDumper to `Editor` (add it
if missing):

    { "Name": "ContentTextDumper", "Enabled": true, "TargetAllowList": [ "Editor" ] }

Without this, the plugin auto-enables for **all** targets, and a packaged/cook build fails with
*"Plugin 'ContentTextDumper' failed to load because module 'ContentTextDumper' could not be found"*
(its module is editor-only, so it isn't built for the game target).

> Note: if the project is **Blueprint-only** (no `Source/` and no editor target yet), adding this
> C++ plugin turns it into a code project. You may need to generate project files first
> (`<ENGINE>/Engine/Build/BatchFiles/<Platform>/GenerateProjectFiles.sh` or UnrealBuildTool
> `-projectfiles`), and the build requires a full toolchain. Warn the user before proceeding.

### 4. Build the editor target
Compile so the commandlet exists. macOS:
```sh
"<ENGINE>/Engine/Build/BatchFiles/Mac/Build.sh" <PROJECT_NAME>Editor Mac Development \
    -Project="<UPROJECT>" -waitmutex
```
- Windows: `"<ENGINE>\Engine\Build\BatchFiles\Build.bat" <PROJECT_NAME>Editor Win64 Development -Project="<UPROJECT>" -waitmutex`
- Linux: `"<ENGINE>/Engine/Build/BatchFiles/Linux/Build.sh" <PROJECT_NAME>Editor Linux Development -Project="<UPROJECT>" -waitmutex`

This first build can take a while; subsequent runs are fast.

### 5. Run the dump commandlet
Write to `Saved/ContentTextDump` (under `Saved/`, which UE projects ignore by default, so it never
pollutes tracked files):
```sh
"<ENGINE>/Engine/Binaries/Mac/UnrealEditor-Cmd" "<UPROJECT>" \
    -run=DumpContent \
    -OutDir="<PROJECT_DIR>/Saved/ContentTextDump" \
    -Root=/Game \
    -unattended -nopause -nosplash -NullRHI
```
- `-OutDir=` defaults to `<PROJECT_DIR>/Docs/ContentText` if omitted; pass `Saved/ContentTextDump`
  explicitly as above unless the user wants it committed/diffable, in which case use a tracked dir.
- `-Root=` defaults to `/Game`; narrow it to a subtree (e.g. `/Game/MyGame/Blueprints`) for speed.

### 6. Read the output
Files mirror the `/Game` tree under the output dir, one per asset. Extensions and contents are
described in `reference/output-format.md` (next to this file). Read only the files relevant to the
user's question — Blueprint graphs are complete but verbose.

> **Note:** each node serializes to one very long line (all pins inline) — prefer targeted `grep -oE`
> / `awk` and read by `Begin Object` … `End Object` block. The dump carries no reliable error signal
> (compiler-state fields are stripped at export); judge correctness from the wiring and the user's
> in-editor Compile status, never from node metadata. See `reference/output-format.md`.

## Caching / re-running
If the output dir already exists and the assets haven't changed, reuse it instead of rebuilding and
re-dumping. Re-run (delete the output dir for a clean pass) after asset edits or when the user asks.

## Troubleshooting
- **"DumpContentCommandlet looked like a commandlet, but we could not find the class."** — the
  plugin module didn't load in time. Its `LoadingPhase` must be `Default` (it already is in the
  bundled copy); don't change it.
- **Build fails on a Blueprint-only project** — see the note in step 3; the project needs to become
  a code project (generate project files, full toolchain).
- **Cook/package fails: "Plugin 'ContentTextDumper' ... module 'ContentTextDumper' could not be
  found."** — the editor-only plugin got enabled for the game target. Add the
  `"TargetAllowList": ["Editor"]` uproject entry from step 3.
- **Engine not found** — set `$UE_ROOT` to the engine install root and retry.
- **Build fails on a fresh checkout** — confirm the `.uproject`'s engine version is installed and a
  C++ toolchain is present, and that the project's own plugin dependencies are installed. The project
  must be buildable independently of this tool.
- Tested on UE 5.7; the commandlet uses UE 5.1+ asset-registry APIs.

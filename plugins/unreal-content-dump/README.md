# unreal-content-dump

A Claude Code plugin that makes an Unreal Engine project's Blueprints and Content readable as text.

AI assistants can't read binary `.uasset` files. This plugin bundles an editor-only Unreal plugin
(`ContentTextDumper`) plus a skill that installs it into your project, builds it, and runs a
headless commandlet to export every Blueprint's graph logic, variables, components, and class
defaults — plus user structs, enums, data tables, and data assets — mirroring your `/Content` tree,
one text file per asset.

- Skill: [`skills/content-dump/SKILL.md`](skills/content-dump/SKILL.md)
- Output format: [`skills/content-dump/reference/output-format.md`](skills/content-dump/reference/output-format.md)
- Bundled Unreal plugin source: [`skills/content-dump/ContentTextDumper/`](skills/content-dump/ContentTextDumper/)

**Requirements:** the project's matching Unreal Engine version installed and a C++ toolchain — the
bundled editor plugin compiles once per project. No prior editor session required.

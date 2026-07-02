# unreal-tools — Claude Code marketplace

Claude Code plugins for working with Unreal Engine projects.

## Plugins

### unreal-content-dump

Dumps an Unreal project's Blueprints and Content (structs, enums, data tables, data assets) to
readable **text**, so Claude — or any tool — can inspect, diff, and understand logic that is
otherwise locked inside binary `.uasset` files. It self-installs a small editor-only Unreal plugin,
builds it, and runs a headless commandlet.

## Install

In a Claude Code session:

```
/plugin marketplace add ra9r/unreal-content-dump
/plugin install unreal-content-dump@unreal-tools
```

Then, inside any Unreal project, invoke it with `/unreal-content-dump:content-dump` — or just ask
Claude to "dump this project's Content to text" / "what does this Blueprint do?".

Requirements: the project's matching Unreal Engine version installed and a C++ toolchain (the
bundled editor plugin compiles once per project).

## License

MIT — see [LICENSE](LICENSE).

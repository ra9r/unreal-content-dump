# unreal-tools — Claude Code marketplace

Claude Code plugins for working with Unreal Engine projects.

## Plugins

### unreal-content-dump

Dumps an Unreal project's Blueprints and Content (structs, enums, data tables, data assets) to
readable **text**, so Claude — or any tool — can inspect, diff, and understand logic that is
otherwise locked inside binary `.uasset` files. It self-installs a small editor-only Unreal plugin,
builds it, and runs a headless commandlet.

## Install

In a Claude Code terminal session (run `claude` in your terminal), enter these commands **one at a
time — each on its own line, and wait for the first to finish before running the second**. Pasting
both at once can merge them into a single, broken command.

**1. Add the marketplace** — use the full **HTTPS** URL (avoids SSH host-key issues on public repos):

```
/plugin marketplace add https://github.com/ra9r/unreal-content-dump.git
```

**2. Install the plugin:**

```
/plugin install unreal-content-dump@unreal-tools
```

Then start Claude Code inside your Unreal project and just ask for what you want in plain language —
the skill triggers automatically. For example:

> Review all the Blueprints and C++ in this project and give me a code review as an expert Unreal
> Engine developer.

Behind the scenes it dumps the project's Content to text, then reads it to answer.

Requirements: the project's matching Unreal Engine version installed and a C++ toolchain (the
bundled editor plugin compiles once per project).

## License

MIT — see [LICENSE](LICENSE).

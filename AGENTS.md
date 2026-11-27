# Project ownership and vendor boundaries

- Vendor ACF source **and assets are READ ONLY**. This includes the entire
  `Plugins/ACFUAsceb6d4445a7304V7/` tree, `Plugins/ACFUltimate/`, and other
  vendor ACF installations. Do not patch, reparent, resave or otherwise modify
  vendor files/assets. Reading, compiling and linking unmodified vendor code
  is allowed. Do not reapply historical vendor patches from `Saved/`.
- Implement adaptations in Pangea-owned `Source/` modules or
  `Plugins/GameFeatures/GF_Pangea*/`. Use supported subclasses, components,
  delegates, interfaces and configuration. If no supported extension point
  exists, explain the limitation; do not modify or monkey-patch the vendor.
- Vendor/sample content is reference material. Make project-owned copies when
  using ACF Blueprints/content in project features: create a project-owned copy
  or a child class, with `Pangea` after the asset-type prefix (for example
  `BP_Pangea_*`, `WBP_Pangea_*`, `DA_Pangea_*`). Keep the original unchanged.
  Parent/dependency references do not grant permission to modify vendor assets.
  New Editor assets go under `/Game/Yarin` unless
  the user explicitly specifies another destination. Do not edit `Content/FullSample`
  or `Content/AnimationLibrary` as a shortcut.
- Preserve unrelated and pre-existing changes. A vendor file already dirty in
  git is not permission to edit or discard it. Restore only changes whose
  ownership has been established and whose rollback the user requested.
- New reflected C++ classes/properties/functions require closing the Editor
  and a normal rebuild before asset wiring or acceptance tests. Never certify
  a vendor-clean result using an Editor still running old vendor Live Coding patches.
- Run `powershell -NoProfile -File Scripts/Protect-PangeaVendor.ps1 -Action Check`
  before and after asset/source work. Vendor authored files must have the Windows
  read-only attribute; never clear it to make a customization or use Save All.
  Save only explicitly named Pangea-owned packages. Build output/cache directories
  are excluded from file protection so unmodified vendor code can still build.
  Only an explicitly requested vendor update may temporarily lift protection.
  After an update, audit the new vendor baseline and reapply `-Action Protect`.

# Project index

- Read `Docs/Index/INDEX.md` before scanning the repo. It is the table of
  contents: tags and subtags mapped to code roots, assets and docs.
- `Docs/Index/code.md` lists modules, dependencies and every reflected C++
  type. `Docs/Index/content.md` lists project assets, maps and gameplay tags.
  `Docs/Index/epics.md` lists epics and stories with status.
- Update the matching index row when adding a module, reflected type, project
  asset, gameplay tag or quest.

# Coding standards

- Before writing or reviewing Unreal C++, read
  `.agents/rules/cpp-coding-standards.md` and `.agents/skills/ue5/SKILL.md`
  and follow them. Before driving the Editor through Python (Blueprint graph,
  Material or asset edits), read the guardrail sections of the same skill.
- This file wins wherever those files conflict with it. In particular:
  - **Vendor and engine code stay read only.** The engine is a launcher build
    (5.8) and the vendor rules above forbid patching.
  - **Copyright header (rule 2.2).** Match the header already used by the
    surrounding module.
  - **Source control is git.** Never clear a read-only attribute to edit a
    file: in this project it marks vendor files. Commit or push only when the
    user asks.
  - **Asset saves** follow the vendor rules above: save only explicitly named
    Pangea-owned packages, never Save All.
- `.agents/rules/agent-workflow.md` is tool-specific (Antigravity). Other
  agents ignore it.
- Apply the standards to new and touched Pangea-owned code. Do not restyle
  untouched code, and never restyle vendor code.

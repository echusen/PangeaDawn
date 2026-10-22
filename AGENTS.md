# Project ownership and vendor boundaries

- Vendor ACF source **and assets are READ ONLY**. This includes the entire
  `Plugins/ACFUAsce5ab7c1439afbV5/` tree, `Plugins/ACFUltimate/`, and other
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

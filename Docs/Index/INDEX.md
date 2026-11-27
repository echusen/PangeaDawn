# Pangea Dawn: project index (TOC)

Generated 2026-11-27 from branch `Programming-Thomas` (HEAD `2a40d23b`). Engine UE 5.8.3, ACF Ultimate 4.6.
Read this file first, then open only the section you need. Line numbers are the reflection macro line and drift with edits; class names and paths are the stable keys.

## Files in this index

| File | Holds | Open it when |
|---|---|---|
| `Docs/Index/INDEX.md` | This TOC: tags, subtags, root paths | Always first |
| `Docs/Index/code.md` | Modules, dependencies, every reflected C++ type (192) | You need a class, header or module dependency |
| `Docs/Index/content.md` | Project-owned assets, maps, name prefixes, gameplay tags | You need an asset path or a gameplay tag |
| `Docs/Index/epics.md` | Epics, stories, status, Miro node references | You need to know what is built, open or blocked |

## Ownership (see root `AGENTS.md` for the rules)

| Area | Paths | Rule |
|---|---|---|
| Project C++ | `Source/`, `Plugins/GameFeatures/GF_Pangea*/` | Editable |
| Project assets | `Content/Yarin/` (new work), `Content/_Game/` | Editable; new assets go under `/Game/Yarin` |
| Vendor ACF | `Plugins/ACFUAsceb6d4445a7304V7/` (ACF Ultimate 4.6), `Plugins/ACFUltimate/`, `Plugins/ACFUAsce5ab7c1439afbV5/` | Read only |
| Vendor sample content | `Content/FullSample/` (4,939 assets) | Read only, reference |
| Editor tooling | `Plugins/Blueprin0b23223502eaV17/` (BlueprintAssist 4.9.5), `Plugins/DarkerNo0302b0105931V13/` (Darker Nodes 3.6), `Plugins/Developer/RiderLink/` | Not project code |
| Engine template leftovers | `Source/Pangea_Dawn/Variant_*`, `Content/Variant_*`, `Content/ThirdPerson`, `Content/Mannequin`, `Content/LevelPrototyping` | Sample code; do not build on it |
| Local only (git-ignored) | `Saved/Yarin/` (Miro export, quest spec, playtest shots) | Source of truth for story text |

## Tags and subtags

Each tag maps to code, assets and docs. Subtags narrow to one folder or one concern.

| Tag | Subtags | Code root | Assets | Docs (`Writerside/topics/`) |
|---|---|---|---|---|
| `#core` | `definitions`, `interfaces`, `types` | `Source/PangeaCore/` | none | `acf-creature-profiles.md` |
| `#dino` | `base-character`, `profile`, `gas-stats`, `species` | `Source/PangeaDinosaurAI/` | `Content/_Game/Characters/Dinosaurs/` | `dinosaur-ai.md`, `dinosaur-base-character.md`, `dinosaur-integration.md` |
| `#player` | `controller`, `abilities`, `retarget` | `Source/Pangea_Dawn/Pangea_Dawn*.h` | `Content/_Game/Characters/Player/`, `Content/Yarin/BP_Pangea_*PlayerControllerBase` | none |
| `#acf-bridge` | `items`, `hit-action`, `setup`, `library`, `bridge-assets` | `Source/Pangea_Dawn/Public/ACF/` | `Content/_Game/Core/BridgeAssets/` | `acf-modernization-baseline.md` |
| `#taming` | `component`, `ability`, `config`, `states-roles`, `minigame-ui` | `Plugins/GameFeatures/GF_PangeaTamingSystem/` | `DA_Pangea_Tame_*`, `WBP_TamingMinigame`, `BP_TamingComponent` | `taming-system.md`, `taming-component.md`, `taming-config.md`, `taming-states-roles.md`, `taming-integration.md` |
| `#breeding` | `farm`, `egg`, `genetics`, `species-data`, `persistence`, `ui` | `Plugins/GameFeatures/GF_PangeaBreedingSystem/` | `Content/_Game/BreedingSystem/`, `Content/_Game/UI/WBP_Breed*`, `WBP_EggEntry` | `breeding-system.md`, `breeding-components.md`, `breeding-genetics.md`, `breeding-farm.md`, `breeding-integration.md`, `acf-breeding-persistence.md` |
| `#base` | `village`, `facility`, `requirements`, `actions`, `fragments`, `milestones`, `ui` | `Plugins/GameFeatures/GF_PangeaBaseSystem/` (module `PangeaBaseUpgradeSystem`) | `Content/_Game/BaseUpgradeSystem/` | `base-upgrade-system.md`, `base-upgrade-components.md`, `base-upgrade-data-assets.md`, `base-upgrade-requirements-actions.md`, `base-upgrade-integration.md`, `acf-base-upgrade-safety.md` |
| `#mining` | `site`, `discovery`, `stockpile`, `chest`, `routine`, `transport`, `convoy`, `presentation`, `smart-objects`, `ui`, `editor-tests` | `Plugins/GameFeatures/GF_PangeaMiningSystem/` (4 modules) | `Content/_Game/MiningSystem/` (27, test scaffolding), `Content/Yarin/*Mining*`, `*Korro*` | `acf-mining-routines.md`, `acf-mining-timing.md`, `acf-mining-public-apis.md` |
| `#hunting` | `clue`, `tracks`, `hunter-sense`, `species-config`, `ui` | `Plugins/GameFeatures/GF_PangeaHuntingSystem/` | `Content/_Game/HuntingSystem/` | none |
| `#quest` | `tutorial`, `clear-the-area`, `meet-the-elder`, `objectives`, `dialogue`, `characters`, `voices` | `Source/Pangea_Dawn/Public/ACF/PangeaTutorialObjectives.h` | `Content/Yarin/Quests/` | `opening-chapter-gaps.md` |
| `#ui` | `hud`, `pages`, `registry`, `theme`, `skills`, `notifications` | per system `UI/` folders | `Content/Yarin/WBP_Pangea*`, `DA_PangeaWidgetRegistry`, `DA_PangeaUITheme`, `Content/_Game/UI/` | none |
| `#gameplay-tags` | `quest`, `character`, `tame`, `actions`, `village`, `smartobject`, `ui` | no native C++ tags | `Content/_Game/GameplayTags/DT_Pangea_GameplayTags`, `Config/DefaultGameplayTags.ini` | none |
| `#maps` | `demo`, `menu`, `mining-test` | none | `Content/Maps/`, `Plugins/GameFeatures/GF_PangeaMiningSystem/Content/Mining/Maps/` | none |
| `#tests` | `core`, `dino`, `base`, `breeding`, `hunting`, `mining`, `taming` | `**/Private/Tests/` | none | `acf-modernization-baseline.md` |
| `#vendor` | `acf`, `fullsample`, `protection` | `Plugins/ACFU*/` | `Content/FullSample/` | `acf-modernization-baseline.md` |
| `#docs` | `systems`, `checkpoints`, `backlog` | `Writerside/` (`pd.tree`, `topics/*.md`) | none | `overview.md` |

## Quick routes

| Question | Go to |
|---|---|
| Where is class X? | `code.md`, search the name |
| What does module X depend on? | `code.md` section 1 |
| Which asset holds X? | `content.md` section 2 or 3 |
| Does gameplay tag X exist? | `content.md` section 6 |
| Is quest or mechanic X built? | `epics.md` |
| What does the Miro board say for node N? | `Saved/Yarin/Quests/miro_export/quest_graph.json`, key on `id` (board codes repeat) |
| Which map has the demo content? | `Content/Maps/SC_Demo.umap` |
| Vendor file protection | `Scripts/Protect-PangeaVendor.ps1 -Action Check` |
| C++ coding standards, UE5 Python editor guardrails | `.agents/rules/cpp-coding-standards.md`, `.agents/skills/ue5/SKILL.md`; project overrides in root `AGENTS.md` |

## Keeping this current

Update the matching row when you add a module, a reflected type, a project asset, a gameplay tag or a quest. Regenerate the whole index after a large merge or an ACF update.

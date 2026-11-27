# Content and gameplay tag index

Part of `Docs/Index/INDEX.md`. Asset names only; no `.uasset` was opened in the Editor.
Object path form: `Content/Yarin/X.uasset` is `/Game/Yarin/X`.

## 1. Content folders

| Folder | Assets | Owner |
|---|---|---|
| `Content/FullSample` | 4,939 | Vendor sample, read only |
| `Content/Environment` | 601 | Not verified (looks like art or sample packs) |
| `Content/Entities` | 260 | Not verified |
| `Content/Characters` | 258 | Not verified |
| `Content/_Game` | 93 | Project |
| `Content/Yarin` | 69 indexed here | Project; new assets go here |
| `Content/Animation`, `MSPresets`, `Landscape_Master`, `Audio`, `Input`, `GASP`, `Blueprints` | 45, 43, 33, 17, 9, 7, 1 | Not verified |
| `Content/Maps` | 19 | Project maps |
| `Content/Variant_Combat`, `Variant_SideScrolling`, `Variant_Platforming`, `ThirdPerson`, `Mannequin`, `LevelPrototyping` | 29, 19, 11, 5, 17, 26 | Engine template samples |

The vendor check on 2026-11-27 also listed newly added files under `Yarin/BowAndArrow/` plus `Yarin/M_Arrow_SteelWoodFeathers` and `Yarin/T_Arrow_SteelWoodFeathers_BC`. They are not in the lists below.

## 2. `Content/Yarin/` (project, current work)

### Root (41)

| Tag | Assets |
|---|---|
| `#mining` | `ABP_Pangea_MiningLocomotion`, `ABP_Pangea_MiningUnarmedMoveset`, `BS_Pangea_MiningLocomotion`, `AS_Pangea_MinerDeposit`, `AS_Pangea_MinerRetrieve`, `AS_Pangea_MiningStrike`, `BP_Pangea_MiningChariot`, `BP_Pangea_MiningRoutineController`, `BP_Pangea_MiningDiscovery_HybridPrototype`, `BP_Pangea_MiningSite_HybridPrototype`, `BP_Pangea_MiningStockpile_HybridPrototype`, `DA_Pangea_MiningSite_HybridPrototype`, `BP_Pangea_KorroDraft`, `DA_Pangea_KorroDraft`, `GF_PangeaMiningSystem` (GameFeatureData override) |
| `#player` | `BP_Pangea_FullPlayerControllerBase`, `BP_Pangea_UltimatePlayerControllerBase`, `GA_Pangea_Jump` |
| `#quest`, NPCs | `BP_Pangea_NPC` (Huscarl in SC_Demo), `BP_Pangea_Enemy`, `BP_Pangea_AIGroupSpawner`, `ACF_CinematicLibrary_BP` |
| `#dino` | `DA_Character_Triceratops`, `DA_Character_Triceratops_Child`, `DT_Pangea_CreatureAttributes`, `DT_Pangea_SerializableAttributes`, `GE_Pangea_AttributeDependencies`, `GE_Pangea_InheritedStatTargets`, `GE_Pangea_StatInit` |
| `#ui` | `DA_PangeaUITheme`, `DA_PangeaWidgetRegistry`, `WBP_PangeaHUDRoot`, `WBP_PangeaNotifications`, `WBP_PangeaBreedingPage`, `WBP_PangeaMiningChestPage`, `WBP_PangeaMiningSitePage`, `WBP_PangeaStockpilePage`, `WBP_PangeaVillagePage`, `WBP_Pangea_SkillInfo`, `WBP_Pangea_SkillTrees`, `WBP_Pangea_SkillTreesNavigation` |

### `Content/Yarin/Quests/` (`#quest`)

| Folder | Assets |
|---|---|
| root | `DT_Pangea_Quests` (quest registry, row struct `AQSQuestData`), `DA_Pangea_VoicesConfig`, `DA_Pangea_DialogueMediumShot` |
| `Characters/` | `DA_Pangea_Daughter`, `DA_Pangea_DesperateMan`, `DA_Pangea_Doro`, `DA_Pangea_Druwid`, `DA_Pangea_Elder`, `DA_Pangea_ScaredWoman` |
| `Tutorial/` | `AQS_Pangea_Tutorial`, `DA_Pangea_Huscarl`, `DLG_Pangea_Huscarl_CS1`, `DLG_Pangea_Huscarl_CS2`, `WDLG_Pangea_Tutorial_Attack`, `_Move`, `_MultiAttack`, `_Roll`, `_Sneak`, `_Sprint`, `_Weapon` |
| `ClearTheArea/` | `AQS_Pangea_ClearTheArea`, `DLG_Pangea_Huscarl_CS3`, `DLG_Pangea_Daughter_N0028`, `DLG_Pangea_ScaredWoman_N0029`, `DLG_Pangea_DesperateMan_N0030` |
| `MeetTheElder/` | `AQS_Pangea_MeetTheElder`, `DLG_Pangea_Elder_CS4`, `DLG_Pangea_Elder_CS5` |

## 3. `Content/_Game/` (project, older layout)

| Folder | Tag | Assets |
|---|---|---|
| root | | `DA_PangeaTeamConfig`, `Pangea_BP_Farm_Base` |
| `Core/GameMode`, `GameState` | `#player` | `BP_Pangea_GameMode`, `BP_Pangea_GameState` |
| `Core/BridgeAssets` | `#acf-bridge` | `BP_Bridge_GASGameMode`, `BP_Bridge_GASHUD`, `BP_Bridge_GameState`, `BP_Bridge_GasPlayer`, `BP_Bridge_PlayerController`, `BP_Bridge_SpectatorController` |
| `Core/Components`, `Interfaces` | | `BP_TamingComponent`, `Pangea_BPI_Dinosaur`, `Pangea_BPI_Player` |
| `Characters/Player` | `#player` | `DA_Pangea_AS_Player`, `GA_Pangea_Player_Command_FollowMe`, `Blueprints/BP_Pangea_Player`, `Blueprints/BP_Pangea_PlayerController`, `Animations/ABPPangea_GenericRetargeter`, `Retargeter/` (3) |
| `Characters/Dinosaurs` | `#dino`, `#taming` | `BP_Pangea_DinosaurBase`, `DA_Pangea_Tame_Config`, `Abilities/GA_Dinosaur_Attack`, `Abilities/GA_Dinosaur_Flee`, `DataAssets/DA_PangeaSpecies` |
| `Characters/Dinosaurs/Mounts/Cerato` | | `BP_Pangea_Cerato`, `DA_Pangea_Cerato_AS`, `DA_Pangea_Tame_Cerato`, `Animations/ABP_Pangea_Cerato`, `Animations/BS_Cerato_IdleWalkRun` |
| `Characters/Dinosaurs/Mounts/Korro` | | `BP_Pangea_Korro`, `CD_Korro`, `DA_Pangea_Korro_AS`, `DA_Pangea_Tame_Korro`, `Animations/ABP_Pangea_Korro`, `Animations/BS_Korro_WalkRun` |
| `Characters/Dinosaurs/Triceratops` | `#breeding` | `BP_Pangea_Triceratops`, `BP_Pangea_TriceratopsChild`, `CD_Triceratops`, `CD_Triceratops_Child`, `DA_Pangea_Triceratops_AS`, `Animations/ABPPangea_Triceratops`, `Animations/BS_Triceratops_WalkRun`, `Blueprints/BP_Egg` |
| `BaseUpgradeSystem` | `#base` | `BUD_Village`, `Blueprints/BP_Village`, `Blueprints/BP_FG_Blacksmith`, `Blueprints/BP_PangeaBaseUpgradeComponent`, `UI/WBP_VillageUpgradeMenu`, `UI/WBP_FacilityUnlockEntry`, `UI/WBP_RequirementEntry` |
| `BreedingSystem` | `#breeding` | `GE_Pangea_BreedingInheritedStats`, `Pangea_BP_Farm_Base` |
| `HuntingSystem` | `#hunting` | `BP_HuntClueActor`, `DA_HuntSpecies_Default`, `M_HuntBlood_Decal`, `M_HuntBrokenFoliage_Decal`, `M_HuntTrack_Visual` |
| `MiningSystem` (27) | `#mining` | Test scaffolding, all suffixed `_Test` except `BP_MiningSiteChest_Clean` and `BP_SettlementStockpile`. Per role (Worker, Guard, Courier): `BP_Mining<Role>Actor_Test`, `BP_Mining<Role>AIController_Test`, `GI_BD_Mining<Role>_Test`, `SO_Mining<Role>_Test`, `ST_Mining<Role>_Test`, `ST_GI_Mining<Role>_Test`. Also `BP_MiningSite_Test`, `BP_MiningSiteVisual_Test`, `BP_MiningDiscoveryNode_Test`, `DA_MiningSite_Iron_Test`, `DA_MiningSitePresentation_Test`, `WBP_MiningSiteMenu_Test`, `WBP_MiningSettlementStockpile_Test` |
| `UI` | `#ui` | `WBP_BreedableEntry`, `WBP_BreedingFarmPanel`, `WBP_EggEntry`, `WBP_TamingMinigame`, `HUD/BP_Pangea_HUD`, `HUD/Pangea_HUD_WB` |
| `GameplayTags` | `#gameplay-tags` | `DT_Pangea_GameplayTags` |

GameFeature plugin content: each plugin has a `GF_Pangea*System` GameFeatureData asset. Hunting adds `Materials/M_HuntTrack_Visual` and `DataAssets/DA_HuntSpecies_Default`. Mining adds `Mining/Test/BP_MiningDiscoveryNode_Test` and `Mining/Maps/L_MiningTest.umap`.

## 4. Maps (`#maps`)

| Map | Use |
|---|---|
| `Content/Maps/SC_Demo.umap` | Main demo map: tutorial quest, Huscarl, Korro transport prototype area |
| `Content/Maps/SC_MenuMap.umap` | Menu |
| `Content/Maps/SC_PrimBur.umap` | Purpose not verified |
| `Content/Maps/L_EngineOpen.umap` | Light default map for fast Editor start (commit `1f2bb80b`) |
| `Plugins/GameFeatures/GF_PangeaMiningSystem/Content/Mining/Maps/L_MiningTest.umap` | Mining test map |
| `Content/FullSample/**` (7 maps) | Vendor: `FullMap`, `L_UltOpenWorld`, `L_Gameplay`, `L_Routine`, `L_Landscape`, `L_SharedGameplay`, `MenuMap` |
| `Content/Variant_*/Lvl_*`, `Content/ThirdPerson/Lvl_ThirdPerson` | Engine template samples |

## 5. Asset name prefixes

| Prefix | Holds |
|---|---|
| `BP_Pangea_` | Project Blueprints (actors, controllers, NPCs) |
| `WBP_Pangea_`, `WBP_Pangea` | Widgets (two spellings in use) |
| `DA_Pangea_`, `DA_Pangea` | Data assets: characters, species, taming, attribute sets, UI |
| `AQS_Pangea_` | ACF quest graphs |
| `DLG_Pangea_<Speaker>_<CSn or N00nn>` | ACF dialogues; suffix is the cutscene or the Miro board code |
| `WDLG_Pangea_` | World dialogues (tutorial prompts) |
| `DT_Pangea_` | Data tables |
| `GA_`, `GE_`, `AS_` | Gameplay abilities, gameplay effects, mining animation assets (`AS_Pangea_Miner*`) |
| `ABP_`, `ABPPangea_`, `BS_` | Animation Blueprints (two spellings), blend spaces |
| `CD_`, `BUD_` | Creature definition, base upgrade definition |
| `GI_BD_`, `SO_`, `ST_`, `ST_GI_` | Mining test scaffolding: gameplay interaction behaviour, smart objects, StateTrees |
| `GF_` | GameFeatureData |

Rule for new assets (root `AGENTS.md`): `Pangea` after the type prefix, under `/Game/Yarin`.

## 6. Gameplay tags (`#gameplay-tags`)

Sources, as registered in `Config/DefaultGameplayTags.ini`:
- Project table: `/Game/_Game/GameplayTags/DT_Pangea_GameplayTags`
- Project ini entries: `Config/DefaultGameplayTags.ini`
- Vendor tables (18): `/AscentCombatFramework/Configuration/` (`ActionTags`, `ItemTags`, `RPGTagsNew`, `MovesetTags`, `CommandTags`, `ItemSlot`, `MountTypesTag`, `TeamTags`, `NavigationMarkerTags`, `AIStateTags`), `/AscentCombatFramework/UITools/Blueprints/` (`UIActionsTag`, `IconTags`), `/Game/FullSample/GameplayTagTables/` (`RarityTags`, `Animations`, `CharactersTag`, `StatusTags`), `/Game/FullSample/Integrations/ATSIntegrations/` (`SampleQuestTags`, `FSMTags`)
- Native C++ tags: none

### Project table `DT_Pangea_GameplayTags`

Read from the asset's name table on 2026-11-27. Confirm in the Editor before relying on an entry.

| Root | Tags |
|---|---|
| `Quest.Tutorial` | `.Move`, `.Sprint`, `.Roll`, `.Sneak`, `.RetrieveWeapon`, `.EquipWeapon`, `.Attack`, `.MultiAttack`, `.TalkToHuscarl`, `.ReportToHuscarl` |
| `Quest.ClearTheArea` | `.ClearDoros`, `.LocateHuscarl`, `.TalkDaughter`, `.TalkDesperateMan`, `.TalkScaredWoman` |
| `Quest.MeetTheElder` | `.MeetElder`, `.TalkElderAgain` |
| `Character` | `.Player`, `.Huscarl`, `.Elder`, `.Druwid`, `.Daughter`, `.DesperateMan`, `.ScaredWoman`, `.Doro` |
| `Tame.State` | `.Wild`, `.Hostile`, `.Tamed` |
| `Tame.Role` | `.Mount`, `.Companion` |
| `Tame.Attempt` | `.InProgress`, `.Success`, `.Fail` |
| `Tame.Event` | `.Target` |
| `Actions` | `.Tame`, `.Flee`, `.Player.FollowMe` |
| `SmartObject.Mine` | `.Items`, `.DepositItems`, `.Rest` |
| `Village.Milestone` | `.Blacksmith` |
| `Test.Village` | `.Blacksmith` |
| `Pangea.State` | `.DialogueInputGuard` |

### Project entries in `Config/DefaultGameplayTags.ini`

| Root | Tags |
|---|---|
| `UI.Widget.Pangea` | `.Breeding`, `.MiningChest`, `.MiningSite`, `.Stockpile`, `.Village` (feature pages), `.Taming` (non-pausing overlay) |
| `UI.Layer` | `.PangeaActivity` (non-interactive live minigames) |
| `UI.Widget` | `.Settings` (ACF settings registry key) |
| `UI.Action` | `.AddMarker`, `.Cancel`, `.Confirm`, `.Continue`, `.Next`, `.NextTab`, `.Previous`, `.PreviousTab` |
| `Village.Milestone` | `.Mine` |
| `Test.Village` | `.Mine` |
| `Teams` | `.Dinosaurs` |
| `Abilities`, `GameplayCue` | `Abilities.ContextualAnim`, `Abilities.MoveTo`, `GameplayCue.ContextualAnim` |
| `Character` | `.AncientStone`, `.Grumpy`, `.Itchy` (sample leftovers) |
| `Conquest` | `.Capital`, `.Egypt`, `.Island`, `.Land`, `.Snow` (sample leftovers) |
| `Foley.Event` | `.Handplant`, `.Jump`, `.Land`, `.Run`, `.RunBackwds`, `.RunStrafe`, `.Scuff`, `.ScuffPivot`, `.ScuffWall`, `.Tumble`, `.Walk`, `.WalkBackwds` |
| `MotionMatching` | `.Default`, `.Idle`, `.Loops`, `.Pivots`, `.Starts`, `.Stops` |

Redirects in the ini: `RPG.Attribute` to `RPG.PrimaryAttributes`, `RPG.Parameters` to `RPG.Attributes`, `RPG.Statistic.{Health,Mana,Stamina}` to `RPG.Statistics.*`, `Conquest.Vikings` to `Conquest.Snow`.

# Code index

Part of `Docs/Index/INDEX.md`. 192 reflected types: `Source/` 87, GameFeature plugins 105.
Format of type rows: `Name` : parent, `Header.h:line`. Purposes come from names and parents; bodies were not read.
No native gameplay tags exist in C++ (`UE_DEFINE_GAMEPLAY_TAG*` has zero hits). No subsystems and no GameInstance class exist in project code.

## 1. Modules and dependencies

Targets: `Source/Pangea_Dawn.Target.cs` (Game), `Source/Pangea_DawnEditor.Target.cs` (Editor). Both list `Pangea_Dawn`, `PangeaBaseUpgradeSystem`, `PangeaCore`; build settings V7, include order `Unreal5_8`.

| Module | Location | Type | Project dependencies | ACF and engine dependencies of note |
|---|---|---|---|---|
| `PangeaCore` | `Source/PangeaCore/` | Runtime | none | GameplayTags |
| `PangeaDinosaurAI` | `Source/PangeaDinosaurAI/` | Runtime | PangeaCore | AscentCombatFramework, AscentSaveSystem, AscentCoreInterfaces, AdvancedRPGSystem, AscentGASRuntime, MountSystem, CharacterController, CollisionsManager, InventorySystem, ModularGameplay |
| `Pangea_Dawn` | `Source/Pangea_Dawn/` | Runtime (primary) | GF_PangeaMiningSystemRuntime (dynamic) | AscentCombatFramework, ActionsSystem, CraftingSystem, AscentQuestSystem, CommonUI, StateTree, AdvancedRPGSystem, AscentGASRuntime, InventorySystem, MountSystem |
| `PangeaBaseUpgradeSystem` | `Plugins/GameFeatures/GF_PangeaBaseSystem/Source/` | Runtime | none | InventorySystem, AscentQuestSystem, AscentMapsSystem, AscentSaveSystem, AdvancedRPGSystem, AscentUINavigationSystem, CommonUI |
| `PangeaBreedingSystem` | `Plugins/GameFeatures/GF_PangeaBreedingSystem/Source/` | Runtime | PangeaCore | InventorySystem, StatusEffectSystem, ActionsSystem, AdvancedRPGSystem, AscentUINavigationSystem |
| `GF_PangeaHuntingSystemRuntime` | `Plugins/GameFeatures/GF_PangeaHuntingSystem/Source/` | Runtime | PangeaCore, PangeaDinosaurAI | CharacterController, AdvancedRPGSystem, AscentUITools, GameFeatures |
| `GF_PangeaMiningSystemRuntime` | `Plugins/GameFeatures/GF_PangeaMiningSystem/Source/` | Runtime | PangeaCore, PangeaBaseUpgradeSystem | AIFramework, AscentSaveSystem, InventorySystem, SmartObjects, GameplayInteractions, StateTree, NavigationSystem |
| `MiningSystemPresentation` | same plugin | Runtime | GF_PangeaMiningSystemRuntime | SmartObjects, StateTree, NavigationSystem |
| `MiningSystemUI` | same plugin | Runtime | GF_PangeaMiningSystemRuntime, PangeaBaseUpgradeSystem | InventorySystem, AscentUITools, CommonUI |
| `MiningSystemEditor` | same plugin | Editor | the three mining modules | UnrealEd, LevelEditor, AIFramework, AscentTeams |
| `PangeaTamingSystem` | `Plugins/GameFeatures/GF_PangeaTamingSystem/Source/` | Runtime | PangeaCore | GameplayAbilities, AscentTeams, InventorySystem, ActionsSystem, AIFramework, CommonUI |

Plugin-level dependencies (`.uplugin`): the mining plugin depends on `GF_PangeaBaseSystem`. Every GameFeature plugin depends on `AscentCombatFramework`.
Mining GameFeatureData is overridden to `/Game/Yarin/GF_PangeaMiningSystem` in `Plugins/GameFeatures/GF_PangeaMiningSystem/Config/Settings.ini`.

## 2. Entry points

| Class | Parent | Header |
|---|---|---|
| `APangea_DawnCharacter` | ACharacter | `Source/Pangea_Dawn/Pangea_DawnCharacter.h:21` |
| `APangea_DawnGameMode` | AGameModeBase | `Source/Pangea_Dawn/Pangea_DawnGameMode.h:12` |
| `APangea_DawnPlayerController` | APlayerController | `Source/Pangea_Dawn/Pangea_DawnPlayerController.h:16` |
| `APDDinosaurBase` | AACFCharacter | `Source/PangeaDinosaurAI/Public/Characters/PDDinosaurBase.h:21` |

Blueprint entry points (GameMode, PlayerController, HUD) are in `content.md` section 3.

## 3. `#core`: `Source/PangeaCore/Public/`

- `Definitions/PangeaCreatureDefinition.h:11` `UPangeaCreatureDefinition` : UPrimaryDataAsset. Creature definition; holds fragments.
- `Definitions/PangeaDefinitionFragment.h:9` `UPangeaDefinitionFragment` : UObject. Abstract base for every system fragment.
- `Interfaces/PDBreedableInterface.h:8` `UPDBreedableInterface`
- `Interfaces/PDCreatureIdentityInterface.h:7` `UPDCreatureIdentityInterface`
- `Interfaces/PDDefinitionProviderInterface.h:9` `UPDDefinitionProviderInterface`
- `Interfaces/PDTameableInterface.h:11` `UPDTameableInterface`
- `Types/BreedingTypes.h`: `ECreatureGender` :9, `ECreatureGenderAssignmentMode` :17, `FGeneticTrait` :24, `FGeneticTraitSet` :36, `FParentSnapshot` :66, `FPangeaInheritedColor` :112, `FPangeaInheritedScalar` :122, `FPangeaInheritedAppearance` :133, `FIncubationConfig` :146, `FFertilityConfig` :155
- `Types/TamingTypes.h`: `ETameState` :7, `ETamedRole` :16, `FTameOutcome` :25

Fragment subclasses in other modules: `UPangeaACFProfileFragment`, `UPangeaBreedingFragment`, `UPangeaHuntingFragment`, `UPangeaMiningFragment`, `UPangeaTamingFragment`.

## 4. `#dino`: `Source/PangeaDinosaurAI/Public/`

- `Characters/PDDinosaurBase.h:21` `APDDinosaurBase` : AACFCharacter. Base for every dinosaur.
- `Components/PangeaCharacterInitializerComponent.h`: `EPangeaCharacterProfileSource` :7, `UPangeaCharacterInitializerComponent` :16 : UACFCharacterInitializerComponent
- `Components/PangeaGASStatisticsComponent.h:8` `UPangeaGASStatisticsComponent` : UACFGASStatisticsComponent
- `Definitions/PangeaACFProfileFragment.h:10` `UPangeaACFProfileFragment` : UPangeaDefinitionFragment
- Tests: `Source/PangeaDinosaurAI/Private/Tests/` (3 files)

## 5. `#acf-bridge`, `#quest`, `#player`: `Source/Pangea_Dawn/`

Root: `Pangea_DawnCharacter.h`, `Pangea_DawnGameMode.h`, `Pangea_DawnPlayerController.h` (see section 2).

`Public/ACF/`:
- `PangeaACFIntegrationLibrary.h:11` `UPangeaACFIntegrationLibrary` : UBlueprintFunctionLibrary
- `PangeaAdvancedHitAction.h:11` `UPangeaAdvancedHitAction` : UACFActionAbility
- `PangeaCharacterSetupComponent.h:8` `UPangeaCharacterSetupComponent` : UActorComponent
- `PangeaItemsManagerComponent.h:12` `UPangeaItemsManagerComponent` : UACFItemsManagerComponent
- `PangeaTutorialObjectives.h`: `EPangeaTutorialMoveState` :12, `UPangeaTutorialObjectiveBase` :25 : UAQSQuestObjective, `UPangeaDistanceTravelledObjective` :73, `UPangeaActionPerformedObjective` :104, `UPangeaWeaponWieldedObjective` :140

Tests: `Source/Pangea_Dawn/Private/Tests/` (3 files).

### Engine template sample code (do not build on it)

`Source/Pangea_Dawn/Variant_Combat/` (33 types): `ACombatCharacter`, `ACombatGameMode`, `ACombatPlayerController`, `AI/` (`ACombatAIController`, `ACombatEnemy`, `ACombatEnemySpawner`, `CombatStateTreeUtility.h` with 17 StateTree structs, `UEnvQueryContext_Player`), `Animation/` (3 AnimNotifies), `Gameplay/` (`ACombatActivationVolume`, `ACombatCheckpointVolume`, `ACombatDamageableBox`, `ACombatDummy`, `ACombatLavaFloor`), `Interfaces/` (`UCombatActivatable`, `UCombatAttacker`, `UCombatDamageable`), `UI/UCombatLifeBar`.
`Source/Pangea_Dawn/Variant_Platforming/` (4 types): `APlatformingCharacter`, `APlatformingGameMode`, `APlatformingPlayerController`, `UAnimNotify_EndDash`.
`Source/Pangea_Dawn/Variant_SideScrolling/` (14 types): character, game mode, controller, camera manager, `AI/`, `Gameplay/` (jump pad, moving platform, pickup, soft platform), `Interfaces/`, `UI/`.

## 6. `#base`: `Plugins/GameFeatures/GF_PangeaBaseSystem/Source/PangeaBaseUpgradeSystem/Public/` (35 types)

- `Actors/VillageBase.h:15` `AVillageBase` : AActor, IACFInteractableInterface, IALSSavableInterface
- `Actors/FacilityGroup.h:13` `AFacilityGroup` : AActor
- `Components/UpgradeSystemComponent.h:26` `UPangeaUpgradeSystemComponent` : UActorComponent, IPDBaseUpgradeInterface. Runs upgrade attempts.
- `Components/FacilityManagerComponent.h`: `FFacilityEntry` :10, `UPangeaFacilityManagerComponent` :25
- `Components/FacilitySlotComponent.h:12` `UFacilitySlotComponent` : USceneComponent
- `DataAssets/BaseUpgradeDefinition.h:10` `UBaseUpgradeDefinition` : UPrimaryDataAsset
- `Definitions/BaseUpgradeFragment.h:9` `UBaseUpgradeFragment` : UObject
- `Definitions/Fragments/`: `UBaseFacilityCatalogFragment` :8, `UBasePresentationFragment` :7, `UBaseProgressionFragment` :8, `UBaseSaveRulesFragment` :7
- `Interfaces/PDBaseUpgradeInterface.h:11` `UPDBaseUpgradeInterface`
- `Objects/UpgradeAction.h:14` `UUpgradeAction` : UObject (base)
- `Objects/UpgradeRequirement.h:13` `UUpgradeRequirement` : UObject (base)
- `Objects/Requirements/`: `UReq_FacilityUnlocked` :13, `FRequiredACFItem` (`Req_HasItems.h:9`), `UReq_HasItems` :24, `UReq_MilestoneCompleted` :13, `UReq_QuestCompleted` :13
- `Private/Objects/Requirements/Req_PlayerLevel.h:12` `UReq_PlayerLevel` (header is under Private)
- `Actions/UA_EnableFacility.h`: `EFacilityEnableMode` :10, `UUA_EnableFacility` :18
- `Actions/UA_GrantInventory.h:14` `UUA_GrantInventory`
- `Types/BaseUpgradeContext.h:12` `FBaseUpgradeContext`
- `Types/BaseUpgradeTypes.h`: `EPangeaUpgradeResult` :11, `FPangeaUpgradeResult` :24, `FPangeaUpgradeItemCost` :36, `FFacilityGroupReference` :46, `FUpgradeMilestoneDefinition` :58, `FUpgradeLevelDefinition` :76
- `Types/FacilityTypes.h:6` `EFacilitySlotType`
- `UI/`: `UFacilityUnlockEntryWidget` :12, `URequirementEntryWidget` :12, `UVillageUpgradeMenuWidget` :27
- Tests: `Private/Tests/` (quest requirement, upgrade safety)

## 7. `#breeding`: `Plugins/GameFeatures/GF_PangeaBreedingSystem/Source/PangeaBreedingSystem/Public/` (17 types)

- `Actors/PangeaBreedingFarmActor.h:12` `APangeaBreedingFarmActor` : AActor
- `Actors/PangeaEggActor.h`: `EPangeaEggState` :13, `APangeaEggActor` :26 : AACFWorldItem
- `Components/PangeaBreedableComponent.h:25` `UPangeaBreedableComponent` : UActorComponent, IPDBreedableInterface
- `Components/PangeaBreedingFarmComponent.h:19` `UPangeaBreedingFarmComponent` : UActorComponent, IPangeaFarmInterface
- `Data/PangeaSpeciesDataAsset.h:14` `UPangeaSpeciesDataAsset` : UPrimaryDataAsset
- `Definitions/PangeaBreedingFragment.h`: `EPangeaInheritedStatType` :16, `FPangeaInheritedStatRule` :24, `FPangeaInheritedStatValue` :54, `FPangeaInheritedStatProfile` :69, `FMaterialColorGeneticGroup` :83, `FMaterialScalarGeneticGroup` :95, `UPangeaBreedingFragment` :107
- `Helpers/PangeaBreedingLibrary.h:16` `UPangeaBreedingLibrary` : UBlueprintFunctionLibrary
- `Interfaces/PangeaFarmInterface.h:10` `UPangeaFarmInterface`
- `Objects/PangeaGeneticStrategy.h:13` `UPangeaGeneticStrategy` : UObject
- `UI/PangeaBreedingFarmWidget.h:17` `UPangeaBreedingFarmWidget` : UUserWidget
- Tests: `Private/Tests/` (compatibility, persistence, hatching)

## 8. `#taming`: `Plugins/GameFeatures/GF_PangeaTamingSystem/Source/PangeaTamingSystem/Public/` (6 types)

- `Abilities/PangeaTamingAbility.h:13` `UPangeaTamingAbility` : UACFActionAbility
- `Components/PangeaTamingComponent.h:22` `UPangeaTamingComponent` : UActorComponent, IPDTameableInterface
- `DataAssets/TameSpeciesConfig.h`: `FTameStatRequirement` :19, `UTameSpeciesConfig` :41 : UPrimaryDataAsset
- `Definitions/PangeaTamingFragment.h:9` `UPangeaTamingFragment`
- `UI/TamingWidget.h:14` `UTamingWidget` : UACFHUDUserWidget
- Tests: `Private/Tests/` (compatibility)

## 9. `#hunting`: `Plugins/GameFeatures/GF_PangeaHuntingSystem/Source/GF_PangeaHuntingSystemRuntime/Public/` (10 types)

- `Actors/PangeaHuntClueActor.h:16` `APangeaHuntClueActor` : AActor, IACFInteractableInterface
- `Components/PangeaHunterSenseComponent.h`: `FPangeaHuntVisualSlot` :20, `UPangeaHunterSenseComponent` :37
- `Components/PangeaTrackEmitterComponent.h:16` `UPangeaTrackEmitterComponent`
- `DataAssets/HuntSpeciesConfig.h`: `FHuntSpecialClueConfig` :16, `UHuntSpeciesConfig` :52 : UPrimaryDataAsset
- `Definitions/PangeaHuntingFragment.h:9` `UPangeaHuntingFragment`
- `Types/HuntingTypes.h`: `EHuntClueType` :7, `FHuntTrackPoint` :19
- `UI/PangeaFootprintFocusWidget.h:7` `UPangeaFootprintFocusWidget`
- Tests: `Private/Tests/` (lifecycle, replication)

## 10. `#mining`: `Plugins/GameFeatures/GF_PangeaMiningSystem/Source/` (37 types)

### `GF_PangeaMiningSystemRuntime/Public/`

| Subtag | Types |
|---|---|
| `site` | `Actors/MiningSiteActor.h:25` `AMiningSiteActor`; `Components/MiningSiteComponent.h:21` `UMiningSiteComponent` (core site state, harvest timing); `DataAssets/MiningSiteDefinition.h:14` `UMiningSiteDefinition`; `Types/MiningTypes.h`: `FMiningItemQuantity` :9, `FMiningStoredItem` :24, `FMiningSiteLevelDefinition` :39 |
| `discovery` | `Actors/MiningDiscoveryNodeActor.h:18` `AMiningDiscoveryNodeActor`; `Actions/UA_UnlockMiningSite.h`: `EMiningUnlockMode` :8, `UUA_UnlockMiningSite` :16 : UUpgradeAction |
| `stockpile`, `chest` | `Actors/MiningSettlementStockpileActor.h:18` `AMiningSettlementStockpileActor`; `Actors/MiningSiteChestActor.h:15` `AMiningSiteChestActor` |
| `routine` | `Components/MiningRoutineJobComponent.h`: `EMiningRoutineJobStatus` :18, `UMiningRoutineJobComponent` :25; `Private/Tasks/MiningRoutineTask.h:8` `UMiningRoutineTask` : UACFTask; `Private/Tasks/MiningSmartObjectInteractionTask.h:12` `UMiningSmartObjectInteractionTask` : UAITask |
| `transport`, `convoy` | `Components/PangeaMiningTransportComponent.h`: `EPangeaMiningShipmentPhase` :11, `FPangeaMiningShipment` :15, `UPangeaMiningTransportComponent` :63; `Components/PangeaMiningTransportJobComponent.h:17`; `Components/PangeaMiningTransportStorageComponent.h:7` : UACFStorageComponent; `Components/PangeaMiningConvoyComponent.h:12`; `Private/Tasks/PangeaMiningTransportRoutineTask.h:7` : UACFTask |
| `presentation` | `Components/MiningSitePresentationCoordinatorComponent.h:18`; `DataAssets/MiningSitePresentationConfig.h`: `FMiningPresentationStation` :14, `FMiningPresentationRoleConfig` :26, `UMiningSitePresentationConfig` :56; `Interfaces/MiningPresentationAgentInterface.h`: `EMiningPresentationRole` :7, `EMiningPresentationState` :15, `UMiningPresentationAgentInterface` :29; `Interfaces/MiningSitePresentationCoordinatorInterface.h:12` |
| fragment | `Definitions/PangeaMiningFragment.h:9` `UPangeaMiningFragment` |

Private implementation folders: `Private/Runtime/` (presentation sync, movement, route, routines, smart-object behaviour), `Private/Tasks/`, `Private/Tests/` (9 files: discovery, routine job, ownership, timing, workforce, transport, smart-object task, replication).

### Other mining modules

- `MiningSystemPresentation/Public/Components/MiningPresentationAgentComponent.h`: `FMiningAgentPresentationSnapshot` :17, `UMiningPresentationAgentComponent` :35. Test: `Private/Tests/` (ownership).
- `MiningSystemUI/Public/UI/`: `UMiningSettlementStockpileRowWidget` :12, `UMiningSettlementStockpileWidget` :14, `UMiningSiteChestWidget` :13, `UMiningSiteMenuWidget` :14. Test: `Private/Tests/` (UI navigation).
- `MiningSystemEditor/Private/Tests/`: 5 PIE and Editor tests (locomotion, multiplayer PIE, routine PIE, station animation, transport prototype).
- Script: `Plugins/GameFeatures/GF_PangeaMiningSystem/Scripts/SetupMiningPresentationConfig.py`.

## 11. `#tests` summary

| Area | Location | Files |
|---|---|---|
| Pangea_Dawn | `Source/Pangea_Dawn/Private/Tests/` | 3 |
| PangeaDinosaurAI | `Source/PangeaDinosaurAI/Private/Tests/` | 3 |
| Base | `.../PangeaBaseUpgradeSystem/Private/Tests/` | 2 |
| Breeding | `.../PangeaBreedingSystem/Private/Tests/` | 3 |
| Hunting | `.../GF_PangeaHuntingSystemRuntime/Private/Tests/` | 2 |
| Mining runtime | `.../GF_PangeaMiningSystemRuntime/Private/Tests/` | 9 |
| Mining presentation, UI, editor | `.../MiningSystemPresentation`, `MiningSystemUI`, `MiningSystemEditor` `/Private/Tests/` | 1, 1, 5 |
| Taming | `.../PangeaTamingSystem/Private/Tests/` | 1 |

Automation test prefix in the docs: `Pangea.ACF`.

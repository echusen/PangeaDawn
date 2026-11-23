# Opening Chapter: Systems Backlog

Source: Miro board `uXjVG9S_IZs=` ("Frame 1"), exported 2026-11-23.
A local copy of the export lives in `Saved/Yarin/Quests/miro_export/` (`IMPLEMENTATION_SPEC.md`, `quest_graph.json`).
`Saved/` is git-ignored, so move the export into source control if the team needs it.

This page lists the mechanics the board needs that the project does not have yet.
Each row says what exists today, so the gap is clear before work starts.

Status key:
- **Exists (partial)**: Pangea already has a system that covers part of it.
- **Vendor only**: ACF ships a module or class for it, but Pangea has not wired it up.
- **Missing**: nothing in Pangea or ACF covers it.

## Status of the chapter

| Board section | Quest | State |
|---|---|---|
| S1 Tutorial | `Quest.Tutorial` | Built: `/Game/Yarin/Quests/Tutorial/AQS_Pangea_Tutorial`, registered in `DT_Pangea_Quests`, auto-starts from the player controller. Huscarl (`BP_Pangea_NPC`) placed in SC_Demo. Weapon is the ACF sample `ACFSwordBP` placeholder. Not yet play-tested. |
| S2.1 Clear the area | Q_ClearTheArea | Not started. Blocked by Doro AI, stealth and observe gaps. |
| S2.2 Meet the Elder | Q_MeetTheElder | Not started. Dialogue only; needs new characters. |
| S3.1 Build a palisade | Q_BuildPalisade | Not started. Blocked by perimeters, ambush, tree felling. |
| S3.2 Learn to track | Q_LearnToTrack | Not started. Board branch outcomes are empty cards. |
| S5.1 to S5.3 Side quests | Scared Woman, Desperate Man, Daughter | Not started. Need world-state flags and stat changes. |

## Mechanics backlog

| # | Mechanic | Board refs | Status | What exists | What to build |
|---|---|---|---|---|---|
| 1 | Player stealth detection (crouched and unseen) | S2.1 Path B, N0027, NN0133/NN0134 | Vendor only | `ACFAIController::HandlePerceptionUpdated` (AIFramework). The tutorial objective checks crouch only. | A "player is stealthed" state from crouch plus AI perception, readable by AI and quests. |
| 2 | Doro AI: flee and despawn when surprised, fight when approached openly | S2.1, N0073 | Exists (partial) | Taming `TameSpeciesConfig` (`bRunAwayOnFail`, `Actions.Flee`), `GA_Dinosaur_Flee`, ACF `EFlee` AI state. | Doro character data asset, combat behaviour that switches to flee on a stealth approach or attack, despawn after fleeing, fled and slain counters. |
| 3 | Observe a creature from behind (10 to 15 m, crouched, press E) | N0026 | Missing | Nothing. | An interaction gated by distance, angle and stealth that completes an objective and unlocks an achievement. |
| 4 | Achievements | N0027 "You identified a creature" | Missing | No achievement manager. | An achievement subsystem (platform hook optional) plus a toast prompt. |
| 5 | Tracking: dung heap reveals tracks, proficiency makes track outlines last longer | S3.2, mechanic note `n_882057` | Exists | The team's PangeaTracking module covers this (not yet in this repo checkout as of 2026-11-23). Related code in the hunting system: `EHuntClueType::Droppings`, `PangeaTrackEmitterComponent`, `PangeaHuntClueActor`, `PangeaHunterSenseComponent`, track decals. | Wire the S3.2 quest (Taco and Meddylos perimeters, halfway-point trigger) to PangeaTracking once it is merged. |
| 6 | Search perimeters on the map; item spawns in the chosen or the last-searched perimeter | N0068, N0097, N0102, N0122, `n_731877` | Vendor only | ACF Maps System (AMS) with quest marker category `Marker.Quest`. | A perimeter actor (map area plus searched state) and a spawner that picks the target perimeter from world state. |
| 7 | Ambush on the party after the first perimeter search | `n_036039`, N0073 | Vendor only | ACF group AI and spawners; the Pangea `CombatEnemySpawner` is sample code only. | A triggered Doro wave on the camp, plus a "Protect the party" completion when every Doro has fled or is slain. |
| 8 | Random NPC combat barks | N0070 to N0072 | Missing | Nothing. | A bark component that picks a random line from a pool on an event, plus subtitles. ADS world dialogues could serve as the line source. |
| 9 | Tree felling for timber, and inspecting fallen branches for feathers | N0076, N0077, N0078, `n_240069` | Exists (partial) | Mining system: harvest timing in `MiningSiteComponent`, interactable `MiningDiscoveryNodeActor`. | A harvestable tree (axe required) that yields timber, a fell animation, and an inspectable drop spot. |
| 10 | Revival stones; respawn at the nearest unlocked one | N0125, N0126 | Exists (partial) | Sample `CombatCheckpointVolume`; ACF save components. | A revival-stone actor, an unlock flag, and nearest-stone respawn in the game mode. |
| 11 | Palisade construction: progress, build time, camp safety and morale | S3.1, `n_767230`, N0112 | Vendor only | ACF `AscentBuildingSystem`; Pangea BaseUpgrade covers facility upgrades. | A palisade build (timber cost, build time cut by helpers or bindings) and a camp safety/morale value. |
| 12 | Stats: Intimidation +1, strength gain after the ambush, combat skill from use | N0085 ×3, `n_731079`, `n_893350` | Exists (partial) | ARS stats (Strength), ACF Morality System. | An `Intimidation` attribute (or morality point) and an XP/stat grant on quest events. |
| 13 | Saved world-state flags read by quests and dialogue | A4 table in the spec | Vendor only | Quest manager and GAS stats save; ADS/AQS support tag conditions. | A small saved world-state store (tags or a typed map) with quest and dialogue condition and action classes. |
| 14 | Dialogue choices that set flags and stats | N0082, N0091, N0103, N0116 options | Vendor only | ADS node actions. | ADS/AQS actions that set a world-state flag or add a stat, built on #12 and #13. |
| 15 | "Follow Huscarl" escort to the Elder | N0038 | Vendor only | `ACF_FollowNPCObjective_BP`, `ACFCompanionGroupAIComponent`. | Wire the follow objective plus a Huscarl move-to path. Mostly content. |
| 16 | Crafting lures from feathers and bindings from reeds | N0083, N0107, N0111 | Vendor only | ACF `CraftingSystem` (already a Pangea dependency). | Items (feathers, reeds, lures, bindings) and recipes on the NPC crafters. |
| 17 | Blacksmith whose identity and ware quality depend on the father's fate | N0113 | Exists (partial) | BaseUpgrade facility groups (`BP_FG_Blacksmith`). | A vendor NPC swap driven by `FatherTreatment`, with ware-quality tiers. |
| 18 | Wounded party NPCs and a healer (the Druwid) | N0024, N0028, N0052 | Missing | Nothing. | A wounded-NPC state, Druwid healing, and the daughter's father lives/dies over time. |
| 19 | Rounding up creatures for farming; population set by fled vs slain Doros | `n_109947`, `n_410087` | Exists (partial) | Taming, breeding, farm actor. | Herd roundup plus spawn density read from the Doro counters. |

## Content gaps (not systems)

- **Characters to author**: Huscarl (no NPC asset exists today), Elder, Druwid, Scared Woman, Desperate Man, Daughter, a random party member, and the creatures Doro, Taco and Meddylos.
  Each needs a `Character.*` tag, a character data asset with a dialogue fragment, and a quest target component.
- **Items**: tutorial weapon, basic axe, timber, colourful feathers, apothecary parchments (wound dressing, fever), herb, riverbank weed, water reeds, headstones.
- **Achievement**: "You identified a creature".

## Board issues to resolve with design

Taken from section A7 of the export; these need a design decision, not a guess.

1. Empty cards in S3.2 (`n_767860`, `n_904669`, `n_277826`, `n_437663`): the halfway-point and stealthy/non-stealthy tracking outcomes have no content.
2. Empty cards in S5.3 (`n_257082`, `n_485982`, `n_707163`): follow-ups after finding each parchment.
3. Board codes are reused (N0039, N0078, N0085, and others). Key assets on the node id, never the code.
4. N0128 says "before N0078", but N0078 appears three times. The assumed meaning is "before the palisade timber quest completes".
5. There is no Section 4 on the board.
6. "Exploration to find mining area" (`n_255866`) is a hook with no content after the palisade quest.

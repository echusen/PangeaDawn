# Epics and stories

Part of `Docs/Index/INDEX.md`. Compiled 2026-11-27 from `Writerside/topics/`, the Miro export in `Saved/Yarin/Quests/miro_export/`, and the asset lists.
Status is what those sources say; nothing here was play-tested for this index. Update the row when a story changes state.

Status key: **Done** (built and verified per docs), **Built** (assets or code exist, acceptance not recorded), **Partial**, **Open**, **Blocked**.

## Epic list

| ID | Epic | Tag | State |
|---|---|---|---|
| E1 | Opening chapter (story) | `#quest` | S1 to S2.2 built, rest open |
| E2 | Opening chapter systems backlog | `#quest` and others | 19 mechanics, mostly open |
| E3 | Mining | `#mining` | Core verified; release gates open |
| E4 | Base upgrade (village) | `#base` | Verified |
| E5 | Breeding | `#breeding` | Verified |
| E6 | Taming | `#taming` | Built; no recent checkpoint |
| E7 | Dinosaur AI and creature profiles | `#dino`, `#core` | Built |
| E8 | Hunting and tracking | `#hunting` | Built; PangeaTracking module not in repo |
| E9 | ACF modernization and vendor hygiene | `#vendor` | Ongoing |
| E10 | UI shell | `#ui` | Built |

## E1. Opening chapter (story)

Source board: Miro `uXjVG9S_IZs=`. 205 nodes, 213 edges, 8 sections, 9 cutscene markers. There is no Section 4. Key on node `id`; board codes such as N0039 repeat.
Quest registry: `/Game/Yarin/Quests/DT_Pangea_Quests`. All built content plays in `Content/Maps/SC_Demo`.

| Story | Section | Quest tag | Assets | Status |
|---|---|---|---|---|
| E1-S1 Tutorial, "Back on Your Feet" | S1 (25 nodes; CS1, CS2) | `Quest.Tutorial` (10 objective tags) | `Content/Yarin/Quests/Tutorial/`; objectives in `Source/Pangea_Dawn/Public/ACF/PangeaTutorialObjectives.h`; spec `Saved/Yarin/Quests/AQS_Pangea_Tutorial.spec.json` | Built. Auto-starts from the player controller. Weapon is the ACF sample sword placeholder. |
| E1-S2 Clear the area | S2.1 (25 nodes; objectives N0025, N0025.1, N0026, N0031) | `Quest.ClearTheArea` (5 objective tags) | `Content/Yarin/Quests/ClearTheArea/` (quest plus 4 dialogues) | Built. `opening-chapter-gaps.md` still says "Not started"; that row is stale. Stealth, observe and Doro flee behaviour are still backlog (E2-1, E2-2, E2-3). |
| E1-S3 Meet the Elder | S2.2 (33 nodes; CS3, CS4, CS5; objectives N0038, N0039, N0061, N0059) | `Quest.MeetTheElder` (2 objective tags) | `Content/Yarin/Quests/MeetTheElder/` (quest plus 2 dialogues) | Built. Same stale row in the gaps doc. Follow-Huscarl escort is E2-15. |
| E1-S4 Build a palisade | S3.1 (33 nodes; N0068, N0073, N0076, N0079) | none yet | none | Blocked by E2-6, E2-7, E2-9, E2-11 |
| E1-S5 Learn to track | S3.2 (20 nodes, no objectives) | none yet | none | Blocked: four outcome cards are empty on the board; needs E8 tracking |
| E1-S6 Side quest: Scared Woman | S5.1 (10 nodes; N0086) | none yet | Character asset `DA_Pangea_ScaredWoman` only | Open; needs E2-13, E2-16 |
| E1-S7 Side quest: Desperate Man | S5.2 (18 nodes; N0123, N0122) | none yet | `DA_Pangea_DesperateMan` only | Open; needs E2-13 |
| E1-S8 Side quest: Daughter | S5.3 (40 nodes; N0097, N0102, N0104, N0109, N0110) | none yet | `DA_Pangea_Daughter` only | Open; three follow-up cards are empty; needs E2-13, E2-16, E2-18 |

Characters with data assets: Huscarl, Elder, Druwid, Daughter, Desperate Man, Scared Woman, Doro. Not authored: a random party member, Taco, Meddylos.
Open design questions (board issues) are in `Writerside/topics/opening-chapter-gaps.md`, last section.

## E2. Opening chapter systems backlog

Full detail per row: `Writerside/topics/opening-chapter-gaps.md`. The user asked on 2026-11-23 that these be noted and scheduled later, not built during quest import.

| Story | Mechanic | Starting point | Status |
|---|---|---|---|
| E2-1 | Player stealth detection | ACF `ACFAIController::HandlePerceptionUpdated` | Open (vendor only) |
| E2-2 | Doro AI: flee, despawn, fight | `TameSpeciesConfig`, `GA_Dinosaur_Flee` | Partial |
| E2-3 | Observe a creature from behind | none | Open |
| E2-4 | Achievements | none | Open |
| E2-5 | Tracking from dung heaps | Hunting system; PangeaTracking module | Partial; module not in repo |
| E2-6 | Search perimeters on the map | ACF Maps System | Open (vendor only) |
| E2-7 | Ambush on the party | ACF group AI and spawners | Open (vendor only) |
| E2-8 | Random NPC combat barks | none | Open |
| E2-9 | Tree felling, branch inspection | `MiningSiteComponent`, `MiningDiscoveryNodeActor` | Partial |
| E2-10 | Revival stones | sample `CombatCheckpointVolume` | Partial |
| E2-11 | Palisade construction | ACF `AscentBuildingSystem`, base upgrade | Open (vendor only) |
| E2-12 | Stats: Intimidation, strength gain | ARS stats, ACF Morality | Partial |
| E2-13 | Saved world-state flags | quest manager, GAS stats save | Open (vendor only) |
| E2-14 | Dialogue choices set flags and stats | ADS node actions | Open; depends on E2-12, E2-13 |
| E2-15 | Follow Huscarl escort | `ACF_FollowNPCObjective_BP` | Open; mostly content |
| E2-16 | Crafting lures and bindings | ACF `CraftingSystem` | Open (vendor only) |
| E2-17 | Blacksmith depends on father's fate | `BP_FG_Blacksmith` | Partial |
| E2-18 | Wounded NPCs and the Druwid healer | none | Open |
| E2-19 | Creature roundup, population from Doro counters | taming, breeding, farm | Partial |

## E3. Mining

Docs: `acf-mining-routines.md` (current checkpoint), `acf-mining-timing.md` (B05), `acf-mining-public-apis.md` (B06).

| Story | Status |
|---|---|
| E3-1 Timing with pause-offline accounting (B05) | Done: 11/11 focused, 40/40 suite |
| E3-2 Public APIs (B06) | Done: 16/16 focused, 45/45 suite |
| E3-3 ACF routine bridge through Pangea-owned actions, controllers, skill UI | Done: 63/63 `Pangea.ACF` twice in a row |
| E3-4 Korro transport prototype in SC_Demo (tiers 4 and 5, 450 cm/s, kinematic towing) | Done: transport 12/12, regression 83/83 (2026-10-22) |
| E3-5 Pangea-owned walking blendspace, moveset, AnimBP copies | Done: 2/2 animation checks |
| E3-6 Day-end worker losses, vacancies, paid replacement | Partial: prepared, needs closed-Editor rebuild and acceptance; prices not configured |
| E3-7 Walking animation tuning | Open: deferred to last at the user's request |
| E3-8 Corrected economy, authored preview settings | Open |
| E3-9 Release gates: dedicated server, late join, latency, disk save/load of active duty, multi-site navigation, packaging | Open |
| E3-10 Physical cargo, working chariot, escort | Open: cart is cosmetic |

## E4. Base upgrade (village)

Docs: `base-upgrade-*.md`, `acf-base-upgrade-safety.md` (B04).

| Story | Status |
|---|---|
| E4-1 Data-driven levels, requirements, actions, facilities, milestones | Done |
| E4-2 Upgrade safety with separate explicit costs (B04) | Done: 31-test suite |
| E4-3 Mine milestone and mining-site unlock action | Done (`UUA_UnlockMiningSite`, `Village.Milestone.Mine`) |
| E4-4 Blacksmith milestone | Built (`BP_FG_Blacksmith`, `Village.Milestone.Blacksmith`); see E2-17 |

## E5. Breeding

Docs: `breeding-*.md`, `acf-breeding-persistence.md` (B03).

| Story | Status |
|---|---|
| E5-1 Farm, egg, incubation, fertility cooldown | Done |
| E5-2 Genetics: traits, material colour and scalar inheritance, stat inheritance | Done |
| E5-3 Persistence and replication with pause-offline policy (B03) | Done: 25-test suite |

## E6. Taming

Docs: `taming-*.md`. No recent checkpoint page.

| Story | Status |
|---|---|
| E6-1 States (Wild, Hostile, Tamed) and roles (Mount, Companion) | Built |
| E6-2 Stat and item requirements, species config | Built (`UTameSpeciesConfig`, `DA_Pangea_Tame_*`) |
| E6-3 Taming minigame overlay | Built (`UTamingWidget`, `WBP_TamingMinigame`, tag `UI.Widget.Pangea.Taming`) |

## E7. Dinosaur AI and creature profiles

Docs: `dinosaur-*.md`, `acf-creature-profiles.md`.

| Story | Status |
|---|---|
| E7-1 `APDDinosaurBase` on ACF character | Built |
| E7-2 Creature definition with per-system fragments | Built (`UPangeaCreatureDefinition`, 5 fragment types) |
| E7-3 Species: Triceratops (adult, child), Cerato, Korro | Built |
| E7-4 Doro, Taco, Meddylos | Open (Doro has a quest character asset only) |

## E8. Hunting and tracking

No Writerside page.

| Story | Status |
|---|---|
| E8-1 Clue actor, track emitter, hunter sense, species config | Built; lifecycle and replication tests exist |
| E8-2 PangeaTracking module | Not in the repo as of 2026-11-23, per the user |
| E8-3 Wire S3.2 quest to tracking | Blocked by E8-2 and board gaps |

## E9. ACF modernization and vendor hygiene

Docs: `acf-modernization-baseline.md`. Rules: root `AGENTS.md`.

| Story | Status |
|---|---|
| E9-1 UE 5.8.3 and ACF 4.5 baseline inventory | Done |
| E9-2 ACF 4.6 update | Done in commit `2a40d23b` |
| E9-3 Vendor read-only protection (`Scripts/Protect-PangeaVendor.ps1`) | In place. The check failed on 2026-11-27 with "Vendor baseline is dirty"; git shows `Plugins/ACFUAsceb6d4445a7304V7/Content/Integrations/Objectives/ACF_KillGroupObjective_BP.uasset` modified. Needs an owner decision. |
| E9-4 Remove engine template sample code (`Variant_*`) | Not planned; listed for awareness |

## E10. UI shell

| Story | Status |
|---|---|
| E10-1 HUD root, notifications, widget registry, theme | Built (`WBP_PangeaHUDRoot`, `DA_PangeaWidgetRegistry`, `DA_PangeaUITheme`) |
| E10-2 Feature pages: village, breeding, mining site, chest, stockpile | Built (`WBP_Pangea*Page`, tags `UI.Widget.Pangea.*`) |
| E10-3 Skill trees | Built (`WBP_Pangea_SkillTrees`, `_SkillInfo`, `_SkillTreesNavigation`) |

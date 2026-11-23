---
name: configure-dialogue
description: Full workflow for creating ACF dialogue assets, configuring voices (ElevenLabs TTS), wiring character data assets, and setting up participants. Covers screenplay-based graph authoring (writing brief, validate, preview, import), voice config duplication, participant mapping, and runtime verification.
globs: []
alwaysApply: false
---

# Configure Dialogue in ACF Ultimate

This skill covers end-to-end dialogue configuration in the Ascent Combat Framework (ACF) using the Ascent Dialogue System (ADS). It covers **three orthogonal paths** that converge at runtime:

```
Voice Config (DA_VoicesConfig)  ←  maps Character.* tag to ElevenLabs voice
Dialogue Graph (ADS asset)      ←  nodes + edges + participant tags + text
Character Data Asset            ←  ACFDialogueFragment with participant tag + dialogue ref
```

At runtime: Character `ACFDialogueFragment` → ADS asset selects a node → node's `Participant` tag → Voice Config → ElevenLabs voice ID → TTS audio plays.

---

## 1 — Understand the assets

| Asset | Class | Location (sample) | Purpose |
|---|---|---|---|
| Voice Config | `UACFVoicesConfig` (DataAsset) | `/Game/Integrations/ATSIntegrations/Dialogue/DA_VoicesConfig` | Maps `Character.*` tags to ElevenLabs voice IDs |
| Dialogue Graph | `UADSDialogue` / `UADSWorldDialogue` | Your content folder | Nodes (lines/responses) + edges + participant bindings |
| Character Data Asset | `UACFCharacterDataAsset` | Your content folder | Holds `ACFDialogueFragment` linking character to dialogue |

> **Never edit sample assets.** Duplicate `DA_VoicesConfig` and sample dialogue assets into your own content folder.

---

## 2 — Voice Config (see the elevenlabs-tts-integration skill)

Voice setup (duplicating `DA_VoicesConfig`, entering the ElevenLabs API key, refreshing and mapping voices per `Character.*` tag) is owned by the **elevenlabs-tts-integration** skill. Follow that skill for the full voice workflow; this skill only assumes the result: every dialogue node's `Participant` tag resolves to a voice through your Voice Config asset.

---

## 3 — Author a Dialogue Graph (screenplay text)

A dialogue graph is authored as **screenplay text** and built by ADS's own importer. This is the same flow as the **Generate with AI** button in the dialogue editor; the Yarin dialogue tools (`ADSDialogueToolset`) drive it from chat. The graph only changes when you review the text in the preview window and click **Import**.

### Create or update the dialogue asset

`SaveDialogue` (`DialoguePath`, `ParticipantTags`, `ConversationDescription`, `PlayerResponseMode`, `TreeLength`, `DialogueKind`) creates the dialogue asset when it is missing, or updates the writing settings of an existing one, and saves it. Call it first: every other dialogue tool needs an existing dialogue with participants. It never builds the graph, so there is still nothing in the graph until you import the preview.

| Argument | Meaning |
|---|---|
| `DialoguePath` | e.g. `/Game/MyProject/Dialogues/DA_Guard`, in your own content folder. Engine content is refused. |
| `ParticipantTags` | The registered `Character.*` tags that speak, **including the player's**. Get them from one `TagList` call with root `Character`; an unregistered tag is refused, never invented. Only the tag whose last segment contains "Player" can have response options (branching); every other character speaks linear lines. An empty list leaves the tags unchanged. |
| `ConversationDescription` | What the conversation is about; include "the player can decide whether to ..." when you want player branching. `""` leaves it unchanged. |
| `PlayerResponseMode` | `OptionButtons`, `DialogueNodes` or `Mixed`. `""` leaves it unchanged (a new dialogue starts on `Mixed`; a world dialogue is always `DialogueNodes` and refuses the other two). |
| `TreeLength` | Approximate number of speaker turns. `0` leaves it unchanged (a new asset starts at 8). |
| `DialogueKind` | `Dialogue` (default, `""`) or `WorldDialogue` for a world-space dialogue. Only used on creation. |

An invalid argument changes nothing. The result lists the saved path, whether it was created, the settings it now holds, and warnings (no player participant, no description, a tag outside `Character.*`). Read the warnings before writing the screenplay.

You also need a **Voice Config** asset set in Project Settings → Ascent Dialogue Settings: the writing brief refuses to build without it.

### Screenplay format

```
Guard:
Halt! Who goes there?

Player (Option 1):
A friend. Let me pass.

Player (Option 2):
None of your business.
```

A `NAME:` line, the dialogue text below it, one blank line between entries, and `NAME (Option N):` for player choices. Speaker names are the last segment of the participant tags. Every entry needs text; empty text is what voice generation skips.

When revising, export the existing graph first: the text carries `[Node:GUID]` lines. Keep them, and the import updates the matching nodes instead of replacing the graph.

### Workflow (Yarin tools, `AscentDialogueSystemEditor.ADSDialogueToolset`)

1. `SaveDialogue` (`DialoguePath`, `ParticipantTags`, `ConversationDescription`, `PlayerResponseMode`, `TreeLength`, `DialogueKind`) — create the asset and set its participants, as above. Skip it only when the dialogue already exists with participants.
2. `GetWritingBrief` (`DialoguePath`, `Description`) — the format rules and request the editor's AI button would send: screenplay format, branching rules for the dialogue's player response mode, allowed speakers, personalities, target length. Write the screenplay from it.
3. `ValidateText` (`DialoguePath`, `Text`) — checks the text with the importer's parser without opening anything or changing the asset. Fix every error and re-run until it reports `bValid`.
4. `PreviewText` (`DialoguePath`, `Text`) — opens the editable preview window. Review the text, then click **Import**. Nothing is built until you do.
5. `ExportText` (`DialoguePath`) — reads the saved graph back as screenplay text, for revisions.

### Audio generation

After the graph is imported, configure audio:

1. `GetVoiceCasting` (`DialoguePath`) to list each participant's current voice and every available voice. If no voices are cached yet it starts **Refresh Voices** and asks you to call again once the editor reports the refresh.
2. `SetParticipantVoice` (`ParticipantTag`, `Voice`) to assign a voice in the Voice Config. It applies to every dialogue that uses that participant.
3. `GenerateVoices` (`DialoguePath`, `bForceRegenerate`) only after you confirm the voice choices and the TTS spend. It runs in the background and reports through editor notifications.

---

## 4 — Wire to a Character Data Asset

The character (NPC or player) must carry an `ACFDialogueFragment` on their `UACFCharacterDataAsset`:

- Open the character's Data Asset (e.g. `DA_NPC_Shopkeeper`)
- Scroll to **Fragments** → **+** → pick `ACFDialogueFragment`
- Set:
  - **Participant Tag** — same `Character.*` tag used in Voice Config AND on dialogue nodes (e.g. `Character.Shopkeeper`)
  - **Dialogues** — reference your dialogue asset(s)
  - **Default Camera Config** — optional (camera framing during dialogue)

The `UACFCharacterInitializerComponent` on the character Blueprint applies this fragment at `BeginPlay`.

---

## 5 — Verify

**Checklist before testing:**

- [ ] `Character.*` tag exists in Project Settings → GameplayTags
- [ ] Voice Config DA has an entry for every `Character.*` tag that speaks
- [ ] Project Settings → Ascent Dialogue Settings → Voice Config DA points to **your duplicated copy**
- [ ] The dialogue's `AI Participant Tags` list every speaker, and the player's tag contains "Player" if the player has choices
- [ ] Each dialogue node's `Participant` matches a key in Voice Config
- [ ] Character Data Asset has `ACFDialogueFragment` with matching `Participant Tag`
- [ ] `ValidateText` reports `bValid` for the screenplay, and the graph was imported in the preview window
- [ ] ElevenLabs TTS Voice Gen Key is set in Project Settings

**Common failures:**

| Symptom | Fix |
|---|---|
| `GetWritingBrief` says "No AI Participant Tags" | Call `SaveDialogue` with the speaking characters' `Character.*` tags as `ParticipantTags` |
| `GetWritingBrief` says "No Voice Config Data Asset" | Set your Voice Config in Project Settings → Ascent Dialogue Settings |
| `GetWritingBrief` says to fill the description | Call `SaveDialogue` with a `ConversationDescription`, or pass a `Description` to `GetWritingBrief` |
| `PreviewText` raises "Dialogue text is not valid" | Run `ValidateText`, fix every listed error (speaker names must match the participant names) |
| Preview opened but the graph is unchanged | The graph only changes when you click **Import** in the preview window |
| "No voice config" warning | Add the participant's `Character.*` tag to your Voice Config DA |
| Wrong voice plays | Check `Participant` tag on the node matches Voice Config key exactly (case-sensitive) |
| Dialogue doesn't start | Confirm the Character Data Asset has `ACFDialogueFragment` with the dialogue asset assigned |
| TTS generation does nothing | Verify TTS Voice Gen Key in Project Settings; click **Refresh Voices** on your Voice Config |
| Node text empty | Put text under every `NAME:` line in the screenplay before previewing |

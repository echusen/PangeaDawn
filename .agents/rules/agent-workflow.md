---
trigger: always_on
---

# Antigravity Workflow Rules

## Context & Error Handling

- After a context window compaction (`<CONTEXT_SUMMARY>`), only read the specific small target line ranges (`read_file` with 15-30 lines) required immediately before an `edit_file` call; do NOT re-read entire files or previously inspected sections that do not need edits.
- When an error or mismatch is caused by context compaction or a missed file read, state the cause plainly in one sentence and move straight to the fix without extended self-diagnosis.
- **Full-Resolution Clipboard Screenshots**: Whenever the user pastes an image into chat, immediately save the uncompressed image currently on the Windows clipboard (`[System.Windows.Forms.Clipboard]::GetImage()`) as a timestamped PNG in `<appDataDir>\brain\<conversation-id>\screenshots\`, overwrite the corresponding `.user_uploaded\media_*.jpg`/`.png` file with that full-resolution image so clicking the chat thumbnail opens it at native resolution, and provide a clickable markdown link to the `screenshots\` file.

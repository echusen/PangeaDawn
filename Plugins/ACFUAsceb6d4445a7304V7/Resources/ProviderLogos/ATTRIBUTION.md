# Provider identity without vendor marks

The Yarin panel shows no third-party image marks. Each provider reads as a
colored dot plus its plain-text name:

- Claude Code: burnt-orange dot (#D97757), name in the same orange.
- ChatGPT (Codex): green dot (#10A37F), name in the same green.
- Yarin gateway: Yarin's own icon and wordmark, name in white.

`openai.svg` / `claude.svg` (and their `openai.png` / `claude.png` renders,
sourced from `@lobehub/icons-static-svg` 1.95.1, MIT) are no longer referenced
by the panel. Delete the files once every supported branch stops loading them.

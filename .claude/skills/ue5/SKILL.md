---
name: ue5
description: >-
  Unreal Engine 5 C++ coding standards and UE5 Python/Blueprint/Material editor
  guardrails for Pangea Dawn. Use whenever writing or reviewing UE5 C++, or
  editing UE5 assets through Python editor scripts.
---

# Unreal Engine 5 (Pangea Dawn)

The standards live in `.agents/` so every agent tool shares one copy. Paths are
relative to the repository root. Read these, in this order, before writing or
reviewing code:

1. `AGENTS.md`, section "Coding standards". It lists the project overrides and
   wins on any conflict (vendor and engine code are read only, source control
   is git).
2. `.agents/skills/ue5/SKILL.md` for the guardrails: const-correctness, code
   simplicity, and the UE5 Python Blueprint/Material editing crash list.
3. `.agents/rules/cpp-coding-standards.md` for the numbered C++ conventions.
   Cite rule numbers (for example `7.11`) in review findings.

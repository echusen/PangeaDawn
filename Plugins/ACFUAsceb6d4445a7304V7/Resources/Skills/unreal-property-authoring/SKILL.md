---
name: unreal-property-authoring
description: Use when creating or editing Unreal Engine properties exposed in Details panels or Blueprint, especially Category hierarchies and gameplay-tag picker filters.
---

# Unreal property authoring

## Categories must subdivide

Categories exist exclusively to **SUBDIVIDE** properties into at least **two meaningful groups**. They are not namespaces, branding, or repetitions of the owning class name.

- Add a category or nested level only when it separates properties into at least two meaningful sibling groups at that level.
- Never create a chain such as `MyGame|Abilities` for a single property. Each level must earn its place by subdividing, not merely enclosing another level.
- If Unreal requires Category metadata but there is no useful subdivision, use one concise flat category, preferably an appropriate existing category.
- Do not invent extra properties or groups to justify a hierarchy. Inspect the actual exposed properties before choosing categories.

## Gameplay-tag pickers

Constrain a gameplay-tag picker to the branches relevant to the property's consumer using `Categories` metadata. Inspect the actual tag roots and existing API filters; do not guess their spelling.

For selectors of one tag family, filter by that family's roots, for example `meta = (Categories = "Ability,Actions")` when the project's tags are rooted at `Ability` and `Actions`. Apply it to the tag-bearing property, including a map whose keys are gameplay tags.

Picker filtering is an Editor selection aid, not runtime validation, and must not change existing serialized tag values or matching behavior.

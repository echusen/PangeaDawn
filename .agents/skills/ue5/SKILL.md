---
name: ue5
description: >-
  Unreal Engine 5 coding standards, source-control workflow, and UE5
  Python/Blueprint/Material editor guardrails for Pangea Dawn. Use this skill
  whenever writing or reviewing UE5 C++, or editing UE5 assets and Python
  scripts.
---

# Unreal Engine 5 (Pangea Dawn) Skill

## Standards & Rules

All UE5 C++ in Pangea Dawn must follow the workspace rules in `.agents/rules/` and the root `AGENTS.md`:

- [C++ Coding Standards](../../rules/cpp-coding-standards.md) — C++ conventions (Sections 1–14: style, files, includes, comments, classes, functions, variables, enums, lambdas, casts, `auto`, control flow, UE5 types, and engine changes).
- Root `AGENTS.md` — vendor boundaries (engine and vendor plugins are read only), asset ownership and naming, and the project index. It wins on any conflict.

---

## Source Control & Builds (git — Strict Human-in-the-Loop)

- **NEVER commit or push without explicit user approval.** Leave changes in the working tree so the user can review and diff them.
- **NEVER clear a read-only file attribute to make an edit.** In this project the attribute marks protected vendor files.
- **UE5 Assets**: Save modified project-owned assets immediately after compiling and refreshing the editor with `unreal.EditorAssetLibrary.save_loaded_asset(asset, False)`—never leave modified assets unsaved in memory. Save only explicitly named Pangea-owned packages; never use Save All.
- **Let the User Compile C++**: Do NOT run `Build.bat` / UnrealBuildTool yourself after editing C++ files unless explicitly asked or the task requires proof that the code compiles.

---

## UE5 Python Remote Execution & Material Editing Guardrails

- **NEVER route local UE5 Python remote execution through VPN / UDP multicast (`239.0.0.1:6766`).** Stock `remote_execution.py` uses UDP multicast discovery (`239.0.0.1:6766`), which gets intercepted or dropped by VPN adapters and causes long timeouts. Always connect directly over local loopback (`127.0.0.1`) by listening on TCP `127.0.0.1:6776` and sending an `open_connection` JSON packet (with `dest` omitted) directly via UDP unicast to `('127.0.0.1', 6766)`.
- **Always call `unreal.MaterialEditingLibrary.refresh_material_editor(mat)` after editing a `UMaterial` in Python.** `MaterialEditingLibrary.recompile_material(mat)` only calls `NotifyExternalMaterialChange()`, which does not sync the open `FMaterialEditor`'s transient working `Material` / `MaterialGraph` from `OriginalMaterial` (leaving the open Material Editor tab stale and able to overwrite `OriginalMaterial` on save/apply). Calling `refresh_material_editor(mat)` triggers `MaterialEditor->RefreshGraphFromOriginal()` so new/modified nodes appear immediately in the open Material Editor tab.
- **NEVER iterate blindly over `unreal.MaterialProperty` when calling `MaterialEditingLibrary.get_material_property_input_node(mat, prop)` or `get_material_property_input_node_output_name(mat, prop)`.** In `MaterialEditingLibrary.cpp`, both functions dereference `Material->GetExpressionInputForProperty(Property)` without a `nullptr` check; passing unsupported/sentinel enum values (such as `MP_MAX`, `MP_DIFFUSE_COLOR`, `MP_SPECULAR_COLOR`, etc.) causes an immediate fatal null-pointer Read Access Violation crash. Only pass an explicit allowlist of valid surface/attribute properties (e.g., `MP_MATERIAL_ATTRIBUTES`, `MP_BASE_COLOR`, `MP_METALLIC`, `MP_SPECULAR`, `MP_ROUGHNESS`, `MP_EMISSIVE_COLOR`, `MP_NORMAL`, `MP_AMBIENT_OCCLUSION`, `MP_OPACITY`, `MP_OPACITY_MASK`).

---

## UE5 Python Blueprint Editing Guardrails

- **NEVER call the `unreal.BlueprintGraphEditor(bp, graph_name)` constructor directly.** In UE5 Python, `UObject(outer, name)` calls `NewObject` and will fatally crash the editor (`Cannot replace existing object of a different class`) if `name` collides with an existing `EdGraph` on the Blueprint (such as `'EventGraph'`).
  - Always use `unreal.BlueprintGraphEditor.get_graph_editor_by_name(bp, graph_name)` to inspect or edit an existing graph, or `unreal.BlueprintGraphEditor.create_and_edit_function_graph(bp, func_name)` to create a new function graph.
- When creating a `ResolveSoftReference` node via `BlueprintGraphEditor.create_node_from_name('Utilities|ResolveSoftReference', pos, context_pins, None)`, always pass an empty list `[]` for `context_pins` and wire the pins afterward with `BlueprintGraphPinLibrary.try_create_connection`.
- **NEVER call `add_call_function_node('/Game/.../BP_Name.BP_Name_C:FuncName')` to call a function or custom event on the same Blueprint (`self`), and NEVER pass `None` node/pin references into `BlueprintEditorLibrary` or `BlueprintGraphPinLibrary`.**
  - Referencing the Blueprint's own generated class (`_C`) via `add_call_function_node` binds an external `UFunction*` on the transient class that gets destroyed/TRASHCLASS'd during `compile_blueprint(bp)`, causing a fatal Read Access Violation crash. Build shared node sequences inline via a Python helper function instead of creating self-function call nodes across `compile_blueprint` calls.
  - Always assert that `create_node_from_name(...)` and `add_call_function_node(...)` return non-`None` `UEdGraphNode` objects before passing them to `set_node_pos`, and always validate `FBlueprintGraphPin` structs with `unreal.BlueprintGraphPinLibrary.is_valid(pin)` (not `pin is not None`, which is always `True` in Python because `BlueprintGraphPin` is a `USTRUCT` returned by value).
- **NEVER call `BlueprintGraphEditor.create_node_from_name(...)` in the same Python execution (tick) after structurally modifying a Blueprint (`create_and_edit_function_graph`, `add_member_variable`, `set_blueprint_variable_*`, `remove_nodes`, etc.), and NEVER use `create_node_from_name('Variables|<BP>|...', ...)` to spawn member/timeline variable nodes.**
  - Structural Blueprint modifications (`MarkBlueprintAsStructurallyModified`) regenerate `Blueprint->SkeletonGeneratedClass` and destroy its old `FProperty` instances immediately, whereas `FBlueprintActionDatabase` defers refreshing its cached `UBlueprintVariableNodeSpawner` entries until the next editor tick. Calling `create_node_from_name` in the same tick iterates over every cached `UBlueprintNodeSpawner` in `FBlueprintActionDatabase::Get().GetAllActions()` and dereferences destroyed `FProperty*` pointers inside `GetUiSpec()`, causing a fatal Read Access Violation crash.
  - Instead, always use `BlueprintGraphEditor.add_get_member_variable_node(var_name, '')` / `add_set_member_variable_node(var_name, '')` (which query `Blueprint->SkeletonGeneratedClass->FindPropertyByName` directly without touching `FBlueprintActionDatabase`), and call `BlueprintEditorLibrary.compile_blueprint(bp)` after adding/modifying variables so `SkeletonGeneratedClass` is up to date.
- **Re-query pins after each connection on wildcard/promotable operator nodes (`UK2Node_PromotableOperator`, e.g., `Divide_DoubleDouble`, `Multiply_DoubleDouble`, `Add_DoubleDouble`).**
  - Connecting a typed pin to a promotable operator triggers `NotifyPinConnectionListChanged` -> `EvaluatePinsFromChange` -> `UpdatePinsFromFunction`, which mutates/reconstructs the node's pins. Always call `find_input_pin` / `find_output_pin` immediately before each `try_create_connection` call rather than caching `FBlueprintGraphPin` structs across connections.
- **Always pass `bIncludeInheritedMembers=False` to `BlueprintEditorLibrary.list_member_variable_names(bp, False)`, and NEVER call `get_editor_property` / `set_editor_property` on Blueprint `UUserDefinedEnum` properties.**
  - `list_member_variable_names(bp)` defaults `bIncludeInheritedMembers` to `True`, returning 100+ native `AActor`/`UObject` properties (such as `HiddenEditorViews`).
  - Calling `cdo.get_editor_property(var_name)` or `set_editor_property` on a variable typed with a Blueprint `UUserDefinedEnum` crashes the editor inside `PyConversion.cpp` (`PythonizeEnumEntry` / `NativizeEnumEntry`): `FPyWrapperTypeRegistry::Get().GetWrappedEnumType(EnumType)` returns `nullptr` for unwrapped `UUserDefinedEnum` types and passes that `nullptr` directly into `PyUtil::GetFriendlyTypename(PyEnumType)`, causing a fatal null-pointer Read Access Violation. Always check `get_member_variable_type(bp, var_name).export_text()` first and skip `byte`/enum properties backed by `UUserDefinedEnum` before calling `get_editor_property`.
  - Similarly, avoid blindly iterating `BlueprintEditorLibrary.list_graph_names(bp)` into `get_graph_editor_by_name`: `list_graph_names` includes `DelegateSignatureGraphs` and `ImplementedInterfaces` stub graphs (which can have duplicate names across `FunctionGraphs` and `ImplementedInterfaces`). Only open specific target graphs (`'EventGraph'`, `'UserConstructionScript'`, or known function graphs) and always verify the returned `BlueprintGraphEditor` is not `None`.
- **NEVER call `BlueprintGraphEditor.remove_nodes(...)` + `BlueprintEditorLibrary.compile_blueprint(bp)` while PIE/Simulate is running or while the Blueprint Editor tab for `bp` is open.**
  - Always check `unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is None` (stop/abort if PIE is active) and call `unreal.get_editor_subsystem(unreal.AssetEditorSubsystem).close_all_editors_for_asset(bp)` before removing/rewiring graph nodes and recompiling, then reopen the asset afterward. Mutating/recompiling a Blueprint mid-PIE reinstantiates live actors/Timelines, and deleting compiled nodes while `SGraphPanel` is open can dereference destroyed nodes before the next Slate tick.

---

## C++ Const-Correctness & Validation Guardrails

- **NEVER accept a `const` pointer or `const` reference parameter (`const T*` or `const T&`) and then modify it (e.g., via `const_cast`), and NEVER mutate object state or dirty packages inside a `const` method (such as `UObject::IsDataValid(...) const`).**
  - If a function modifies an object or parameter, the parameter and method signature MUST be non-`const`.
  - Keep `IsDataValid` / Data Validation strictly read-only (reporting errors/warnings only); perform any automatic fixups or mutations in non-`const` lifecycle hooks such as `PreSave`, `OnPackagePreSave`, editor commands/utilities, or runtime initialization.

## Code Simplicity, Commenting & C++ Standards

In addition to [C++ Coding Standards](../../rules/cpp-coding-standards.md):

- **Keep Code as Simple as Possible**:
  - Always favor the simplest, most direct implementation with the smallest footprint.
  - Reuse existing engine/project properties and data structures rather than introducing parallel config files, redundant state, or over-engineered abstractions.
  - Prefer small, flat **private member functions** on the appropriate class over deeply nested lambdas, `TFunction` recursion, duplicated code blocks, or file-local free functions (keeps implementation readable on the class and avoids refactoring when access to `this` is needed).
  - **Avoid anonymous `namespace` blocks in `.cpp` files**: UE5's Unity Build can merge `.cpp` files and cause symbol/naming collisions between anonymous namespaces. If a non-member helper namespace is genuinely needed, use an explicitly named namespace such as `namespace <Filename>Helpers`.
- **Make Work Well-Commented**:
  - Document classes, functions, and member variables with concise `/** ... */` docstrings explaining *what* they do and *why*.
  - Add brief, high-signal inline comments for non-obvious behavior or engine quirks—explain the *why* and intent rather than restating what the code literally does.
- **Additional C++ Guardrails & Style Preferences**:
  - Use `IsValid(...)` when validating `UObject*` pointers (`AActor*`, `UActorComponent*`, `UBlueprint*`, `UClass*`, etc.) rather than raw pointer truthiness.
  - Qualify local variables and pointers with `const` (`UClass* const`, `const UObject* const`, etc.) whenever they are not reassigned.
  - Use `GET_MEMBER_NAME_CHECKED` instead of hard-coded property name strings where the type and property are accessible.
  - Never use `const_cast`; ensure mutating code paths receive non-`const` pointers/references.
  - Strongly **PREFER** explicit `!= nullptr` / `== nullptr` checks (e.g., `if (Ptr != nullptr)`) unless obvious from a scoped variable assignment (`if (UObject* Obj = GetObj())`) or an Unreal Engine global pointer like `GEditor` or `GEngine`.
  - Lightly prefer an additional nested branch over `continue` within a `for` loop.

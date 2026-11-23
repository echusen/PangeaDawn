---
name: crafting-system
description: Build item crafting and upgrading in ACF — recipe DataAssets, the crafting component (vendor/storage based) and integration with the Inventory System.
globs: []
alwaysApply: false
---

# Crafting System — ACF Ultimate

The **CraftingSystem** module lets pawns craft and upgrade items from recipes. The hub is `UACFCraftingComponent`, which extends `UACFVendorComponent` (which itself extends `UACFStorageComponent`) — so a crafting station is also a vendor/storage actor. A recipe is a `UACFCraftRecipeDataAsset` wrapping an `FACFCraftingRecipe` (`RequiredItems` → `OutputItem`, plus a `CraftingCost`). Crafting consumes the required items (and currency) from the instigating pawn's inventory and produces the output via the Inventory System (`UACFItem`, `FBaseItem`, `FInventoryItem`). This skill covers authoring recipes, configuring a crafting station and crafting/upgrading at runtime.

---

## 1 — Understand the assets

| Asset | Class | Location (sample) | Purpose |
|---|---|---|---|
| Recipe DataAsset | `UACFCraftRecipeDataAsset` | sample crafting folders | One craftable recipe (`FACFCraftingRecipe`) |
| Crafting station actor | actor with `UACFCraftingComponent` | `/Game/FullSample/` (forge/workbench) | Holds recipes, performs craft/upgrade, acts as vendor/storage |
| Item DataAssets | `UACFItem` / `UACFWeapon` / `UACFArmor` | `/AscentCombatFramework/Items/` | Required ingredients and crafted outputs (see Inventory System) |
| Currency component | `UACFCurrencyComponent` | on pawn / vendor | Optional cost handling for `CraftingCost` |

> **Never edit sample assets.** Duplicate sample recipe DataAssets and the crafting-station Blueprint into your own folder (e.g. `Content/YourGame/Crafting/`), then customize **your copies**. Editing the originals = lost on update.

### Key classes

| Class | Role |
|---|---|
| `UACFCraftingComponent` | Crafting hub: holds recipes, validates and performs craft/upgrade (server) |
| `UACFVendorComponent` | Parent: buy/sell, price multipliers, currency, item descriptors |
| `UACFCraftRecipeDataAsset` | Wraps an `FACFCraftingRecipe`; the recipe asset you add to a station |
| `UACFCraftingFunctionLibrary` | Blueprint helpers for crafting queries |
| `UACFItemsManagerComponent` | Resolves item descriptors / values (used by the vendor base) |

### Important structs

- `FACFCraftingRecipe` — `RequiredItems` (`TArray<FBaseItem>`), `OutputItem` (`FBaseItem`), `CraftingCost` (float).
- `FBaseItem` — `ItemClass` (`TSubclassOf<UACFItem>`) + `Count`.
- `FInventoryItem` — a concrete item instance in an inventory (used for upgrades and queries).
- `ItemsRecipes` (`TArray<UACFCraftRecipeDataAsset*>`) — the station's one recipe list: authored in the editor, server authoritative at runtime, replicated to clients and persisted as asset paths.

---

## 2 — Setup / Configuration

### A. Author the ingredient & output items first

Recipes reference item classes, so create/duplicate the relevant `UACFItem`/`UACFWeapon`/`UACFArmor` DataAssets first (see the **inventory-system** skill). Note their classes and stack counts.

### B. Create a recipe DataAsset

1. **Duplicate** a sample `UACFCraftRecipeDataAsset` into `Content/YourGame/Crafting/` (or create a new Data Asset of that class).
2. Open it and fill the `RecipeConfig` (`FACFCraftingRecipe`):
   - `RequiredItems` — add a `FBaseItem` per ingredient (`ItemClass` + `Count`).
   - `OutputItem` — the produced `FBaseItem` (`ItemClass` + `Count`).
   - `CraftingCost` — currency cost (0 = free).

### C. Configure the crafting station

1. **Duplicate** a sample crafting-station Blueprint (or add a `UACFCraftingComponent` to your station actor).
2. Add your recipe DataAssets to the component's **`ItemsRecipes`** array (the recipes the station starts with).
3. Vendor/currency settings inherited from `UACFVendorComponent`:
   - `bUseVendorCurrencyComponent` — whether the station has its own currency pool.
   - `PriceMultiplierOnBuy` / `PriceMultiplierOnSell` — if the station also trades.
4. As a `UACFStorageComponent`, the station can also hold items (chest-like behaviour) — populate its storage if desired.

> `ItemsRecipes` is the **only** recipe list. It is replicated, so runtime changes made by the server reach clients, and every query (`HasRecipe`, `GetCraftableRecipes`, `GetAllRecipeDataAssets`) reads it — which also means a recipe authored in the editor can be removed at runtime with `RemoveRecipe`. On save the list is written out as asset paths and restored in `OnComponentLoaded`, so a save replaces the authored defaults; a save taken before the component was persisted leaves them untouched.
>
> `AddRecipe` / `RemoveRecipe` / `AddRecipes` / `RemoveRecipes` / `ClearRuntimeRecipes` are **server only** — call them on the authority and let replication update clients; calling them on a client logs a warning and does nothing.

---

## 3 — Core Workflow / Runtime API

### Recipe management (`UACFCraftingComponent`)

```
Crafting->AddRecipe(MyRecipeDataAsset);                 // learn a recipe at runtime
Crafting->AddRecipes(MyRecipeArray);
Crafting->RemoveRecipe(MyRecipeDataAsset);
bool bKnows = Crafting->HasRecipe(MyRecipeDataAsset);
Crafting->ClearRuntimeRecipes();
TArray<UACFCraftRecipeDataAsset*> all = Crafting->GetAllRecipeDataAssets();
```

### Querying what can be crafted

```
TArray<FACFCraftingRecipe> recipes = Crafting->GetCraftableRecipes();

FACFCraftingRecipe outRecipe;
bool bFound = Crafting->TryGetCraftableRecipeForItem(SomeBaseItem, outRecipe);

// Pre-checks (validate before showing a "Craft" button as enabled):
bool bCanCraft   = Crafting->CanPawnCraftItem(outRecipe, PlayerPawn);
TArray<FInventoryItem> upgradables = Crafting->GetAllPawnUpgradableItems(PlayerPawn);
bool bCanUpgrade = Crafting->CanPawnUpgradeItem(InventoryItem, PlayerPawn);
```

### Performing the craft / upgrade — **server-side**

```
Crafting->CraftItem(outRecipe, PlayerPawn);            // consumes RequiredItems + cost, grants OutputItem
Crafting->UpgradeItem(InventoryItem, PlayerPawn);      // upgrades an existing inventory item
```

### Blueprint events

| Event (`BlueprintImplementableEvent`) | When it fires |
|---|---|
| `OnRecipeAdded(Recipe)` | A runtime recipe was learned |
| `OnRecipeRemoved(Recipe)` | A runtime recipe was forgotten |
| `OnRecipesChanged()` | The known-recipe set changed (refresh the crafting UI) |

---

## 4 — Wire to Characters / Blueprints

1. **Open the crafting UI**: when the player interacts with the station (ACF interaction system / `ACFBaseInteractableBP` duplicate), open your crafting widget and populate it from `GetCraftableRecipes()`.
2. **Validate before crafting**: gray out recipes where `CanPawnCraftItem` is false; the component checks the pawn's inventory for `RequiredItems` and currency for `CraftingCost`.
3. **Trigger the craft**: on button press, call `CraftItem(recipe, playerPawn)` **on the server**. The component removes ingredients from the player's `UACFEquipmentComponent`/inventory and adds the `OutputItem`.
4. **Upgrades**: list `GetAllPawnUpgradableItems(pawn)` in the UI; call `UpgradeItem(inventoryItem, pawn)` to consume materials and improve an existing item.
5. **Currency**: with `bUseVendorCurrencyComponent`, the station's `UACFCurrencyComponent` is used for cost handling; otherwise the player's currency is used.
6. **Refresh**: rebind your UI to `OnRecipesChanged` so newly learned recipes appear immediately.

---

## 5 — Verify

**Checklist before testing:**

- [ ] Ingredient and output item DataAssets exist (duplicated) and are referenced by class in the recipe.
- [ ] Recipe DataAssets are **duplicates** under `Content/YourGame/Crafting/` with `RequiredItems`, `OutputItem` and `CraftingCost` filled.
- [ ] Static recipes are added to the station component's `ItemsRecipes` array.
- [ ] The crafting station has a `UACFCraftingComponent` and (if trading) currency settings configured.
- [ ] The player pawn has a working inventory/equipment component holding the ingredients.
- [ ] `CraftItem` / `UpgradeItem` are invoked **server-side**.
- [ ] Crafting UI is bound to `OnRecipesChanged`.

**Common failures:**

| Symptom | Fix |
|---|---|
| Recipe doesn't show in the station UI | Not added to `ItemsRecipes` or not learned via `AddRecipe`; `AddRecipe` called on a client instead of the server; UI not reading `GetCraftableRecipes` / not bound to `OnRecipesChanged` |
| "Craft" button always disabled | `CanPawnCraftItem` false — missing `RequiredItems` in inventory or insufficient currency for `CraftingCost` |
| Craft does nothing / no output item | `CraftItem` called on client — must run on the **server**; or `OutputItem.ItemClass` is null |
| Ingredients not consumed | Pawn has no inventory component, or required item classes don't match what's in inventory |
| Learned recipes lost after reload | The station actor must be saved at all — check its `ShouldBeIgnored` override, an inverted condition there silently drops the whole actor from the save — **and** list `ACFCraftingComponent` in `ComponentsToSave` on its `ALSLoadAndSaveComponent`, otherwise the component is never serialized. Recipes persist as asset paths; a warning is logged if a saved recipe asset no longer exists |
| Upgrade option never appears | Item not eligible — verify it appears in `GetAllPawnUpgradableItems` and `CanPawnUpgradeItem` returns true |

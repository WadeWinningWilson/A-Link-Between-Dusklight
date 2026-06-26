# D-Pad Quick Swap — ALBW-Dusklight

Design + **completed research** for repurposing the d-pad cross (beyond Extra Item Slot / Midna on left) to support in-field sword cycling, shield cycling, and quick wolf transform.

**Status:** Implemented (2026-06-19). **Shield cycle crash + L1 item wheel** fixed and playtested (2026-06-20).

**Feature name:** **D-Pad Quick Swap** (settings UI label; doc filename stays `d-pad-reworking.md`).

**Decision:** Use **Option A** — extend the existing `ActionBinds` system (rebindable in controller config), not hardcoded PC-only intercepts.

**Research date:** 2026-06-19 (Cursor session + product clarifications).

---

## Implementation progress (2026-06-20)

Session work after initial Quick Swap ship. All builds validated with `build_run.bat` (RelWithDebInfo). **Not committed** unless product asks.

### L1 item wheel (Extra + Quick Swap only)

| Item | Detail |
|------|--------|
| Action bind | `OPEN_ITEM_WHEEL` in `include/dusk/action_bindings.h` + `action_bindings.cpp` |
| Auto-preset | L1 bound only when `extraItemSlot` mode == **Extra + Quick Swap** (`applyDpadQuickSwapPresetBinds()` in `src/dusk/dpad_quick_swap.cpp`) — not a global default |
| Open path | Handled in vanilla `key_wait_proc()` in `src/d/d_menu_window.cpp` (same as d-pad item ring) |
| Visual fix | Early open from `f_ap_game.cpp` skipped `ring_open_init` / `dMeter2Info_setWindowStatus(2)` — removed that path; L1 must go through `key_wait_proc()` so the ring draws |

### Shield quick-swap crash fix (save-editor multi-shield)

**Symptom:** Immediate crash on d-pad Right shield cycle when multiple shields granted via save editor (Ordon → Hylian).

**Root causes (stacked):**

1. **`setShieldArcName()`** used first owned `isItemFirstBit` when several flags set → wrong arc unloaded (e.g. CWShd while Hylian equipped) → heap corruption after `freeAll()`.
2. **`setCollision()`** called `getShieldMtx()` → `mShieldModel->getBaseTRMtx()` with **no null check** during reload delete phase (`mShieldModel = NULL`) → `EXCEPTION_ACCESS_VIOLATION` at fault addr `0x44`.
3. Guard state could persist during reload (`FLG2_UNK_8000000`) while model was null — looked like “raising shield” on cycle press.

**Fixes (current tree):**

| File | Change |
|------|--------|
| `src/d/actor/d_a_alink_swindow.inc` | PC `setShieldArcName()` maps arc from `getSelectEquipShield()` first; `setShieldChange()` no-ops if timer active, clears guard flag on start; PC `deleteObjectResMain` fallback in `loadShieldModelDVD()` delete phase (same pattern as clothes) |
| `src/d/actor/d_a_alink.cpp` | `setCollision()`: only calls `getShieldMtx()` when `mShieldModel != NULL`; field `execute()` runs `loadShieldModelDVD()` when not in status-window draw |
| `src/d/actor/d_a_alink_guard.inc` | `setShieldGuard()` early-out while `mShieldChangeWaitTimer != 0` |
| `src/d/d_meter2.cpp` | `dMeter2_equipOwnedShield`, `dMeter2_shieldIsOwned`, `dMeter2_ensureShieldOwned`, `dMeter2_requestLinkShieldModelUpdate()` — ownership sync + skip reload restart while timer active |
| `src/dusk/dpad_quick_swap.cpp` | `cycleNextShield()` blocks while reload timer active |

**Diagnostics:** Temporary `[ShieldSwapDbg]` logging (`include/dusk/shield_swap_debug.h`) added for investigation, **removed** after user confirmed fix (2026-06-20).

**Related doc:** [shield-ordon-quest-assets.md](shield-ordon-quest-assets.md) — item ID ↔ arc table; PC arc selection now follows equipped ID, not first bit.

### Playtest notes (2026-06-20)

- Shield cycle Ordon ↔ Hylian with save-editor multi-shield: **stable** after collision null guard.
- L1 item wheel in Extra + Quick Swap: opens and **draws** correctly.
- D-pad Right cycle shield does **not** map to guard; guard is ZR/R2 via `manualShieldButton()`. Guard pose during swap was reload artifact — fixed by clearing guard on `setShieldChange()`.

**Paste into any implementation chat:**

> Before coding: read `docs/build-fps-guidelines.md` and `docs/commit-and-push.md`. Build with `build_run.bat` only (RelWithDebInfo). No drive/conavigate code in `src/`. This work touches meter/shield equip feedback — also skim `docs/hud-performance-handoff.md` (no new HUD panes planned; shield row already auto-updates). When done: git diff summary + build result → build-analysis chat for FPS check **before** commit. Push to **upstream** (ALBW-Dusklight), not origin. Do not commit/push unless explicitly asked.

---

## Summary

| Question | Answer |
|----------|--------|
| Is this feasible? | **Yes.** Extra Item Slot already proves the pattern: reserve a d-pad direction, suppress vanilla handlers, run new gameplay. |
| Wolf transform on d-pad? | **Easiest.** `handleQuickTransform()` exists; today triggered by **R + Y** when **Quick Transform (R+Y)** is on. Rewiring to d-pad down is straightforward. |
| Sword / shield switch? | **Medium effort.** Equip APIs exist; in-field **cycle-to-next-owned** logic does not. Must be written. |
| Map / item ring? | Vanilla d-pad directions freed must be relocated via existing or new action binds (map binds already exist). |
| HUD changes required now? | **No.** LoP HUD hides the d-pad cross; shield row icons already reflect equipped shield. No new cross labels at this stage. |

---

## Product decisions (locked for v1)

| Topic | Decision |
|-------|----------|
| Feature name | **D-Pad Quick Swap** |
| Architecture | **Option A** — new `ActionBinds` entries + settings + controller config |
| Settings UI | **Tree under Extra Item Slot** — not a separate always-visible bool. See § Extra Item Slot mode below. |
| Proposed default mapping (Quick Swap mode) | **Left:** Call Midna · **Up:** cycle sword · **Right:** cycle shield · **Down:** quick wolf transform |
| Quick Transform (R+Y) | **Replaced** when Quick Swap mode is active — do not run R+Y chord in `f_ap_game.cpp` while Quick Swap is on. Standalone `enableQuickTransform` still applies in **Extra, No Quick Swap** and **Off** modes. |
| Map keyboard defaults | **Proposed only** (user has not locked keys) — see § Proposed keyboard map binds. Assign on entering Quick Swap mode if binds still invalid; user can rebind. |
| ALBW shields | Today: **one shield at a time** (`dMeter2_canAcquireShield`). **Future upgrade:** player can purchase and hold **two shields** — cycle logic must be written to tolerate that without a rewrite. |
| Menus / sub-screens | Open menus **correctly suppress** Quick Swap (same as Midna / vanilla). |
| Rental shop | Shop **owns d-pad** while open (left/right = category tabs today). Quick Swap must not fire. |
| Wolf Link | Sword/shield cycle **human only**. Transform rules unchanged (metamorphose eligibility, etc.). |
| LoP HUD | **No new d-pad cross artwork** for this stage. Shield icons on the bash row update automatically on equip change. |
| Collection menu | Remains the full equip UI; Quick Swap is a **field shortcut**, not a replacement. |
| Stage gating | Less relevant once map moves off d-pad to action binds. |
| Code gate | All new behavior **`#if TARGET_PC`** (ALBW-Dusklight convention). |

### Extra Item Slot mode (replaces bool `extraItemSlot` + separate Quick Swap toggle)

Single setting in **ALBW Settings** — a **3-way enum** (or nested select) replacing the current lone **Extra Item Slot** checkbox:

| Mode | Value (proposed) | Extra Item Slot (Midna left, Z item) | D-Pad Quick Swap |
|------|------------------|--------------------------------------|------------------|
| **Off** | `0` | No | No — vanilla d-pad |
| **Extra, No Quick Swap** | `1` | Yes | No — Midna on left; Up/Down/Right keep vanilla item ring + map |
| **Extra + Quick Swap** | `2` | Yes | Yes — full target layout below |

**Rules:**

- Quick Swap **cannot** be enabled without Extra Item Slot (no fourth “Quick Swap only” mode).
- Migrating saves: old `extraItemSlot == true` → **`Extra, No Quick Swap`** (preserves v0.55 behavior until user opts into Quick Swap).
- Helper: `isExtraItemSlotEnabled()` → mode ≥ 1; `isDpadQuickSwapEnabled()` → mode == 2.

**Settings UI copy (draft):** one control labeled **Extra Item Slot** with options **Off** / **Extra only** / **Extra + Quick Swap** (exact strings TBD).

---

## Current d-pad layout

### Vanilla Twilight Princess (GC/Wii)

All directions flow through `dMw_*_TRIGGER()` in `src/d/d_menu_window.cpp`, which wrap `mDoCPd_c::getTrigUp/Down/Left/Right(PAD_1)`.

| Direction | Primary gameplay use | Key code paths |
|-----------|---------------------|----------------|
| **Up** | Item ring (tools), opens from top | `dMw_c::key_wait_proc()` → `dMw_ring_create(0)` |
| **Down** | Item ring (alternate origin / wolf senses ring) | `key_wait_proc()` → `dMw_ring_create(2)` when `dMw_DOWN_TRIGGER()` |
| **Left** | Map — minimap toggle, full map open | `dMeterMap_c::keyCheck()`, `meter_map_move()` via `dMw_LEFT_TRIGGER()` |
| **Right** | Map — full map, minimap toggle | Same files via `dMw_RIGHT_TRIGGER()` |

**Start** (not d-pad) opens the **Collection / Quest** screen (`COLLECT_OPEN`), where sword and shield equip already live (`changeSword()`, `changeShield()` in `src/d/d_menu_collect.cpp`).

### With Extra Item Slot enabled (shipped v0.55)

Documented in `docs/albw-port.md` and `docs/patch-notes-v0.55.md`.

| Direction | Behavior |
|-----------|----------|
| **Left** | **Call Midna** — replaces vanilla Z for human Link. Default: left d-pad when no custom `CALL_MIDNA` bind. |
| **Up / Down / Right** | Unchanged vanilla (item ring + map). |

**Implementation hooks (existing):**

| File | Role |
|------|------|
| `src/d/actor/d_a_alink.cpp` — `midnaTalkTrigger()` | Reads `getActionBindTrig(CALL_MIDNA)` or `getTrigLeft(PAD_1)` when Extra Item Slot on |
| `src/dusk/action_bindings.cpp` — `callMidnaReservesDpadLeft()` | Returns true when Midna owns left d-pad |
| `src/d/d_menu_window.cpp` — `dMw_LEFT_TRIGGER()` | Returns **false** when `callMidnaReservesDpadLeft(0)` so map logic does not double-fire |
| `src/d/d_meter2_draw.cpp` | Midna icon on left cross arm (`kMidnaCrossLeftJujiIndex = 1`); hides map label `cont_ju6` |

**Z button:** Third item slot via `SELECT_ITEM_DOWN` when Extra Item Slot enabled (`dComIfGp_getSelectItem(SELECT_ITEM_DOWN)`, `checkItemSetButton()`, etc.).

### LoP HUD note

When `mLopHudActive`, `dMeter2Draw_c` **hides** `mpButtonCrossParent` (whole d-pad cross cluster). Inputs still work; the cross just is not drawn. Midna icon uses separate pane `midona_n`.

---

## Proposed target layout

When **Extra + Quick Swap** mode is active:

| D-pad | Action | `ActionBinds` |
|-------|--------|---------------|
| **Left** | Call Midna | `CALL_MIDNA` *(existing)* |
| **Up** | Cycle equipped sword | `CYCLE_SWORD` *(new)* |
| **Right** | Cycle equipped shield | `CYCLE_SHIELD` *(new)* |
| **Down** | Quick wolf ↔ human transform | `QUICK_TRANSFORM` *(new)* |

**Relocated vanilla functions (Quick Swap mode only):**

| Former d-pad role | Relocation |
|-------------------|------------|
| Item ring (Up/Down) | X / Y / Z item slots; optional future bind for ring if needed |
| Map (Left/Right) | `OPEN_MAP_SCREEN`, `TOGGLE_MINIMAP` action binds (already wired in `d_meter_map.cpp` for PC) |

### Proposed keyboard map binds (Quick Swap mode — not locked by product)

Applied **only when** entering Quick Swap mode **and** the bind is still `PAD_NATIVE_BUTTON_INVALID` (do not overwrite user/custom binds). All use SDL scancodes in `ActionBindConfigVar`.

| Action | Proposed key | SDL scancode | Rationale |
|--------|--------------|--------------|-----------|
| `OPEN_MAP_SCREEN` | **M** | `SDL_SCANCODE_M` | Common “map” mnemonic; not a default gameplay letter in TP |
| `TOGGLE_MINIMAP` | **Tab** | `SDL_SCANCODE_TAB` | Distinct from M; typical “toggle overlay” |
| Alternate minimap (if Tab conflicts) | **`** (backtick) | `SDL_SCANCODE_GRAVE` | Fallback only — document in settings help |

**Controller defaults (port 0)** when entering Quick Swap mode:

| Bind | Default |
|------|---------|
| `CYCLE_SWORD` | D-pad Up (`SDL_GAMEPAD_BUTTON_DPAD_UP` / `PAD_BUTTON_UP`) |
| `CYCLE_SHIELD` | D-pad Right |
| `QUICK_TRANSFORM` | D-pad Down |
| `CALL_MIDNA` | D-pad Left *(unchanged from Extra Item Slot)* |

**Not preset:** item ring — no default keyboard replacement in v1.

---

## Option A — ActionBinds extension (chosen approach)

### Existing infrastructure

`include/dusk/action_bindings.h` / `src/dusk/action_bindings.cpp`:

- Enum `ActionBinds`: `FIRST_PERSON_CAMERA`, `CALL_MIDNA`, `OPEN_MAP_SCREEN`, `TOGGLE_MINIMAP`, `OPEN_DUSKLIGHT_MENU`, `TURBO_SPEED_BUTTON`
- Per-port config in `include/dusk/settings.h` → `actionBindings.*`
- `updateActionBindings()` polls SDL gamepad / keyboard each frame
- `getActionBindTrig()` / `getActionBindHold()` for edge detection
- `setVirtualActionBind()` for touch overlay (`src/dusk/ui/touch_controls.cpp`)
- Controller config UI lists binds (`src/dusk/ui/controller_config.cpp`)

### Pattern to copy: Midna reserves left d-pad

```cpp
// action_bindings.h
bool callMidnaReservesDpadLeft(u32 port = 0);

// d_menu_window.cpp
BOOL dMw_LEFT_TRIGGER() {
#if TARGET_PC
    if (dusk::callMidnaReservesDpadLeft(0)) {
        return false;
    }
#endif
    // ...
}
```

Extend with sibling helpers, e.g.:

- `dpadUpReservedForQuickSwap(port)`
- `dpadDownReservedForQuickSwap(port)`
- `dpadRightReservedForQuickSwap(port)`
- Aggregate: `isDpadQuickSwapEnabled()` (mode == 2)

Each direction helper returns true when Quick Swap mode is on **and** the corresponding bind’s effective button is that d-pad direction (native SDL d-pad or remapped `PAD_BUTTON_*`).

Vanilla handlers that must respect reservations (**Quick Swap mode only**):

| Trigger | Suppress when reserved |
|---------|------------------------|
| `dMw_UP_TRIGGER()` | Up reserved → no item ring from up |
| `dMw_DOWN_TRIGGER()` | Down reserved → no item ring from down |
| `dMw_LEFT_TRIGGER()` | Already suppressed for Midna |
| `dMw_RIGHT_TRIGGER()` | Right reserved → no map from right |
| `dMeterMap_c::keyCheck()` / `meter_map_move()` | Same left/right reservations |
| `d_menu_ring.cpp` `isMoveEnd()` | Up/down ring dismiss — only when not reserved |

### New settings (implementation)

Under **ALBW Settings** — replace bool `game.extraItemSlot` with enum `game.extraItemSlotMode` (names TBD):

| Field | Type | Default | Notes |
|-------|------|---------|-------|
| `game.extraItemSlotMode` | `ConfigVar<int>` enum | `0` (Off) | `0` Off · `1` Extra only · `2` Extra + Quick Swap |
| `actionBindings.cycleSword[4]` | `ActionBindConfigVar` | invalid | Preset d-pad Up when entering mode 2 |
| `actionBindings.cycleShield[4]` | `ActionBindConfigVar` | invalid | Preset d-pad Right when entering mode 2 |
| `actionBindings.quickTransform[4]` | `ActionBindConfigVar` | invalid | Preset d-pad Down when entering mode 2 |

**Backward compat:** on load, if legacy bool `extraItemSlot` was true → mode `1`.

**Relationship to `enableQuickTransform` (QoL — Quick Transform R+Y):**

| Extra Item Slot mode | R+Y transform in `f_ap_game.cpp` | D-pad down transform |
|----------------------|----------------------------------|----------------------|
| Off | Runs if `enableQuickTransform` | N/A (vanilla d-pad down = item ring) |
| Extra, No Quick Swap | Runs if `enableQuickTransform` | N/A (vanilla down) |
| **Extra + Quick Swap** | **Disabled** (Quick Swap replaces it) | Runs via `QUICK_TRANSFORM` bind → `handleQuickTransform()` |

Inside `handleQuickTransform()`, gate on **`isDpadQuickSwapEnabled()` OR `enableQuickTransform`** so the shared function works from both code paths when allowed.

---

## Feature implementation detail

### 1. Quick wolf transform (d-pad down)

**Existing code:** `src/d/actor/d_a_alink_dusk.cpp` — `daAlink_c::handleQuickTransform()`

**Current trigger:** `src/f_ap/f_ap_game.cpp`

```cpp
if ((mDoCPd_c::getHold(PAD_1) & (PAD_TRIGGER_R | PAD_TRIGGER_L)) == PAD_TRIGGER_R
    && mDoCPd_c::getTrigY(PAD_1)) {
    dynamic_cast<daAlink_c*>(link)->handleQuickTransform();
}
```

**Gated by:** `isDpadQuickSwapEnabled()` for d-pad path; existing `enableQuickTransform` for R+Y path (see settings table above).

**Guards inside `handleQuickTransform()` (do not duplicate — call existing function):**

- `dComIfGs_isEventBit(M_077)` — transform ability unlocked
- Not in cutscene (`checkEventRun()`)
- Not in STAR tent (`R_SP161`)
- Z button not dimmed (`getButtonZAlpha() == 1.f`)
- Not holding Ball and Chain
- `m_midnaActor->checkMetamorphoseEnableBase()`
- Grounded, Midna ride, mode flags (wolf/human branches)
- Clears `mPressedButtonFlags` before transform

**Transform execution:** `procCoMetamorphoseInit()` → existing metamorphose proc in `d_a_alink.cpp`.

**New work:** On `getActionBindTrig(QUICK_TRANSFORM, port)` in gameplay context (see § When actions fire), call `handleQuickTransform()`. Suppress `dMw_DOWN_TRIGGER()` when down is reserved.

---

### 2. Cycle sword (d-pad up)

**No in-field cycle exists today.** Reference implementation: `dMenu_Collect2D_c::changeSword()` in `src/d/d_menu_collect.cpp` — cursor-column-based, not cyclic.

**Equip API:**

```cpp
dMeter2Info_setSword(u8 i_itemId, bool i_offItemBit);
// Valid IDs: WOOD_STICK, SWORD, MASTER_SWORD, LIGHT_SWORD, NONE
// Updates dComIfGs_setSelectEquipSword / dComIfGp_setSelectEquipSword
```

**Model swap:** Link picks up sword model in existing equip path (`d_a_alink.cpp` ~4360+) when `dComIfGs_getSelectEquipSword()` changes; sets `mSwordChangeWaitTimer = 5` on model change.

**New helper (proposed):** `dusk::cycleEquipSword()` or `dMeter2_cycleNextSword()`:

1. If wolf, cutscene, menu, rental shop, or `getSwordChangeWaitTimer() != 0` → no-op (optional error SE).
2. Build ordered list of **owned** swords (check `dComIfGs_isItemFirstBit` / collect flags / story progress — mirror collection menu rules).
3. Find current index in list; advance to next (wrap).
4. If unchanged, return.
5. `dMeter2Info_setSword(next, false)`.
6. Feedback: `Z2SE_SY_ITEM_SET_X`, `dMeter2Info_set2DVibration()` (match collection menu).

**Suggested cycle order (human Link, verify against collection menu columns):**

1. Ordon Sword (`dItemNo_SWORD_e`) or Wooden Sword (`dItemNo_WOOD_STICK_e`) — tier 1 column logic
2. Master Sword (`dItemNo_MASTER_SWORD_e`) or Light Sword (`dItemNo_LIGHT_SWORD_e`) — tier 2
3. Light Sword — tier 3 column if applicable

Exact ordering must match quest progression gates in `changeSword()` switch cases — **port the eligibility checks, not the cursor UI.**

**No `setSwordChange()`** — shield has `daAlink_c::setShieldChange()`; sword relies on wait timer in model update path.

---

### 3. Cycle shield (d-pad right)

**Reference:** `dMenu_Collect2D_c::changeShield()` + `daAlink_getAlinkActorClass()->setShieldChange()`.

**Equip API:**

```cpp
dMeter2Info_setShield(u8 i_itemId, bool i_offItemBit);
// Valid: WOOD_SHIELD, SHIELD, HYLIA_SHIELD, NONE
```

**PC-specific block in `dMeter2Info_setShield()`:** When switching to a non-NONE shield and player already has any shield, clears other shield first bits then sets new — aligns with **one shield at a time** today.

**ALBW today (`src/d/d_meter2.cpp`):**

```cpp
bool dMeter2_canAcquireShield(u8 itemNo) {
    // One shield at a time until a future upgrade path allows multiples.
    return false; // when another shield owned
}
```

**Future — two-shield upgrade:** Cycle logic should:

- Maintain ordered list of **currently owned** shields (1 or 2 entries after upgrade).
- Advance equipped shield to the other owned shield.
- Still call `setShieldChange()` on Link after `dMeter2Info_setShield()`.
- Respect `getShieldChangeWaitTimer()`.

Implement cycle helper so **`maxOwnedShields` is not hardcoded to 1** — e.g. cap at 2 when upgrade bit/setting is set.

**LoP HUD:** Bash shield row (`d_albw_shield.cpp`, `d_meter2_draw.cpp`) already reflects equipped shield — no extra HUD work for v1.

---

## When Quick Swap may fire

Quick Swap actions must **not** run when vanilla would block d-pad gameplay menus. Mirror conditions used elsewhere:

| Blocker | Check |
|---------|-------|
| Any Dusklight UI document | `dusk::ui::any_document_visible()` — action bind update already skips most binds |
| Pause / menu window active | `dMeter2Info_getMenuWindowClass()` proc not `NO_MENU` |
| Item ring open | Ring proc active |
| Map full-screen | `dMeter2Info_getMapStatus()` in open states (2–9) |
| Collection / sub-windows | Collect proc or fade |
| Event / cutscene | `dComIfGp_event_runCheck()` (transform already checks) |
| Rental shop | `dALBWRental_isOpen()` — **shop owns d-pad**; suppress all Quick Swap actions |
| NPC dialogue | `dMsgObject_isTalkNowCheck()` where map already checks |
| Wolf form | Block sword/shield cycle; allow transform (to human) per existing rules |
| Heap lock / stage transition | `dComIfGp_isEnableNextStage()` etc. |

**Recommended central gate:** `dusk::canUseDpadQuickSwap(port)` — returns false unless `isDpadQuickSwapEnabled()` and gameplay context is clear. Call from one update site (e.g. end of `f_ap_game.cpp` PC block or new `src/dusk/dpad_quick_swap.cpp`).

---

## File touch list (implementation checklist)

| File | Change |
|------|--------|
| `include/dusk/action_bindings.h` | Add `CYCLE_SWORD`, `CYCLE_SHIELD`, `QUICK_TRANSFORM`; reserve helpers |
| `src/dusk/action_bindings.cpp` | Register binds; implement reserve helpers |
| `include/dusk/settings.h` | `extraItemSlotMode` enum; deprecate bool `extraItemSlot`; new `actionBindings` arrays |
| `src/dusk/settings.cpp` | Defaults, migration from bool, `Register()` |
| `src/dusk/ui/settings.cpp` | 3-way Extra Item Slot control under ALBW Settings |
| `src/dusk/ui/controller_config.cpp` | Expose new binds in rebinding UI |
| `src/d/d_menu_window.cpp` | `dMw_*_TRIGGER()` suppress when reserved (Quick Swap mode) |
| `src/d/d_meter_map.cpp` | Suppress left/right map when reserved |
| `src/f_ap/f_ap_game.cpp` | Poll new binds; **skip R+Y** when Quick Swap on; call handlers |
| `src/d/actor/d_a_alink_dusk.cpp` | Update `handleQuickTransform()` gate (minimal) |
| **New:** `src/dusk/dpad_quick_swap.cpp` + header | `cycleNextSword()`, `cycleNextShield()`, `canUseDpadQuickSwap()`, `isDpadQuickSwapEnabled()` |
| `docs/albw-port.md` | Update Extra Item Slot row + link this doc *(when implementing)* |
| `docs/patch-notes-*.md` | Release notes *(when shipping)* |

**Do not change for v1:** `d_meter2_draw.cpp` cross labels (LoP hides cross); collection menu equip flow.

---

## Conflicts and mitigations

| Risk | Mitigation |
|------|------------|
| Double-fire (bind + vanilla) | Reserve helpers in `dMw_*_TRIGGER()` + map keyCheck |
| Rental shop d-pad tabs | `dALBWRental_isOpen()` in central gate; Midna already suppressed in shop |
| Midna on left + map on left | Already solved by `callMidnaReservesDpadLeft` |
| Quick transform + item ring on down | Suppress `dMw_DOWN_TRIGGER()` when down reserved |
| R+Y + d-pad both transform | **Resolved:** R+Y disabled when Quick Swap mode on |
| One-shield → two-shield upgrade | Cycle helper uses dynamic owned list; `dMeter2_canAcquireShield` updated separately when upgrade ships |
| Sword cycle during guard / attack | Check `mSwordChangeWaitTimer`, proc ID if needed (collection menu checks wait timers) |
| Action bind on keyboard | Supported by existing bind system — no special case |

---

## Effort estimate

| Piece | Size |
|-------|------|
| ActionBinds + settings + controller config | Small–medium |
| Reserve flags in `dMw_*` + meter map | Small |
| Quick transform on bind | Small |
| `cycleNextSword()` / `cycleNextShield()` | Medium |
| Central gameplay gate + rental/menu suppress | Small |
| Docs / patch notes | Small |

**Total:** ~1 week focused work including test pass on Ordon field, dungeon map stages, wolf/human transform edge cases, and rental shop.

---

## Test plan (for implementation)

- [x] Mode **Off** → vanilla d-pad unchanged
- [x] Mode **Extra, No Quick Swap** → Midna left + Z item; vanilla Up/Down/Right; R+Y transform still works if QoL toggle on
- [x] Mode **Extra + Quick Swap** → left Midna, up/right/down Quick Swap; R+Y transform **does not** fire
- [x] Entering Quick Swap mode presets d-pad binds + proposed M/Tab map keys only when still unbound
- [x] Midna call does not open map when left reserved
- [x] Sword cycle skips unowned tiers; respects wait timer
- [x] Shield cycle with save-editor multi-shield ownership (Ordon + Hylian): **no crash** (2026-06-20 fix)
- [ ] Shield cycle with one shield (no-op or single entry); **placeholder** for two-shield upgrade
- [x] Transform blocked in cutscene, STAR tent, ball & chain, metamorphose disabled
- [x] Transform works wolf→human and human→wolf when eligible
- [x] Item ring does not open on reserved up/down (Quick Swap mode)
- [x] Map does not open on reserved left/right; `OPEN_MAP_SCREEN` / M key still works
- [x] Rental shop: d-pad tabs work; no Midna / Quick Swap during shop
- [x] Collection menu equip still works; Quick Swap uses same equip state
- [x] LoP HUD: shield row updates after shield cycle without cross labels
- [ ] Rebind cycle/transform to keyboard in controller config
- [x] Wolf form: sword/shield cycle blocked; transform allowed per rules
- [x] Save migration: old `extraItemSlot=true` → mode 1
- [x] L1 item wheel opens and **draws** in Extra + Quick Swap mode (2026-06-20)

---

## Related docs and code

| Resource | Path |
|----------|------|
| Extra Item Slot shipped notes | `docs/patch-notes-v0.55.md`, `docs/albw-port.md` |
| Shield item ID ↔ arc (save-editor cycling) | `docs/shield-ordon-quest-assets.md` |
| Midna / Z slot | `src/d/actor/d_a_alink.cpp` — `midnaTalkTrigger()` |
| Action binds | `src/dusk/action_bindings.cpp` |
| D-pad Quick Swap logic | `src/dusk/dpad_quick_swap.cpp` |
| Shield equip / ownership API | `src/d/d_meter2.cpp` — `dMeter2_equipOwnedShield()`, `dMeter2_shieldIsOwned()` |
| Shield model reload | `src/d/actor/d_a_alink_swindow.inc` — `setShieldChange()`, `loadShieldModelDVD()` |
| Quick transform | `src/d/actor/d_a_alink_dusk.cpp`, `src/f_ap/f_ap_game.cpp` |
| Collection equip | `src/d/d_menu_collect.cpp` — `changeSword()`, `changeShield()` |
| Item wheel (L1) | `src/d/d_menu_window.cpp` — `key_wait_proc()` |
| Shield one-at-a-time | `src/d/d_meter2.cpp` — `dMeter2_canAcquireShield()` |
| Map PC binds | `src/d/d_meter_map.cpp` — `OPEN_MAP_SCREEN`, `TOGGLE_MINIMAP` |
| Rental shop d-pad | `src/d/d_albw_rental.cpp` |
| LoP HUD cross hide | `src/d/d_meter2_draw.cpp` ~3450 |

---

## Build & handoff (for implementation agents)

### Always read first

| Doc | Why |
|-----|-----|
| [build-fps-guidelines.md](build-fps-guidelines.md) | Canonical build (`build_run.bat` / RelWithDebInfo), launch hygiene (no `DUSK_DRIVE*`), what never goes in the repo |
| [commit-and-push.md](commit-and-push.md) | Push to **upstream** (`WadeWinningWilson/A-Link-Between-Dusklight`), not `origin`; stage/don’t-stage rules |

### Also read for this feature

| Doc | Why |
|-----|-----|
| [hud-performance-handoff.md](hud-performance-handoff.md) | Equip changes touch meter/shield HUD refresh paths — no new per-frame work in `draw()` |
| [albw-hud-lop-layout-brief.md](albw-hud-lop-layout-brief.md) | LoP hides d-pad cross; shield row auto-updates on equip |

This feature does **not** require LoP layout edits or boss docs unless scope creeps.

### Hard rules (one-liners)

| Rule | Detail |
|------|--------|
| **Build** | `build/windows-msvc-relwithdebinfo/dusklight.exe` via **`build_run.bat`** only for validation |
| **Never commit** | `local_dev_backup/`, drive/conavigate sources, `albw_*_debug.txt` |
| **FPS** | Don’t revert features to fix FPS — optimize in place; hand off to build-analysis chat before commit |
| **Commit/push** | Only when product explicitly asks |
| **Remote** | Push to **`upstream`** (ALBW-Dusklight), not TwilitRealm `origin` |

### After implementation — hand off

1. `git status` + `git diff --stat`
2. Confirm `build_run.bat` succeeds
3. Summarize diff + manual playtest notes → **build-analysis chat** for FPS check
4. Do not commit until product approves post-FPS review

---

## Resolved product questions

| # | Question | Resolution |
|---|----------|------------|
| 1 | Feature / settings name | **D-Pad Quick Swap**; 3-way **Extra Item Slot** tree under ALBW Settings |
| 2 | Require Extra Item Slot? | **Yes** — Quick Swap only in mode 2; cannot enable alone |
| 3 | Default keyboard map binds | **Proposed M + Tab** when entering Quick Swap if unbound — not product-locked; see § Proposed keyboard map binds |
| 4 | R+Y quick transform | **Replaced** when Quick Swap mode on; R+Y remains in Off / Extra-only modes |

## Open questions (remaining)

| # | Question |
|---|----------|
| 5 | **Two-shield upgrade** — event bit / setting name when spec'd (cycle code must not assume max 1) |
| 6 | **Exact settings UI strings** for the 3-way control (Off / Extra only / Extra + Quick Swap) |
| 7 | **Confirm M + Tab** map defaults after playtest on keyboard layout |

---

## Implementation gate

**Do not merge until:** product gives final **go implement** (spec in this doc is otherwise locked).

After confirmation, implement in order:

1. `extraItemSlotMode` enum + save migration + helpers (`isExtraItemSlotEnabled`, `isDpadQuickSwapEnabled`)
2. ActionBinds enum/config for `CYCLE_SWORD`, `CYCLE_SHIELD`, `QUICK_TRANSFORM`
3. Preset binds + proposed M/Tab when entering mode 2
4. Reserve helpers + vanilla d-pad / map suppress
5. `f_ap_game.cpp`: Quick Swap poll + **disable R+Y** when mode 2
6. `cycleNextSword()` / `cycleNextShield()`
7. Settings UI (3-way tree) + controller config
8. Test plan pass
9. `build_run.bat` → build-analysis FPS handoff → `docs/albw-port.md` + patch notes *(when shipping)*

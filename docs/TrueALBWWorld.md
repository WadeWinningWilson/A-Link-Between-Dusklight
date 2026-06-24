# True ALBW World / TRUETEST — design handoff & session archive

**Purpose:** Preserve open-world / nonlinear progression planning and TRUETEST bootstrap design so you **do not need to reopen the long Cursor chat** (it may OOM). Start new chats with:

> Continue True ALBW world / TRUETEST work — read `docs/TrueALBWWorld.md` first.

**Status:** Phase 1 + initial bootstrap implemented (2026-06-21). Layer override table, Skull Kid AppearSet, and Shade Watcher services remain future work.

**Related docs:** [albw-port.md](albw-port.md), [boss-fights-handoff.md](boss-fights-handoff.md) (Boss Refinement), [shield-combat.md](shield-combat.md) (references Dawnlight fork), [build-fps-guidelines.md](build-fps-guidelines.md), [building.md](building.md).

---

## Save isolation guarantee (design intent)

**Yes — TRUETEST is per-save only and must not touch other files or vanilla behavior.**

| Concern | How we stay safe |
|---------|------------------|
| Other save slots | Bootstrap writes **only** to the save being created/loaded. Memory-card slots are independent. |
| Existing vanilla saves | Load with **no TRUETEST stamp** → zero bootstrap, zero layer overrides, zero flag bundles. |
| Release / default build | Global mode defaults **Off**. TRUETEST code paths require **both** global enable **and** save stamp. |
| Reversibility | Delete or overwrite the TRUETEST slot; set global mode to Off/True ALBW; load any other save — world state is whatever that save contains. |
| Mid-session toggle | **Forbidden** for bootstrap. Global mode change while in-game is disabled; loaded save always reads **stamp**, not live global. |

**What “selecting TRUETEST at startup” means in practice:**

1. **Global setting** (config): `TrueAlbwMode = TRUETEST` — enables the new-save prompt and test hooks. Does **not** mutate any save until you create or load a stamped file.
2. **New empty slot:** Vanilla yes/no UI (Dawnlight Boss Rush pattern) → choose **TRUETEST** → stamp written to **that save only** → bootstrap runs **once** after name + horse name.
3. **Load TRUETEST save:** Game reads stamp → applies TRUETEST runtime behavior (layers, hooks) for **this session only** on **this file’s data**.
4. **Load vanilla save (same session):** Stamp absent → normal game. No leftover bootstrap from the previous file unless incorrectly implemented (must clear per-load from stamp).

**True ALBW** (middle tier) = shop unlock only (`trueAlbwShop` today). **No** world bootstrap, layers, AppearSet, or MS→mirror sync.

---

## Mode tree (agreed)

Replace bool `game.trueAlbwShop` with enum `TrueAlbwMode`:

| Mode | Config / behavior |
|------|-------------------|
| **Off** | Vanilla reclaim-what-you-lost rental shop |
| **True ALBW** | All rental items unlocked from Postman (current `trueAlbwShop` logic) — **shop only** |
| **TRUETEST** | True ALBW shop + world bootstrap + experimental hooks (see below) |

Pattern exists: enums like `ExtraItemSlotMode`, `LopHudMode` in `include/dusk/settings.h` + select UI in `src/dusk/ui/settings.cpp`.

**Gating rules:**

- Global mode: changeable only when **not** in gameplay (`!IsGameLaunched` / file select or title).
- Per-save stamp: chosen at **new save creation** (file select), immutable for that file.
- Bootstrap: run **once** on first field entry if stamp = TRUETEST and `bootstrapApplied` bit clear.

---

## Dawnlight reference — new-save UI (Boss Rush)

**Fork:** [BeZide93/dusk](https://github.com/BeZide93/dusk) branch `dawnlight`.

Dusklight doc cross-ref: [shield-combat.md](shield-combat.md) cites Dawnlight commit `9ff9d35`.

### What to copy (pattern, not Boss Rush logic)

| Piece | Dawnlight | TRUETEST equivalent |
|-------|-----------|---------------------|
| Hook | `dFile_select_c::dataSelectStart()` when `mIsDataNew[mSelectNum]` | Same — before `startNewGameNameInput()` |
| UI | Vanilla yes/no panes + `headerTxtSetRaw("Select story mode", ...)` | e.g. `"New Game"` / `"TRUETEST"` |
| Pending flag | `mBossRushPending` | `mTrueTestPending` (name TBD) |
| Apply timing | `nameInput2()` case 2, after horse name | `applyTrueTestBootstrap()` + stamp |
| Save flag | `dSv_reserve_c::setBossRush(true)` | `setTrueTest(true)` in reserve bytes |
| Slot label | `" BR"` suffix in `d_file_sel_info.cpp` | Optional `" TT"` or similar |
| Preset module | `dusk::bossrush::apply_new_save_preset()` | `dusk::truetest::apply_new_save_preset()` |

### Dawnlight proc chain (for implementation)

New `DATASELPROC_*` entries in `d_file_select.h` / `dataSelProc[]`:

- `DATASELPROC_BOSS_RUSH_MODE_IN` → `_SELECT` → `_CURSOR_MOVE` → `_CLOSE` / `_CANCEL`

Dusklight vanilla today: empty/new slot goes **straight to name entry** (`DATASELPROC_SELECT_DATA_NAME_MOVE`) — no mode prompt.

### Dusklight file-select flow (baseline)

```
Empty slot (mIsNoData) → create confirm (yes/no)
New slot (mIsDataNew)  → name → horse name → DATASELPROC_NEXT_MODE_WAIT
                       → optional TV bright check → changeGameScene()
New game spawn: d_s_name.cpp ~412 — isDataNew() → F_SP108 room 21
setInitSaveData(): card init only (d_file_select.cpp ~5721)
setInitEventBit(): empty in d_save_init.cpp
```

---

## Save data — where stamps live

| Location | Current state | Planned use |
|----------|---------------|-------------|
| `dSv_reserve_c` | `u8 unk[80]` — unused | `isTrueTest()`, `bootstrapApplied`, optional `trueAlbwShopSave` |
| `dSv_event_c::mEvent[]` | Master Quest uses bytes 100–102 | Avoid unless reserve fills up; see `d_albw_master_quest.cpp` |

**Precedent:** Master Quest stores tier/hearts in event regs 100–102 (confirmed unused by vanilla).

**Backward compatibility:** Existing saves have reserve = 0 → interpreted as vanilla. No migration required.

---

## TRUETEST bootstrap bundle (planned, not coded)

Run **once** per TRUETEST-stamped save on first field entry (idempotent bit).

### Core world state

- `onDarkClearLV(0..3)` — twilight always cleared
- Per-stage **endgame layer overrides** via `getLayerNo_common_common()` hook (Ordon exempt: `F_SP103`, `F_SP104`, `R_SP01`)
- Avoid setting `F_0250` (spawns Ganon wall); prefer layer override for North Faron
- Set `F_0542` (Ganon wall down), `M_077` (transform), skip `M_071` (Desperate Hour)

### Sacred Grove / sewers skip (design)

- Enter grove via **North Faron** (`F_SP108`) — wolf + Midna jumps, **not** interior warp
- **Shade Watcher** row in `kShadeWatchers[]` at `F_SP108` room 6 (North Faron per `include/dusk/map_loader_definitions.h`)
- Shop service: warp to entrance + flag bundle (not grove interior)
- Sewers skip = Midna-healed bootstrap without playing sewers

### Sacred Grove path flags (candidate bundle)

- `0x0801`, `0x1710`, `0x0A08`, `0x0910`, `F_0238`

### TRUETEST-only hooks

- **Skull Kid:** try `AppearSet()` on grove `E_PM` without wolf howl first; fallback = pre-set area flags
- **MS → mirror sync:** `CollectMirror(0–3)`, `CollectCrystal(0–2)`, `F_0354` — **TRUETEST only**, not production True ALBW

### Ganon path

- `F_0542` for wall down
- **Boss Refinement** (`game.bossRefinement`) handles any-sword boss gates; MS/Light Sword still matter for overworld gates

---

## Shop gaps (vanilla keys OK)

Dungeon keys stay **vanilla in-dungeon** (`getKeyNum()` — no shop interaction).

**Missing from rental shop today:** Iron Boots (sumo/Ordon), Lantern (Coro oil hook), Horse whistle, Zora armor, swords.

**Field gates** (not keys) are real blockers: Zora armor, boots, MS, Light Sword, mirror shards.

---

## Key code references (Dusklight)

| Topic | Location |
|-------|----------|
| Layer selection | `src/d/d_com_inf_game.cpp` — `getLayerNo_common_common()` |
| Twilight | `src/d/d_kankyo.cpp` — `dKy_darkworld_stage_check()` |
| True ALBW shop (today) | `include/dusk/settings.h` (`trueAlbwShop`), `src/d/d_albw_rental.cpp` |
| Boss Refinement | `src/d/d_albw_boss.cpp` |
| Shade Watcher spawn | `src/d/d_s_room.cpp` — `kShadeWatchers[]` |
| Shade shop service | `src/d/d_albw_shade_refuge.cpp` |
| Skull Kid | `src/d/actor/d_a_e_pm.cpp` — `AppearSet()`, `mStage==0` |
| Wolf howl trigger | `src/d/actor/d_a_alink_wolf.inc` ~4197 |
| MS pull | `item_func_MASTER_SWORD()` in `src/d/d_item.cpp` |
| Mirror/CitS gate | `F_0354` — `daTagLv8Gate_c`, `daObjMirrorTable_c` |
| Ganon wall | `F_0250` spawns, `F_0542` removes |
| File select | `src/d/d_file_select.cpp`, `src/d/d_s_name.cpp` |
| Event flag docs | `src/dusk/imgui/ImGuiEventFlags.hpp` |
| Save reserve | `include/d/d_save.h` — `dSv_reserve_c` |

---

## Implementation checklist (when building)

### Phase 1 — Mode + save stamp (no bootstrap yet)

- [x] `TrueAlbwMode` enum in `settings.h` / `settings.cpp` / `config.cpp`
- [x] Settings UI: three-way select; disabled when `IsGameLaunched`
- [x] Extend `dSv_reserve_c` with typed accessors (keep 80-byte size)
- [x] Dawnlight-style file select prompt when global = TRUETEST and `mIsDataNew`
- [x] Stamp + `apply_new_save_preset()` stub (shop stamp only first — prove isolation)
- [x] Load path: read stamp; gate shop in `d_albw_rental.cpp` from stamp **or** global True ALBW
- [x] Optional slot label in `d_file_sel_info.cpp` (`TT` suffix)

### Phase 2 — Bootstrap module

- [x] `src/dusk/truetest.cpp` — `applyNewSavePreset()`, `onStageLoad()`
- [ ] Layer override table
- [x] Flag bundle (twilight clear, transform, Ganon wall, grove puppet flag, MS→mirror sync)
- [x] `bootstrapApplied` bit — never re-run on same save

### Phase 3 — Runtime hooks (TRUETEST stamp only)

- [ ] Skull Kid `AppearSet()` on grove entry
- [ ] MS→mirror sync
- [ ] Shade Watcher + shop services (later)

### Phase 4 — Shop additions

- [ ] Iron Boots, Lantern, Horse, Zora, swords (separate from bootstrap)

---

## Open decisions (user input before coding)

1. **True ALBW shop on new saves:** Live global toggle vs save-stamp only for shop tier?
2. **TRUETEST picker UI:** Two-option yes/no (Vanilla new game / TRUETEST) vs three-way (Off / True ALBW / TRUETEST)?
3. **Skull Kid AppearSet:** Grove entry only, or also Shade Watcher service?
4. **User building guide:** Pending — read before first implementation PR.

---

## Session history (condensed)

- **Option C hybrid** agreed: per-stage endgame layer overrides, twilight cleared, optional flag bootstrap for NPCs/paths; cutscenes not required.
- **Sacred Grove:** North Faron path, not sewer warp; Shade Watcher at grove entrance for skip service.
- **All bootstrap ideas TRUETEST-only;** True ALBW = shop behavior only.
- **Dawnlight Boss Rush** researched as the vanilla UI pattern for new-save mode selection.
- User wants **easily reversible** test saves; other slots and release behavior unchanged.

---

## Building TRUETEST / True ALBW World (new chats)

**Always read first:**

| Doc | Why |
|-----|-----|
| [build-fps-guidelines.md](build-fps-guidelines.md) | Canonical build (`build_run.bat` / RelWithDebInfo), launch hygiene (no `DUSK_DRIVE*`), what never goes in the repo |
| [commit-and-push.md](commit-and-push.md) | Push to **upstream** (`WadeWinningWilson/ALBW-Dusklight`), not `origin`; stage/don't-stage rules |
| **This file** | TRUETEST design, save isolation, Dawnlight file-select pattern, bootstrap bundle |

**If the work touches…**

| Area | Also read |
|------|-----------|
| HUD / meter / shield / `game.lopHud` | [hud-performance-handoff.md](hud-performance-handoff.md) + [albw-hud-lop-layout-brief.md](albw-hud-lop-layout-brief.md) |
| LoP layout details | [albw-hud-lop-layout-v3-instructions.md](albw-hud-lop-layout-v3-instructions.md) |
| Boss fights / refinement | [boss-fights-handoff.md](boss-fights-handoff.md) |
| Boss HP bar tuning | [albw-boss-hp-hud-tuning-brief.md](albw-boss-hp-hud-tuning-brief.md) |
| Rental shop / Postman | [albw-port.md](albw-port.md), `src/d/d_albw_rental.cpp` |
| File select / new save | Dawnlight `d_file_select.cpp` (see [Dawnlight reference](#dawnlight-reference--new-save-ui-boss-rush) above) |

**After they build — hand off for FPS review:**

| Chat | Doc entry |
|------|-----------|
| Build-analysis / FPS review | [build-fps-guidelines.md](build-fps-guidelines.md) → Doc map section, then [hud-performance-handoff.md](hud-performance-handoff.md) if HUD changed |

**Short prompt (paste into TRUETEST implementation chat):**

> Before coding: read `docs/TrueALBWWorld.md`, `docs/build-fps-guidelines.md`, and `docs/commit-and-push.md`.  
> Build with `build_run.bat` only (RelWithDebInfo). No drive/conavigate code in `src/`.  
> TRUETEST bootstrap is **save-stamped only** — never global mid-save.  
> Model new-save UI on Dawnlight Boss Rush (`dataSelectStart` + `nameInput2` apply hook).  
> True ALBW = shop only; no world mutation.  
> When done: git diff summary + build result → build-analysis chat for FPS check before commit.  
> Push to **upstream** (ALBW-Dusklight), not origin.

**Hard rules (one-liners):**

- **Build:** `build/windows-msvc-relwithdebinfo/dusklight.exe` via `build_run.bat`
- **Never commit:** `local_dev_backup/`, drive/conavigate sources, `albw_*_debug.txt`
- **Don't revert features to fix FPS** — optimize in place; use the build-analysis chat
- **Don't commit/push unless the user explicitly asks**

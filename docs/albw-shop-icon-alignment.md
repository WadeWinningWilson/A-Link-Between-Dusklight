# ALBW rental shop — rentable row icon alignment

**For:** future shop UI work (Postman's Lending Service)  
**Repo:** Dusklight / ALBW-Dusklight (`TARGET_PC_NATIVE_UI`)  
**Status:** Fixed (2026-06) — rentable item icons left-aligned in mail column; locked `?????` rows unchanged.

**Primary files:** `src/d/d_albw_shop.cpp`, `include/d/d_albw_shop.h`  
**Related:** `src/d/d_albw_rental.cpp` (catalog, `purchasable` flag)  
**Debug log:** `albw_shop_debug.txt` (local, optional)  
**Baseline reference:** git commit `7e80d6d` (native shop before icon alignment work)

---

## Goal

Locked rows show a brown envelope at the **far left** of each `flame_*` row bar. Rentable rows (Ooccoo, Slingshot, Boomerang, etc.) were drawing the white item-box swirl (`tt_do_icon7`) and wheel icon **centered over the item name**. The fix aligns rentable icons to the **same left column** as mail envelopes, without breaking:

- black/gold `flame_*` row bars  
- item names and rupee prices (`fenu_t*`)  
- parchment description column  
- footer / wallet HUD  

---

## BLO layout (why this was hard)

The shop uses `zelda_letter_select_6menu.blo` (`letres.arc`). Important hierarchy:

| Layer | Tags | Role |
|-------|------|------|
| Row bar | `flame_00`–`flame_05` | Semi-transparent row background |
| Row container | `let_00_n`–`let_05_n` (PAN2) | **Contains `flame_*` and centered item art** |
| Text (siblings) | `fenu_t0`–`fenu_t11` | Names + prices — **outside** `let_*` |
| Mail gfx | Envelope WIN/PIC near `let_*` | Left column on locked rows |
| Rentable icons | PIC/WIN under `let_*` + `mpRowBloItemGfx` | Native centered item box + icon |

Rentable rows draw icons through **two layers**:

1. **BLO** — `6menu.blo` draws centered item-box + icon inside `let_*` during `mpMenuScreen->draw()`.
2. **Heap** — `drawRowWheelIcons()` draws `mpItemBoxPic` (`tt_do_icon7`) and per-row `mpRowItemPic[]` from `itemicon.arc`.

Fixing alignment required coordinating both layers, not moving one in isolation.

---

## What failed (do not repeat)

| Approach | Why it broke |
|----------|--------------|
| Hide all `let_*` children for rentable rows | `flame_*` lives inside `let_*` — bars vanished |
| Hide panes overlapping `fenu_t*` name boxes | Name panes span full row width — also hid `flame_*` |
| Heap reposition only (no BLO hide) | Centered BLO art still visible after menu draw |
| Single mail X from row-0 for all rows | Scroll drift — each `let_*` has slightly different native X |
| `paneTrans` on `mpRowLetterMgr` (`let_*`) | Shifts entire row ~250px left; rows clip off ornamental frame |
| Blanket hide/show cycles on `let_*` subtree | Oscillation between missing bars vs wrong icon position |

**Price column** already uses `paneTrans` on `fenu_t6` **mgr** panes — that pattern does **not** work on `let_*` mgr because it contains the row bar.

---

## Working solution

### Architecture: heap draw at mail column + hide centered BLO duplicates

**Step 1 — Anchor mail column** (`calcShopListIconLeft`):

1. Prefer visible envelope on a locked `?????` row in viewport (`mpRowLetterGfx` / `mpRowLetter`).
2. Else min `let_*` slot X across all six screen rows.
3. Else min `flame_*.x + kRowFrameInsetX`.

**Step 2 — Keep row containers fixed** (`updateRowLetters`):

- `mgr->paneTrans(0, 0)` always — **never shift `let_*`**.
- Rentable: hide `mpRowBloItemGfx`, letter-in-slot gfx; show `flame_*`, name, price.
- Locked: `restoreLetterIconsToPane()`, show envelope; `applyLetRowSubtreeRental(..., rental=false, listIconLeft=-1)`.
- `ensureRowWheelPic()` loads heap textures (used in step 3).

**Step 3 — Draw order in `draw()`** (order is critical):

```
applyListColumnLayout()
cacheRowLetterSlotBounds() per row
updateRowLetters()              // setup only; no rental subtree hide here

hideRentableCenterBloIcons()    // hide left + center BLO duplicates BEFORE menu draw
mpMenuScreen->draw()            // native BLO (no rentable icon duplicates)
drawRowListText()
drawRowWheelIcons()             // heap icons at mail column
```

### `hideRentableCenterBloIcons`

For purchasable rows only, walks `let_*` via `applyLetRowSubtreeRental(..., rental=true, listIconLeft)`:

- Hides letter-in-slot panes (envelope TIMG in icon slot).
- Hides **small** PIC/WIN in the mail column (`width < 120px`, overlaps icon slot) **and** centered duplicates past `listIconLeft + kRowLetterIconBoxMaxW`.
- **Never** hides wide panes — preserves `flame_*`.

### `drawRowWheelIcons`

Uses `calcRowHeapIconDrawPos`:

- **X** = `listIconLeft`
- **Y** = `flame_*` top; white box height = row height; wheel icon centered inside (`drawY = slotY + 0.5×(slotH−drawH)`)
- **Heap `J2DPicture` must use `J2DBasePosition_0` (top-left)** — `J2DBasePosition_4` applies anchor offsets from the texture’s native size (~160×174), not the ~25px draw size, so Y tweaks in `calcRowHeapIconDrawPos` had no visible effect
- White box: shared `mpItemBoxPic` (`tt_do_icon7_160_174.bti`)
- Item icon: `mpRowItemPic[row]` with wheel sizing from `itemicon.arc`
- Heap draw uses plain `0..640` ortho (widescreen fix in `9e191bc8aa`) — do not revert to `gInf` ortho for this pass

### `walkLetRowSubtree` selection rule (rentable + `listIconLeft`)

```cpp
const bool centerItemGfx =
    b.valid && b.w < 120.0f && b.x > mailColRight - 4.0f;
```

Wide panes (`flame_*`) fail the width check; centered item PICs pass it.

---

## Key constants

| Constant | Value | Purpose |
|----------|-------|---------|
| `kListColumnRatio` | 0.47 | List vs parchment split |
| `kRowLetterIconBoxMaxW` | 34px | Mail/icon column width |
| `kRowFrameInsetX` | 6px | Inner padding from `flame_*` left edge |
| `kListIconScissorPad` | 48px | Scissor extends left so heap icons aren't clipped |
| `kRowNameFontScale` | 0.88 | Name text scale |

---

## Header additions

- `mpRowBloItemGfx[6]` — sibling item gfx under `let_*` (separate from envelope `mpRowLetterGfx`); always hidden for rentable rows.
- `hideRentableCenterBloIcons(int visCount, f32 listIconLeft)` — post-menu-draw BLO cleanup.
- `mRowLetterInitCenterX[6]` — stored at create; **not used for `paneTrans`** (reference only).

---

## Rules for future edits

1. **Never `paneTrans` `let_*`** — moves `flame_*` off-screen.
2. **Never hide by `fenu_t*` name overlap** — spans full row.
3. **Never hide all `let_*` children** — `flame_*` is in that subtree.
4. **Hide rentable BLO icons before `mpMenuScreen->draw()`** — post-draw hide leaves native art on screen for the current frame; hide left mail-column + center duplicates in `updateRowLetters` / pre-draw pass.
5. **Align heap draw to envelope X**, not `flame_*` alone.
6. **Do not touch** parchment, footer, title, `applyDescColumnShift` unless explicitly requested.
7. **Do not hide** `fenu_t6`–`fenu_t11` or footer panes (see [albw-port.md](albw-port.md) maintainer notes).

---

## Debug

With shop open, `albw_shop_debug.txt` may log per-row `slotXYWH`, `purch`, `frameVis`, `ensure` (wheel pic load). Useful when viewport is all rentable rows (`gfx=0` on locked envelope — fallbacks in `calcShopListIconLeft` handle this).

---

## Core lesson

Alignment required knowing **which layer draws what**:

- Mail icons = BLO envelope (left, correct natively)
- Rentable icons = BLO centered under `let_*` (wrong) + heap overlay at mail X (correct)
- `flame_*` = child of `let_*` (must never move or blanket-hide)

**Winning pattern:** leave `let_*` / `flame_*` in place, hide only centered duplicate icon PICs after menu draw, draw correct art on heap at measured mail column X.

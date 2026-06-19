# LoP HUD v3 — close the gap vs mockup

**For:** HUD implementation agent  
**Visual target:** [`docs/mockups/dusklight-hud-lop-layout-v3-shield-icons-mockup.png`](mockups/dusklight-hud-lop-layout-v3-shield-icons-mockup.png) (in repo — agents: Read this path)  
**Setting:** `game.lopHud` (`dLopHudOn()` in `d_meter2_draw.cpp`)  
**Prior brief:** `docs/albw-hud-lop-layout-brief.md`

Phase 1 layout code is **partially landed**. This doc maps **what exists → what the v3 screenshot still needs**.

---

## Progress log (updated 2026-06-18 — Claude)

**Landed & verified in-game** (all gated behind `game.lopHud`, default Off; toggling Off fully restores vanilla):

| Area | What | Where |
|------|------|-------|
| Toggle | `Lies of Link HUD` bool under ALBW Visuals, default Off | `settings.h/.cpp`, `ui/settings.cpp` |
| Rupees → top-right | Self-calibrated Y lift (measure wallet-vs-heart row once, hold); anchor `TopRight` | `drawRupee()` |
| Button ring → bottom-right | Lift from cross row, `kLopButtonRaiseY=64`, anchor `BottomRight` | `exec()` |
| Ring trim → B-only | Sweep `cont_n` children hiding all but `bbtn_n`/`b_text_b`/`midona_n`; explicit hides of A/X/Y/Z circles, item icons, labels, X/Y items, A-context text; skip A & X/Y pikari glows | `draw()` |
| Spurs/wolf → bottom-left | `getShieldHudAnchorCenter()` re-pointed to (hidden) `mpButtonCrossParent` center + `(kLopSpurAnchorOffX=48, kLopSpurAnchorOffY=-10)`; row drawn **below** anchor | `getShieldHudAnchorCenter()`, `d_albw_shield.cpp` |
| Auto Shield Only | `dShield_effectiveParryMode()` forces `ShieldOnly` when LoP on | `d_albw_shield.cpp` |
| Hide d-pad cross + ITEMS/MAP | `mpButtonCrossParent->hide()` under LoP (Midna `midona_n` is separate, stays) | `drawButtonCross()` |
| Suppress top-left durability | Early-return in LoP (relocation in progress, §2) | `drawShieldDurabilityBelowAlbw()` |
| **Reversibility** | Root cause: `drawRupee`/`drawButtonCross`/button groups are layout-on-**change**, not per-frame. Fixes: force re-layout flags on toggle (`d_meter2.cpp` rupee + cross); invalidate `mDoStatus/mAStatus/mRStatus/mZStatus/mItemStatus`; `lopRestoreButtonRing()` shows all `cont_n` children + individually-hidden item icons/labels before the forced redraw re-applies visibility | `d_meter2.cpp`, `lopRestoreButtonRing()` |

**Tuning constants** (in `d_meter2_draw.cpp` near `dLopHudOn()`): `kLopSpurAnchorOffX/Y`, `kLopButtonRaiseY`. Bash row sizing in `computeBashHudLayout()` LoP branch.

**Remaining P0:** shield icon row final raise/shrink; §2 durability bar → bottom-left under the shield icons (in progress).
**Remaining P1:** top-left horizontal bars, rupee pill, vertical X/Y item belt.

---

## What Phase 1 already does

| Feature | File | Mechanism |
|---------|------|-----------|
| Layout toggle | `include/dusk/settings.h`, `d_meter2_draw.cpp` | `dLopHudOn()` ← `game.lopHud` |
| Rupees → top-right | `d_meter2_draw.cpp` ~2553 | Calibrated Y lift from `mpLifeParent`; `HudCorner::TopRight` |
| Button ring → bottom-right | `d_meter2_draw.cpp` ~661 | Lift from cross row; `kLopButtonRaiseY = 64`; anchor `BottomRight` |
| Bash/wolf anchor moved off wallet | `getShieldHudAnchorCenter()` ~1955 | Life center + `(kLopSpurAnchorOffX, kLopSpurAnchorOffY)` = `(24, 360)` |
| Hide ITEMS / MAP labels | `drawButtonCross()` ~3239 | `mpTextI` / `mpTextM` hidden when LoP on |
| Shield-icon bash charges | `d_albw_shield.cpp` | `applyParryIcons()` + `ParryIcons::ShieldOnly` draws `itemicon.arc` BTIs on `haku_b_n` pane |
| Shield durability (guard) | `drawShieldDurabilityBelowAlbw()` | Still draws **under ALBW top-left** — not v3 layout |

**Enable in settings:** Lies of Link HUD = ON. For shield icons (not spurs): **Parry Icons → Shield Only**.

---

## v3 mockup vs current build (gap list)

| Mockup element | Current build | Fix priority |
|----------------|---------------|--------------|
| Top-left: **hearts + green ALBW only** (2 rows) | Hearts + ALBW OK; **durability bar still appears under ALBW when guarding** | **P0** — hide or relocate durability in LoP |
| Top-left: no gem row | N/A (never implemented) | — |
| Top-right: rupee pill | Implemented | Tune Y if misaligned |
| Bottom-left: X/Y item squares + Midna | Vanilla cross + items (labels hidden) | **P1** — optional vertical item belt polish |
| Bottom-left: **Ordon shield icons** (not gold spurs) | Only if `parryIconsMode == ShieldOnly`; default is **Spur+Shield** | **P0** — auto ShieldOnly when LoP on |
| Bottom-left: **yellow durability bar under shields** | Durability draws top-left, not here | **P0** — relocate bar in LoP |
| Bottom-right: **B sword + wolf box only** | **Full A/B/X/Y/Z ring** still visible | **P0** — strip ring to B (+ wolves) |
| Bottom-right: no A/B/X/Y mini row | Mini prompts still part of ring | **P0** — hide with ring trim |

---

## P0 fixes (match v3 screenshot)

### 1. Force shield icons when LoP HUD is on

**Problem:** Default `parryIconsMode` is `SpurShield` (`settings.cpp` line 53). User sees gold spurs, not Ordon shields.

**Fix (pick one):**

**A — Auto-couple (recommended):** In `dShield_drawBashCharges()` / `applyParryIcons()`, when `dLopHudOn()` use `ParryIcons::ShieldOnly` regardless of user setting (or treat LoP as implicit Shield Only for bash row).

**B — Default change:** Set `parryIconsMode` default to `ShieldOnly` when `lopHud` default becomes true (weaker — two settings to flip).

**Existing art path (do not reimplement):**

```cpp
// d_albw_shield.cpp — Ordon tier
shieldIconBtiForTier(ShieldTier::Ordon) → "ttdelunotate_s3tc.bti"
// Shield Only: both filled/empty via sHudShieldOffPic + setBlackWhite white vs (105,105,105)
```

---

### 2. Relocate shield durability bar (yellow) to bottom-left

**Problem:** `drawShieldDurabilityBelowAlbw()` (~2188) always positions at `albwY + frameHeight + 8` — third row **top-left**. v3 puts a **thin yellow bar under the shield-icon row**.

**Fix:**

```cpp
void dMeter2Draw_c::drawShieldDurabilityBelowAlbw() {
    if (!dShield_shouldDrawDurabilityHud() || ...) return;

    f32 barX, barY;
    f32 widthScale = dShield_getDurabilityMeterWidthScale();

#if TARGET_PC
    if (dLopHudOn()) {
        // Anchor from bash row (same as getShieldHudAnchorCenter), draw BELOW icons
        Vec anchor;
        if (!getShieldHudAnchorCenter(&anchor)) return;
        const f32 barW = 120.0f * widthScale;  // tune vs icon row width
        const f32 barH = mpMagicBase->getInitSizeY() * g_drawHIO.mMagicMeterScale * 0.75f;
        barX = anchor.x - barW * 0.5f;
        barY = anchor.y + 28.0f;  // below 2-up shield icons — tune in-game
        applyMagicMeterLayoutTransient(32, fill32, barX, barY, widthScale, 0);
        // ... tier colors unchanged ...
        return;
    }
#endif
    // vanilla: under ALBW top-left (existing code)
}
```

**Also:** When LoP on and **not** guarding, hide the bar entirely (existing `shouldDrawDurabilityHud()` gate is fine).

---

### 3. Fix bash/shield-icon row position (below items, not above wallet)

**Problem:** `dShield_drawBashCharges()` sets:

```cpp
posY = rupeeCenter.y - layout.rowOffsetY;  // draws ABOVE anchor
```

With LoP anchor at life+(24,360), icons still float wrong relative to X/Y cross.

**Fix in `computeBashHudLayout()` and/or draw loop:**

```cpp
#if TARGET_PC
if (dLopHudOn()) {
    // Anchor to d-pad cross center (item cluster), not life row
    dMeter2Draw_c* draw = ...;
    if (draw && draw->getButtonCrossGlobalCenter(&anchor)) {
        layout.spacing = 36.0f;      // shield icons wider than spurs — tune
        layout.iconScale *= 1.15f;   // Ordon shield readable at bottom-left
        layout.rowOffsetY = -32.0f;  // NEGATIVE = draw below anchor (flip sign in draw)
    }
}
#endif
```

In draw: `posY = anchor.y + (dLopHudOn() ? layout.rowOffsetBelow : -layout.rowOffsetY)`.

**Better anchor:** Add `getBottomLeftHudAnchorCenter(Vec*)` on `dMeter2Draw_c`:

```cpp
// Prefer mpButtonCrossParent global center + offset right (past Midna)
Vec c = mpButtonCrossParent->getGlobalVtxCenter(false, 0);
c.x += 48.0f;   // right of Midna / below Y slot
c.y -= 24.0f;   // below item squares
```

Replace life-based `kLopSpurAnchorOffX/Y` once cross anchor is stable.

---

### 4. Strip bottom-right button ring to B + wolves only

**Problem:** `exec()` moves the **entire** `mpButtonParent` (A/B/X/Y/Z + vine ring) to bottom-right. Mockup shows **only B sword panel + wolf row**.

**Fix in `dMeter2Draw_c::draw()` or `exec()` when `dLopHudOn()`:**

Hide panes (keep `mpBTextB` + sword item visible):

| Hide | Pane tags (examples) |
|------|----------------------|
| A button cluster | `cont_at`, `cont_at1`–`4` |
| X / Y item wheel on ring | `cont_xt*`, `cont_yt*` |
| Z / R | `cont_zt*`, `cont_rt` |
| Decorative vines | `ju_ring*`, `ring_n` (already hides `ju_ring5` at init) |
| Action text ("Put away") | `mpAText*` when not context-needed |

**Scale:** When LoP on, multiply button parent scale by **~0.55** (mockup B panel is compact):

```cpp
const f32 lopScaleMul = lopButtons ? 0.55f : 1.0f;
mpButtonParent->scale(mainButtonsScale * lopScaleMul, ...);
```

**Position:** After hide, nudge `paneTrans` so B box aligns with mockup (bottom-right, above wolf HUD). Tune `kLopButtonRaiseY` (currently `64`) if clipped.

**Wolf icons:** `dAlbwWolfChargeHud_draw()` already mirrors bash layout — will follow once bottom-left anchor is fixed.

---

### 5. Suppress top-left durability row in LoP (until relocated)

Quick guard at top of `drawShieldDurabilityBelowAlbw()`:

```cpp
#if TARGET_PC
if (dLopHudOn() && !sLopDurabilityRelocated) {
    // skip top-left draw until bottom-left path (§2) is implemented
    return;
}
#endif
```

Remove guard once §2 lands.

---

## P1 polish (optional after P0)

### Top-left bar proportions

Mockup uses long thin bars (~30% screen width). Vanilla hearts + kantera frames are shorter.

- Add LoP branch in `drawLife()`: optional horizontal stretch `mLifeRowStretch = 1.35f` on heart parts only.
- In `drawMagic()` / `moveKantera()` when LoP: bump `mMagicMeterScale` width × **1.25**, height × **0.8** so green ALBW sits flush under hearts.

Constants live in HIO or next to `kLopSpurAnchorOffX`.

### Rupee pill background

Mockup shows dark rounded rect behind rupee. Vanilla is icon + digits only. Optional: draw `tt_block8x8` tinted rect behind `mpRupeeKeyParent` when LoP (same technique as boss bar).

### Item belt vertical stack

Mockup X over Y in squares. Currently X/Y orbit the button ring. After ring trim, expose `mpItemXY[0/1]` at fixed offsets from `mpButtonCrossParent` (+8, +38) in LoP mode.

---

## Tuning constants cheat sheet (start here, iterate in Ordon)

| Constant | Location | v3 starting value | Notes |
|----------|----------|-------------------|-------|
| `kLopSpurAnchorOffX` | `d_meter2_draw.cpp:109` | **48** | Move right of Midna; replace with cross anchor when ready |
| `kLopSpurAnchorOffY` | `d_meter2_draw.cpp:110` | **340–380** | Below item cluster |
| `kLopButtonRaiseY` | `d_meter2_draw.cpp:115` | **64** | ↑ if B panel clips bottom |
| Bash `spacing` (LoP) | `computeBashHudLayout()` | **36–40** | Ordon shield wider than spur |
| Bash `iconScale` mul (LoP) | `computeBashHudLayout()` | **1.1–1.2** | |
| Durability bar Y offset | `drawShieldDurabilityBelowAlbw` LoP branch | **+28** below icon row | Yellow/tan tier-0 colors already correct |
| Button ring scale mul | `exec()` LoP branch | **0.55** | |

---

## Draw order (keep)

```cpp
// dMeter2Draw_c::draw() tail — do not reorder
dAlbwWolfChargeHud_draw();
dShield_drawBashCharges();
dAlbwRupeePopup_draw();
dAlbwBossHpHud_draw();
```

Relocated durability should draw **before** or **with** bash charges so icons sit on top of the bar track.

---

## Test matrix

1. **Ordon field**, LoP ON, Shield Only (or auto), manual shield held → 2 Ordon shield icons bottom-left, white + grey, yellow bar under them while guarding.
2. **No third bar top-left** when LoP on (durability only bottom-left when guarding).
3. **Top-right** rupees only — no button wheel.
4. **Bottom-right** B sword visible; no A/X/Y/Z vine ring; wolf icons when wolf combat enabled.
5. **16:9 + 21:9** — anchors stay on corners (`dAnchorHudScale`).
6. **LoP OFF** — full vanilla layout restored; spur mode respected.

---

## Files to touch (summary)

| File | Changes |
|------|---------|
| `src/d/d_meter2_draw.cpp` | Cross anchor getter; LoP durability relocate; button ring hide/scale; optional life/meter stretch |
| `src/d/d_albw_shield.cpp` | LoP layout in `computeBashHudLayout()`; flip Y offset; auto ShieldOnly when LoP |
| `src/d/d_albw_wolf_charge_hud.cpp` | Same anchor as bash row |
| `src/dusk/settings.cpp` | Optional: document LoP + Shield Only coupling in help text |
| `docs/albw-hud-lop-layout-brief.md` | Link to this v3 doc |

**Do not change:** HP math, bash charge logic, `dAlbwHP_getLockonDisplayHp`, visibility gates (`shouldShowBashHud`, linger frames).

# Dusklight HUD — Lies of P layout implementation brief

**For:** HUD / meter implementation agent  
**Target:** `TARGET_PC` only (Dusklight / ALBW port)  
**Reference mockups:**
- Concept (simplified bars): `assets/dusklight-hud-lop-layout-mockup.png`
- Vanilla icons: `assets/dusklight-hud-lop-layout-vanilla-mockup.png`

This brief describes how to recreate the LoP-inspired HUD **using existing TP BLO panes and ALBW draw paths**, not new art.

---

## Design goal

Reorganize vanilla Twilight Princess HUD widgets into a **Lies of P spatial layout**:

| Zone | LoP analogue | Dusklight source |
|------|--------------|------------------|
| Top-left stack | HP / Stamina / Resource bars | Hearts + ALBW meter + shield durability |
| Top-right | Ergo currency | Rupee counter |
| Bottom-left | Quick items + Legion bar | X/Y item slots + bash spur row + Midna |
| Bottom-right | Weapon panel + stance | B-button sword + wolf charge icons + compact prompts |

**Do not** replace textures with flat modern bars unless a follow-up art pass is requested. Reposition and rescale **existing panes**.

---

## Coordinate system

| Concept | Value |
|---------|-------|
| Design resolution | **640 × 480** (`FB_WIDTH`, `FB_HEIGHT`) |
| HUD ortho | `dComIfGp_getCurrentGrafPort()` after `setup2D()` |
| Widescreen | Left/right HUD uses `mDoGph_gInf_c::ScaleHUDXLeft()` / `ScaleHUDXRight()` in `d_meter_HIO.cpp` init |
| User scale | `dGetUserHudScale()` — `dusk::getSettings().game.hudScale` clamped **0.5–2.0** |
| Corner pinning | `dAnchorHudScale(pane, HudCorner, &x, &y, pull)` in `d_meter2_draw.cpp` |

All `paneTrans` targets below are **starting HIO offsets** in design space. Tune in-game via debug HIO sliders, then bake defaults into `d_meter_HIO.cpp`.

---

## Target layout (640 × 480 design space)

Percentages are for **16:9 widescreen** visual alignment (mockup reference).

```
┌─────────────────────────────────────────────────────────────┐
│ [♥♥♥]                    (green rupee 055)                │  ~4% from top
│ [ALBW green meter────────]                                  │
│ [shield durability───────]                                  │
│                                                             │
│                      (gameplay — keep clear)                │
│                                                             │
│ [Midna] [X slingshot]              [B sword panel]          │  ~82% from top
│         [Y boomerang]              [wolf wolf]              │
│         [spur spur spur spur]      [A B X Y mini]            │
└─────────────────────────────────────────────────────────────┘
```

### Recommended HIO defaults (Phase 1 tuning)

| Widget | HIO fields | Suggested start | Notes |
|--------|--------------|-----------------|-------|
| Hearts | `mLifeGaugePosX`, `mLifeGaugePosY`, `mLifeParentScale` | **(14, -178)**, scale **0.72** | Top-left; `dAnchorHudScale(..., TopLeft, pull=0.6)` already applied in `drawLife()` |
| ALBW stamina | `mLanternMeterPosX`, `mLanternMeterPosY`, `mMagicMeterScale` | **(14, -152)**, scale **(1.35, 0.75)** | PC draws via `drawMagic()` at lantern pos in `moveKantera()` |
| Shield durability | code offset in `drawShieldDurabilityBelowAlbw()` | **+22px below ALBW** base | Currently `albwY + mpMagicBase->getInitSizeY() * albwScaleY + 8` — increase gap to **22** for LoP stack |
| Rupees | `mRupeeKeyPosX`, `mRupeeKeyPosY`, `mRupeeKeyScale` | **(-14, -178)**, scale **0.95** | **Change anchor** from `BottomRight` → `TopRight` (see draw logic) |
| Button wheel | `mMainHUDButtonsPosX/Y`, `mMainHUDButtonsScale` | **(-18, 155)**, scale **0.55** | Move to bottom-right; hide A ring text clutter |
| X/Y items | item pane offsets inside button parent | Re-parent or offset from cross | See item belt section |
| D-pad cross | `mButtonCrossOFFPosX/Y`, `mButtonCrossScale` | **(14, 158)**, scale **0.85** | Bottom-left; hide MAP/ITEMS text labels |
| Bash spurs | `computeBashHudLayout()` anchor | Bottom-left cluster | Stop anchoring to rupee center |
| Wolf charges | `dAlbwWolfChargeHud_draw()` | Same row as spurs | Shares bash layout math |

Vertical spacing between top-left rows: **~26 design px** (mockup uses ~24–28 px at 1080p ≈ 2.4% screen height).

---

## Scaling rules (from existing code)

### Hearts (`drawLife`)
- File: `d_meter2_draw.cpp`
- Parent: `mpLifeParent` (`heart_n`)
- Scale: `mLifeParentScale * dGetUserHudScale()`
- **LoP adjustment:** reduce `mLifeParentScale` ~25% but increase horizontal spacing by editing heart part positions OR add a horizontal stretch factor (optional `mLifeRowStretch` HIO, default **1.4**) applied in `drawLife()` after `paneTrans`.

### ALBW meter (`drawMagic` slot 0)
- Files: `d_meter2.cpp` `moveKantera()`, `d_meter2_draw.cpp` `drawKanteraScreen(0)`
- Uses `zelda_game_image_kantera.blo` magic panes: `mpMagicBase`, `mpMagicMeter`, `mpMagicFrameL/R`
- Width scales with upgrades via `barMax` (32 × `sOilMaxVar / sOilBaseMax`)
- **LoP adjustment:** bump `mMagicMeterScale` width ~**1.35×**, height ~**0.75×** so bar reads longer/thinner. Apply in `drawMagic()` or add `mAlbwMeterWidthMul` HIO.

### Shield durability (`drawShieldDurabilityBelowAlbw`)
- File: `d_meter2_draw.cpp` ~2120
- Reuses magic meter widget transiently; tier colors in switch on `dShield_getDurabilityTierStyle()`
- Width: `dShield_getDurabilityMeterWidthScale()` (Ordon narrower than Hylian)
- **LoP adjustment:** position relative to **life stack anchor**, not dynamically under wherever ALBW landed, once stack positions are fixed:

```cpp
// Proposed: single stack anchor
struct HudStackAnchor { f32 x, y; };
static HudStackAnchor getTopLeftHudStackAnchor(); // life pos + scale-aware offset

void drawShieldDurabilityBelowAlbw() {
    const HudStackAnchor anchor = getTopLeftHudStackAnchor();
    const f32 shieldY = anchor.y - kShieldRowOffsetY; // e.g. 48 design px below hearts row
    // ...
}
```

### Bash spur row (`dShield_drawBashCharges`)
- File: `d_albw_shield.cpp`
- Layout: `computeBashHudLayout()`:
  - `iconScale = mSpurIconScale * (rupeeSize / hakushaBase)`
  - `spacing = rupeeSize * 1.08`
  - `rowOffsetY = rupeeSize * 1.12` (above anchor)
- Position: centered on `getShieldHudAnchorCenter()` → `getRupeeAnchorCenter()`
- **LoP adjustment:** add `dMeter2Draw_c::getBottomLeftHudAnchorCenter(Vec*)` returning center of repositioned item belt. Replace rupee anchor in both `computeBashHudLayout()` and `dAlbwWolfChargeHud_draw()`.

Suggested bottom-left spur row:
- `posX = anchor.x + spacing * 0.5`
- `posY = anchor.y + rupeeSize * 0.4` (below item slots, not above)

### Rupees (`drawRupee`)
- File: `d_meter2_draw.cpp` ~2471
- Parent: `mpRupeeKeyParent` (`rupi_n` cluster)
- **Critical change:** line ~2479 uses `HudCorner::BottomRight`. For LoP layout use **`HudCorner::TopRight`** and new HIO defaults above.

### X/Y item belt
- Panes: `mpItemXY[0/1]` drawn in `dMeter2Draw_c::draw()` ~729
- Currently orbit the top-right button ring
- **LoP adjustment:** either:
  - **A (minimal):** move `mpButtonParent` to bottom-left and rotate item offsets 90° (HIO-only), or
  - **B (clean):** new `drawItemBeltBottomLeft()` that draws `mpItemXY` at fixed `(14, 168)` / `(14, 198)` with scale `0.9 * dGetUserHudScale()`, hide ring decorative vines (`ju_ring*`, `ring_n` panes)

### Button prompts
- Hide or shrink: `mpButtonParent` decorative ring, "Put away" text when not needed
- Keep `mpBTextB` sword icon in bottom-right weapon panel area
- Touch controls path already hides `mpButtonParent` when enabled — no change needed

---

## Draw-order changes

Current PC tail in `dMeter2Draw_c::draw()` (~874):

```cpp
dAlbwWolfChargeHud_draw();
dShield_drawBashCharges();
dAlbwRupeePopup_draw();
dAlbwBossHpHud_draw();
```

**Keep this order** after repositioning anchors. Spur/wolf rows should draw **above** item belt but **below** popups.

Suggested full draw sequence:
1. `mpScreen` base (hidden relocated panes)
2. Top-left: life → ALBW (`drawKanteraScreen(0)`) → durability
3. Top-right: rupees
4. Bottom-left: d-pad cross (minimal) + item belt
5. Bottom-right: compact buttons + B sword
6. ALBW overlays: wolf, bash, rupee popup, boss HP

---

## Files to modify (checklist)

| Priority | File | Change |
|----------|------|--------|
| P0 | `src/d/d_meter_HIO.cpp` | New default positions/scales (table above) |
| P0 | `src/d/d_meter2_draw.cpp` | Rupee anchor `TopRight`; optional `getTopLeftHudStackAnchor()` / `getBottomLeftHudAnchorCenter()` |
| P0 | `src/d/d_albw_shield.cpp` | Retarget `computeBashHudLayout()` + `dShield_drawBashCharges()` anchor |
| P0 | `src/d/d_albw_wolf_charge_hud.cpp` | Same bottom-left anchor as bash row |
| P1 | `src/d/d_meter2_draw.cpp` | `drawShieldDurabilityBelowAlbw()` stack under ALBW with fixed gap |
| P1 | `src/d/d_meter2_draw.cpp` | Hide MAP/ITEMS text; shrink button ring vines |
| P2 | `src/d/d_meter2_draw.cpp` | Optional heart-row horizontal stretch HIO |
| P2 | `include/d/d_meter2_draw.h` | Declare new anchor getters if added |

---

## Visibility gates (do not break)

| Element | Gate |
|---------|------|
| ALBW meter | `mMeterAlphaRate[0]`, pause/heap lock != 6 |
| Shield durability | `dShield_shouldDrawDurabilityHud()`, `mMeterAlphaRate[0] > 0` |
| Bash spurs | `shouldShowBashHud()`, manual shield / linger — see `d_albw_shield.cpp` |
| Wolf icons | wolf form + `dAlbwWolfCombat_isEnabled()`, supersedes spurs |
| Rupees | existing rupee alpha animation |

---

## Widescreen / aspect regression tests

1. **4:3** — top-left stack not clipped; rupees stay in top-right safe zone  
2. **16:9** — match mockup; no overlap with button wheel  
3. **21:9** — `ScaleHUDXLeft/Right` keeps corners pinned; spurs stay on item belt  
4. **`hudScale` 0.5 / 1.0 / 2.0** — `dAnchorHudScale` prevents corner drift  
5. **Horse ride** — bash HUD hidden; no spur/bottom-left clash with hakusha subcontent  
6. **Wolf form** — wolf icons replace spurs on same anchor  
7. **Shop / pause / cutscene** — all rows respect existing alpha/pause gates  

---

## Phase plan

### Phase 1 — Position-only (1 PR)
- HIO default retune + rupee `TopRight` anchor + bash/wolf anchor moved to bottom-left + hide ITEMS/MAP labels  
- No new textures; verify in Ordon field screenshot pose

### Phase 2 — Stack polish
- Fixed durability gap under ALBW; optional heart-row stretch; compact bottom-right button ring

### Phase 3 — Optional bar mode (out of scope unless requested)
- Convert hearts to horizontal bar (new pane or runtime stretch) — **not required** for LoP layout if heart row is repositioned

---

## Mockup → code quick reference

| Mockup element | Vanilla asset | BLO / code |
|----------------|---------------|------------|
| Red hearts row | 3 heart containers | `heart_n`, `hpb_XX`, `hear_XX` |
| Green bar | ALBW stamina | `zelda_game_image_kantera.blo` magic panes, slot 0 |
| Blue/brown bar | Shield durability | transient magic meter in `drawShieldDurabilityBelowAlbw()` |
| Green rupee + digits | Wallet | `rupi_n`, `mpRupeeTexture` |
| X/Y squares | Item icons | `mpItemXY`, itemicon BTIs |
| Gold spur dots | Bash charges | `zelda_game_image_hakusha_parts.blo` via `dShield` HUD |
| Wolf silhouettes | Wolf charges | `tt_wolf_icon_mini64.bti` in `msgres05.arc` |
| Midna head | Companion | `mpButtonMidona` / cross-left positioning |

---

## Out of scope

- Boss HP bar relocation (already separate `dAlbwBossHpHud_draw()`)
- Touch control overlay layout
- New BLO art or font changes
- Console (`!TARGET_PC`) builds

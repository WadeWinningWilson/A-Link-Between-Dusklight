# Boss HP HUD — layout & color tuning brief

**For:** HUD implementation agent (Claude)  
**File:** `src/d/d_albw_boss_hp_hud.cpp`  
**Setting gate:** `dusk::getSettings().game.bossHealthBars`  
**HP source (do not change):** `dAlbwHP_getLockonDisplayHp(boss)` — single source of truth  

**Reference mockup (in repo):**
- [`docs/mockups/dusklight-boss-hp-bar-tuning-mockup.png`](mockups/dusklight-boss-hp-bar-tuning-mockup.png) — Armogohma arena; tuned bar/name only; agents should **Read** this file path

**External references (optional):** LoP Parade Master bar aesthetic — thin wide crimson fill, translucent charcoal empty track, name 2–4 px above bar.

**Status:** Basic reactive bar works. This pass is **visual polish only** — geometry constants, colors, alpha, name scale/gap. Do not duplicate HP math.

---

## Problem summary (from playtest screenshot)

| Issue | Cause in code |
|-------|----------------|
| Boss name dominates the bar | `kNameFontFrac = 0.050f` → **5% of screen height** (~54 px at 1080p) |
| Lost-health track reads as brown/maroon, not “empty” | `drawRect(..., 70, 26, 26)` fully opaque; no alpha |
| Fill red is slightly orange/washed | `(200, 36, 36)` — high green/blue channel |
| Bar slightly thick vs LoP | `kBarHeightFrac = 0.022f` |
| Name feels detached | `kNameGap = 5.0f` is OK in px but **font is so large** the visual gap reads wide |

---

## Target look (blend LoP + current TP bar)

LoP reference drives **proportions and translucency**. Keep TP-style **white name + black outline** (already correct).

```
        Armogohma          ← small, centred, ~2–4 px above bar
  ████████░░░░░░░░░░░░     ← deep crimson fill | translucent charcoal empty
  ^~58% screen width, ~1.6% screen height, bottom margin ≈ one bar height
```

---

## Constants to change (lines 44–51)

Replace the current block with these **recommended Phase 1 values**:

```cpp
// --- Geometry (fractions of ortho extent) ---
static constexpr f32 kBarWidthFrac  = 0.58f;   // was 0.52 — LoP ~60%, current shot ~55–60%
static constexpr f32 kBarHeightFrac = 0.016f;  // was 0.022 — thinner bar
static constexpr f32 kBarTopFrac    = 0.908f;  // was 0.865 — sit lower; bottom margin ≈ barH
static constexpr f32 kBarBorder     = 1.0f;    // was 2.0 — LoP 1 px outline

// --- Boss name ---
static constexpr f32 kNameFontFrac  = 0.028f;  // was 0.050 — ~30 px @ 1080p; ~⅕ bar width feel
static constexpr f32 kNameGap       = 3.0f;    // was 5.0 — 2–4 px “attached” to bar (LoP)
static constexpr f32 kNameOutline   = 1.0f;    // was 2.0 — thinner stroke at smaller type
```

### Geometry math (for verification)

All positions use widescreen ortho from `mDoGph_gInf_c::getMinXF/YF`, `getWidthF()`, `getHeightF()`:

```cpp
barW = scrW * kBarWidthFrac;
barH = scrH * kBarHeightFrac;
barX = minX + (scrW - barW) * 0.5f;
barY = minY + scrH * kBarTopFrac;
fontSz = scrH * kNameFontFrac;
nameY  = barY - kNameGap - fontSz;   // top of text box
```

**Bottom margin check:** bar bottom = `barY + barH`. Target ≈ `minY + scrH - barH` (one bar-height pad).  
With `kBarTopFrac = 0.908` and `kBarHeightFrac = 0.016` → bottom at **92.4%** of screen height → ~**7.6%** margin from bottom (includes bar height ≈ 1.6% → ~**6%** clear pad). Tune `kBarTopFrac` ±0.005 if needed.

**Name width:** name draws in box width `barW` with `HBIND_CENTER` — do **not** widen text box beyond bar; that keeps long names aligned to bar edges.

---

## Color & alpha targets

### Required code change: alpha in `drawRect`

Current `drawRect` forces alpha **255**. Lost-health track **must** be translucent.

```cpp
static void drawRect(f32 x, f32 y, f32 w, f32 h, u8 r, u8 g, u8 b, u8 a = 255) {
    if (w <= 0.0f || h <= 0.0f) return;
    sBlock->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(r, g, b, a));
    sBlock->setAlpha(a);
    sBlock->draw(x, y, w, h, false, false, false);
}
```

### Recommended palette (Phase 1)

| Layer | RGB | Alpha | Hex / notes | Reference |
|-------|-----|-------|-------------|-----------|
| **Border** | `(32, 32, 36)` | 255 | `#202024` — near-black 1 px frame | LoP thin outline (use dark, not white, on TP bright floors) |
| **Lost health (track)** | `(18, 18, 22)` | **168** | ~**66%** opaque charcoal | LoP “see floor through empty bar”; current shot reads 70–80% |
| **Current health (fill)** | `(176, 8, 8)` | 255 | `#B00808` deep crimson | LoP `#B00000` family — **not** neon `#FF0000` |

Draw order (unchanged):

```cpp
// 1) Border
drawRect(barX - kBarBorder, barY - kBarBorder, barW + kBarBorder * 2, barH + kBarBorder * 2,
         32, 32, 36, 255);
// 2) Full-width empty track (translucent)
drawRect(barX, barY, barW, barH, 18, 18, 22, 168);
// 3) Left fill only
drawRect(barX, barY, barW * ratio, barH, 176, 8, 8, 255);
```

### Optional A/B variants (HIO or constexpr toggles later)

| Variant | Fill RGB | Track RGB | Track α | When to use |
|---------|----------|-----------|---------|-------------|
| **LoP crimson (default)** | `(176, 8, 8)` | `(18, 18, 22)` | 168 | Match Parade Master reference |
| **TP vivid** | `(210, 24, 20)` | `(28, 20, 18)` | 180 | If crimson reads too dull on bright arenas |
| **LoP darker** | `(152, 0, 0)` | `(12, 12, 16)` | 150 | Rain/night bosses |

**Avoid:** `(200, 36, 36)` fill + `(70, 26, 26)` track — reads as **brown maroon**, not LoP empty bar (current bug).

---

## Boss name sizing guide

LoP name is **small relative to bar width** — roughly **15–22%** of bar width in cap height, not 40%+.

| Resolution | `kNameFontFrac=0.050` (old) | `kNameFontFrac=0.028` (new) | Bar width @ 0.58 |
|------------|----------------------------|----------------------------|------------------|
| 1080p (H=1080) | ~54 px | ~30 px | ~1253 px |
| 720p (H=720) | ~36 px | ~20 px | ~835 px |

**Rule of thumb:** `fontSz ≈ barW * 0.024` → `kNameFontFrac ≈ kBarWidthFrac * 0.024 / aspect`  
At 16:9, `0.58 * 0.024 ≈ 0.014` minimum; **0.028** adds legibility for TP msg font. If still large, try **0.024**.

**Name-to-bar gap:** LoP uses **2–4 px**. Formula:

```cpp
nameY = barY - kNameGap - fontSz;  // kNameGap = 3.0f
```

Do **not** scale gap with screen height — keep **3 px ortho units** so name stays “attached” at all resolutions.

**Outline:** at `fontSz ≈ 30`, use `kNameOutline = 1.0f`. Restore `2.0f` only if `kNameFontFrac ≥ 0.040`.

---

## Widescreen / draw-context notes

- Bar already uses full ortho extent (`setOrtho(minX, minY, scrW, scrH)`) — **no 640×480 clamp**. Fractions are correct for 16:9 and 21:9.
- Draw tick lives in `dMeter2Draw_c::draw()` after bash/wolf HUD — keep order so bar renders on top of gameplay, under pause if gated elsewhere.
- `dusk::frame_interp::set_ui_tick_pending(true)` — leave as-is for UI interpolation.

---

## Implementation checklist

- [ ] Update constexpr geometry (table above)
- [ ] Add alpha parameter to `drawRect`
- [ ] Apply new border / track / fill colors
- [ ] Reduce `kNameOutline` to 1.0
- [ ] Playtest Armogohma @ 1080p and 1440p — name must not overlap bar; empty track must show floor through it
- [ ] Compare side-by-side with LoP reference screenshot
- [ ] Do **not** change `dAlbwHP_getLockonDisplayHp()` or ratio math

---

## Playtest acceptance

1. **Name scale:** “Armogohma” cap height ≈ **¼–⅓** of bar height (not 2× bar height as now).
2. **Name gap:** gap between name baseline area and bar top ≈ **2–4 px** at 1080p.
3. **Fill color:** deep crimson, clearly red, not orange-brown.
4. **Empty track:** dark grey-black, **floor texture faintly visible** through right portion.
5. **Bar width:** spans a little over half the screen; does not touch player HUD corners.
6. **Reactive:** arrow chip / sword hit still moves fill width via `ratio` (unchanged).

---

## Future (out of scope this pass)

- HIO sliders for colors/geometry (mirror `g_drawHIO` pattern)
- Generalise `bossDisplayName()` beyond `fpcNm_B_GM_e`
- LoP-style damage number at bar right (`1523` in reference)
- Serif font swap (TP msg font is fine for Phase 1)

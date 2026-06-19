# Armogohma boss bar — composite meter spec (suggested edits)

**Status:** Design / implementation plan — **no code yet**  
**Scope:** Boss Health Bars + Armogohma only (`B_GM` + `E_GM` TYPE_GOMA)  
**Related:** [albw-boss-hp-hud-tuning-brief.md](albw-boss-hp-hud-tuning-brief.md), [boss-fights-handoff.md](boss-fights-handoff.md)

---

## Goal

One bottom-centre **“Armogohma”** bar that reflects **both** fight segments:

| Segment | Actor | Backend (not raw `health` alone) |
|---------|--------|----------------------------------|
| **First half** | `B_GM` (giant) | Boss Refinement pool `s_armogohmaMaxHp` + `B_GM.health` (or vanilla pool when Refinement off) |
| **Second half** | `E_GM` param 3 (`TYPE_GOMA`, tiny eye) | Hit counter `field_0xa74` (starts at 3, decrements on meaningful hits) |

The HUD must **not** hard-code phase math. It should consume a **normalized fill ratio** (0..1) and optional layout hints so changes to bar width, height, name scale, or colors in the tuning brief **do not require re-touching meter logic**.

---

## Design choice (locked for this spec)

**Composite single bar, unified fill ratio** — not two separate widgets, not two separate names.

- **One** track width (`barW`), **one** fill rect, **one** name (“Armogohma”).
- Query layer maps both phases into **`fillRatio ∈ [0, 1]`** where `1 = full`, `0 = dead/hidden`.
- **First half** of the ratio range (1.0 → 0.5): phase 1 refinement %.
- **Second half** (0.5 → 0.0): phase 2 hit counter.

Alternative (full-width reset at transition) is noted in §8; this spec uses **continuous composite** to avoid a stuck bar at ~35% and a jarring 35%→100% jump.

---

## Architecture principle: three layers

Keep responsibilities split so art tuning and fight logic evolve independently.

```
┌─────────────────────────────────────────────────────────────┐
│  Layer 1 — Fight / actor state (d_albw_boss.cpp, d_a_*.cpp) │
│  B_GM pool, mIsDisappear, E_GM field_0xa74, visibility gates │
├─────────────────────────────────────────────────────────────┤
│  Layer 2 — Meter query (d_albw_boss.cpp + d_albw_hp_mult)   │
│  dAlbwBoss_armogohmaQueryHealthBar() → normalized fillRatio │
│  dAlbwHP_getLockonDisplayHp() for lock-on overlay parity     │
├─────────────────────────────────────────────────────────────┤
│  Layer 3 — Draw only (d_albw_boss_hp_hud.cpp)               │
│  Layout struct from screen fractions → rects + name          │
│  fillWidth = barW * fillRatio  (no HP math here)             │
└─────────────────────────────────────────────────────────────┘
```

**Rule:** `d_albw_boss_hp_hud.cpp` never reads `B_GM.health`, `field_0xa74`, or `s_armogohmaMaxHp` directly after this work.

---

## Layer 1 — Fight facts (unchanged vanilla + Refinement)

Reference only — implementers should not duplicate this table in the HUD.

### Phase 1 (`B_GM`)

- Win segment ends on **3rd Dominion Rod statue hit** → demo 30 → eventually `mIsDisappear = true`.
- **Boss Refinement ON:** `armogohmaSetHp()` clamps **`health ≥ 1`**; rod snaps at hits 1/2 → **60% / 35%**; bow chips **−4%** of `s_armogohmaMaxHp`.
- **Boss Refinement OFF:** vanilla `health` (create default **500**); arrow/core damage applies normally.
- Actor **never deleted** at transition; stays as demo controller.

### Phase 2 (`E_GM`, `fopAcM_GetParam == 3`, `TYPE_GOMA`)

- Spawned in `B_GM` demo case 32 (`params = 3`).
- **`health = 1`, `field_0x560 = 1`** — display useless for bar.
- **`field_0xa74 = 3`** at create; `core_damage()` / `core_pechanko_damage()` decrement; **0 → death**.
- Parent `B_GM` sets `mDemoMode` 40/50 when `CheckDeath()` fires; bar should hide via query layer.

### Visibility gates (existing + tighten)

| Condition | Bar |
|-----------|-----|
| `!bossHealthBars` setting | Hidden |
| `s_armogohmaBarSuppressed` (post-victory) | Hidden |
| `B_GM` missing | Hidden |
| `B_GM.mDemoMode >= 50` | Hidden |
| Transition demos 30–33, eye not spawned yet | Hidden (keep current behaviour) |
| Phase 2 active, eye alive | Visible, second-half ratio |

---

## Layer 2 — Suggested API / struct edits

### 2a. Extend `dAlbwBoss_ArmogohmaBarState` (`include/d/d_albw_boss.h`)

Replace “raw current/max per phase” as the HUD’s primary input with **normalized draw fields**:

```cpp
enum dAlbwBoss_ArmogohmaSegment : u8 {
    dAlbwBoss_Armogohma_HIDDEN = 0,
    dAlbwBoss_Armogohma_PHASE1,   // giant only
    dAlbwBoss_Armogohma_PHASE2,   // tiny eye only
    dAlbwBoss_Armogohma_TRANSITION // optional: cutscene, bar off
};

struct dAlbwBoss_ArmogohmaBarState {
    bool visible;
    dAlbwBoss_ArmogohmaSegment segment;

    // Single value the HUD uses for fill width (0..1).
    f32 fillRatio;

    // Debug / lock-on overlay (informational — not for HUD ratio math).
    s16 segmentCurrent;
    s16 segmentMax;
    s16 actorHealth;
    s16 actorHealthMax;
};
```

**Deprecate for HUD:** `phase`, `current`, `max` as the driver for `drawRect` width — keep internally or for F5 overlay only.

### 2b. Fill ratio mapping (`dAlbwBoss_armogohmaQueryHealthBar`)

**Constants** (boss module, not HUD):

```cpp
static constexpr f32 kAlbwArmogohmaPhase1FillMax = 1.0f;  // top of bar
static constexpr f32 kAlbwArmogohmaPhase1FillMin = 0.5f;  // end of giant segment
static constexpr f32 kAlbwArmogohmaPhase2FillMax = 0.5f;
static constexpr f32 kAlbwArmogohmaPhase2FillMin = 0.0f;
static constexpr int kAlbwArmogohmaPhase2HitMax = 3;       // matches l_damage_count[TYPE_GOMA]
```

**Phase 1** (target = `B_GM`, `mIsDisappear == 0`, existing resolve checks):

```
phase1Progress = currentRefinement / maxRefinement   // via dAlbwBoss_armogohmaFillDisplayHp
fillRatio = lerp(kPhase1FillMax, kPhase1FillMin, 1.0f - phase1Progress)
           = kPhase1FillMin + (kPhase1FillMax - kPhase1FillMin) * phase1Progress
```

So full refinement pool → **fillRatio = 1.0**; empty refinement pool → **0.5** (not 0 — second half reserved).

**Phase 1 end at 3rd rod:** refinement may sit at **~35%** → **fillRatio ≈ 0.675**, not 0.5. That is acceptable: the “first half” is **ratio space**, not strict 50% of fight time. Optional polish (later): on `mIsDisappear` snap `fillRatio` to exactly **0.5**.

**Phase 2** (target = `E_GM` param 3, `mIsDisappear != 0`, `mDemoMode < 40`):

```
hitsRemaining = field_0xa74   // read from daE_GM_c, clamp 0..3
hitProgress = hitsRemaining / kPhase2HitMax
fillRatio = lerp(kPhase2FillMin, kPhase2FillMax, hitProgress)
           = kPhase2FillMax * hitProgress
```

- 3 hits left → **0.5** (bar half full — continuous from phase 1 end if snapped).
- 0 hits left → **0.0** → query sets `visible = false`.

**Boss Refinement OFF, phase 1:** use `dAlbwBoss_armogohmaFillDisplayHp` without refinement pool (same lerp formula on raw max).

### 2c. Parity — lock-on overlay (`d_albw_hp_mult.cpp`)

Add Armogohma branch when lock-on target is `B_GM` **or** phase-2 `E_GM` TYPE_GOMA:

- Set `customMeter = true`.
- Expose same `segmentCurrent` / `segmentMax` as query (refinement % or hits left).
- Optionally add `dAlbwHP_ArmogohmaPhase` enum (mirror Darknut pattern) instead of overloading `darknutPhase`.

**Do not** teach the overlay about `fillRatio`; it can show `segmentCurrent / segmentMax` text while HUD uses `fillRatio`.

### 2d. Accessor for phase-2 hits

Add a small read-only helper (pick one home):

- `dAlbwBoss_armogohmaGetPhase2HitsRemaining()` in `d_albw_boss.cpp`, **or**
- `daE_GM_c::albwGetBossHitRemaining()` behind `#if TARGET_PC`

Avoid duplicating `field_0xa74` reads in HUD and overlay.

### 2e. Early-out fix in HUD draw

Current gate hides bar when `current <= 0`:

```cpp
if (!bar.visible || bar.max <= 0 || bar.current <= 0) return;
```

**Change to:**

```cpp
if (!bar.visible || bar.fillRatio <= 0.0f) return;
```

Otherwise phase 2 can never show (1/1 HP bug) and composite ratio 0.5 would fail if `current` were ever 0.

---

## Layer 3 — Draw layer edits (layout-decoupled)

### 3a. Introduce `BossHpHudLayout` (local struct in `d_albw_boss_hp_hud.cpp`)

Compute **once per frame** from screen size + **existing** tuning constants:

```cpp
struct BossHpHudLayout {
    f32 barX, barY, barW, barH;
    f32 nameY, fontSz;
    f32 border;
};

static BossHpHudLayout computeLayout(f32 minX, f32 minY, f32 scrW, f32 scrH) {
    BossHpHudLayout L{};
    L.barW = scrW * kBarWidthFrac;
    L.barH = scrH * kBarHeightFrac;
    L.barX = minX + (scrW - L.barW) * 0.5f;
    L.barY = minY + scrH * kBarTopFrac;
    L.fontSz = scrH * kNameFontFrac;
    L.nameY = L.barY - kNameGap - L.fontSz;
    L.border = kBarBorder;
    return L;
}
```

**Any tuning brief change** (`kBarWidthFrac`, `kBarHeightFrac`, `kBarTopFrac`, `kNameFontFrac`, `kNameGap`, `kNameOutline`) only touches `computeLayout()` and draw helpers — **not** `d_albw_boss.cpp`.

### 3b. Optional: derive name size from bar width (future-proof)

Document in tuning brief — when art wants name tied to bar width:

```cpp
// Alternative / clamp: keep name readable at ultrawide
fontSz = std::max(scrH * kNameFontFrac, barW * kNameToBarWidthRatio);
// e.g. kNameToBarWidthRatio = 0.024f per albw-boss-hp-hud-tuning-brief.md
```

HUD spec: **one place** for this formula (`computeLayout`), not scattered in draw.

### 3c. Draw pass uses only `layout` + `fillRatio`

```cpp
const BossHpHudLayout layout = computeLayout(minX, minY, scrW, scrH);
const f32 fillW = layout.barW * clamp(fillRatio, 0.f, 1.f);

drawBorder(layout);
drawTrack(layout);           // full barW
drawFill(layout, fillW);     // partial — independent of phase
drawName(layout, bossName);  // width = layout.barW, HBIND_CENTER
```

Colors / alpha stay as tuning brief (`drawRect` with alpha on track). **No phase branches in draw.**

### 3d. Name source

Keep **`bossDisplayName(fpcNm_B_GM_e)` → "Armogohma"** for both segments (one boss, two forms). Generalise later via profile arg when other bosses ship.

### 3e. Optional visual: segment divider (cosmetic, layout-safe)

If desired later, draw a **1 px vertical tick** at `barX + barW * 0.5f` using `layout` only — indicates phase boundary without second bar. Off by default; constant `kShowArmogohmaMidTick = false`.

---

## File-by-file edit checklist

| File | Edits |
|------|--------|
| `include/d/d_albw_boss.h` | Extend `dAlbwBoss_ArmogohmaBarState`; document `fillRatio` contract |
| `src/d/d_albw_boss.cpp` | Rewrite `dAlbwBoss_armogohmaQueryHealthBar()` fill mapping; read `field_0xa74` for phase 2; optional snap to 0.5 on `mIsDisappear`; keep `dAlbwBoss_armogohmaResolveBarTarget()` for actor lookup |
| `src/d/d_albw_hp_mult.cpp` | Armogohma `customMeter` in `dAlbwHP_getLockonDisplayHp()` for `E_GM` TYPE_GOMA + existing `B_GM` refinement path |
| `include/d/d_albw_hp_mult.h` | Optional `dAlbwHP_ArmogohmaPhase` enum |
| `src/d/d_albw_boss_hp_hud.cpp` | `computeLayout()`; draw from `fillRatio` only; fix visibility gate; remove direct `current/max` ratio |
| `docs/albw-boss-hp-hud-tuning-brief.md` | Add § “Meter vs layout”: HUD must not compute HP; list `fillRatio` as sole fill input |
| `docs/boss-fights-handoff.md` | Update § Armogohma HP bar to link this spec |

**Out of scope:** changing Refinement % gates, rod snap values, or vanilla `d_a_b_gm` demo flow.

---

## Behaviour matrix (acceptance tests)

Settings: **Boss Health Bars ON**.

| Scenario | Expected `fillRatio` (approx) | Visible |
|----------|-------------------------------|---------|
| Fight start, Refinement ON, full pool | 1.0 | Yes |
| After bow chips only | 1.0 → 0.5+ (smooth) | Yes |
| After rod 1 / 2 snaps (60% / 35%) | ~0.8 / ~0.675 | Yes |
| 3rd rod, during demo before eye spawn | — | **No** |
| Phase 2 start, 3 hits left | 0.5 | Yes |
| Phase 2, 2 / 1 hits left | ~0.33 / ~0.17 | Yes |
| Tiny dead / victory demo | 0 | No |
| Refinement OFF, phase 1 mid-fight | lerp on vanilla 500 pool | Yes |
| Boss HP × = 4 | Phase 1 drains slower; phase 2 unchanged (3 hits) | Yes |

**Lock-on overlay:** segment text matches segment (refinement % or “3 hits” style), not necessarily same numeric as fill width.

**Layout regression:** after changing only `kNameFontFrac` or `kBarWidthFrac`, bar fill behaviour unchanged at same fight state.

---

## Adaptation guarantees (when art tuning changes)

| Change location | Should NOT require edits to |
|-----------------|----------------------------|
| `kBarWidthFrac`, `kBarHeightFrac`, `kBarTopFrac` | `d_albw_boss.cpp` meter math |
| `kNameFontFrac`, `kNameGap`, `kNameOutline` | Query / fill ratio |
| Track/fill RGB, track alpha | Query / fill ratio |
| New palette variants / HIO sliders | Boss fight logic |
| `drawRect` → nine-slice / texture swap | Query layer (still `fillW = barW * fillRatio`) |
| Generalise to Zant / Ganon later | Armogohma query; add `dAlbwBoss_*QueryHealthBar()` per boss |

**Contract for future bosses:** each boss exports `{ visible, fillRatio, displayName }`; HUD stays one layout path.

---

## Alternative: full-width reset at phase 2 (not recommended default)

If playtest rejects composite lerp:

- Phase 1: `fillRatio = phase1Progress` (0..1).
- On `mIsDisappear` rising edge: latch `phase2Active`; phase 2: `fillRatio = hitsRemaining / 3`.

**Trade-off:** visible reset at transition; simpler mental model; worse continuity from ~35% end state.

Implement as a **constexpr mode** in boss module (`kAlbwArmogohmaBarMode_COMPOSITE` vs `_RESET`) so HUD draw path stays identical.

---

## Implementation order

1. Layer 2 query + `fillRatio` (no visual change until HUD wired).
2. HUD gate fix + `computeLayout` + draw from `fillRatio`.
3. Lock-on overlay parity (`customMeter` for phase 2).
4. Playtest matrix above.
5. Update tuning brief + handoff docs.
6. Optional: mid-bar tick, phase-1 snap to 0.5 on disappear.

---

*Last updated: 2026-06-15 — composite meter spec; no code landed.*

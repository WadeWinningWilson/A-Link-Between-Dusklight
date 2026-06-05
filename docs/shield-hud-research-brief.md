# Shield HUD research brief — spur icons + durability meter

**For:** follow-up research / implementation agent (Claude or human)  
**Repo:** ALBW-Dusklight (`TARGET_PC`)  
**Prerequisite:** Phase 4 parry + bash charges in `d_albw_shield.cpp`.

**Implementation status (2026-05-31):** R-H8 steps 1–4 **implemented** — `dShield_drawBashCharges()` in `d_meter2_draw.cpp`, getters in `d_albw_shield.h`, rupee-anchored layout, combat visibility gate. Playtest: Ordon = **2** icons (correct). Bash icon **scale** is derived each frame from `rupi_n` global bounds vs `haku_n` init size (replaces fixed 0.42× shrink). Step 5 matrix ongoing; durability bar (step 6) deferred.

**Ordon tier / 4-spur bug:** If Ordon shows 4 icons instead of 2, use **[shield-ordon-tier-research.md](shield-ordon-tier-research.md)** (equip ID vs arc naming, log interpretation, hypothesis tree).

## Player-facing requirements (fixed)

1. **Bash charge icons** — reuse Epona spur visuals (`haku_n` / `haku_b_n` from `zelda_game_image_hakusha_parts.blo`), resized, **max 6** icons (Hylian cap).
2. **Placement** — bottom-right, **above rupee counter**, not overlapping wallet UI.
3. **Visibility** — draw bash icons **and** shield durability meter **only while the player is manually using the shield**:
   - `dusk::getSettings().game.manualShielding == true`
   - **AND** guard is actively held: `daAlink_c::manualShieldButton()` (L+R) **or** at minimum `checkUpperGuardAnime()` while manual mode is on (confirm which matches “using shield” feel).
4. **Bash deny feedback** — spur-style **pikari flash** when bash is blocked (`dShield_tryBeginGuardAttack()` fails); reuse `dMeter2Draw_c::drawPikariHakusha()` like horse spur loss.
5. **Durability meter** (Phase 2) — same visibility gate; tier max follows shield (Ordon / Wooden / Hylian); do **not** use ALBW meter index 0.

---

## Research questions (answer each with file:line evidence)

### R-H1 — Spur asset pipeline

- What BLO/TIMG panes does `d_meter_hakusha.cpp` use (`haku_n00`–`haku_n05`, `hakunall`, `haku_n`, `haku_b_n`)?
- Can a **second** `J2DScreen` load `zelda_game_image_hakusha_parts.blo` while horse `dMeterHakusha_c` is **not** active, without double-free or archive conflicts?
- Minimum code path to draw **N filled + (max−N) empty** spur icons without `dMeter2Info_getHorseLifeCount()`?

### R-H2 — Layout above rupees

- Where are rupees positioned (`g_drawHIO.mRupeePosX/Y`, `d_meter2_draw.cpp` `drawRupee`)?
- Default spur bar HIO (`mSpurBarPosX = 6.6`, `mSpurBarPosY = -1.3`) — map to **rupee-relative** offset for 2/4/6 icon widths.
- How does `updateHakusha()` space icons between `haku_n00` and `haku_n05` centers (`temp_f28 = (sp20.x - sp2C.x) / hakushaNum`) — can the same formula scale for 2–6 bash charges?
- Widescreen / `TARGET_PC_NATIVE_UI`: any existing HUD safe-zone helpers?

### R-H3 — Lifecycle: SubContents vs inline draw

- Horse spurs use `dMeter2_c::checkSubContents()` → `mStatus & 0x2000000` → `dMeterHakusha_c` subcontent type **1** (`d_meter2.cpp` ~3272).
- Should bash charges use:
  - **A)** New `dMeterSub_c` subclass (mirror hakusha),
  - **B)** Draw inside `d_meter2_draw.cpp` / `moveKantera` when gated,
  - **C)** Extend `d_albw_shield.cpp` with pane refs created once at meter init?
- Pros/cons for **coexistence** with horse riding (mutually exclusive subcontent types?).

### R-H4 — Visibility gate (manual shield only)

- Confirm `manualShieldButton()` vs `checkUpperGuardAnime()` for “holding shield” during slip/recovery.
- Should icons **fade out** one frame after release (match hakusha `alphaAnimeHakusha` flags) or hard hide?
- When manual shield setting is **off**, ensure **zero** bash/durability HUD draw (no horse-style leak).

### R-H5 — Bash charge → icon mapping

- Source of truth: `d_albw_shield.cpp` (`sBashCharges`, tier max/threshold from **`getSelectEquipShield()` first**, then `mShieldArcName`). See `shield-ordon-tier-research.md` if Ordon shows 4 slots.
- Expose read API: `dShield_getCharges()`, `dShield_getMaxCharges()`, `dShield_getBashThreshold()`, `dShield_consumeBashDenyFlash()` for HUD.
- **Hylian threshold 4:** show 6 slots with 4+ lit = bash allowed, or gray out slots 1–3 until threshold? (Design choice — document recommendation.)
- Ordon/Wooden: bash only at full bank — dim icons below cap?

### R-H6 — Pikari flash on bash deny

- Horse spur flash: `mHakushaAnimFrame`, `drawPikariHakusha`, HIO `mSpurIconPikariScale` / anim speed.
- Wire `sBashDenyFlashFrames` (12 frames today) to one icon flash or whole row?
- SE: reuse existing UI SE or silent?

### R-H7 — Shield durability gauge (Phase 2 prep)

- Cat Deluxe / `shield-combat.md`: gauge **index 3** or custom BLO — survey `d_meter2_draw.cpp` / `getMeterGaugeAlphaRate` indices 0–2 (ALBW, kantera, oxygen).
- Can durability share spur row (segmented bar) or needs separate pane?
- Same manual-shield visibility gate — document hook point in `dShield_onBlockHit(..., perfect)`.

### R-H8 — Implementation checklist (deliverable)

Ordered minimal PR:

1. `dShield_*` getters + deny-flash flag public to meter code  
2. `dMeterShieldBash_c` or gated draw fn — spur icons above rupees  
3. Call from meter execute/draw after rupee draw  
4. Playtest matrix: manual on/off, 2/4/6 shields, horse ride (no clash), bash deny flash  
5. (Later) durability bar with same gate  

---

## Code anchors (start here)

| Topic | Path |
|--------|------|
| Bash state | `src/d/d_albw_shield.cpp`, `include/d/d_albw_shield.h` |
| Spur reference | `src/d/d_meter_hakusha.cpp`, `include/d/d_meter_hakusha.h` |
| Pikari draw | `src/d/d_meter2_draw.cpp` `drawPikariHakusha` |
| Rupee layout | `src/d/d_meter2_draw.cpp` `drawRupee`, `d_meter_HIO.cpp` `mRupeePos*` |
| SubContents | `src/d/d_meter2.cpp` `checkSubContents`, `killSubContents` |
| Manual guard | `src/d/actor/d_a_alink_guard.inc`, `manualShieldButton()` |
| Design spec | `docs/shield-combat.md` § Phase 4 |

---

## Out of scope for this research pass

- Changing parry window tuning  
- Save-file persistence of bash charges  
- Cat Deluxe exe decompilation  

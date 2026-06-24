# Shield combat — ALBW-Dusklight

Design + **completed research** for manual shielding, shield durability, parry rewards, and ALBW meter integration.

**Implemented:** Phase 1 manual shielding, Phase 3 ALBW recovery pause while guarding. **Next:** Phase 4 parry + bash charge economy (single phase).

| Source | What we take from it |
|--------|----------------------|
| **[BeZide93/dusk](https://github.com/BeZide93/dusk)** (Dawnlight, tag **v1.3.1-1**) | Manual shielding (commit **9ff9d35**, kamilink7) |
| **Cat Deluxe** (Captainkittyca2; **exe only**, no public source) | Shield durability **concept** + settings names; implement from spec |
| **ALBW-Dusklight** | Durability HUD, parry → `dMeter2_addALBWFraction()`, no ALBW recovery while guarding |

---

## Design goals

| Goal | Notes |
|------|--------|
| Manual shield | No auto-guard on Z-target alone when setting is on; explicit guard chord (Dawnlight: **L+R held**, not ZR alone). |
| Shield durability | **Separate** from `sALBWMeter`; own HUD; break at 0 → `procGuardBreakInit()`. |
| Parry / perfect block | No durability loss; bonus ALBW refill via `dMeter2_addALBWFraction()`. |
| ALBW recovery while guarding | **No passive ALBW recovery** while `checkUpperGuardAnime()` (including idle boost). |
| Encourage Z-target blocking | Lock-on + manual guard + parry reward loop. |

**Out of scope v1:** Dawnlight Z(R) item slots, full Wii U control parity.

---

## Research status

| ID | Deliverable | Status |
|----|-------------|--------|
| R1 | Dawnlight commit | **Done** — see below |
| R2 | Manual shield files/functions | **Done** |
| R3 | Pad mapping | **Done** |
| R4 | Durability in Dawnlight | **Done — No** |
| R4b | Durability in **Cat Deluxe** (C1–C10) | **Done** — see § R4b below |
| R5 | ALBW hook map | **Done** |
| R6 | Risk matrix | **Done** |
| R7 | Phase order + effort | **Done** |

Research report date: integrated from Claude session (no source edits in that pass).

---

## R1 — Dawnlight commit studied

| Field | Value |
|-------|--------|
| Release | **v1.3.1-1** |
| Release SHA | `75809f74038394cadd30adfd5df4b8805da5d15d` |
| Branch | `dawnlight` (not `main`) |
| Manual shield introduced | **`9ff9d35`** — "Add manual shielding option", BeZide93, 2026-05-17 |

---

## R2 — Manual shield (Dawnlight `9ff9d35`)

Five files; **paths match ALBW-Dusklight** (port = apply diff + `#if TARGET_PC` if desired).

| File | Change |
|------|--------|
| `include/dusk/settings.h` | `ConfigVar<bool> manualShielding` |
| `src/dusk/settings.cpp` | Default `false`, `Register()` |
| `src/dusk/ui/settings.cpp` | Menu option + description |
| `include/d/actor/d_a_alink.h` | `manualShieldButton()`, `manualShieldAttackTrigger()` |
| `src/d/actor/d_a_alink.cpp` | Implement both; patch `swordSwingTrigger()` + shield-attack branch in `checkItemAction()` |
| `src/d/actor/d_a_alink_guard.inc` | Extra AND on `setShieldGuard()` |

```cpp
// d_a_alink.cpp (~9413 in Dawnlight)
BOOL daAlink_c::manualShieldButton() const {
    if (dusk::getSettings().game.manualShielding)
        return mDoCPd_c::getHoldLockL(PAD_1) && mDoCPd_c::getHoldLockR(PAD_1);
    return false;
}

BOOL daAlink_c::manualShieldAttackTrigger() {
    return manualShieldButton() && itemTriggerCheck(BTN_B);
}
```

```cpp
// d_a_alink_guard.inc:193 — suppress auto-guard when manual mode on
// Add to final AND:
    (!dusk::getSettings().game.manualShielding || manualShieldButton())
```

- `swordSwingTrigger()` returns false while `manualShieldButton()` — avoids sword during guard chord.
- Shield attack uses `manualShieldAttackTrigger()` when setting on.

**Local verification:** `getHoldLockL` / `getHoldLockR` exist in `include/m_Do/m_Do_controller_pad.h`. `manualShielding` is **not** in this tree yet.

---

## R3 — Input mapping

`mHoldLockL` / `mHoldLockR` = left/right trigger held past threshold.

| Action | GC | Wii U / Xbox | Keyboard (PC) |
|--------|-----|--------------|----------------|
| Lock-on | L | ZL | Lock binding (e.g. Shift) |
| Guard (manual on) | **L + R held** | **ZL + ZR held** | Both lock bindings held |
| Shield attack (manual on) | L+R held → B | ZL+ZR held → B | Both lock + B |
| Shield attack (vanilla) | R | ZR | R binding |
| Sword | — | — | Suppressed while L+R held |

Setting **off** → vanilla auto-guard on Z-target preserved.

> Release notes sometimes say “ZR while locked on”; the **implemented** Dawnlight chord is **both triggers held** (`manualShieldButton`).

---

## R4 — Shield durability

### Dawnlight

**No.** No shield HP / durability fields in `d_a_alink.h`, guard, or damage. `field_0x2fcb` is vanilla wood-shield fire only.

### Cat Deluxe (R4b — complete)

Shield durability is in **Cat Deluxe**, not Dawnlight. **Cannot cherry-pick** — mod source is private (`C:\duskTest\duskMayhem\dusk\` per PDB); [Captainkittyca2/duskLight](https://github.com/Captainkittyca2/duskLight) `main` @ `7a77d489` also has **no** mod code. Only artifact: `D:\Decomps\Dusk Mods\CatDeluxe\dusk.exe`. Local zip `source code\duskLight-1.1.1\` is **stale vanilla**.

| C# | Finding |
|----|---------|
| **C1** | Git `main` `7a77d48954847330a5ab55f5c7f2e64cdd25d846` (2026-05-13) — no Cat Deluxe features |
| **C2** | Local zip ≠ exe; exe only |
| **C3** | Logic inline in `d_a_alink.cpp` / `.inc` includes — no `d_albw_shield.*` in PDB paths |
| **C4** | `game.enableShieldDurability` (default off); max/drain **unknown** (hardcoded); break toast **"Your shield broke"**; no recovery strings |
| **C5** | HUD **unknown** — no separate UI Editor slot; use gauge **index 3** or custom draw in ALBW |
| **C6** | ImGui tab order: Z Items → TWW HD Armor → Manual Shielding → Armors D-pad → **Shield Durability** → … |
| **C7** | When durability on: **zero `field_0x2fd5`** on hit; make new HP sole break path; `field_0x2fcb` untouched |
| **C8** | Same as R5 hooks + `settings.h` / `dusk/ui/settings.cpp` + toast after `procGuardBreakInit` |
| **C9** | Cat Deluxe manual = **R only while targeting**; Dawnlight = **L+R** — **use Dawnlight for Phase 1** |
| **C10** | **Write from spec**, effort **M** (3–5 d); expose drain/max as settings for tuning |

**Cat Deluxe settings (exe strings):**

| Label | ConfigVar | Notes |
|-------|-----------|--------|
| Manual Shielding | `game.enableManualShielding` | "Hold **R** to guard… Hold R and press **B** for shield attack" |
| Shield Durability | `game.enableShieldDurability` | Checkbox only, no tooltip |
| TWW HD Magic Armor | `game.enableTWWHDArmor` | Separate feature |
| Armors Quick-Toggle | `game.enableArmorsQuickToggle` | D-pad down armor swap |

Full research log: **[cat-deluxe-research-brief.md](cat-deluxe-research-brief.md)** § Results.

---

## R5 — Hook map (ALBW-Dusklight)

| Hook | Location | Notes |
|------|----------|--------|
| Manual guard | `d_a_alink_guard.inc` ~193, `setShieldGuard()` final AND | Same as Dawnlight diff |
| Durability drain (normal) | `d_a_alink_damage.inc` ~717, ~720 `procGuardSlipInit` | `dShield_onBlockHit(at_spl, false)`; if broken → `procGuardBreakInit` |
| Durability drain (small) | `setSmallGuard()` path ~714 | Lesser drain |
| ALBW recovery pause | `d_meter2.cpp` ~1583 `sALBWMeter += sRecoveryRate` | Skip when `checkUpperGuardAnime()`; clear idle boost ~1553 while guarding |
| Parry | `d_a_alink_damage.inc` ~683, inside `ChkTgShieldHit()` before slip | Window vs guard onset; parry → no drain + `dMeter2_addALBWFraction(1, 6)` |
| Guard onset frame | `setUpperGuardAnime` / `onNoResetFlg2(FLG2_UNK_8000000)` | Track `mGuardOnsetFrame` for parry |

### HUD gauge indices (verified in `d_meter2_draw.cpp`)

| Index | Use in this tree |
|-------|------------------|
| **0** | ALBW stamina meter draw |
| **1** | Kantera / oil visibility checks in `d_meter2.cpp` |
| **2** | Oxygen |

Shield durability HUD needs **index 3**, standalone draw, or new BLO — do not reuse index 0 (`sALBWMeter`).

---

## R6 — Risk matrix

| Risk | Level | Mitigation |
|------|-------|------------|
| Magic armor | LOW | `armor_no_dmg` skips shield branch before slip |
| Iron ball | MEDIUM | Mirror `mEquipItem != IRONBALL` on drain hook |
| Wolf | LOW | No shield guard; idle already wolf-gated |
| **`field_0x2fd5` vs new meter** | **HIGH** | Zero or bypass slip counter when using durability as single break source |
| Multi-hit | MEDIUM | `PROC_GUARD_SLIP` re-entry guard; handle `setSmallGuard` separately |
| Rental / talk | LOW | `checkGuardAccept()` needs `checkModeFlg(0x80)` — events block guard |

---

## R7 — Implementation order

| Phase | Scope | Effort | Files |
|-------|--------|--------|-------|
| **1** | Port Dawnlight `9ff9d35` manual shield (`manualShielding`, **L+R**) | **S** (1–2 d) | `d_a_alink_guard.inc`, `d_a_alink.cpp`, `d_a_alink.h`, `settings.*` |
| **2** | Durability from spec (`enableShieldDurability`, **not** Cat Deluxe port) | **M** (3–5 d) | After Phase 4; `d_albw_shield.*` or inline + guard/damage + meter draw |
| **3** | ALBW recovery pause while guarding | **S** (1–2 d) | `d_meter2.cpp` — **done** |
| **4** | Parry + bash charge economy (see § Phase 4) | **M** (3–6 d) | `d_a_alink_damage.inc`, `d_albw_shield.*`, `d_meter2.cpp`, charge HUD |

### Phase 1 checklist (coding agent)

- [x] Apply `9ff9d35` to six Dawnlight files (paths unchanged)
- [x] Gameplay wrapped in `#if TARGET_PC`; setting/UI available on all platforms (default off)
- [x] `getHoldLockL` / `getHoldLockR` used per Dawnlight
- [x] Playtest: setting off = vanilla; on = L+R guard, no auto-guard on lock-on only
- [x] Phase 1 complete (code + playtest)

---

## Phase 4 — Parry + bash charge economy (spec, 2026-05-28)

**Do not implement charges without parry** — charges are earned only on successful parries. Ship Phase 4 as one vertical slice.

### Bash charges (session-only)

| Rule | Behavior |
|------|----------|
| Gain | **+1 per successful parry** until shield tier cap; **no passive regen** |
| Persistence | **In-memory only** — quit or reload save → **0 charges**; bank **does not clear** when leaving combat (HUD hidden until combat or guard again) |
| Spend | Each shield bash costs **1 charge** |
| Parry while spending | Still **+1 until cap** (base rule; future bonus for parrying during spend loop is optional) |

### Shield tiers (weakest → strongest)

| Shield | Max charges (cap) | Bash unlock threshold | Max bash uses if bank full |
|--------|-------------------|------------------------|----------------------------|
| Ordon | 2 | **2** (must be full) | 2 |
| Wooden | 4 | **4** (must be full) | 4 |
| Hylian | 6 | **≥ 4** to allow bash | 6 (spend from 6 down to 0; re-unlock at 4+) |

**Hylian loop:** At 6 charges, bash until 0. To bash again, parry back to **at least 4**. Between 1–3 charges, parry adds charges but bash stays disabled. Failed/normal blocks cost **2 charges** (other tiers: 1).

**Hylian durability (Phase 2, planned):** Only Hylian regains durability on parry (by %); also takes higher % durability damage per hit than Ordon/Wooden.

**Ordon / Wooden loop:** First bash requires **full bank** (2/2 or 4/4). After that, a **spend chain** allows further bashes until charges hit **0** (e.g. 2/2 → bash → 1/2 → bash → 0/2). Parries during the chain refill charges (+1 to cap) without ending the chain until empty.

### Three outcomes on shield hit (not one “failed parry” bucket)

| Outcome | Timing | +Charge | ALBW bonus | Durability (Phase 2) | UI flash |
|---------|--------|---------|------------|----------------------|----------|
| **Perfect parry** | In parry window (buffer TBD in tuning) | +1 if below cap; **none at cap** | Yes (`dMeter2_addALBWFraction`) | No loss | — |
| **Failed parry** | Guard up, hit outside parry window | **−1** (Ordon/Wooden) / **−2** (Hylian) if any | **−10% `sOilMaxVar`** | Yes (normal block) | — |
| **Normal block** | Same charge path as failed parry (shield hit, no parry window) | Same as failed parry | **No** | Yes | — |

**Clarification (Q4):** “Zero charge bank” is **not** a third parry failure mode. At **0 bash charges**, a **perfect parry still works** (+1 charge, ALBW, no durability). What **does not** work at 0 is **spending** a bash — that is separate from parry timing.

| Action at 0 bash charges | Result |
|--------------------------|--------|
| Perfect parry | +1 charge, ALBW bonus, no durability loss |
| Failed parry / normal block | No charge, no ALBW, durability loss (when enabled) |
| Attempt shield bash (B with guard chord) | **Blocked**; **UI flash** on charge meter (secondary polish) |

**At cap (e.g. 6/6 Hylian):** Perfect parry → ALBW + no durability; **no +charge**. No flash required for “full bank” in v1.

### Parry vs bash

- **Parry** — skill check on block; builds bash charges and refills ALBW.
- **Bash** — spends 1 charge; independent input (manual: L+R + B); requires charges ≥ unlock threshold for current shield.

### Testing Parry Rework (optional — not shipped by default)

Parked experiment (hold-guard + R1/RB parry, parry VFX/SFX) documented in **[testing-parry-rework.md](testing-parry-rework.md)** — **disabled in shipping builds** (`kTestingParryReworkEnabled = false`); menu toggle removed after playtest. Shipping parry remains guard-onset below.

### Perfect parry timing (LoP-style, PC) — shipping default

Inspired by **Lies of P Perfect Guard** (not enemy anim windows):

| Rule | Implementation |
|------|----------------|
| Window start | Frame guard becomes active (`FLG2_UNK_8000000` + guard input held) |
| Window length | **5** sim frames base + **2** buffer (~LoP 8–9f@60 Hz scaled to **30 Hz** sim); tune in `d_albw_shield.cpp` |
| Window end | **Guard input released** (manual: L+R; vanilla: upper guard no longer active) |
| Perfect parry | `ChkTgShieldHit` + in window + still holding → ALBW fraction, +1 charge (if below cap), **no** `procGuardSlipInit` |
| Failed parry | Block connects outside window or after release → `procGuardSlipInit` (bad block; durability in Phase 2) |
| Hold vs tap | Holding through impact is required; tapping off early closes the window (LoP “press & hold”) |

Enemy attack timing is **hitbox contact** only (vanilla `ChkTgShieldHit`); no per-enemy parry tables in v1.

### Implementation notes

- Module: `d_albw_shield.cpp` / `d_albw_shield.h`
- Hook: `ChkTgShieldHit()` / `d_a_alink_damage.inc` ~683 before `procGuardSlipInit`; `dShield_updateGuardTracking()` from `setShieldGuard()`.
- Gate bash: `procGuardAttackInit()` / early `checkItemAction()` — require `dShield_getBashCharges() >= dShield_getBashThreshold()`.
- HUD: spur-style bash icons above rupees while manual guard active (`dShield_drawBashCharges`); deny pikari flash; research: **[shield-hud-research-brief.md](shield-hud-research-brief.md)**. Durability bar still Phase 2.
- Failed parry: `dMeter2_subALBWFraction(1, 10)` (−10% `sOilMaxVar`) + lose 1 bash charge. Setting: `game.shieldParryCombat` (default on).
- Bash HUD: manual shielding + shield combat context (Z-target enemy, shield hits, or magic-armor encounter); visible in combat, not only while guarding.
- Charge tiers from `mShieldArcName` (`setShieldArcName()`): **SWShd** = Ordon shop shield (2), **CWShd** = Goron wooden (4), **HyShd** = Hylian (6). Asset filenames (al_shc/al_shb) can label the opposite English name — trust Link equip logic, not the browser string.
- Successful shield bash **hit** (`dShield_onGuardAttackConnect()` on `ChkAtHit`): +**5%** `sOilMaxVar` (`5/100`). Log: `[dShield] bash-hit ALBW`. Invisible if meter already full. Parry window: **4+1** sim frames.
- Charge tier: mirrors `setShieldArcName()` — `checkShopWoodShieldEquip()` → Ordon (2), `checkCarvingWoodShieldEquip()` → Wooden (4), Hylian equip → 6. Bash HUD combat gate uses enemy Z-target / shield hits only (not magic-armor encounter IDs).
- Failed parry may use **buffer frames** during playtest for feel — document tuned values in code comments.

### Failed parry — ALBW penalty (design draft)

Perfect parry uses `dMeter2_addALBWFraction(1, 6)` → **+16.7% of `sOilMaxVar`**. Failed parry penalty should use the **same ceiling** (`sOilMaxVar`, not `sOilBaseMax`) so upgraded meters scale.

| Penalty % of `sOilMaxVar` | Base (10900) | +1 unlock (16350) | +2 unlock (21800) | vs sword swing (1817 fixed) |
|---------------------------|-------------|-------------------|---------------------|------------------------------|
| 2% | 218 | 327 | 436 | ~12% of swing |
| 3% | 327 | 490 | 654 | ~18% of swing |
| **5%** | **545** | **818** | **1090** | **~30% of swing** |
| 8% | 872 | 1308 | 1744 | ~48% of swing |

**Context:** Most combat drains are **`sOilBaseMax` fractions** (swing 16.7%, sidestep 20%, bomb 50%). A failed parry is already `procGuardSlipInit` (tempo loss); meter tax stacks in long fights.

**Recommendation:** Start **3% `sOilMaxVar`** (~one third of a sword swing at base meter). **5%** is fair if parries are frequent and profitable; harsh if the player spam-blocks without timing. **Do not exceed ~8%** without lowering perfect-parry reward. Optional: cap penalty at **one sidestep cost** (`sOilBaseMax / 5`) per failed parry.

### Phase 4 checklist

- [ ] Parry window + guard onset frame
- [ ] Session charge state by equipped shield tier
- [ ] +1 on parry until cap; ALBW on parry even at cap
- [ ] Bash spends 1; Hylian threshold 4; Ordon/Wooden require full bank
- [ ] Bash blocked + meter flash when spend not allowed
- [ ] Reload save resets charges to 0
- [ ] Playtest: parry-spend-parry loop on all three shields

---

## Current codebase anchors

- `setShieldGuard()` — `src/d/actor/d_a_alink_guard.inc`; called from `d_a_alink.cpp` ~18464
- Manual shield: hold **ZR** (`getHoldLockR`); `checkGuardAccept()` skips `MODE_GUARD_ENABLED` while moving; `checkGuardActionChange()` needs no Z-target
- ALBW recovery — `src/d/d_meter2.cpp` ~1521–1596
- `dMeter2_setALBWPlayerIdle` — `d_a_alink.cpp` `procWait()`, `#if TARGET_PC`

---

## Suggested APIs (`#if TARGET_PC`)

```cpp
// include/d/d_albw_shield.h (tentative)
void dShield_onBlockHit(int atSpl, bool perfect);
bool dShield_isBroken();
void dShield_drawMeter();  // or integrate in d_meter2_draw
```

```cpp
// Recovery pause: early-out in moveKantera() when checkUpperGuardAnime()
```

---

## ALBW recovery pause (player rule)

While **guarding** (`checkUpperGuardAnime()` recommended):

- No 100ms `sALBWMeter += sRecoveryRate` tick
- No idle 5% boost from `sALBWPlayerIdle`
- Parry refill only via explicit `dMeter2_addALBWFraction()`

Phase 3 ships pause during active guard regardless of manual-shield setting (PC).

---

## Playtest checklist

- [ ] Manual off: auto-guard on Z-target unchanged
- [ ] Manual on: Z-target without L+R → no guard anim
- [ ] Manual on: L+R while locked → guard works
- [ ] ALBW meter flat while guarding; recovers after release
- [ ] Bad block → shield meter drops
- [ ] Perfect parry → +1 bash charge (if below cap), ALBW bonus, no durability loss
- [ ] Failed parry / normal block → no charge, no ALBW, durability drops (Phase 2)
- [ ] Bash at 0 or below threshold → denied + charge UI flash
- [ ] Hylian: bash 6→0, then parry to 4+ to bash again
- [ ] Reload save → bash charges 0
- [ ] Shield 0 → break
- [ ] Wolf / iron ball / magic armor / Postman shop — no regressions

---

## Credits (when shipped)

- **kamilink7** — manual shielding concept
- **BeZide93 / Dawnlight** — manual shield implementation (`9ff9d35`)
- **Captain Kitty Cat / Cat Deluxe** — shield durability concept (no public source to port)
- **ALBW-Dusklight** — parry rewards, ALBW pause while guarding, integration

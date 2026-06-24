# Testing Parry Rework — design brief

**Status:** **Parked** — Phase 1 coded but **disabled** (`kTestingParryReworkEnabled = false` in `d_albw_shield.cpp`). Menu setting **removed** after playtest. Phase 2 ALBW recovery still research-only.  
**Chat origin:** Refinements (shield parry feel).  
**Goal:** Optional test path for **hold guard + RB/R1 parry** with clear **VFX/SFX** feedback, without changing shipping parry behavior when off.

---

## Playtest log (2026-06)

| Item | Verdict |
|------|---------|
| Parry spark **VFX** (`setHitMark(2)`) | **Keep candidate** — liked in playtest |
| Parry **SFX** (`Z2SE_EN_TN_SHIELD_BND`) | Works; user **may want a different clang** later |
| Hold guard + **R1/RB** parry window | **Not adopted** — overall feel not compelling enough |
| Midna suppress while guarding | Shipped with rework; inactive while parked |
| Guard-onset shipping parry | Unchanged — still default with rework off |

**Decision:** Remove **`game.testingParryRework`** from settings UI; leave implementation in tree for one-flag re-enable. If revisiting, consider applying **VFX-only** on shipping perfect parries before re-enabling full RB parry flow.

### Re-enable locally (dev)

1. Set `kTestingParryReworkEnabled = true` in `src/d/d_albw_shield.cpp` (anonymous namespace, ~line 80).
2. Rebuild with `build_run.bat`.
3. Requires `game.shieldParryCombat` on. Hold **RT/R2** guard, tap **RB/R1** before hit.

To restore a user-facing toggle, re-add `testingParryRework` to `settings.h` / `settings.cpp` / `ui/settings.cpp` and wire `dShield_isTestingParryReworkEnabled()` to the config var again (see git history).

---

## Agent onboarding (read before coding)

### Always read first

| Doc | Why |
|-----|-----|
| [build-fps-guidelines.md](build-fps-guidelines.md) | Canonical build (`build_run.bat` / RelWithDebInfo), launch hygiene (no `DUSK_DRIVE*`), what never goes in the repo |
| [commit-and-push.md](commit-and-push.md) | Push to **upstream** (`WadeWinningWilson/ALBW-Dusklight`), not `origin`; stage/don’t-stage rules |

### If the work touches…

| Area | Also read |
|------|-----------|
| HUD / meter / shield / `game.lopHud` | [hud-performance-handoff.md](hud-performance-handoff.md) + [albw-hud-lop-layout-brief.md](albw-hud-lop-layout-brief.md) |
| LoP layout details | [albw-hud-lop-layout-v3-instructions.md](albw-hud-lop-layout-v3-instructions.md) |
| Boss fights / refinement | [boss-fights-handoff.md](boss-fights-handoff.md) |
| Boss HP bar tuning | [albw-boss-hp-hud-tuning-brief.md](albw-boss-hp-hud-tuning-brief.md) |
| Shield / parry context (shipping behavior) | [shield-combat.md](shield-combat.md) |

### After build — hand off here

| Chat | Doc entry |
|------|-----------|
| Build-analysis / FPS review | [build-fps-guidelines.md](build-fps-guidelines.md) → Doc map, then [hud-performance-handoff.md](hud-performance-handoff.md) if HUD changed |

### Short prompt (paste into any new feature chat)

```
Before coding: read docs/build-fps-guidelines.md and docs/commit-and-push.md.
Build with build_run.bat only (RelWithDebInfo). No drive/conavigate code in src/.
If HUD/meter/shield: also read docs/hud-performance-handoff.md.
When done: git diff summary + build result → build-analysis chat for FPS check before commit.
Push to upstream (ALBW-Dusklight), not origin.
```

### Hard rules (one-liners)

- **Build:** `build/windows-msvc-relwithdebinfo/dusklight.exe` via `build_run.bat`
- **Never commit:** `local_dev_backup/`, drive/conavigate sources, `albw_*_debug.txt`
- **FPS:** Don’t revert features to fix FPS — optimize in place; use the build-analysis chat
- **Git:** Don’t commit/push unless the user explicitly asks

---

## VFX vs SFX (use these terms consistently)

| Term | Definition | Examples in this feature |
|------|------------|---------------------------|
| **VFX** | Visual-only feedback (particles, sparks, flashes, animations) | `dComIfGp_setHitMark(2, …)` shield spark at contact; skipping `procGuardSlipInit` (no bad-block anim) |
| **SFX** | Audio feedback (sound effects) | `Z2SE_EN_TN_SHIELD_BND` parry clang; generic block clang via `setGuardSe()` → `startCollisionSE()` |
| **Haptic** | Controller rumble (optional tuning) | `StartShock` in `d_a_alink_damage.inc` — not VFX/SFX |

**Not parry-success VFX/SFX:** bash charge HUD flash (UI), `Z2SE_ITEM_RING_IN` (charge-gain SFX), ALBW meter changes (gameplay).

---

## Settings (historical)

| Setting | Config key | Notes |
|---------|------------|-------|
| Shield Parry & Bash Charges | `game.shieldParryCombat` | **Active** — master parry economy |
| ~~Testing Parry Rework~~ | ~~`game.testingParryRework`~~ | **Removed from menu** — use `kTestingParryReworkEnabled` in code |

**Gate helper (current — parked):**

```cpp
constexpr bool kTestingParryReworkEnabled = false;  // d_albw_shield.cpp

bool dShield_isTestingParryReworkEnabled() {
    return kTestingParryReworkEnabled && dShield_isParryCombatEnabled();
}
```

**Former menu gate (git history):**

```cpp
bool dShield_isTestingParryReworkEnabled() {
    return dShield_isParryCombatEnabled() &&
           dusk::getSettings().game.testingParryRework.getValue();
}
```

| `shieldParryCombat` | Rework | Behavior |
|---------------------|--------|----------|
| off | * | Vanilla TP guard (no parry economy) |
| on | **off** (default / shipped) | **Shipping mod** — parry window on guard **raise**; no RB parry; no extra parry VFX/SFX |
| on | **on** (`kTestingParryReworkEnabled`) | **Parked test** — RB/R1 parry window; parry spark **VFX** + clang **SFX**; Midna suppressed while guarding |
| on | on (+ Phase 2 later) | **+ Phase 2** — ALBW **recovers while guarding**; smaller failed-block drain (not implemented) |

Related settings (unchanged by this brief): `manualShielding`, `shieldDurability`, `hiddenSkillRework`, `parryIconsMode` (HUD only).

---

## Problem (why this exists)

Shipping parry opens the timing window when guard **becomes active** (`sGuardOnsetFrame` on guard onset in `dShield_updateGuardTracking()`). That ties the read to the **shield-raise animation**, which feels visually awkward. Desired feel: **hold guard**, then **press to parry** at the right moment (bash stays on **B**).

---

## Input model (rework ON)

| Action | Input | API / notes |
|--------|-------|-------------|
| **Hold guard** | RT / R2 (ZR) | `getHoldLockR(PAD_1)` → `manualShieldButton()` when shield equipped |
| **Parry attempt** | **RB / R1** (shoulder bumper) | `getTrigZ(PAD_1)` (`PAD_TRIGGER_Z`) — separate from RT/R2 guard hold |
| **Shield bash** | ZR + **B** | `manualShieldAttackTrigger()` — unchanged |

**RB vs Midna:** Call Midna can bind to RB (`ActionBinds::CALL_MIDNA`). While rework is on and Link is **guarding**, suppress Midna in `midnaTalkTrigger()` (same pattern as `dALBWRental_isOpen()` in `d_a_alink.cpp`). Suppress for the **whole guard hold**, not only inside the parry window.

---

## Parry timing (rework ON)

Only the **window start** changes; resolution stays in existing hooks.

| Rule | Shipping (`testingParryRework` off) | Rework (`testingParryRework` on) |
|------|-------------------------------------|----------------------------------|
| Window opens | Guard becomes active | **RB edge** while `checkUpperGuardAnime()` + guard input held |
| Window length | `PARRY_WINDOW_FRAMES` + buffer (4+1 sim frames in code today) | Same constants unless playtest retune |
| Perfect parry | `dShield_onShieldHit()` → true | Same |
| Failed block | `procGuardSlipInit` + penalties | Same |
| Bash | ZR+B + charge gate | Same |

**Hook:** `dShield_updateGuardTracking()` in `src/d/d_albw_shield.cpp` — branch onset logic on `dShield_isTestingParryReworkEnabled()`.

**Unchanged hooks:** `d_a_alink_damage.inc` (`ChkTgShieldHit` → `dShield_onShieldHit`), charge/ALBW/durability, bash economy.

---

## Successful parry feedback (rework ON only)

### Already happens on every shield hit (including parry)

| Layer | Type | What |
|-------|------|------|
| Block clang | **SFX** | `setGuardSe()` before parry check |
| Rumble | **Haptic** | `StartShock` by attack size |
| Generic hit mark | **VFX** | Collision `TgHitMark` 6/8 via `d_cc_s.cpp` (subtle) |

### New on **successful** parry (rework ON)

Reference: Darknut `setShieldEffect()` in `d_a_b_tn.cpp`.

| Layer | Type | Call | Notes |
|-------|------|------|-------|
| Parry spark | **VFX** | `dComIfGp_setHitMark(2, link, hitPos, …)` | Hit mark type **2** = shield clang spark |
| Parry clang | **SFX** | `Z2GetAudioMgr()->seStart(Z2SE_EN_TN_SHIELD_BND, &hitPos, …)` | Same ID as Darknut shield bound (`d_a_b_tn.cpp`) |
| Alt clang (optional) | **SFX** | `def_se_set(&link->mZ2Link, hitObj, 42, link)` | Darknut paired collision clang; pick one clang path in playtest |

**Hit position:** `var_r29->GetTgHitPosP()` in `d_a_alink_damage.inc` (available at shield hit).

**Hook:** After `dShield_onShieldHit()` returns true, call e.g. `dShield_playParrySuccessFeedback(link, var_r29)` when rework enabled.

**Charge SFX:** Keep `playParryEarnSe()` (`Z2SE_ITEM_RING_IN`) for +charge only; **always** play parry VFX + clang SFX on success (including at cap).

**Audio stack:** Rework adds clang **on top of** generic `setGuardSe`. Tune in playtest (additive vs replace on parry only).

---

## Files to touch (implementation estimate ~80–120 LOC)

| File | Change |
|------|--------|
| `src/d/d_albw_shield.cpp` | `kTestingParryReworkEnabled`, RB/R1 window, `dShield_playParrySuccessFeedback()` |
| `include/d/d_albw_shield.h` | `dShield_isTestingParryReworkEnabled()`, feedback helper |
| `src/d/actor/d_a_alink.cpp` | Midna suppression in `midnaTalkTrigger()` |
| `src/d/actor/d_a_alink_damage.inc` | Invoke feedback on parry success |
| ~~`include/dusk/settings.h`~~ | ~~`testingParryRework`~~ — removed |
| ~~`src/dusk/settings.cpp`~~ | ~~Register / default~~ — removed |
| ~~`src/dusk/ui/settings.cpp`~~ | ~~menu option~~ — removed |

All `#if TARGET_PC` / existing ALBW shield module patterns.

**Out of scope for Phase 1 of this toggle:** enemy stagger on parry, new particles beyond `setHitMark`, replacing shipping guard-onset parry when toggle off, HUD layout changes (read hud-performance-handoff if bash HUD flicker appears), **ALBW recovery rework (Phase 2 below)**.

---

## Phase 2 (future) — ALBW recovery while guarding

**Status:** Research only; implement **after** Phase 1 (RB parry + VFX/SFX) playtest.  
**Intent:** Under **`testingParryRework`**, stop punishing **idle guard hold** with a full ALBW recovery freeze; instead, **non-perfect shield hits** pay a **small ALBW cost** (meter drain, not Link HP).

### Current build — guard hold pauses ALBW recovery

**Not gated by `shieldParryCombat`.** On PC builds, passive ALBW refill is skipped whenever Link is in a guard state:

```505:507:c:\Users\<user>\Documents\dusklight\src\d\d_meter2.cpp
static bool dMeter2_isALBWRecoveryPausedByGuard() {
    return daPy_getPlayerActorClass()->checkPlayerGuard() != 0;
}
```

Used in the passive recovery tick (~2076–2117): `allowPassiveRecovery = !sALBWGuarding`.

**What counts as “guarding”** (`checkPlayerGuard()` — broader than upper guard alone):

```180:183:c:\Users\<user>\Documents\dusklight\src\d\actor\d_a_alink_guard.inc
BOOL daAlink_c::checkPlayerGuard() const {
    return (checkSmallUpperGuardAnime() || (checkShieldGet() && checkHorseLieAnime())) ||
           checkUpperGuardAnime();
}
```

**Also paused while guarding:** idle recovery boost (`sALBWPlayerIdle` → +5% rate) — same `allowPassiveRecovery` gate.

**Documented as:** Phase 3 in [shield-combat.md](shield-combat.md); [albw-port.md](albw-port.md) notes it is **always on** for PC ALBW builds.

| Situation | ALBW passive recovery |
|-----------|------------------------|
| Standing / moving, not guarding | Refills per `computeALBWRecoveryRate()` (~every 100 ms) |
| **Holding shield (RT/ZR or Z-target guard)** | **Paused** — meter does not passively refill |
| Perfect parry on block | Explicit grant: `dMeter2_addALBWFraction(1, 6)` (+~16.7% max) |
| Failed block (parry combat on) | Explicit penalty below + guard slip |

So today: **holding guard is expensive even if no enemy hits you** — you trade all passive refill for defensive posture.

### Current build — non-perfect blocks already hurt ALBW (on hit)

When `shieldParryCombat` is on and a shield hit is **not** a perfect parry, `dShield_onFailedGuardBlock()` runs **before** guard slip:

| Tier | ALBW penalty | Bash charges |
|------|--------------|--------------|
| Ordon / Wooden | **−10%** of `sOilMaxVar` (`1/10`) | −1 |
| Hylian | **−20%** of `sOilMaxVar` (`1/5`) | −2 |

Constants in `d_albw_shield.cpp` (`FAIL_ALBW_PENALTY_*`). If meter hits empty after drain → `procGuardBreakInit()`.

**This is ALBW meter damage, not heart damage.** Link HP is unchanged by failed parry economy (unless durability/shield break paths apply separately).

**Three block outcomes today** (parry combat on):

| Outcome | When | ALBW |
|---------|------|------|
| **Perfect parry** | Hit inside parry window | **+** fraction; no failed penalty |
| **Failed parry / normal block** | Hit outside window | **−** fraction (above) + slip |
| **Hold guard, no hit** | — | **No drain**, but **no passive recovery** |

### Phase 2 proposal (under `testingParryRework` only)

**Design swap:**

| | Shipping / rework Phase 1 | Rework **Phase 2** (test) |
|--|---------------------------|---------------------------|
| Hold guard, no enemies hitting | Recovery **paused** | Recovery **allowed** (same as not guarding) |
| Non-perfect shield hit | Large lump penalty (−10% / −20%) + slip | **TBD:** keep slip; use **smaller** ALBW hit (e.g. −3% / −5%) **or** replace lump with “small” tuned value |
| Perfect parry | +1/6 max + charge | Unchanged |

**Rationale:** RB parry encourages **holding** guard; the global recovery pause makes long guard holds feel doubly punishing (no refill + big tax on every miss). Phase 2 moves cost to **failed contacts only**.

**Implementation sketch (when ready):**

1. **`d_meter2.cpp`** — `dMeter2_isALBWRecoveryPausedByGuard()`: return false when `dShield_isTestingParryReworkEnabled()` (requires forward decl or small query in `d_albw_shield.h` to avoid circular includes).
2. **`d_albw_shield.cpp`** — `dShield_onFailedGuardBlock()`: when rework enabled, use **Phase 2 penalty constants** (separate from shipping `FAIL_ALBW_*`); shipping values unchanged when toggle off.
3. **Optional:** reduce or remove guard-slip **ALBW** double-punish if slip stays — failed block already drains; tune so “small damage” feels fair vs perfect parry reward.

**Not proposing (unless playtest asks):**

- HP damage on failed parry (would need new damage hook; out of ALBW economy).
- Changing recovery pause when **`testingParryRework` off** (shipping stays frozen-while-guarding).
- Phase 2 without Phase 1 (RB parry) — doc assumes same master toggle.

**Open tuning questions for Phase 2 playtest:**

1. Replace −10%/−20% entirely, or add a **smaller** failed-hit tax **in addition** to a lighter slip penalty?
2. Should **perfect parry** still grant +1/6 when recovery is no longer paused (probably yes)?
3. Should **idle +5% recovery boost** apply while holding guard in Phase 2 (probably yes if recovery unpaused)?
4. Separate sub-toggle `testingParryReworkAlbw` vs single setting with both phases — **recommend one toggle** until playtest splits them.

**Files (Phase 2 add-on):** `src/d/d_meter2.cpp`, `src/d/d_albw_shield.cpp`, doc/constants comments; no HUD draw changes expected (meter still shows same values; recovery just runs while guarding).

---

## Playtest checklist

1. **Toggle off:** Guard-onset parry unchanged; no RB parry; no extra spark/clang.
2. **Toggle on:** RT hold + RB tap opens window; bash still ZR+B only.
3. **Midna on RB:** No summon while guard up; Midna still works when not guarding.
4. **Cap parry:** 6/6 Hylian still gets spark **VFX** + clang **SFX** even without +charge.
5. **Keyboard:** Digital R bound separately from RT axis (Advanced Input).
6. **Z-target guard:** RB parry works without holding RT if vanilla lock-on guard is active.
7. **Build:** `build_run.bat` pass → build-analysis chat for FPS if any HUD-adjacent code touched.

### Phase 2 playtest (when implemented)

8. **Guard hold:** ALBW meter **ticks up** while holding RT guard with rework on (no enemies).
9. **Failed block:** ALBW drops by **new small fraction**; compare feel vs shipping −10%/−20%.
10. **Perfect parry:** Still grants +1/6; recovery pause off does not remove parry reward.
11. **Toggle off:** Recovery still pauses while guarding (shipping).

---

## Cross-references

- Shipping parry economy: [shield-combat.md](shield-combat.md) § Phase 4  
- Hero’s Shade / parry plumbing: [heros-shade-secret-boss.md](heros-shade-secret-boss.md)  
- Doc map: [build-fps-guidelines.md](build-fps-guidelines.md)

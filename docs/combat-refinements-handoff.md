# Combat refinements — handoff & backlog

**Purpose:** Track **field combat** refinements (not boss-specific scripts) so new chats can continue without reopening old sessions.

> Continue combat refinement work — read `docs/combat-refinements-handoff.md` first.

**Related:** [boss-fights-handoff.md](boss-fights-handoff.md) (boss layer), [shield-combat.md](shield-combat.md) (parry/guard), [albw-port.md](albw-port.md) (settings overview).

---

## Priority queue (2026-06)

| # | Topic | Status |
|---|--------|--------|
| 1 | **Enemy windup targeting** (ALBW strafe-guard exposure) | **In progress** — Bokoblin patch landed; playtest + roll out to other enemies |
| 2 | **Focused Arts** (Hidden Skill Rework) | **Tier API + test settings shipped** — combat hooks + shop rows + HUD next |

---

## 1. Enemy windup targeting

### Problem (confirmed)

Enemies (notably **Bokoblins**) can whiff when Link **strafes while holding ZR guard** without Z-target. This is **not** caused by Boss Refinement or ALBW edits to enemy AI — it is **vanilla TP windup aim-lock** made more visible by **Link-side ALBW shield** (`manualShieldButton()` / hold-to-guard without lock-on).

Boss Refinement (`game.bossRefinement`) does **not** touch field-enemy attack routines.

### Implemented (needs playtest)

| Enemy | File | Change |
|-------|------|--------|
| **Bokoblin** (`E_OC`) | `src/d/actor/d_a_e_oc.cpp` | Track Link during club windup (frames `< 22`), same turn rate as `executeFind` chase |

**Playtest matrix (Bokoblin — required before rolling out pattern):**

1. Stand still, block — should feel **vanilla** (guard bounce OK).
2. Z-target strafe — slight improvement OK, not magnetism.
3. **Hold ZR, no Z-target, circle** — fewer whiffs, still dodgeable.
4. Late dodge during commit phase — enemy should still whiff **on purpose**.

Build: `build_run.bat` → `build/windows-msvc-relwithdebinfo/dusklight.exe`.

### Design rule for future enemy patches (not full homing)

**Do not** track through the entire attack animation — that reads as unfair and removes readable telegraphs.

**Default ALBW tuning idea:**

- If vanilla effectively stops tracking around **~50%** of windup, extend the correction window toward **~75% of anim length** (or last frame before hitbox active — whichever is **stricter**).
- Use **soft** `cLib_addCalcAngleS` rates (match chase/find), not instant snap.
- Stop when hitbox goes active **or** commit frame is reached.
- Optional later setting: Off / Subtle (default) / Strong.

### Backlog — vanilla gaps worth patching (same class as Bokoblin)

| Enemy | File | Vanilla behavior | Suggested starting window |
|-------|------|------------------|---------------------------|
| **Stalfos** fast slash | `d_a_e_sf.cpp` `e_sf_attack_0` | No rotation frames 0–22 | Track ~0–16 (~75% of windup to hit) |
| **Dynalfos** quick knife | `d_a_e_dn.cpp` `e_dn_attack_0` | No track; hits 22–28 | Track until ~frame 20 |
| **Redead** club | `d_a_e_rd.cpp` `e_rd_fight` mode 2 | Frozen aim during swing 44–47 | Track through ~frame 46 |
| **Leever** rush | `d_a_e_hp.cpp` `executeAttack` | One-shot aim at rush start | Track first half of charge case |
| **Bulblin** spear | `d_a_e_bs.cpp` `e_bs_attack` | Tracks `< 10`, hits 13–20 | Extend to ~frame 12–13 |

**Likely OK without changes:** Wolfos (`E_WS` live joint aim), White Wolfos (`E_WW`), King Bulblin (`E_KK`), Ook (`E_MF`), Guay (`E_GM`), Stalchild leap prediction (`E_SW`).

**Separate perception issue:** Many enemies use `checkPlayerGuard()` guard-bounce — more frequent with ALBW hold-guard; can look like a “miss” but is correct block feedback.

### Key files

| Area | Files |
|------|-------|
| Bokoblin AI | `src/d/actor/d_a_e_oc.cpp` |
| Link guard (ALBW exposure) | `src/d/actor/d_a_alink_guard.inc`, `d_a_alink.cpp` |
| Meter recovery pause while guarding | `src/d/d_meter2.cpp` |

---

## 2. Focused Arts — design spec (canonical: design thread 2026-06)

**System name:** **Focused Arts**

**Setting:** `game.hiddenSkillRework` (UI label: **Hidden Skill Rework**) + `game.focusedArtsTest` (**Focused Arts Test**, requires rework) + `game.focusedArtsCheat` (**Focused Arts Cheat**, requires test; playtest only)

**Description:** *A further reworking of the Hidden Skills system that challenges players to test their combat skills with the Focused Arts system.*

| Setting | When Off |
|---------|----------|
| **Hidden Skill Rework** | Vanilla TP hidden skills + vanilla ALBW meter only — **no Focused Arts**, no rework hooks. |
| **Focused Arts Test** | Rework hooks only (Helm bash, JS charge gate, etc.) — **no FA meter/bank/spend**. |
| **Focused Arts Cheat** | FA tiers come from **save/shop** (`dFocusedArts_getPurchasedTier()`) only. |
| **Wolf Link Combat** | 1-heart **Mortal Draw finisher** wolf section = vanilla MD (no forced wolf / bite buff). |

**Coexistence:** Focused Arts runs **alongside** existing Hidden Skill Rework behavior. FA **only overrides** at the specified **tier spend columns** — not a full replacement of rework hooks until those spend moments.

**When ON (test + tiers), before bank:** HS use **HS ALBW New tweaks** column (40% new base, etc.) — not full vanilla.

**HS-specific lockout:** **Removed.** Hidden Skills are **never stripped** by Focused Arts.

**ALBW empty-meter lockout:** **Remains.** Blocks **sword + HS** per existing ALBW rules. Distinct from old HS lockout design.

---

### Damage formula (melee HS except Ending Blow)

**Design doc numbers use Ordon-equivalent bases only to explain the math.** At runtime, **all Focused Arts damage resolves from the current equipped sword’s HS baseline**, then FA stacks, spend columns, finishers, and the **upcoming hardcoded sword damage multiplier** (may reduce or increase per sword). Do **not** hardcode Ordon literals in combat code.

**Structure (per skill, per current sword):**

- Let **V** = that skill’s **vanilla HS attack value for the sword Link is holding** (after existing sword-type rules, before FA).
- **New base** = **0.4 × V**
- **Per charged tier add** = **0.2 × V** (same flat step each tier — e.g. Ordon Back Slice: V=30 → new 12, +6 per tier)
- **T1 / T2 / T3 charged** = new base + 1 / + 2 / + 3 tier steps
- Spend columns: **Maintain stacked damages** (+ finisher bonuses where listed)

**Ordon examples (documentation only):**

| V (Ordon) | New base | + per tier | T1 | T2 | T3 |
|-----------|----------|------------|----|----|-----|
| **30** | **12** | **+6** | **18** | **24** | **30** |
| **200** | **80** | **+40** | **120** | **160** | **200** |

**Ending Blow:** inst-kill track — no tier ladder (still subject to sword/multiplier where applicable for add-ons only, e.g. GS AOE finisher references **current sword’s** Great Spin value).

---

### Focused Arts table (PDF + formula)

**Columns:** HS Vanilla | HS ALBW Vanilla | HS New tweaks | T1 charged | T2 charged | T3 charged (**Begin spend**) | T3 spend | T2 spend | T1 spend (**Special Finishers**)

#### Ending Blow

| Vanilla | ALBW Vanilla | New tweaks | T1 chg | T2 chg | T3 chg | T3 spend | T2 spend | T1 spend |
|---------|--------------|------------|--------|--------|--------|----------|----------|----------|
| Inst kill | Inst kill; **½ base ALBW** | — | Inst kill | Inst kill | Inst kill; **Begin spend** | Inst kill; no ALBW | Inst kill; no ALBW | **Finisher:** GS AOE **50%** on contact |

#### Shield Attack (bash damage by FA stage — see spend rules)

| Vanilla | ALBW Vanilla | New tweaks | T1 chg | T2 chg | T3 chg | T3 spend | T2 spend | T1 spend |
|---------|--------------|------------|--------|--------|--------|----------|----------|----------|
| Bash | Parry system | **+5** bash + **+10** HS → **15** dmg | 15×1 | 15×1 | **Begin spend**; 15×1 | 15×2 | 15×3 | 15×5 → reset **15**; **+10% shield durability** |

#### Back Slice / Helm Splitter / Mortal Draw / Jump Strike / Great Spin

See formula table. PDF column semantics:

- **T3/T2 spend:** Maintain stacked damages; **no ALBW spend** where PDF says so.
- **MD T1 finisher:** **150%** (new base) @ full HP (**−½ HP**); **300%** @ 1 HP + forced wolf (**10s or manual transform, whichever first**); wolf bite buff under **Wolf Link Combat** (~2 bites → 2 hearts).
- **JS T1 finisher:** Maintain stacked damages; **ALBW → empty-meter lockout**; items **+300% + current damage bonus** until lockout ends (see below).
- **GS T1 finisher:** **Hurricane Spin** sustained proc — prototype **complete** (`game.hurricaneTest`); FA integration: tier-3 **T1 spend** → `procCutGsHurricaneInit()` + FA damage on Great Spin ATP (§ Hurricane Spin below). Fallback design was +150% single spin if product rejects hurricane.
- **Back Slice T1 finisher:** Maintain stacked damages + **3s no ALBW spend**; reset to **new base (0.4×V)** after.
- **Helm T1 finisher:** **150%** + shield bashes charged to max.

**Mortal Draw ALBW Vanilla:** spends **full base ALBW meter** (others mostly **½ base**).

#### ALBW damaging items

| | Notes |
|--|--------|
| Vanilla bases | **Research before implementation** (per-item `SetAtAtp` / `mAttackPower`) |
| T1–T3 charged | **+50%** attack per charged tier (stacks) |
| T3/T2 spend | Maintain stacks (**150%**); normal ALBW spend |
| T1 spend | Maintain stacks **+ another 50%**; no ALBW spend |
| After spending | **Stack bonuses reset** |

---

### Shop tiers → player max charges

Purchased from shop: **Focused Arts tier 1 / 2 / 3** (Postman shop **Upgrades & Services** page when FA test is on).

**Shop tier API** (`d/d_focused_arts.cpp` — shop calls the same entry points as playtest):

| Function | Purpose |
|----------|---------|
| `dFocusedArts_canPurchaseShopTier()` | FA enabled and save tier &lt; 3 (cheat does not block real purchases) |
| `dFocusedArts_tryPurchaseShopTier()` | Increment save tier by 1 (caller deducts rupees) |
| `dFocusedArts_getPurchasedTier()` | Save-backed tier 0–3 |
| `dFocusedArts_getEffectiveTier()` | Cheat → 3 when test+cheat on; else purchased |
| `dFocusedArts_getShopTierPrice(tier)` | Placeholder prices: 500 / 1500 / 3500 |
| `dFocusedArts_getShopTierName/Desc(tier)` | Shop copy for tiers 1–3 |

**Playtest:** `game.focusedArtsCheat` — **Off** / **On** (tier 3) / **WithDebug** (tier 3 + in-game overlay).

### How to playtest (v0)

1. **Settings → Gameplay:** Hidden Skill Rework **ON** → Focused Arts Test **ON** → Focused Arts Cheat **FA Cheat ON** or **With Debug**.
2. Equip **Ordon / Master / Light** sword (wooden = no FA meter).
3. Load field with **Focused Arts Cheat → With Debug** — FA overlay appears top-right (no dev console).
4. Hit enemies with sword: Ordon +2 fill/step (6 hits → 1 charge), Master/Light +1 (12 hits → 1 charge). Bombs/arrows +2 per connect.
5. Bank to **3/3** (with cheat), use hidden skills in order — overlay shows spend columns **T3 → T2 → T1**. Partial bank + HS resets fill only.
6. Take unguarded damage — partial fill drains to 0.

**Wired (2026-06-20):** FA damage resolver (`dFocusedArts_resolveMeleeDamage`), item tier stacks, T1 finishers (GS Hurricane, JS lockout, Back Slice ALBW suppress, MD/Helm damage), Postman shop tier row.

**Still open:** MD forced-wolf @ 1 HP, bank +1 SFX, item vanilla attack table research.

| Purchased | Max bank | Damage while banking | Spend order (high → low) | Special Finishers |
|-----------|----------|----------------------|--------------------------|-------------------|
| **Tier 1 only** | **1** | **T1 charged** only (new base + 1× tier step for **current sword**) | **1 spend** → **T1 spend** column (no finisher perks) | **No** |
| **Tier 1 + 2** | **2** | **T1 + T2 charged** stacks | **2nd spend** → **T2 spend**; **1st spend** → **T1 spend** (incl. T2 spend no-ALBW behavior) | **No** |
| **Tier 1 + 2 + 3** | **3** | Full **T1→T2→T3 charged** ladder | **T3 spend → T2 spend → T1 spend** | **Yes** on **T1 spend** |

**Special Finishers** require **tier 3 Focused Arts purchased** (even if spend uses fewer banked charges).

---

### Charge meter (fill & bank)

| Source | Rate |
|--------|------|
| Connected **sword** hit | **Sword-dependent** (see below) |
| Connected **damaging item** hit | **+1/6** |
| Miss sword/item | No fill, no drain of **bank** |
| **Parry** (success or fail) | No fill, no drain |
| **Damage taken** (not block/parry/guard) | Drains **current fill** quickly (UI shows drop); **bank unchanged** |
| **Healing** | Does not fill |

**Sword-specific sword fill (intentional tradeoff: higher-damage swords fill slower):**

| Sword | Hidden skills | FA meter per connected sword hit |
|-------|---------------|----------------------------------|
| **Wooden** | **None** | **No FA meter** (system inactive) |
| **Ordon** | Yes | **+1/6** |
| **Master** | Yes | **+1/12** |
| **Light** | Yes | **+1/12** *(matches Master)* |

**Banking:** Tier meter must reach **full (12/12)** to **bank +1 charge** (up to purchased max). Each charge requires its **own** full 12/12 cycle. **Partial fill persists** in UI.

**During ALBW empty-meter lockout:** **No** sword fill; **items still** +1/6; bank/fill loss rules otherwise apply.

---

### Spend rules

**Begin spend:** Player has **banked charges = purchased max** for their shop tier **and** relevant tier meters banked as above.

**What spends a Focused Arts charge:**

| Action | Spends FA charge? |
|--------|-------------------|
| **Hidden Skill** (during spend sequence) | **Yes** |
| **Damaging item** (during spend sequence) | **Yes** |
| **Shield bash** | **Only** **connected bash** at **T1 spend Special Finisher** when **tier 3 purchased** |
| Other bashes | **No** — bash damage follows FA **stage column** (15×1 … ×5); without tier 3 finisher, bash behaves like **T1 charged** (15×1) |

**HS use vs fill:** Using HS **resets current tier fill progress** (e.g. 7/12 → 0). **Does not** remove **banked** charges.

---

### ALBW empty-meter lockout & items

**Normal ALBW lockout** (any cause): **FA item stacks remain** — tier stacked damage + item stack bonuses (simpler rule).

**Jump Strike Special Finisher** (T1 spend, tier 3 purchased):

1. Spends **last banked FA charge** → **no FA stacks** on that window.
2. **ALBW → empty-meter lockout** (existing code path).
3. While active: damaging items = **+300% + current damage bonus** (supplants FA stacks; aligns with existing lockout item boost in code).
4. Bonus **ends when meter exits lockout**.
5. If player **banks a new FA charge/tier during** that JS lockout window → **JS +300% bonus ceases** immediately.

**During any ALBW lockout:** sword + HS **blocked**; wolf/items per ALBW rules.

---

### UI brief (Claude)

- **Pips (1–3):** banked charges — **sticky** (once pip 1 on, stays until spent); no flicker on whiff/spam.
- **Arc / partial meter:** **1/12** (sword) or **1/6** (item) progress toward next bank; visible drain on hit.
- **No HS-lockout overlay** — optional indicator when **ALBW meter lockout** active.
- **Spend-ready** when bank = purchased max (and tier 3 bought → finisher available on final spend).
- Setting gate: **`game.hiddenSkillRework`** + **`game.focusedArtsTest`**.

---

### Great Spin finisher research (handoff for another agent)

**Task:** Research vanilla TP **hurricane spin** / Great Spin finisher (procs, anims, damage, radius) in Dusklight vs **+150% placeholder** in table.

**Files:** `src/d/actor/d_a_alink_cut.inc` (`procCutTurnInit`, `checkCutLargeTurnState`, `ANM_CUT_TWIRL` / large turn), `src/d/d_albw_combat.cpp`, `src/d/d_meter2.cpp`.

**Deliverable:** Short appendix under this §2 — recommend hurricane reconstruction **or** keep +150% until art pass.

#### Appendix — research complete (2026-06-20)

**Naming clarification:** *Hurricane Spin* is a **Wind Waker** technique (Orca / Knight's Crests; hold spin charge → multi-second mobile whirlwind; magic cost; dizzy after). **Twilight Princess has no Hurricane Spin.** TP's equivalent hidden skill is **Great Spin** (Hero's Spirit / event `F_0344`) — a **single-revolution** enhanced spin, not a sustained whirlwind. The handoff “hurricane” label refers to the **WW-style finisher fantasy** for Focused Arts GS T1 spend, not an existing TP move.

**Is WW Hurricane Spin present in Dusklight files?**

| Search | Result |
|--------|--------|
| `hurricane` / `Hurricane` string | **Zero hits** outside this doc |
| WW-specific procs / anims / magic hook | **None** — no ported WW Link spin code or assets |
| Closest TP analogue | **Great Spin** (`checkCutLargeTurnState` → `CUT_TYPE_LARGE_TURN_*`) |

**What TP *does* ship (usable today):**

| Piece | Location | Behavior |
|-------|----------|----------|
| Spin charge + move | `procCutTurnChargeInit` → `procCutTurnCharge` → `procCutTurnMoveInit` → `procCutTurnMove` | Hold sword while charging; Link can **move** during charge (`ANM_CUT_TURN_CHARGE`, `ANM_CUT_TURN_CHARGE_MOVE`). Release → **one** spin via `procCutTurnInit`. |
| Great Spin attack | `procCutTurnInit` when `checkCutLargeTurnState()` | `CUT_TYPE_LARGE_TURN_LEFT/RIGHT`, ATP **4** (~**200** dmg via `at_power_get`), larger sphere radius (`mLargeAttackRadius` / `mLargeAttackAccel`), voice `Z2SE_AL_V_KAITENGIRI_L`, slash `Z2SE_AL_KAITEN_L_SLASH`, particles `ID_ZI_J_KAITENGIRIL_*` (6 emitters) in `d_a_alink_effect.inc`. |
| Great Spin gate | `checkCutLargeTurnState()` | Event `F_0344` (or training flag) **and** full HP; Dusk cheat `game.alwaysGreatspin` bypasses HP gate. |
| Normal spin | Same proc, no large-turn flag | ATP **3** (~**30** dmg), smaller `mAttackRadius`. |
| Hero's Shade teach | `daNpc_Kn_c::teach07_*` (`superTurnAttackWait`) | Training set-piece reacts to `CUT_TYPE_LARGE_TURN_*`; not a separate player proc. |
| `ANM_CUT_TWIRL` | `d_a_alink_cut.inc` | **Jump Strike / uppercut** finisher — **not** Great Spin. |

**WW Hurricane ≠ TP Great Spin (gameplay):**

| | WW Hurricane | TP Great Spin |
|--|--------------|---------------|
| Duration | ~5–10 s multi-revolution | ~1 spin (~47 f anim) |
| Movement | Mobile whirlwind; **fixed forward speed** while stick steers direction (`procCutRoll`) | Brief forward speed during hit window only |
| Cost | Magic bar | Full-HP passive upgrade (ALBW meter spend added on PC) |
| Aftermath | Dizzy / vulnerable | Returns to normal move proc |

**Can a Hurricane-style finisher be recreated in Dusklight?**

**Yes — but only as new ALBW/Focused Arts code**, not by enabling dormant WW assets.

| Approach | Effort | Notes |
|----------|--------|-------|
| **A. Keep +150% placeholder** | **Low** | On FA GS **T1 spend** (tier 3 purchased): multiply Great Spin damage (`SetAtAtp` / FA resolver) by **2.5×** on the existing single `procCutTurn`. No new anims; reads as “stronger Great Spin.” **Recommended for first implementation pass.** |
| **B. Extended spin proc (TP assets)** | **Medium** | New proc (e.g. `PROC_CUT_GS_HURRICANE`) forked from `procCutTurnMove` + `procCutTurn`: loop spin anim or re-enter `procCutTurnInit` on a timer; keep `mAtSph` active with periodic `ResetAtHit`; reuse `KAITENGIRIL` FX; optional end stumble via existing tired/wait proc. **No new art required** if single-spin BCK is looped or chained. |
| **C. WW port** | **High / not recommended** | Import WW BCK, SE, magic UI — out of scope for TP decomp. |
| **D. Reference: Zant last-phase roll** | Research only | `daB_ZANT_c` last fight modes **11–13**: sustained `shape_angle.y` spin + `mRollCc` + fatigue anim — proves engine supports **multi-frame spin + dizzy**, but on boss skeleton, not Link. Pattern reference only. |

**Implementation hooks for Focused Arts GS T1 finisher:**

1. Gate: `dAlbw_isHiddenSkillReworkEnabled()` + FA tier-3 T1 spend path (not yet wired).
2. Entry: from `procCutTurnMove` when `checkCutLargeTurnState()` and finisher armed → branch to extended proc instead of single `procCutTurnInit(0,1)`.
3. Damage: FA table says **+150% attack** placeholder; resolver should use **current sword’s Great Spin baseline (V)** × FA stacks × **2.5** (or tier column), not hardcoded Ordon literals.
4. Ending Blow cross-ref: GS AOE finisher uses **50% of current sword Great Spin value** — keep `CUT_TYPE_LARGE_TURN_*` / `at_power_get` ATP 4 path as the shared damage reference.

**Recommendation**

| Phase | Ship |
|-------|------|
| **Now (Focused Arts v1)** | **Approach A** — +150% on existing Great Spin proc; document as “Great Spin Finisher” in UI, not “Hurricane Spin.” |
| **Later polish** | **Approach B** if product wants WW-like **duration + mobility**; schedule after FA core + meter UI land. **→ Prototype shipped as PC test cheat (2026-06-19); ready for FA wiring.** |

**Open for product**

- Should finisher **require** vanilla Great Spin conditions (full HP / `F_0344`) or bypass when FA spend fires?
- Is **post-spin vulnerability** (WW dizzy) desired for balance, or skip for ALBW pacing?

#### Coding chat answers — v4 Hurricane Spin briefing (2026-06-20)

Source-level answers to the nine feasibility questions. **FA charge bank is not implemented yet** — only `game.hiddenSkillRework` toggle exists (`d_albw_combat.cpp`). Charge consume-on-activation is a design/API choice when FA lands, not a constraint of existing code.

| # | Question | Answer |
|---|----------|--------|
| **1** | Directional input during finisher proc (A1)? | **`procCutTurnMove` already reads stick input** via `checkInputOnR()` — accel/decel, direction anims (`ANM_CUT_TURN_CHARGE`, `ANM_CUT_TURN_CHARGE_MOVE`), `mMoveAngle` steering. This is the **charge** phase, not the attack. **`procCutTurn` (actual spin hit) does not read stick** — only applies `mNormalSpeed` burst + decel. **A1 is feasible** in a **new finisher proc** by reusing `procCutTurnMove`'s movement block while looping spin attack logic; risks: `checkDamageAction()` / `checkNextAction()` must be gated; `execute()` stick→`mMoveAngle` still runs if proc flags allow. **A2 fallback** remains safe: capture `mNormalSpeed` + `current.angle.y` at init, apply `setNormalSpeedF` decay without stick. |
| **2** | Forward burst — proc velocity or BCK root motion? | **Proc velocity.** `procCutTurn` sets `mNormalSpeed = mpHIO->mCut.mCutTurn.m.mSpeed` (default **10.0**) during attack frames, then `cLib_chaseF` decel via standard Link movement integration. **Not dependent on BCK root motion** — suppress by zeroing/skipping that assignment in finisher proc. |
| **3** | Unused continuous spin BCKs in `AlAnm.arc`? | **No dedicated multi-second hurricane BCK found.** Spin family: `CUTT` / `CUTTB` (single revolution ~47/42 f), `CUTTP` + `CUTTPWFB` / `CUTTPWLR` (charge/move — **already loop** in `procCutTurnMove` via `frameCtrl->setLoop`). `SHCROUCH` is in `l_mainBckData` with **0xFE unused slot** — **zero gameplay references** (TCRF WW holdover confirmed). `HS_SPIN` is **Hero's Shade hookshot item** BCK (`d_a_alink_hook.inc`), not Link body spin. |
| **4** | Loop Great Spin BCK ~300 f or new proc? | **New proc required** (e.g. `PROC_CUT_GS_HURRICANE`). Vanilla `procCutTurn` plays **once** until `checkAnmEnd` (~47 f). Looping is engine-supported (`setLoop` / `EMode_LOOP` — see charge phase). Implement timer (~300 f) + looped `ANM_CUT_TURN_LEFT/RIGHT` (BCK `CUTT`/`CUTTB`) + periodic `mAtSph.ResetAtHit()`. Do **not** extend vanilla `procCutTurn` in place — too many exit paths. |
| **5** | Spinner ring collision radius? | **`daSpinner_c::mCyl` runtime: `SetR(58.0f)`** default; **100.0f** in boosted mode. Src template `l_cylSrc` uses **40.0f** (overridden at runtime). Outer wall feel uses **~130** (`mAcchCir[1].SetWall(129.99f, 35.0f)`). **Zant roll** (`mRollCc`): **R=200, H=250** during last-phase spin. **Recommend ring hitbox R≈58–100** (Spinner gameplay), tune separately from sword sphere (`mLargeAttackRadius` default **400**). |
| **6** | Exhaustion / dizzy BCK? | Use existing **`PROC_TIRED_WAIT`**: `procTiredWaitInit()` → `ANM_WAIT_TO_TIRED` then loops `ANM_WAIT_TIRED`. Already used for ALBW lockout tired idle. **`procTiredWait()` blocks via `checkNextAction(0)`** until anim completes — matches “no input during exhaustion.” Hit-cancel: **`checkDamageAction()`** already preempts most procs → damage anim; gate exhaustion proc to allow that. **No WW-specific dizzy BCK** on Link. Alternative: `ANM_STEP_TURN_TIRED` if a shorter stumble is desired. |
| **7** | FA consume-on-activation? | **Not implemented yet.** No charge bank code in tree. When FA is built, **consume-on-activation is trivial** — call spend API at finisher `Init`, not `Execute` end. Document explicitly in FA module; no engine blocker. |
| **8** | WW movement physics present? | **No WW player movement module** (`WindWaker` / `WWLink` — zero hits). Closest TP-native patterns: **`procCutTurnMove`** charge movement (stick + `mNormalSpeed`); **`daSpinner_c::posMove`** (item actor `speedF`, not Link). **Build Spinner-like feel from `procCutTurnMove` + `setNormalSpeedF`**, not by importing WW code. |
| **9** | Zant spin vs Link procs? | **Entirely independent.** Zant last phase modes **11–13** in `d_a_b_zant.cpp`: boss `speedF` + `shape_angle.y` spin, **`mRollCc`** cylinder (`cc_zant_roll_src`), fatigue **`BCK_ZAN_SW_FATIGUE`**. **Does not call** `procCutTurn*` or share Link procs. **Already reacts to Link `CUT_TYPE_LARGE_TURN_*`** on `mRollCc.ChkTgHit()` (sword bounce / `field_0x6fd` counter). Hurricane **ring counter** = new **`AT_TYPE`** on Link's ring cyl + Zant-side branch in spin modes — **not** a shared-proc shortcut. Thematic only. |

**Briefing correction:** The doc assumes `procCutTurnMove` “almost certainly locks input.” **Incorrect for charge phase** — it actively supports directional steering. The **attack** proc (`procCutTurn`) is planted/decel-heavy; finisher needs a **dedicated proc** combining movement + sustained hitbox either way.

**I-frames (briefing addendum):** No existing infinite-I-frame spin on Link. Pattern: early-out in `checkDamageAction()` when `mProcID == PROC_CUT_GS_HURRICANE` (same class as `checkEventRun()` guard), or keep `mDamageTimer > 0` artificially — former is cleaner.

**Yellow sword glow:** Charge phase already runs `simpleAnmPlay(m_nSwordBtk)` in `procCutTurnMove` — reuse for finisher visual; `KAITENGIRIL_*` particles already bind to `CUT_TYPE_LARGE_TURN_*`.

**Default Great Spin tuning (HIO `daAlinkHIO_cutTurn_c0::m`):** attack radius 260 / large 400; large accel 35; spin burst speed 10; charge max speed 5.

---

### Implementation hooks

| Area | Files |
|------|-------|
| Rework toggle | `d/d_albw_combat.cpp` — `dAlbw_isHiddenSkillReworkEnabled()` |
| Focused Arts tiers + meter API | `d/d_focused_arts.cpp` — `dFocusedArts_*()` |
| Settings | `game.focusedArtsTest`, `game.focusedArtsCheat` in `src/dusk/settings.cpp`, UI in `src/dusk/ui/settings.cpp` |
| ALBW lockout | `d/d_meter2.cpp`, `d/d_albw_lockout.cpp` |
| Cut / HS procs | `d/actor/d_a_alink_cut.inc` |
| Shield / bash | `d/d_albw_shield.cpp` |
| Wolf / MD finisher | `d/d_albw_wolf_stun.cpp`, `d/d_cc_uty.cpp` |
| Shop (future) | `d/d_albw_rental.cpp` — call `dFocusedArts_tryPurchaseShopTier()` per tier row |
| Settings copy | `src/dusk/ui/settings.cpp` |

---

### Implementation status (2026-06-20)

| Component | Status |
|-----------|--------|
| Settings (`focusedArtsTest`, `focusedArtsCheat`) | ✅ Wired under Hidden Skill Rework |
| Shop tier API (save reg 103, cheat override) | ✅ `d_focused_arts.cpp` |
| Runtime meter fill / spend / combat hooks | ✅ v0 — sword/item fill, HS spend, damage drain, ImGui overlay |
| FA damage resolver + item stacks | ✅ `dFocusedArts_resolveMeleeDamage`, `dFocusedArts_applyItemDamageBoost` |
| Postman shop rows | ✅ FA tier on Upgrades & Services (`d_albw_rental.cpp`) |
| HUD bars | ✅ Lilac FA meter in `d_meter2_draw.cpp` |
| GS finisher FA wiring | ✅ Tier-3 T1 spend → `procCutGsHurricaneInit` (+ cheat fallback) |
| T1 finishers (JS/BackSlice/MD/Helm/EB) | ✅ v1 — JS lockout drain, Back Slice 3s suppress, MD/Helm damage, EB GS AOE 50% |
| Item vanilla attack table | ⏳ Research before per-item baseline tuning |

---

### Still open

- **Item vanilla attack table** (Focused Arts row).
- ~~**Ending Blow** GS AOE finisher (50% on contact)~~ — wired on EB stab contact (T1 spend).
- **Mortal Draw** forced wolf @ 1 HP finisher — deferred (damage mult wired).
- **Hardcoded sword damage multiplier** — tune alongside FA resolver (may reduce sword damage globally).
- **Bank +1 SFX** — optional polish.

## Hurricane Spin prototype (PC test cheat)

**Status: COMPLETE (2026-06-19)** — playtested; FA tier-3 T1 spend launches hurricane when bank is full (`dFocusedArts_shouldLaunchGsHurricaneFinisher`). Cheat `game.hurricaneTest` remains for QA without FA spend.

### Message for FA / Focused Arts chat

> **Great Spin finisher (Hurricane Spin) — prototype is working**
>
> The extended Great Spin finisher is implemented and playtested. **FA integration:** tier-3 purchased, bank full, T1 spend column → hurricane proc (or `game.hurricaneTest` cheat for QA).
>
> **How to try it:** enable **Hurricane test**, hard stick L/R + tap sword → ~5 s sustained spin, then ~2 s tired lockout.
>
> **What ships in the prototype:**
> - `PROC_CUT_GS_HURRICANE` — frozen attack-frame BCK + manual Y spin, stick-driven movement (spinner-like), Great Spin hitbox, i-frames, lock-on orbit
> - `PROC_CUT_GS_HURRICANE_TIRED` — `ANM_WAIT_TO_TIRED` / `ANM_WAIT_TIRED`, 60 f no input, then normal control
> - **Audio (locked):** Zant spin 1.05× + Gale tornado 0.85× @ 25% + spinner ride @ 35%; one-shot Link scream `Z2SE_AL_V_FALL` at start (not looped)
> - **VFX:** `game.hurricaneTestVfx` — **Base VFX** (default) = horizontal great-spin ring; **Whirlwind** = upright spray variant
>
> **Key files:** `src/d/actor/d_a_alink_hurricane.inc`, `src/dusk/hurricane_test.cpp`, `src/d/actor/d_a_alink_effect.inc` (`setCutTurnEffect`), `src/dusk/audio/DuskDsp.cpp` (SE loop hack), voice gate in `d_a_alink.cpp` (`isLinkHurricaneProc`).
>
> **Suggested FA wiring (when charge bank exists):**
> 1. Replace cheat entry (`d_a_alink.cpp` instant-spin path gated by `isHurricaneTestEnabled()`) with FA tier-3 T1 spend + `procCutGsHurricaneInit(direction)`.
> 2. Apply FA damage multiplier on existing Great Spin ATP path (`initCutTurnAt` / `CUT_TYPE_LARGE_TURN_*`) — do not hardcode Ordon values.
> 3. Decide whether to keep test cheat for QA or fold settings into a dev menu only.
> 4. **Bypass vanilla Great Spin gates (`F_0344` / full HP) when FA tier-3 T1 spend fires** — confirmed product decision.
>
> Full tuning tables below. Ping this doc before changing audio/VFX defaults — those were iterated heavily.

---

Gated by `game.hurricaneTest` in settings. Entry: hard stick L/R + tap sword → `PROC_CUT_GS_HURRICANE` (~300 f, then `PROC_CUT_GS_HURRICANE_TIRED`: `ANM_WAIT_TO_TIRED` / `ANM_WAIT_TIRED`, **60 f** input lockout, then `procWaitInit()`). Code: `d_a_alink_hurricane.inc`, `dusk/hurricane_test.cpp`, `d_a_alink_effect.inc`.

### Prototype checklist (all done)

| Component | Status | Notes |
|-----------|--------|-------|
| Proc + movement | ✅ | Frozen BCK @ attack frame, stick steering, `mSpeedModifier = 0` |
| Hitbox / i-frames | ✅ | Great Spin radius, `ResetAtHit` each frame, damage proc gated |
| Exhaustion | ✅ | `PROC_CUT_GS_HURRICANE_TIRED`, 60 f input lockout |
| Audio mix | ✅ | Three-layer loop + one-shot scream; `DuskDsp` loop registration |
| VFX modes | ✅ | Base VFX (horizontal) default; Whirlwind alternate |
| Voice isolation | ✅ | Other Link voices + sword SE blocked during spin proc |
| Settings / speedrun | ✅ | `hurricaneTest`, `hurricaneTestVfx` |
| FA spend / UI | ⏳ | **FA chat** — no charge bank in tree yet |
| Forward momentum pull | ✅ | WW-lite: `l_hurricaneForwardBias` (4.0) + stick speed; init Great Spin burst (`mCutTurn.mSpeed`); neutral stick decels to bias, not 0 |

### Forward momentum pull (implemented 2026-06-19)

**Product ask:** During the spin, Link should have a **slight forward pull** so the move feels a little uncontrollable, while the stick still **fully steers direction**. Prototype today is spinner-pure: stick sets both speed and heading; neutral stick decelerates to zero.

#### How Wind Waker does it (`procCutRoll` — Hurricane Spin)

TWW names this **`PROC_CUT_ROLL` / `procCutRoll`**, not “hurricane”. Source: [zeldaret/tww `d_a_player_sword.inc`](https://github.com/zeldaret/tww/blob/master/src/d/actor/d_a_player_sword.inc) (`procCutRoll_init`, `procCutRoll`, `procCutRollEnd_init`).

| Mechanic | WW behavior |
|----------|-------------|
| **Steering** | If stick magnitude > ~0.05, `current.angle.y` lerps toward stick bearing (`m34E8`); `shape_angle.y` follows. Player picks **direction**, not speed. |
| **Forward pull** | While attack timer `mProcVar0.m34D0 > 0`, **`mNormalSpeed = m_HIO->mCut.mCutRoll.m.field_0x14` every frame** — fixed cruise speed, **not** stick-scaled. Releasing stick does **not** stop drift; Link keeps sliding in the last steered direction. |
| **Spin visuals** | Separate accumulator `m34EC` (visual Y spin) advances by HIO spin rate **plus** frame turn delta — turning while moving affects how “wild” the spin reads. |
| **End** | Timer hits 0 → speed decays (`cLib_addCalc` toward 0) → `procCutRollEnd` dizzy (`ANM_WAITQ` + stars). Wall bonk can crash out early. |

So WW’s “uncontrollable” feel is mostly **mandatory forward velocity**: you steer the vector but cannot brake mid-whirlwind. It is stronger than a “slight” pull — closer to constant ~run-tier drift (HIO-tuned; not the same as roll’s 26 u/f).

#### Dusklight prototype today (`hurricaneApplyMovement`)

File: `d_a_alink_hurricane.inc`.

| | Prototype | WW Hurricane |
|--|-----------|--------------|
| Direction | Stick → `current.angle.y` via `mMoveAngle` | Same pattern |
| Speed | `mMaxSpeed * mMoveValue` (spinner ride cap) | Fixed HIO speed while timer active |
| Neutral stick | Decel to 0 (`cLib_addCalc` 12 / 4) | **No** — keeps full `field_0x14` |
| Feel | Full control, can “park” the spin | Committed drift, direction-only control |

TP reference points for tuning (same file family): **`procCutTurn`** one-shot burst `mCutTurn.m.mSpeed` (default **10.0** during attack frames); **`procCutTurnMove`** stick charge accel/decel — not sustained cruise.

#### Recommended Dusklight implementation (when requested)

**Goal:** WW-style **direction steering + minimum forward commit**, but **lighter** than WW’s full lock speed so ALBW still feels controllable.

1. **Split direction from speed** (match WW structure):
   - Stick always steers `current.angle.y` (keep existing `cLib_addCalcAngleS` toward `mMoveAngle`).
   - Do **not** multiply cruise speed by `mMoveValue` alone for the baseline drift.

2. **Add forward bias floor** (WW-lite, suggested starting point — playtest):
   ```text
   stickSpeed   = mMaxSpeed * mMoveValue          // current spinner cap
   forwardBias  = 3.0f ~ 6.0f                     // tune; TP Great Spin burst is 10.0
   targetSpeed  = forwardBias + stickSpeed        // OR max(forwardBias, stickSpeed)
   ```
   - **`forwardBias + stickSpeed`**: always slides even at neutral stick (WW-like commit, but bias can stay lower than WW’s full cruise).
   - **`max(forwardBias, stickSpeed)`**: softer — neutral = slight pull, hard stick = faster (less “out of control”).

3. **Neutral stick decel**: Replace “ramp to 0” with “ramp to `forwardBias`” (or only decel excess above bias). Avoid full stop unless product wants parking.

4. **Optional polish** (WW fidelity): Couple manual `shape_angle.y` spin rate to `current.angle.y` delta (like `m34EC -= baseRate + turnDelta`) so sharp stick flicks read wilder without changing hitbox.

5. **Init burst**: Consider one-frame **`mNormalSpeed = mCutTurn.m.mSpeed`** on proc start (TP Great Spin lunge) **in addition to** sustained bias — sells the “you committed to this” opener.

6. **Tuning knobs:** `l_hurricaneForwardBias` (4.0) and init burst `mpHIO->mCut.mCutTurn.m.mSpeed` in `d_a_alink_hurricane.inc`.

**Status:** Implemented — WW-lite bias + stick steering. Tune `l_hurricaneForwardBias` in playtest if drift feels too weak or strong.

### Audio mix (locked baseline 2026-06)

Three-layer sustained mix on `mSoundObjSimple2` (Zant + tornado) and `mSoundObjSimple1` (spinner accent). One-shot voice `Z2SE_AL_V_FALL` on `mSoundObjSimple2` at proc start (after SE layers, not looped).

| Layer | SE | Object | Pitch | Volume | Loop |
|-------|-----|--------|-------|--------|------|
| Base | `Z2SE_EN_ZAN_CTL_SPIN_ATK` | simple2 | 1.05× | 100% | PC force-loop |
| Accent | `Z2SE_BOOM_TORNADO` | simple2 | **0.85×** (chosen) | 25% | PC force-loop |
| Accent | `Z2SE_AL_SPINNER_RIDE` | simple1 | 1.0× | 35% | PC force-loop |

Mix is hardcoded in `hurricane_test.cpp` (`kHurricaneSeMix`); no SE swap setting. PC loop hack: `DuskDsp.cpp` registers wave ARAMs; `hurricane_test.cpp` pins ADSR hold and excludes voice/scream channels.

### VFX

Great Spin particles (`KAITENGIRIL_*`) via `setCutTurnEffect()`. Frozen BCK at attack frame + manual `shape_angle.y` spin causes upright “sky spray” when emitter uses sword bone matrix + full `shape_angle`.

| Setting | `game.hurricaneTestVfx` | Behavior |
|---------|-------------------------|----------|
| **Whirlwind** | `WHIRLWIND` | Sword bone pos + `shape_angle`; local tilt table on **left spin only** (sound-setting era upright spray) |
| **Base VFX** (default) | `BASE` | Same sword anchor + `shape_angle`; **always** applies great-spin local tilt/trans (horizontal swing on both directions) |

## Building & process

Same as [boss-fights-handoff.md](boss-fights-handoff.md): `build_run.bat`, no drive stubs in `src/`, push to **upstream** when asked.

**Doc map entry:** add this file when touching field combat refinements; boss-only work stays in `boss-fights-handoff.md`.

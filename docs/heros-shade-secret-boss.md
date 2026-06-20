# Hero's Shade Secret Boss

**Status:** Research / feasibility — **NO code written yet**
**Scope:** A new post-game secret boss: after all Hidden Skills are learned, a final Hero's Wolf Shade appears, warps the player to the Hero's Shade arena, and this time you fight the Hero's Shade as a *real* boss (full health bar + victory condition) instead of a scripted lesson.
**Related:** [project-stargazer-mod (Shade Watcher)](../memory/project_stargazer_mod.md-equivalent), [boss-fights-handoff.md](boss-fights-handoff.md), [albw-boss-hp-hud-tuning-brief.md](albw-boss-hp-hud-tuning-brief.md), [suppress-enemy-spawns]
**Caveat:** All line numbers are point-in-time and WILL drift — re-verify against current source before editing.

---

## The idea

After the player has learned all 7 Hidden Skills (secret techniques), one last golden Hero's Wolf Shade appears in the world. Touching it warps the player to the Hero's Shade spirit arena one more time — but instead of another teaching duel, you fight the Hero's Shade properly: he has a real boss health bar, attacks aggressively, and can be defeated (victory condition + reward/flag).

This is the ONE feature that intentionally *extends* the vanilla Hidden-Skills questline, in contrast to the sibling Shade Watcher / Stargazer system which deliberately **clones** the gold wolf and avoids inheriting the questline.

---

## Feasibility verdict (high level)

**Feasible, and the integration seams are unusually clean.** The "framing" (detect skills → spawn final wolf → warp to arena → pick a duel behavior) is ~80% reuse of code that already exists or is already scoped. The cost concentrates almost entirely in **one place**: authoring a combat AI + defeat sequence for a new behavior mode of an actor that was *built to accept new modes*. There is no architectural fight.

| Beat | Concrete hook | Effort |
|---|---|---|
| Detect all skills learned | `F_0344` event bit ("Obtained 7 secret techniques") | trivial |
| Spawn final wolf | gold-wolf clone runtime spawn (Stargazer pattern, `d_s_room.cpp`), gated on `F_0344` | low |
| Warp to arena | wolf's own `dStage_changeScene` → SCLS exit → `F_SP200` | low |
| Select duel behavior | spawn `d_a_npc_kn` with param → `mType == 7` (existing free slot) | low |
| Gate the duel Shade | extend `isDelete()` with `F_0344` appear / new "duel cleared" del flag | low |
| **Deal damage to Shade** | add `mLife`; decrement in `tgHitCallBack`, gated on `mType == 7` | moderate |
| **Health bar UI** | `dComIfGs_BossLife_public_Set/Get` (same API Fyrus uses) | low–moderate |
| **Combat AI + defeat seq** | new `mType == 7` selector driving the existing move-methods | **the real work** |
| Don't leak into mod systems | classify duel-Shade in `dAlbwHP` `sBoss`/`sFinalBoss` | low |

---

## Code map (verified this session)

### 1. Detection — all skills learned
- `F_0344` = `0x2a20`, "Secret techniques — Obtained 7 secret techniques" — [include/d/d_save_bit_labels.inc:241]. This single event bit IS the gate. (`F_0338`–`F_0344` = skills 1–7 learned.)
- Do NOT confuse with: `F_0472`–`F_0477` = *distant howling* summons; `F_0492`–`F_0497` = gold-wolf *disappearance* flags (the per-session consume pattern, a model for a "duel cleared" flag).

### 2. The final wolf (appear + warp)
- Source to clone: `src/d/actor/d_a_npc_gwolf.cpp` (`daNpc_GWolf_c`).
- Vanilla wolf self-gates appearance on the howling flags via `l_warpAppearFlag[7]` (~:184); warps with `dStage_changeScene(l_warpIdList[mParamMode-1], ...)` (~:1629). Exit IDs 8/9/A/B/C resolve through the **current overworld stage's SCLS exit table** to the spirit arena.
- Runtime-spawn template (gated): the postman block in `src/d/d_s_room.cpp:252-263` (`#if TARGET_PC`, `fopAcM_create(...)` after a stage/room/event-bit gate). This is the established mod pattern for placing a new gated actor.

### 3. The arena
- `F_SP200` = "Wolf Howling Cutscene Map" — [include/dusk/map_loader_definitions.h:613-615], declared with **rooms 0–7** (`{0, {0,1,2,3,4,5,6,7}}`). This is the foggy spirit realm where every vanilla duel happens; it already loads `d_a_npc_kn`. Spare rooms → the final fight can occupy its own clean room.

### 4. The Hero's Shade actor — `d_a_npc_kn`
Files: `src/d/actor/d_a_npc_kn.cpp` (5284 lines) + `d_a_npc_kn_base.inc` (1187) + `d_a_npc_kn_teach01.inc` (757) + `d_a_npc_kn_teach02.inc` (630). Header: `include/d/actor/d_a_npc_kn.h`.

**It is an NPC, not an enemy.** No `mLife`, no HP pool, no defeat/death state anywhere in 7,858 lines.

**Behavior is parameter-selected:**
- `getType()` (~:749) decodes spawn param low byte: param 1–7 → `mType` 0–6 (the seven lessons); **param 0 or ≥8 → `mType` 7 (unused/default slot)**. ← natural home for a new combat mode.
- `getDivideNo()` / `getPath()` (~:771/781): high byte → clone-split count / path index.
- `getFlowNodeNo()` (~:776): from `home.angle.x` (placement data).

**Spawn gating per type — `isDelete()` (~:785):** appear/delete keyed on event bits `0x152`–`0x158` (`l_appearFlag[7]`/`l_delFlag[7]`). A duel mode slots in identically: appear-gate = `F_0344`, del-gate = new "duel cleared" flag.

**The make-or-break hit hook — `tgHitCallBack`** ([d_a_npc_kn_base.inc:6]): currently just reads the player's `getCutType()` (which skill move) and stores it via `setCutType()`. ZERO damage/HP logic. This is THE single function to extend: add `mLife`, decrement here gated on `mType == 7`, route to `mDmgStagger`.

**Damage-reaction component already present:** `daNpcT_DmgStagger_c mDmgStagger` at struct offset `0x0D3C` (d_a_npc_kn.h). Framework-level hit-stagger machinery is already in the struct, unused by the lessons.

### 5. AI architecture — function-pointer state machine
- `action()` (~:1099) just calls `(this->*mpActionFunc)()`. The brain is **`selectAction()`** (~:1447): picks ONE member-function pointer (`mpTeachAction`) by `mType` + `mActionMode` (a single linear lesson counter).
- Per-skill sub-selectors `teachNN_selectAction()` (~:1515–1599) map `mActionMode` → a named move-method.
- Each move-method is its own `mMode` sub-state machine (0/1 init, 2 active, 3 done).

**Move repertoire (all already implemented as methods):**
| Method | Move (skill) | Mechanic |
|---|---|---|
| `teach03_attackWait` (~:1929) | standing attack (Back Slice) | basic swing + approach |
| `teach04_attackWait`/`_headBreakWait`/`_finishWait` | guard/overhead (Helm Splitter) | shield-aware, multi-phase |
| `teach05_surpriseAttackWait` (~:2242) | lunge (Mortal Draw) | fast surprise strike |
| `teach06_superJumpWait` + `_divideMove`/`_superJumpWaitDivide`/`_superJumpedDivide`/`_waitDivide`/`_warpDelete` | jump attack (Jump Strike) | **splits into spectral clones** |
| `teach07_superTurnAttackWait` + divide variants | spin attack (Great Spin) | **clones + warp** |
| `wait` | idle/guard | — |
| `test` (~:1685) | debug | just freezes him — NOT combat |
| (projectile) | energy ball — `Z2SE_KN_V_BALL_READY/THROW` in `setSe()` case 12, `mActionMode == 7` | sword-beam/ball attack |

### 6. One beat traced — `teach03_attackWait` (~:1929)
Active loop (`mMode == 2`) does three things per frame:
1. Configure hurtbox to player's move: read `getCutType()`, flip target cylinder shield/hit-mark (~:1948-1955).
2. **If player hits** (`mCylCc.ChkTgHit()`): react theatrically — fly back as if Back-Sliced, `Z2SE_KN_V_DAMAGE_L`, hit SE (~:1957-1997). ← *the branch a real fight replaces with `mLife--` + normal stagger.*
3. **Else**: count down a randomized attack timer (`attack_start_time` HIO tuning); on expiry/`checkCollisionSword()`, launch his attack (set `mEvtNo` + `mFlowNodeNo`, motion sequence); else `calcSwordAttackMove()` approaches (~:1998-2034).
`teach04_attackWait` (~:2045) adds an `mActionMode = 10` advance on shield-attack hit — the lesson-progress bookkeeping a fight would guard out.

### 7. Shared locomotion — `calcSwordAttackMove(int)` (~:5125)
Pure action-code primitive every attack rides on:
- In range (<250) rolls an attack-ready motion (seq 0x19); far (>250) walks at `move_speed` toward player.
- Always turns to face player (`cLib_addCalcAngleS2` → `searchPlayerAngleY`); `param_0 == 1` = aggressive turn (rate 2 / 0x800), else HIO `rotation_num`/`rotation_speed`.
- `mJntAnm.lookPlayer()` head tracking.
NOT event-bound. Fully reusable verbatim.

### 8. Event/flow system is DIALOGUE-only — the clean split
- `mEvtNo` indexes `l_evtList[26]` (~:209): every entry is a TALK/CUTSCENE event ("FIRST_ENCOUNT", "..._SKILL_EXPLAIN", "..._SKILL_GET", "LARGE_DAMAGE_TALK") — the spoken lessons, not swings.
- `mFlowNodeNo` + `initTalk()` drive `dMsgFlow` (the spoken explanation / "you learned X" payoff).
- **Therefore:** the actual attack (movement + swing animation via `mMotionSeqMngr.setNo()` + `mCylCc` At-collision) is PURE ACTION CODE and reuses verbatim. The event-script-bound parts are ONLY the teaching dialogue, which a boss fight drops entirely. The valuable half (combat) and the discardable half (lessons) live in different subsystems — a clean separation.

### 9. Boss health bar — the engine API
- No boss gauge in `d_meter2`. TP drives it via game-state value: `dComIfGs_BossLife_public_Set()` / `_Get()`.
- Precedent: Fyrus `src/d/actor/d_a_e_fm.cpp` (`_Set(100)` ~:2799, `_Set(0)` on death ~:3764); also `d_a_b_bq`, `d_a_e_rd`, `kytag06/08` read it.
- Hook: on duel start `_Set(max)`, decrement per confirmed hit, `_Set(0)` triggers defeat.
- NOTE: the Shade already calls `dMeter2Info_onLifeGaugeSE()` (~:4668) / `dMeter2Info_getNowLifeGauge()` (~:4761), but those are about **Link's** hearts (the heal-to-full lesson nicety, `dComIfGp_setItemLifeCount(getMaxLife())` ~:4667), NOT a boss bar.

### 10. The ONE real constraint — resource/animation loading
Move animations live in **7 separate arcs**: `l_resNameList[9]` (~:238) = `KN_a`, `KN_tch01D`…`KN_tch07D`. Each lesson loads only the 2–3 it needs (`l_loadResPtrnList[8]`, ~:295). The free slot `mType == 7` currently loads `l_loadResPtrn0 = {1,-1}` (base only) and `heapSize[7] = 0x0000` (~:559).
- To use EVERY move, the new pattern must load the **union** `{KN_a, tch01D…tch07D}` = 8 arcs. Loader caps at exactly 8 phase slots (`mPhase[8]`, `for i<8`) — **fits, zero headroom**.
- Must set `heapSize[7]` (currently 0) to cover the union — larger than any single lesson (~`0x4B10`–`0x5A00`).
- Graceful fallback if RAM tight: **phase-gated movepool** — load a subset, swap arcs at phase transitions (sword skills → jump/spin+clones). Also better-paced than dumping all moves at once.

---

### 11. Clone / divide mechanic (the spectral-copy set piece)
The master Shade spawns child copies of itself — full `d_a_npc_kn` actors with type+copy-index packed into the spawn param:
- Teach06 (Jump Strike) spawn, action case 70 (~:4446): `for i in 0..1: fopAcM_createChild(fpcNm_NPC_KN_e, fopAcM_GetID(this), ((i+1)<<8)+6, &pos, room, &angle, ...)`. Param low byte `6` → `getType()` → `mType 5` (teach06); high byte `i+1` → `mDivideNo` 1 or 2. ⇒ **2 clones spawned, divideNo 1 & 2.**
- Teach07 (Great Spin) spawn, ~:4779: same with low byte `7` → `mType 6`.
- Master vs clone distinction: `mDivideNo == 0` = master; `1`/`2` = clone copy #. The MASTER (only) also spawns a child gold wolf companion (~:869-872, `fopAcM_createChild(fpcNm_NPC_GWOLF_e, ..., 0xFFFFFF02, ...)` gated `mType <= 6 && (mDivideNo==0 || mDivideNo>2)`).
- Clones run the `*_divideMove` / `*WaitDivide` / `*edDivide` action variants (teach06 mActionMode 14-18; teach07 19-24), then leave via `*_warpDelete` (mode 17/23).
- Inter-actor coordination: `fpcM_Search(s_sub6, this)` / `s_subEnd6` (and teach07 equivalents) — the master pings its clones to synchronize the multi-angle attack. `mActorMngr1/2` (`daNpcT_ActorMngr_c`) track the player / partner refs.
- **For a boss:** this is a ready-made "Shade splits into copies and strikes from all sides" special. Reusable, but it depends on the teach06/07 mActionMode choreography — a combat mode would trigger the sequence as a discrete special attack, not free-roam. `mDivideNo` (param high byte) controls copy count.

### 12. HIO tuning block — `mpHIO->m` (the rebalance knobs)
Struct `daNpc_Kn_HIOParam` ([include/d/actor/d_a_npc_kn.h:7]): a `daNpcT_HIOParam common` base (0x00–0x8B: attention/talk distances & angles, body/head look-angle limits, weight — shared NPC params) + Kn-specific fields. Static defaults in `daNpc_Kn_Param_c::m` ([d_a_npc_kn.cpp:71]); in DEBUG a live HIO editor (`daNpc_Kn_HIO_c`, Japanese-labelled) lets you tune at runtime.

Kn-specific fields (offset / label / role):
| Field | Off | JP label | Role in a fight |
|---|---|---|---|
| `attack_wait_time` | 0x8E | 攻撃され待ちタイム | how long he stands open before acting — **lower = less passive** |
| `followup_wait_time` | 0x90 | 追い討ち待ちタイム | pursuit/follow-up delay |
| `small_slide_distance` | 0x94 | 小滑り距離 | minor reposition slide |
| `big_slide_distance` | 0x98 | 大滑り距離 | large reposition slide |
| `warp_distance` | 0x9C | ワープ距離 | teleport distance |
| `attack_disappear_speed_h` | 0xA0 | 攻撃吹っ飛び速度横 | knockback launch speed (horiz) when HE is hit |
| `attack_disappear_speed_v` | 0xA4 | 攻撃吹っ飛び速度縦 | knockback launch speed (vert) |
| `rotation_num` | 0xA8 | 回転分割数 | turn-toward-player steps |
| `rotation_speed` | 0xAA | 回転角速度 | turn rate — **higher = harder to flank** |
| `attack_start_min_time` | 0xAC | (attack-start floor) | **min frames before he initiates — core aggression cadence** |
| `attack_start_time_range` | 0xAE | (attack-start RNG) | random add on top of min (`cLib_getRndValue(min, range)`) |
| `move_speed` | 0xB0 | | approach speed — **higher = pursues faster** |

**Key rebalance levers for a real fight:** lower `attack_start_min_time`/`attack_start_time_range` (attacks far more often), lower `attack_wait_time` (aggressive not patient), raise `move_speed` (closes distance), raise `rotation_speed` (resists side-stepping). Note: in vanilla these are tuned for *survivable lessons* — a boss wants the opposite end of every knob. The DEBUG live editor makes this tuning empirical.

### 13. Player skill inputs (cut-types) + ALBW parry coupling
**Two independent directions of "reading":**

**A. Link → Shade (offensive): the cut-type enum.** `daPy_py_c::CutType` ([include/d/actor/d_a_player.h:616]) classifies which sword move Link used. The Shade reads it via `getCutType()` (stored by `tgHitCallBack`). Hidden-Skill-relevant values:
| Value | Enum | Hidden Skill |
|---|---|---|
| 0x05 | `CUT_TYPE_HEAD_JUMP` | Helm Splitter |
| 0x12-0x14 | `CUT_TYPE_LARGE_JUMP_INIT/_/_FINISH` | Jump Strike |
| 0x17/0x18 | `CUT_TYPE_LARGE_TURN_LEFT/RIGHT` | Great Spin |
| 0x1A/0x1F | `CUT_TYPE_MORTAL_DRAW_A/B` | Mortal Draw |
| 0x1E | `CUT_TYPE_TWIRL` | Back Slice |
| 0x29 | `CUT_TYPE_GUARD_ATTACK` | Shield Attack |
| 0x01-0x04 | `CUT_TYPE_NM_*` | normal swings |
The teach methods already branch on these (e.g. teach03 keys `TWIRL`; teach06 keys `LARGE_JUMP*`). A boss can react per-skill — only Mortal Draw breaks his guard, Back Slice hits his exposed back, etc. — making the fight a literal exam of every skill you learned. Thematically ideal.

**B. Shade → Link (defensive): the ALBW parry READS THE SHADE FOR FREE.**
- The Shade already has a real, active attack collision: `mSphCc[]` set up with `SetAtSpl(dCcG_At_Spl_UNK_1)` (= **0x1**, parryable — NOT a guard-break) + `SetAtType(AT_TYPE_800)` ([d_a_npc_kn.cpp:819-826]); enabled during swings (`OnAtSetBit` ~:1241) and he even self-checks `mSphCc[i].ChkAtShieldHit()` (~:1263).
- The parry hook is **generic and Link-side**: `d_a_alink_damage.inc:708` calls `dShield_onShieldHit(this, at_spl, tghit_ac)` for ANY attacker whose At-collision hits Link's guard, keyed only on `at_spl` + attacker. Guard-break attacks (`at_spl == 9/10/11`) divert to `procGuardBreakInit()` BEFORE the parry path; everything else is parryable.
- The Shade's `at_spl == 1` ⇒ **his sword swings ARE parryable today.** With parry combat enabled (`game.shieldParryCombat`), guarding his attack inside the 4(+1)-frame window (`PARRY_WINDOW_FRAMES`) fires `dShield_onShieldHit` → grants ALBW meter (+1/6), bash charge (+1), durability repair. **No new plumbing on the parry side.**

**What you ADD for a Lies-of-P-style parry feel:** the parry currently rewards the *player* (meter/charge/durability) but doesn't inherently stagger the attacker. Couple a parry-success to a Shade flinch using his existing `daNpcT_DmgStagger_c mDmgStagger` (struct 0x0D3C) — natural trigger point is his own `mSphCc[i].ChkAtShieldHit()` (~:1263), which already tells him "my attack was shield-blocked." That turns a parry into a stagger + punish window.

**Free design levers this unlocks:**
- **Parryable vs unblockable per move:** set a given Shade attack's `at_spl` to 9/10/11 → it becomes a guard-break (forces dodge, not parry). Mix per attack for rhythm.
- **Helm-bash punish tie-in:** the mod's helm-bash credit system (`dAlbwCombat_getHelmBashTier`, Darknut-style) can give the Shade a tier so the Helm Splitter skill opens a punish — another hidden-skill hook into the fight.
- **Settings inheritance:** parry/durability gate on `game.shieldParryCombat` / `dShield_isDurabilityEnabled()`; the fight inherits them automatically.

### 14. Defeat / reward sequence (what `BossLife == 0` triggers)
**Two reference templates exist in-tree; combine them.**

**(a) The Shade's own farewell = the cutscene/exit template.** `ECut_seventhSkillGet` ([d_a_npc_kn.cpp:4797]) is the vanilla "graduation" event cut — an event-manager sequence stepped by `prm` (0/4/6/10/12/15/20/30/40): repositions the player (`setPlayerPosAndAngle` ~:4844), starts farewell BGM (`Z2GetAudioMgr()->bgmStreamPrepare(0x2000038)` / `bgmStreamPlay()` ~:4852), runs farewell dialogue (`initTalk(0x305)`), broadcasts clones to end (`fpcM_Search(s_subEnd7, this)`). NOTE: the skill-grant + completion flags (F_0338–F_0344) are set by the EVENT/BMG SCRIPT that this cut runs, NOT inline C++ — so a custom victory should author its OWN event cut / flag set rather than reuse the skill-granting scripts.

**(b) Real bosses = the death + reward template.** Every TP boss on death does the same three things (e.g. Fyrus [d_a_e_fm.cpp:3363/3758], Diababa `d_a_b_bq`, Morpheel `d_a_b_dr`, Stallord `d_a_b_ds`, Armogohma `d_a_b_gm`, Argorok `d_a_b_ob`):
1. **Reward item:** `fopAcM_createItemForBoss(&pos, dItemNo_UTAWA_HEART_e, room, &angle, ...)` — `UTAWA_HEART` = Heart Container. (Swap the item id for a thematic reward; `fopAcM_createItem` for a plain heart.)
2. **Clear flag:** `dComIfGs_onEventBit(dSv_event_flag_c::saveBitLabels[N])` — sets the boss-defeated/dungeon-clear save bit.
3. **BGM:** `bgmStreamPrepare/Play` fanfare.

**Recommended victory path for the secret boss (on `BossLife` reaching 0):**
- Suppress further damage; broadcast clones to warp out (`fpcM_Search(s_subEnd7/6, this)`).
- Play a collapse/defeat animation + particles (reuse his knockback/`attack_disappear` motions, or a new BCK).
- Set a NEW "secret boss cleared" event flag — mirror the gold-wolf-disappearance pattern (`F_0492`–`F_0497` = "Gold wolf disappearance N") with a fresh unused bit; this also makes the final wolf one-shot (gate its `isDelete()` appear on `F_0344 && !clearedFlag`).
- Optional reward via `fopAcM_createItemForBoss` (Heart Container, or unique item), and/or fire a `src/dusk/achievements.cpp` achievement.
- Run a farewell event cut modeled on `ECut_seventhSkillGet` (BGM `0x2000038` + dialogue + player reposition), then warp the player out (the gold-wolf companion's exit warp, or `dStage_changeScene` back to the overworld).
- **Repeatable vs one-shot:** consume the clear flag (one-shot, canonical) OR leave `F_0344` as the only gate (rematchable). Design choice.

### 15. Clone HP model (shared vs split)
- Clones are **full independent `daNpc_Kn_c` actors** (`fopAcM_createChild` with param-encoded type+`mDivideNo`), each with its own `mSphCc` attack collision and state.
- The master **coordinates them by name-search broadcast**: `s_sub6`/`s_subEnd6` ([d_a_npc_kn.cpp:2363-2379]) (and `s_sub7`/`s_subEnd7` ~:2773) iterate ALL actors and call `setActionMode(18)` (engage) / `setActionMode(17)` (warp-delete) on every `NPC_KN` that isn't self. Clones find the master with `fpcM_SearchByID(parentActorID)`.
- Clones are **ephemeral** in vanilla: spawned for the Jump-Strike / Great-Spin set piece, they attack, then warp-delete on the master's broadcast — they are NOT persistent health pools.
- **Recommended model: MASTER owns the single `BossLife` bar; clones are timed phantom decoys (no independent bar).** Striking a clone can do nothing (pure decoy), dispel it early, or feed a "find the real Shade" beat — only the master's `mLife` moves. The existing broadcast (`s_subEnd*` → mode 17) cleanly dismisses all clones when the special ends or the real one is struck.
- A **split-HP model** (each clone a fraction, kill all to clear the phase) is *possible* since each clone is a real actor that could carry its own `mLife`, but it fights the vanilla ephemeral design (clones leave on a timer/broadcast, not on death) — only pursue if you re-author the divide lifecycle.

## "Can he use ALL his actions in one fight?" — Yes, with caveats
- **Logic is free:** nothing couples the move-methods to lesson order except `selectAction`. A new `mType == 7` selector can call any `teachNN_*` by combat criteria (range/timer/RNG/phase).
- **Teaching tails:** each `*_Wait` interleaves its attack with "wait for correct counter, then advance lesson." Keep the attack-launch + hit-react halves; guard the `mActionMode = next` advances with `if (mType != 7)` (or fork combat variants).
- **Clone-split moves** (teach06/07) are the showpiece boss mechanic and fully reusable; `mDivideNo` controls copy count.
- **Resource union load** is the only non-trivial cost (see §10).

---

## Implementation plan (phased — each phase builds, runs, and is in-game testable; commit at each ✅)

Guiding order: **de-risk infra first, make him damageable, make him winnable, THEN make him fight, then expand, then frame.** Dev-warp to `F_SP200` for all combat iteration; the wolf-summon framing comes last. All new/edited code uses `============` block comment headers + `// === NEW CODE — ALBW Port … ===` banners; build `cmd /c build_run.bat` (RelWithDebInfo); don't commit/push unless asked.

**Phase 0 — Settings toggle + module scaffold.**
- Add `game.heroShadeSecretBoss` setting (off by default) in `dusk/settings.h` + `src/dusk/settings.cpp` + `src/dusk/ui/settings.cpp`, mirroring an existing ALBW toggle.
- New module `src/d/d_albw_shade_boss.cpp` + `include/d/d_albw_shade_boss.h` (`#if TARGET_PC`, file-local `namespace{}`, `dShadeBoss_` externs). Register the TU in `cmake`/`files.cmake`.
- ✅ Test: builds; toggle shows in settings UI; no behavior yet.

**Phase 1 — `mType == 7` actor slot that idles in the arena (RISKIEST INFRA FIRST).**
- Extend `daNpc_Kn_c` for the new type: `getType()` already maps param 0/≥8 → `mType 7`; add `mType == 7` arms to `selectAction()` (→ `wait` for now), `setParam()`, `setCollision()`, `resetType()`, `isDelete()` (appear gate `F_0344`).
- Resource/heap: give `l_loadResPtrn0` (the type-7 pattern) at minimum `{1, -1}` (base `KN_a`); set `heapSize[7]` from `0x0000` to a real value (start ≈ a lesson's `0x4B10`).
- Spawn hook: in `src/d/d_s_room.cpp` (postman/Stargazer pattern), `if (getStartStageName()=="F_SP200" && roomNo==<pick> && isEventBit(F_0344) && setting on) fopAcM_create(fpcNm_NPC_KN_e, param→type7, …)`.
- ✅ Test: save-editor set `F_0344` (`ImGuiSaveEditor`), dev-warp to `F_SP200` (`map_loader` "Wolf Howling Cutscene Map"), confirm he loads, stands, breathes — **no crash** (validates union-load/heap path that everything else depends on).

**Phase 2 — Make him damageable + boss bar.**
- Add `mLife`/`mMaxLife` (new fields) initialized for `mType == 7`.
- Divert `tgHitCallBack` (`d_a_npc_kn_base.inc:6`): when `mType == 7`, decrement `mLife` by the player's hit (optionally weight by `getCutType()`), instead of/in addition to storing cut-type.
- Boss bar: on fight start `dComIfGs_BossLife_public_Set(max)`; each hit update; classify in `dAlbwHP` `sBoss`/`sFinalBoss` so HP-mult + enemy-persistence treat him correctly.
- ✅ Test: hit him → bar drains → at 0 he just freezes (defeat handled next). Validates the full damage→UI loop.

**Phase 3 — Victory / defeat sequence.**
- On `mLife <= 0`: suppress damage, broadcast clones out (`fpcM_Search(s_subEnd7/6)`), play a collapse motion (reuse `attack_disappear` knockback or new BCK), `dComIfGs_onEventBit(<new "shade boss cleared" bit>)`, optional `fopAcM_createItemForBoss(&pos, dItemNo_UTAWA_HEART_e, …)` reward + achievement, fanfare BGM, then warp player out (`dStage_changeScene` / gold-wolf exit).
- Make one-shot: `isDelete()` appear gate → `F_0344 && !clearedBit`.
- ✅ Test: drain bar → full victory → flag set → can't re-trigger (or can, if rematch chosen).

**Phase 4 — Combat AI (the real work): start with ONE move.**
- Replace the `mType == 7` `selectAction` `wait` with a combat selector: pick by range/timer/RNG. Implement the basic attack first (reuse `teach03_attackWait`'s attack-launch half — motion seq + `calcSwordAttackMove(1)` approach — but DROP the lesson tail: no `mActionMode` advance, no `mEvtNo`/`initTalk` dialogue).
- Activate his real `At` collision (`mSphCc[]`, `at_spl == 1`) so the attack deals damage AND is parryable. ALBW parry reads it for free (`dShield_onShieldHit`).
- ✅ Test: he approaches + swings + damages; guarding in the parry window grants a parry. Tune feel via the DEBUG HIO editor.

**Phase 5 — Expand to full moveset + clones.**
- Switch the type-7 resource pattern to the **union load** `{1,2,3,4,5,6,7,8,-1}` (all `KN_tch0ND` arcs; fits the 8-slot `mPhase[8]` cap exactly) and raise `heapSize[7]` to cover it. (If RAM-tight, phase-gate the movepool instead.)
- Add the remaining attacks (Helm-Splitter stance, Mortal-Draw lunge, Jump-Strike + clone split via `fopAcM_createChild`, Great-Spin + clones, energy ball) to the combat selector; clones = master-owned-bar phantom decoys.
- ✅ Test: full attack variety cycles; clone special spawns + warps out; bar still single-owner.

**Phase 6 — Bidirectional skill reactions + balance.**
- Offense: in `tgHitCallBack`, branch on `getCutType()` so specific Hidden Skills do more (Back Slice from behind, Mortal Draw guard-break, etc.).
- Defense: couple parry-success → Shade flinch via `mDmgStagger` at his `mSphCc[i].ChkAtShieldHit()` (~:1263); optionally set some attacks' `at_spl` to 9/10/11 for unblockable variety; helm-bash tier tie-in.
- HIO balance pass (lower `attack_start_*`/`attack_wait_time`, raise `move_speed`/`rotation_speed`).
- ✅ Test: fight feels like a real duel; each skill matters; parry staggers.

**Phase 7 — The final-wolf summon (earn-the-fight framing).**
- Gated final-wolf actor (gwolf clone, Stargazer/postman spawn) appearing on `F_0344 && !clearedBit` in an overworld stage; record live coords (F5 overlay) like the Ook-arena Watcher; warp to `F_SP200` via `dStage_changeScene`/SCLS exit.
- ✅ Test: full loop from overworld — wolf appears → touch → arena → fight → victory → wolf gone.

**Phase 8 — Integration hygiene.**
- Confirm `dAlbwHP` boss classification, enemy-persistence exclusion, settings inheritance (parry/durability), no regressions to vanilla lessons (`mType 0–6` untouched). Patch notes.
- ✅ Final commit / push to `upstream` (when asked).

**Open design picks (defaults chosen above; revisit anytime):** reward = Heart Container (vs unique/none); one-shot (vs rematch); clones = decoys (vs split-HP); all-moves union-load (vs phase-gated pool).

---

## Phase 0 — file-level detail (settings + module scaffold)

**Goal:** a settings toggle and an empty module, both compiled in. Zero behavior. Risk: none — pure scaffolding to verify the build wiring before touching the actor.

**0.1 — Add the toggle to `UserSettings.game` ([include/dusk/settings.h] ~:181-194, beside `bossRefinement`):**
```cpp
// Hero's Shade secret boss: post-game real fight after all Hidden Skills (F_0344).
ConfigVar<bool> heroShadeSecretBoss;
```

**0.2 — Default + register ([src/dusk/settings.cpp]):**
- Init list (~:45-52, beside `.bossRefinement {"game.bossRefinement", false}`): `.heroShadeSecretBoss {"game.heroShadeSecretBoss", false},`  ← **default OFF** (new content, opt-in).
- `Register(...)` block (~:273-280): `Register(g_userSettings.game.heroShadeSecretBoss);`

**0.3 — UI row ([src/dusk/ui/settings.cpp] ~:1419, beside "Boss Refinement"):**
```cpp
addOption("Hero's Shade Secret Boss", getSettings().game.heroShadeSecretBoss,
          "After all Hidden Skills, a final Hero's Shade duel with a real health bar.");
```

**0.4 — New module + CMake:**
- `include/d/d_albw_shade_boss.h` + `src/d/d_albw_shade_boss.cpp` — `#if TARGET_PC`, file-local `namespace {}`, `dShadeBoss_` externs, `============` headers. Pattern-match `d_albw_combat.cpp`. Start with one accessor: `bool dShadeBoss_isEnabled() { return dusk::getSettings().game.heroShadeSecretBoss.getValue(); }` (mirror `dAlbw_isHiddenSkillReworkEnabled()`).
- Register the TU in [files.cmake] (~:185, beside `src/d/d_albw_boss.cpp`): `src/d/d_albw_shade_boss.cpp`.

**✅ Phase 0 test:** `cmd /c build_run.bat`; open Settings → the new toggle appears, persists across launches; no behavior change.

---

## Phase 1 — file-level detail (`mType == 7` idle Shade in `F_SP200`)

**Goal:** with `F_0344` set, dev-warp to `F_SP200` and the Shade loads, stands, breathes — **no crash.** Risk: HIGH — this validates the resource-pattern + `heapSize[7]` path that every later phase depends on. Do this before anything combat-related.

**1.1 — Resource pattern + heap for type 7 ([src/d/actor/d_a_npc_kn.cpp]):**
- `l_loadResPtrn0` (~:290) currently `{1, -1}` (base `KN_a` only) — KEEP for Phase 1 (idle needs no move arcs). Note for Phase 5: this becomes the union `{1,2,3,4,5,6,7,8,-1}`.
- `heapSize[8]` (~:559): change index `[7]` from `0x0000` to a real budget — start at `0x4B10` (a lesson's size). **Critical:** `0x0000` would fail `fopAcM_entrySolidHeap` → no actor. This is the single most likely Phase-1 crash cause.

**1.2 — Type-7 arms in the per-type switches (all in `d_a_npc_kn.cpp`):**
- `getType()` (~:749): already returns 7 for param low byte 0/≥8 — **no change**.
- `selectAction()` (~:1453 switch): add `case 7: mpTeachAction = &daNpc_Kn_c::wait; break;` (the `default` already does this — so optional, but make it explicit for clarity).
- `setParam()` (~:917), `setCollision()` (~:1170 switch), `resetType()` (~:879 switch): add a `case 7:` that mirrors the lightest existing lesson (`mActionMode = <idle>`, a basic `setCollisionTeach01()`/no-op). Goal is "stands with a sane collision," not combat.
- `isDelete()` (~:785): add
```cpp
case 7:
    if (dShadeBoss_isEnabled() && daNpcT_chkEvtBit(344) /* F_0344 */) return 0; // stay
    return 1; // delete
```
(`daNpcT_chkEvtBit(344)` = F_0344 — same flag-number convention as gwolf's `l_warpAppearFlag` `0x01D8`=F_0472. Phase 3 extends this with `&& !clearedBit`.)

**1.3 — Spawn hook ([src/d/d_s_room.cpp], in the existing `#if TARGET_PC` block beside the Postman/Shade-Watcher spawns ~:252-275):**
```cpp
// === NEW CODE — ALBW Port: Hero's Shade secret boss ===
if (dShadeBoss_isEnabled() &&
    strcmp(dComIfGp_getStartStageName(), "F_SP200") == 0 &&
    roomNo == <ROOM> &&                                   // pick an F_SP200 room (0-7); confirm via F5 overlay
    dComIfGs_isEventBit(dSv_event_flag_c::saveBitLabels[344])) {   // F_0344 (index==number, cf. postman [625]=F_0625)
    static const cXyz  kPos   = { /* record live via F5 overlay */ };
    static const csXyz kAngle = { 0, (s16)0, 0 };
    static const cXyz  kScale = { 1.0f, 1.0f, 1.0f };
    fopAcM_create(fpcNm_NPC_KN_e, (0x00u << 8) | 0x08u,   // low byte 8 -> mType 7; high byte 0 -> mDivideNo 0 (master)
                  &kPos, roomNo, &kAngle, &kScale, -1);
}
```
Include `d/d_albw_shade_boss.h`. Param `0x0008`: low byte 8 → `getType()` default → `mType 7`; high byte 0 → `getDivideNo()` 0 → master (not a clone).

**1.4 — Coordinate capture (same method as the Ook-arena Watcher):** dev-warp to `F_SP200` ("Wolf Howling Cutscene Map", [map_loader]), stand where the Shade should be, read Link's pos/angle off the **F5 Player Info overlay**, paste into `kPos`/`kAngle`. Pick the room with the cleanest open geometry for a duel.

**✅ Phase 1 test:**
1. `ImGuiSaveEditor` → set `F_0344` (Obtained 7 secret techniques).
2. Enable the Phase-0 toggle.
3. Dev-warp to `F_SP200` room `<ROOM>`.
4. Confirm: Shade loads, stands in `ANM_WAITSIT`-style idle, breathes, **no crash / no heap assert**. Toggle off or clear `F_0344` → he does not appear.

**Phase 1 gotchas:**
- `heapSize[7] == 0x0000` → silent no-spawn. Set it first.
- Archive-clearance crash trap (cf. [[workflow-npc-dialogue]]): `F_SP200` already loads `KN_*` arcs for the vanilla lessons, so the arcs should be resident — but confirm the room actually streams them; if not, the resLoad will stall. A standing test surfaces this immediately.
- Spawn fires on room LOAD only — re-enter the room (or warp) after setting the flag; it won't pop in mid-room.

## Open questions / next steps
- Exact source of `mType` request for the *final wolf's* warp (param wiring vs event) — partially answered (spawn param byte), confirm end to end at build time.
- Whether `F_SP200` rooms are interchangeable or per-skill specialized geometry — pick a clean room for the duel.
- One-shot vs repeatable: consume `F_0344`→a new flag (mirror `F_0492`-style disappearance) or allow rematch.
- Combat tuning: `mLife` value, HP-mult interaction (boss category), attack damage, phase thresholds for clone moves.
- Conventions: new/modified code uses `============` block comment headers (ALBW port rule); build via `build_run.bat` (RelWithDebInfo); push to `upstream` (ALBW-Dusklight); don't commit unless asked.

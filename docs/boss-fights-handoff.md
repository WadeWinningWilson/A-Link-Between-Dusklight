# Boss fights — design handoff & session archive

**Purpose:** Preserve critical boss-combat work from the long Cursor chat so you **do not need to reopen that session** (it will OOM). Start new chats with:

> Continue boss fight work — read `docs/boss-fights-handoff.md` first.

**Related docs:** [albw-port.md](albw-port.md) (docket), [albw-boss-hp-hud-tuning-brief.md](albw-boss-hp-hud-tuning-brief.md) (HUD art), [shield-combat.md](shield-combat.md) (parry/bash/durability).

**FPS / golden build (separate doc):** [future-performance-leaning.md](future-performance-leaning.md) — not covered here.

---

## Settings (in-game)

| Setting | Config key | Default | What it gates |
|---------|------------|---------|---------------|
| **Boss Refinement** | `game.bossRefinement` | **Off** | Armogohma %-HP pacing, warp bootstrap, any-sword gates (Zant/Ganon/Argorok) |
| **Boss Health Bars** | `game.bossHealthBars` | **Off** | LoP-style reactive bar in `d_albw_boss_hp_hud.cpp` |
| **Boss HP ×** | `game.hpMultBoss` | **1×** | True max HP for boss-tier actors (`fpcNm_B_*`, etc.) |
| **Mid-Boss HP ×** | `game.hpMultMidBoss` | **1×** | Darknut (`B_TN`), Aeralfos (`B_GG`), etc. |
| **Shield Parry & Bash** | `game.shieldParryCombat` | On | Bash charges, perfect parry — used in Darknut fight |
| **Manual Shielding** | `game.manualShielding` | Off | ZR guard required for parry/bash |
| **Darknut Bash Debug Log** | `game.showDarknutBashDebug` | Off | Writes `albw_darknut_debug.txt` |

**Playtest combo for boss work:** Boss Refinement **On**, Boss Health Bars **On**, Boss HP × as desired (4× was used during Armogohma tuning).

---

## Architecture — three layers

```text
┌─────────────────────────────────────────────────────────────┐
│  Boss Refinement (game.bossRefinement)                      │
│  • Layer A: any-sword gates (Zant, GND, MGN, Argorok)       │
│  • Armogohma: %-HP bar, egg gates, warp bootstrap           │
│  • Future Layer B: Zant 6-phase tools, Ganon CHANCE duel    │
├─────────────────────────────────────────────────────────────┤
│  Boss HP HUD (game.bossHealthBars)                          │
│  • Draws bar from dAlbwHP_getLockonDisplayHp()              │
│  • Armogohma phase 1/2 routing via dAlbwBoss_armogohma*   │
├─────────────────────────────────────────────────────────────┤
│  HP multiplier (game.hpMultBoss / hpMultMidBoss)            │
│  • dAlbwHP_tryApplyTrueMaxHp() on actor init                │
│  • Categories keyed on fpcNm_* process name, not filenames  │
└─────────────────────────────────────────────────────────────┘
```

**Important:** Runtime lookups use **`fopAcM_GetName()` → `fpcNm_*`**, not C++ class names or `d_a_*.cpp` filenames.

---

## Boss Refinement — implemented

### Module

- `include/d/d_albw_boss.h`
- `src/d/d_albw_boss.cpp`

### Layer A — any sword (when setting on)

Master Sword checks become “any sword obtained” for:

- **Zant** (`d_a_b_zant.cpp`) — damage type, big damage, pause timer
- **Ganondorf horseback** (`d_a_b_gnd.cpp`) — five MS gates
- **Beast Ganon** (`d_a_b_mgn.cpp`) — MS damage tier
- **Argorok** — MS collider checks

Helpers: `dAlbwBossRefinement_colliderCountsAsMasterSword()`, `dAlbwBossRefinement_playerHasBossSword()`.

### Boss warp bootstrap (Boss Refinement on only)

Hooks:

- `d_a_obj_bosswarp.cpp` — `dAlbwBoss_requestWarpBootstrap()` on warp travel
- `d_s_play.cpp` — `dAlbwBoss_onStageLoad()` on stage load
- `d_a_cstatue.cpp` — skip stale switch gates during Armogohma warp revisit
- `d_a_b_gm.cpp` — actor bootstrap, victory cleanup

**Design rule:** Warp bootstrap fixes **boss warp revisit only** — normal dungeon door entry unchanged.

**Armogohma warp fixes:**

- No egg-loop softlock when quiver empty (ALBW bow uses meter, not arrows)
- Zone/room switches reset so Dominion Rod statues work mid-fight
- Player can add/equip rod during fight — no hard inventory gate at warp-in

---

## Armogohma — refined fight (Boss Refinement ON)

### Design contract (locked in chat)

| Rule | Value |
|------|--------|
| **Win condition** | Still **3 Dominion Rod statue hits** (`mHitCount >= 3`) |
| **Max HP pool** | Scaled `field_0x560` after `dAlbwHP_tryApplyTrueMaxHp()` |
| **Bow core hit** | **−4% of max HP** per eye/core arrow (`kAlbwArmogohmaBowChipPct`) |
| **Egg waves** | **4 total**, queued on downward HP cross, consumed one-at-a-time on ceiling |
| **Egg gates** | **85%, 75%, 65%, 20%** (“last cry”) |
| **Floor openings** | **75%, 45%, 15%** — crossing sets `s_armogohmaPendingCeilingDrop` |
| **Post-statue HP snap** | After hit 1 → **60%**, after hit 2 → **35%** |
| **Eye hits** | Chip HP only — do **not** directly queue eggs |

Eggs fire on ceiling path (`b_gm_move` case 3) via `dAlbwBoss_armogohmaTryBeginEggPhase()`.

When **Boss Refinement OFF**: vanilla egg logic (`field_0x1ad5 == 2` / arrow count).

### Known playtest feedback (tune next)

- Fight can end **too quickly** if bow chip % is high relative to HP mult
- Sometimes only **~2 egg drops** seen — likely skipped gates when statue snaps jump HP past multiple thresholds in one frame; may need staggered consumption or lower bow chip
- **Opening gates** (75/45/15) queue ceiling drop — verify drop animation actually fires
- Arrow damage may still feel strong — check if vanilla `cc_at_check` path also reduces HP outside Boss Refinement chip

### Armogohma HP bar (composite meter — planned)

**Spec (no code yet):** [albw-armogohma-boss-bar-spec.md](albw-armogohma-boss-bar-spec.md)

- One bar, both actors: phase 1 = `B_GM` refinement pool → **fillRatio 1.0..0.5**; phase 2 = `E_GM` TYPE_GOMA hit counter (`field_0xa74`) → **0.5..0.0**
- HUD draws **`fillWidth = barW * fillRatio` only** — layout/name constants stay in `d_albw_boss_hp_hud.cpp`
- Bow arrow vanilla damage suppressed in `d_cc_uty.cpp` when Refinement on; chip handled in `dAlbwBoss_armogohmaOnBowCoreHit()`

---

## Darknut (`B_TN`) — bash / parry integration

Darknut is **mid-boss tier** (`sMidBoss` in `d_albw_hp_mult.cpp`) for HP mult **and** shield durability mult (1.5×).

### Special HP display

Darknut does **not** use plain `health` in phase 2:

| Phase | Meter | Fields |
|-------|-------|--------|
| Armored | Armor pieces remaining | `mNextBreakPart`, `ALBW_ARMOR_PIECE_COUNT` |
| Transition | `ACT_CHANGEDEMO` | Shows 0 / armor total |
| Unarmored | Internal damage meter | `field_0x6fc` taken vs `field_0x700` max |

`dAlbwHP_getLockonDisplayHp()` exposes this via `customMeter` + `darknutPhase`.

### Bash guard break (ALBW)

Methods on `daB_TN_c` (see `d_a_b_tn.h`):

- `albwTryApplyBashGuardBreakFromHit()` — shield bash opens guard window
- Phase 1: `albwApplyPhase1BashGuardBreak()` → 90-frame open window
- Phase 2 unarmored: `albwApplyPhase2BashGuardBreak()` → 75-frame window, `ACT_YOROKE`
- Debug: `showDarknutBashDebug` → `albw_darknut_debug.txt`

Integrates with `d_albw_shield.cpp` bash charge economy and `d_albw_lockout.cpp` boomerang stun (`onBoomerangHitNative` for Darknut).

---

## Boss Health Bars HUD

- **File:** `src/d/d_albw_boss_hp_hud.cpp`
- **HP source:** `dAlbwHP_getLockonDisplayHp(boss)` — do not duplicate HP math in HUD
- **Art brief:** [albw-boss-hp-hud-tuning-brief.md](albw-boss-hp-hud-tuning-brief.md)
- **Armogohma composite meter:** [albw-armogohma-boss-bar-spec.md](albw-armogohma-boss-bar-spec.md)

Target look: Lies of P proportions — thin crimson fill `(176,8,8)`, translucent charcoal track `(18,18,22)` @ ~66% alpha, small white name with thin outline.

---

## HP multiplier tables

**File:** `src/d/d_albw_hp_mult.cpp`

| Tier | Examples |
|------|----------|
| **Mid-boss** | `B_TN` Darknut, `B_GG` Aeralfos, Death Sword, Phantom Zant, Ook, Bulblin, etc. |
| **Boss** | Diababa, Morpheel parts, Stallord, Blizzeta, **Armogohma**, Argorok, Zant phases, Fyrus, Possessed Zelda |
| **Final** | `B_GND` Ganondorf, `B_MGN` Beast Ganon |

**Not arena-specific** — same category everywhere the actor appears.

Shield durability scaling: `dAlbwHP_applyDurabilityMult()` — mid-boss 1.5×, boss/final 2× on non-parry hits.

---

## ALBW meter lockout (boss relevance)

**File:** `src/d/d_albw_lockout.cpp`

When ALBW meter hits 0:

- Ranged/magic/tool perks gated until meter recovers
- Boomerang hit on enemy → 4s “ranged opened” window + stun
- Darknut uses `dAlbwLockout_onBoomerangHitNative()` (native wobble, not fpcM_Pause)

Future: visual feedback when `dAlbwLockout_isRangedOpened()` (shimmer/HUD).

---

## Enemy Death Rupees (boss payouts)

Fight victories grant wallet credit once per profile per session. Hooked in boss actor files (`d_a_b_tn.cpp`, `d_a_b_gm.cpp`, etc.). Additive only — never replaces vanilla drops.

---

## Pending — Layer B (designed, not implemented)

Full briefings preserved below from original chat **`fbc16b50-bd3e-4cad-968e-e1c0bd114747`** (session deleted 2026-06-15 after archival).

### Boss Rush (future setting)

Sequential warp through all 8 dungeon bosses + finale. Shares warp bootstrap infra with Boss Refinement but **separate toggle**. Must not change vanilla dungeon door entry — bootstrap applies **only** on boss-warp revisit.

### Zant multi-phase redesign

**Status:** PENDING — confirm against current `d_a_b_zant.cpp` before coding.

Six phases, each in a different dungeon arena, **tool-locked damage** (wrong tool = no damage + SE). Layer 1 feedback: wrong-item hit → `Z2SE_EN_ZAN_V_NO_DMG` (preferred); backups `Z2SE_D33V_ZANT_BOUZEN`, `Z2SE_D33V_ZANT_IKIGIRE`. Layer 2 (later): segmented boss HP bar with phase icons — coexists with SFX layer.

| Phase | Arena | Damage constraint |
|-------|-------|-------------------|
| 1 | Forest Temple | Slingshot only during vulnerability window (damage scales to sword tier) |
| 2 | Goron Mines | Bomb arrows only during vulnerability window |
| 3 | Lakebed Temple | Water bombs only — Iron Boots to place on floor, Clawshot pull Zant to bomb before fuse; 1–3 bombs/cycle |
| 4 | Ook pillars | Pillar bash unchanged; wolf damage only after knockdown |
| 5 | Snowpeak | Spinner jump only during shrunken state (vanilla window) |
| 6 | Throne Room | Spur charges required; cap = shield tier (Ordon 2 / Wooden 4 / Hylian 6); charges persist across phases |

**Open:** Phase 3 bomb damage scaling; Phase 1 slingshot scaling persistence post-Zant fight.

### Ganon / CHANCE duel

**Status:** PENDING — numeric tuning needs source data + playtest.

Cumulative ALBW mastery test; vanilla one-on-one sword duel preserved.

**Phase 1 — negation active:** Heavy damage negation until **2 successful CHANCE wins** (no time limit).

**Phase 2 — negation removed:** Full sword damage; pressure systems remain.

**CHANCE triggers:**

| Path | Rule |
|------|------|
| Shield bash (preferred) | Full spur charges → CHANCE attempt; **all charges consumed** regardless of outcome |
| Health thresholds (assist, once each) | Auto-CHANCE at **80%**, **55%**, **20%** Ganondorf HP |
| Fishing rod (easter egg) | Full hearts + full spurs + ≥70% ALBW meter; meter drains toward 70%; stagger → near-CHANCE within 2 hits; counts toward 2-win counter if CHANCE won |

**Outcomes:** Win = meaningful damage (through negation in P1). Fail = **10 hearts** damage (placeholder).

**Pressure:** Increased Ganondorf damage mult; failed parry = shield durability + **4 hearts** seeping damage (placeholder). Spur charge level scales player damage in P2 (values TBD).

**Human-form heart recovery:** Mirror wolf bite recovery structure — rate/trigger TBD.

**Open:** Ganon HP pool, negation %, spur scaling, damage mult, meter drain stop at 70% vs overshoot.

---

## Key source files

| Area | Files |
|------|-------|
| Boss Refinement core | `d_albw_boss.h/cpp` |
| Armogohma actor | `d_a_b_gm.cpp` |
| Darknut actor | `d_a_b_tn.cpp`, `d_a_b_tn.h` |
| Zant / Ganon | `d_a_b_zant.cpp`, `d_a_b_gnd.cpp`, `d_a_b_mgn.cpp` |
| Boss warp | `d_a_obj_bosswarp.cpp` |
| Statues (Armogohma) | `d_a_cstatue.cpp` |
| HP mult + lock-on HP | `d_albw_hp_mult.cpp/h` |
| Boss HP HUD | `d_albw_boss_hp_hud.cpp/h` |
| Shield parry/bash | `d_albw_shield.cpp/h`, `d_a_alink_guard.inc`, `d_a_alink_damage.inc` |
| Lockout | `d_albw_lockout.cpp/h` |
| Stage hooks | `d_s_play.cpp`, `f_op_actor.cpp` (true HP apply) |
| Collision | `d_cc_uty.cpp` |
| Settings UI | `src/dusk/ui/settings.cpp` |

---

## Bugs fixed during boss chat (regression watch)

| Issue | Fix area |
|-------|----------|
| Forest temple (Diababa) crash on enter | Shield combat context / attention lock |
| Z-target works but shield won't raise in boss | `checkAttentionLock` / manual shield gating |
| Armogohma egg infinite loop on warp | Arrow-count gate incompatible with ALBW meter bow |
| Armogohma softlock without Dominion Rod at warp | Warp bootstrap + cstatue switch bypass |
| Boss bar only moved on rod snaps, not bow | `dAlbwBoss_armogohmaFillDisplayHp()` + opening gate wiring |

---

## Chat archive note

Original session **`fbc16b50-bd3e-4cad-968e-e1c0bd114747`** (~1.1 GB in Cursor DB) was **deleted 2026-06-15** after this doc was verified. Do not reopen in Composer.

**To continue work:** open a **new** chat and reference this file + specific task (e.g. “tune Armogohma bow chip from 4% to 2%” or “wire Zant phase 1 tool lock”).

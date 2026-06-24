# Boss fights — design handoff & session archive

**Purpose:** Preserve critical boss-combat work from the long Cursor chat so you **do not need to reopen that session** (it will OOM). Start new chats with:

> Continue boss fight work — read `docs/boss-fights-handoff.md` first.

**Related docs:** [albw-port.md](albw-port.md) (docket), [combat-refinements-handoff.md](combat-refinements-handoff.md) (field combat: enemy targeting, hidden skills × meter), [albw-boss-hp-hud-tuning-brief.md](albw-boss-hp-hud-tuning-brief.md) (HUD art), [shield-combat.md](shield-combat.md) (parry/bash/durability).

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
│  • Diababa (in progress): RUNAWAY arrow test; planned 2× side-head cadence, │
│    poison siphon, rhythm parry vs spray, optional distance retreat            │
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

## Building Boss Refinement features (new chats)

**Always read first:**

| Doc | Why |
|-----|-----|
| [build-fps-guidelines.md](build-fps-guidelines.md) | Canonical build (`build_run.bat` / RelWithDebInfo), launch hygiene (no `DUSK_DRIVE*`), what never goes in the repo |
| [commit-and-push.md](commit-and-push.md) | Push to **upstream** (`WadeWinningWilson/ALBW-Dusklight`), not `origin`; stage/don't-stage rules |

**If the work touches…**

| Area | Also read |
|------|-----------|
| HUD / meter / shield / `game.lopHud` | [hud-performance-handoff.md](hud-performance-handoff.md) + [albw-hud-lop-layout-brief.md](albw-hud-lop-layout-brief.md) |
| LoP layout details | [albw-hud-lop-layout-v3-instructions.md](albw-hud-lop-layout-v3-instructions.md) |
| Boss fights / refinement | This file |
| Boss HP bar tuning | [albw-boss-hp-hud-tuning-brief.md](albw-boss-hp-hud-tuning-brief.md) |

**After they build — hand off for FPS review:**

| Chat | Doc entry |
|------|-----------|
| Build-analysis / FPS review | [build-fps-guidelines.md](build-fps-guidelines.md) → Doc map section, then [hud-performance-handoff.md](hud-performance-handoff.md) if HUD changed |

**Short prompt (paste into any new feature chat):**

> Before coding: read `docs/build-fps-guidelines.md` and `docs/commit-and-push.md`.  
> Build with `build_run.bat` only (RelWithDebInfo). No drive/conavigate code in `src/`.  
> If HUD/meter/shield: also read `docs/hud-performance-handoff.md`.  
> When done: git diff summary + build result → build-analysis chat for FPS check before commit.  
> Push to **upstream** (ALBW-Dusklight), not origin.

**Hard rules (one-liners):**

- **Build:** `build/windows-msvc-relwithdebinfo/dusklight.exe` via `build_run.bat`
- **Never commit:** `local_dev_backup/`, drive/conavigate sources, `albw_*_debug.txt`
- **Don't revert features to fix FPS** — optimize in place; use the build-analysis chat
- **Don't commit/push** unless explicitly asked

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

## Diababa — refined fight (Boss Refinement ON)

**Status:** RESEARCH / DESIGN — no code in `d_albw_boss.cpp` yet (Armogohma only today). Forest Temple crash on enter was fixed separately (shield attention lock).

### Actor map

| Role | `fpcNm_*` | File |
|------|-----------|------|
| Middle head | `B_BQ` | `d_a_b_bq.cpp` |
| Side tentacles (“smaller heads”) | `B_BH` | `d_a_b_bh.cpp` |
| Ook / bomb | `E_MB` | `d_a_e_mb.cpp` |

`E_GB` (Giant Baba) is **not** this fight — dungeon room enemy only.

HP mult + Boss Health Bars already list `B_BQ` / `B_BH` in `d_albw_hp_mult.cpp`. Display uses plain `health` / `field_0x560` (no composite meter like Armogohma).

### Vanilla behavior (research summary)

**Side-head attack pacing (`B_BH`):**

- Wait between attack *rolls*: HIO `attack_freq_a` **350** (P1), `attack_freq_b` **250** (P2), each `freq + rand(0…freq)` frames (~12–23 s / ~8–17 s @ 30 fps).
- Attack only commits if player within **2800** units of head anchor (`mBasePos` / `field_0x6b0`).
- Middle head sets shared cooldown `B_BQ.field_0x6fe` (**200–350** P1, **100–200** P2) after a solo attack roll.
- Many hardcoded timer overrides during middle-head spray / cutscenes (`field_0x6fb`, `dComIfGp_event_runCheck()`).
- Lunge **speed** is separate (`speedF` ~200, anim morf 3.0f) — not the same as inter-attack wait.

**Middle-head poison (`B_BQ` `ACTION_ATTACK`):**

- Triggered when bomb damaged middle head (`field_0x11fc++`) and later `mTimers[2] == 1` after return-to-wait.
- Spray length: HIO `mWaterSprayTime` **150** frames.
- Four `mCcMizuSph` spheres (r=35), `AT_TYPE_CSTATUE_SWING`, **`GetAtSpl() == 0xd` (13)** — classified as **large attack** (`checkLargeAttack(13)`).
- Vanilla: no life siphon, no heal-on-hit.

**Vanilla: `BCK_BQ_RUNAWAY` is not active in gameplay.**

| Check | Result |
|-------|--------|
| `anm_init(..., BCK_BQ_RUNAWAY, ...)` | **Never called** — only listed in enum (`d_a_b_bq.cpp`) |
| `anm_se_set` branch for RUNAWAY | **None** (RETURN01/02 have particle hooks; RUNAWAY does not) |
| RUNAWAY-specific SFX | **None** — retreat uses `Z2SE_EN_BQ_V_DAMAGEBACK` / `Z2SE_EN_BQ_V_BACK` with RETURN anims |
| RUNAWAY-specific particles | **None** in `d_particle_name.h` (RETURN uses 0x8322–0x8325) |
| Asset in `B_bq` archive | **Yes** — `dRes_ID_B_BQ_BCK_BQ_RUNAWAY_e = 0x14` in all regional `B_bq.h` headers |
| Distance-based retreat | **No** — `mDistToPlayer` only tweaks hitbox size when far + boomerang aim |

**What vanilla *does* use for submerge:** `BCK_BQ_RETURN01` / `RETURN02` inside `ACTION_DAMAGE` only — after bomb/chance timer, not player range. On finish → `ACTION_WAIT`, `field_0x6fc = 10`, `mTimers[2] = 80`.

**Other unused `B_bq` anims (same status as RUNAWAY):** `BCK_BQ_ATTACK_C`, `BCK_BQ_LOOK_M`, `BCK_BQ_TESTMOTION` — enum only, never referenced.

**Likely if wired for Refinement test:** `anm_init(i_this, BCK_BQ_RUNAWAY, …)` should load like any other BCK from the loaded `B_bq` phase (same morf path as RETURN). Animation data ships with the game; **visual/behavior quality unverified** until in-game play. No existing code path to copy for RUNAWAY-specific FX — would reuse RETURN particles/SFX or leave silent.

**Reversible test pattern (when building):** Boss Refinement flag + optional debug pad trigger → play RUNAWAY vs RETURN02 A/B; no change when Refinement off.

### RUNAWAY / enrage sequence — arrow test (implemented, iterating)

**Status:** Boss Refinement ON + `ACTION_WAIT` + arrow → `ACTION_RUNAWAY_TEST` (phased). Refinement **off** = vanilla NODAMAGE unchanged.

**Implemented phases (v2):**

| Phase | Mode | Behavior |
|-------|------|----------|
| A — RUNAWAY | 0→1 | `BCK_BQ_RUNAWAY` @ **4.5×**; side heads **decoupled** (frozen surface anchor, `field_0xa25 = 2`) |
| B — Hold | 2 | Last RUNAWAY frame held **150f**; `field_0x6fb = 3` → side-head frenzy (faster rolls, half `field_0x6fe`) |
| C — Rise | 3 | `BCK_BQ_APPEAR` → `ACTION_WAIT` + `BCK_BQ_WAIT01`; stems re-couple (`field_0xa25 = 1`) |

No damage-retreat SFX/particles on RUNAWAY. Sequence iframe **450f**. Files: `d_a_b_bq.cpp`, `d_a_b_bh.cpp` (decouple + frenzy only when Refinement on).

**Trigger (unchanged):** `#if TARGET_PC` && `dAlbwBossRefinement_isEnabled()` && `ACTION_WAIT` && arrow (`HIT_TYPE_ARROW` / `AT_TYPE_ARROW`) && `field_0x6de == 0`. Does not increment `field_0x11fc` or enter `ACTION_DAMAGE`.

**Vanilla arrow path (for reference):** body `mCcSph` → `at_power_check` → non-bomb → `BCK_BQ_NODAMAGE` (no HP change).

---

#### Enrage side-head attack conductor (planned — no code yet)

**Problem (playtest):** During submerged hold, independent fast timers make both heads attack **almost** together — mushy, not readable.

**Target pattern (5 beats, then main rises):**

```text
Beat 1 — Left (mID 0) solo lunge
Beat 2 — Right (mID 1) solo lunge
Beat 3 — Left solo
Beat 4 — Right solo
Beat 5 — BOTH simultaneous (reuse field_0x6a0 paired-attack path)
→ then B_BQ APPEAR (main rise); hold timer becomes event-driven, not fixed 150f
```

**Design rules:**

- **Quicker** solo beats (~**45–60f** between attack *starts**, tune in playtest) but **hard alternation** — only beat 5 overlaps.
- **Hold phase ends on beat 5 completion** (prefer: dual lunge anim **stop**, not windup start).
- **Out of range:** pause conductor if player leaves attack band; don’t fire beats into empty room.
- **One head down:** skip that side’s solo beats; decide whether to shorten to 3 beats + dual or cancel enrage.
- **Interrupt policy TBD:** bomb/core hit during conductor?

**Implementation sketch:** small `B_BQ` “conductor” (beat index + allowed attacker mask); `B_BH` checks parent before attack rolls instead of lowering both heads’ random freq independently.

---

#### Door / threshold safe zone — reaching Link (design — no code yet)

**Why the door feels safe today (vanilla + current Refinement):**

| Layer | Code / knob | Effect at door |
|-------|-------------|----------------|
| Side-head **attack roll gate** | `(anchor − player).abs() < **2800**` in `b_bh_wait` / `b_bh_b_wait` | Beyond ~2800 from `mBasePos` / `field_0x6b0`, heads **never commit** an attack |
| Side-head **lunge cap** | `field_0x68c > **1700**` ends lunge early (`b_bh_attack_1`) | Even if a roll happens, stem only extends ~1700 from anchor — may not reach deep alcove |
| Side-head **reach check** | `field_0x688 < 500` ends lunge when close to player | Door camper stays far → lunge may time out before contact |
| Middle **poison beam** | Head-local aim ~**2300** forward; 4× `mCcMizuSph` along segment; `LineCross` vs geometry | Alcove behind wall / angle often **no line-of-sight**; beam stops at geometry |
| Boomerang-aim band | `mDistToPlayer > 1000` + `checkPlayerStatus0(0, 0x80000)` shrinks `B_BQ` body sphere | Encourages “far + aiming” positioning; not door-only but reinforces camping |
| Enrage decouple | Side heads frozen at **surface** anchors while main sinks | Correct for enrage read; does **not** by itself reach door |

**Design goal:** Door is **safer**, not **immune** — camping should incur escalating pressure, preferably using **existing** attack vocabulary before new actors.

**Options compared:**

| Option | Pros | Cons | Verdict |
|--------|------|------|---------|
| **A — Global range extend** (raise 2800, 1700, poison length everywhere) | Easiest diff | Makes **whole arena** harder; breaks mid-room spacing | **Avoid** as global change |
| **B — Door-zone conditional range** (only when Link in door volume: higher lunge cap / slightly wider roll gate) | Reuses `B_BH` lunge; fair “you camped there” | Needs room **volume** (see below); can look like stretchy neck if overtuned | **Good secondary** |
| **C — Middle-head door snap** (player in door band N frames → force poison or aimed spray toward door) | Uses proven `dokuhaki` / head joint; reads as boss answering camper | Needs aim override + maybe LOS exception; telegraph must be clear | **Best primary** |
| **D — Enrage / conductor door beat** (during L/R/L/R/BOTH, beats 4–5 use **extended lunge** toward door sector) | Thematic fit with enrage test | Only covers enrage window | **Combine with B** |
| **E — Room traps** (poison geyser / vines at threshold) | Clearest “this tile isn’t safe”; fixed telegraph | New actors or stage hooks; most implementation surface | **Best flavor, most work** |
| **F — Siphon / heal pressure** (poison siphon when camping) | Reinforces “don’t stall” without geometry | Doesn’t **hit** door if poison never connects; feels indirect | **Supplement**, not sole fix |

**Recommended stack (Refinement only):**

1. **Primary — door volume + middle-head response (C):** Define a **Forest Temple boss door/threshold volume** (stage coords or trigger box — measure in playtest). If Link stays inside for **~2–3s** while middle head is up (or during poison-eligible phases), trigger one of:
   - **Poison fan toward door** — reuse `b_bq_attack` dokuhaki with aim override / extended aim point (head still joint-anchored).
   - **One-shot smoke at threshold** — reuse existing wall-hit smoke particles (`0x82D4` / `0x82D5` path) as telegraphed ground hazard.
2. **Secondary — conditional side-head snipe (B):** *Only inside door volume*, allow attack roll up to **~3400** and lunge cap **~2200** (tune); mid-arena keeps vanilla **2800 / 1700**.
3. **Optional — threshold trap (E):** If volume approach still too safe, one **static poison puddle** at door lip (JPAC already in fight) on a long cooldown.
4. **Enrage tie-in (D):** Conductor beat 5 dual lunge uses door-aware targeting if player in volume; then APPEAR.

**Avoid:** always-on longer necks; trap spam every frame; removing door safety entirely (keep it a **risk/reward** flank, not a kill box).

**Open (door work):**

- Exact door volume in `D_MN01` (or Forest boss room stage name) — needs in-game measurement.
- Camp timer vs instant entry (hysteresis so passing through door doesn’t instantly punish).
- Interaction with rhythm parry / shield bash economy if poison is the door answer.

**Middle-head retreat / safe zone (vanilla baseline):**

- **No distance-based sink** in vanilla. Submerge uses scripted `RETURN01/02` after damage timers.
- Door comfort is mostly **2800 roll gate + 1700 lunge cap + poison LOS + boomerang-aim collider tweak** — not a single “safe flag.”

**Vanilla parry vs poison:**

- Shield Parry & Bash (`d_albw_shield.cpp`): perfect parry = **5 frames** after guard onset; poison splat 13 is **not** guard-break (9/10/11).
- Shield contact → `dShield_onShieldHit()` can negate **one tick**; non-parry shield → **guard slip** (large attack), not clean block.
- Body hits bypass shield entirely.

### Design directions (locked for next implementation pass)

#### 1 — Side-head attack consistency (~2× vanilla)

**Goal:** Halve time between attack *rolls* only — do **not** change lunge anim speed.

| Knob | Vanilla | Refinement target (~2×) |
|------|---------|-------------------------|
| `attack_freq_a` | 350 | **175** (or mult **0.5**) |
| `attack_freq_b` | 250 | **125** |
| `B_BQ.field_0x6fe` solo cooldown | 200–350 / 100–200 | **halve** |
| Post-attack `mTimers[0]` | 60 + rand(50) | **30 + rand(25)** |
| Post-revive `mTimers[3]` | 60 | **30** |
| P2 revive initial `mTimers[1]` | 300 / 400 | **150 / 200** |
| Event / spray interrupt timers in `b_bh_wait` / `b_bh_b_wait` | 50–350 hardcoded | **halve when Refinement on** |

**Hook:** `dAlbwBossRefinement_isEnabled()` + helpers in `d_albw_boss.cpp`; read mult in `d_a_b_bh.cpp` at timer assignment. Keep **2800** range gate in mid-arena; door volume may use conditional extend (see **Door / threshold safe zone** above).

#### 2 — Poison siphon → boss HP bar (revised formula)

**Rejected:** % of Link’s *current* HP pool (too swingy).

**Preferred (user spec):** flat **damage-unit → boss max %** table:

| Link damage taken (TP life units) | Boss regain (% of **max** display HP) |
|-----------------------------------|---------------------------------------|
| 4 units (= 1 heart) | **10%** |
| 2 units (= ½ heart) | **5%** |
| 8 units (= 2 hearts) | **20%** |

General rule: **`bossHealPct = (damageUnits / 4) × 10%`** of `field_0x560` (or scaled max from `dAlbwHP_tryApplyTrueMaxHp`), applied to `B_BQ.health` capped at max.

**Implementation notes:**

- Read damage from `mCcStts.GetDmg()` / delta before `setDamagePoint()` when attacker is `B_BQ` and action is poison (`ACTION_ATTACK`).
- Only heal when HP actually decreases (no siphon on blocked ticks).
- Cap per spray or per frame if multi-sphere stacking feels too strong (four spheres × 150 frames).
- HUD: existing `dAlbwHP_getLockonDisplayHp()` — no new bar math unless phase segmentation added later.

**Hook candidates:** `d_a_alink_damage.inc` (player hit), or `dAlbwBoss_diababaOnPoisonDamage(...)` from there / `d_cc_uty.cpp`.

#### 3 — Poison “rhythm parry” (player-only; boss unchanged)

**Goal:** Not continuous guard-hold — player **parries near the spray’s hit cadence** and takes **no damage** for those ticks. Spray continues; no boss stun / no spray cancel.

**Vanilla baseline:** Each poison tick is splat **13** (large). One perfect parry negates one shield hit; holding guard without parry → guard slip chip.

**Refinement concept:**

- On successful `dShield_onShieldHit()` vs `B_BQ` poison: grant a short **poison immunity window** (e.g. **12–18 frames**, tune to match eff-counter / sphere cadence) where subsequent `B_BQ` poison ticks deal **0** even without re-parrying every frame.
- Missing the rhythm → normal large-attack guard slip or body damage.
- Optional: treat **guard-up** (non-parry) as partial — user asked for **parry rate**, not hold; default to parry-triggered immunity only.
- No changes to `b_bq_attack` state machine unless needed for hit identification.

**Research:** Poison trail advances `field_0x1388 & 15` each active frame; collision uses standard eff-counter debounce (`SetAtEffCounterTimer` in `d_cc_s.cpp`). Tune immunity window to ~1× eff-counter period so one well-timed parry covers the next contact.

**Files:** `d_a_alink_damage.inc` (`ChkTgShieldHit` + `fpcNm_B_BQ_e`), `d_albw_shield.cpp` (immunity timer), optionally `d_albw_boss.cpp` (constants gated on Refinement).

#### 4 — Middle-head distance retreat (design interest)

**Vanilla:** Retreat is **phase-scripted** (`RETURN01/02`), not player-distance.

**Refinement idea (TBD):**

- When `mDistToPlayer > X` (and/or player in boomerang-aim safe band) for **Y** consecutive frames → play submerge (reuse `RETURN02` or wire **`BCK_BQ_RUNAWAY`** if anim fits).
- Must not softlock: re-appear when player re-enters arena band; coordinate with `B_BH` 2800 gate.
- Separate from side-head `BCK_BH_ROUT` downed state.

**Open:** Threshold X/Y, whether retreat cancels active poison, interaction with core-phase `ACTION_DAMAGE`.

### Implementation checklist (when coding)

- [x] `d_a_b_bq.cpp` — RUNAWAY arrow test (`ACTION_RUNAWAY_TEST`, phased: RUNAWAY / hold / APPEAR)
- [x] `d_a_b_bh.cpp` — side-head surface decouple + enrage frenzy (Refinement only)
- [ ] `d_a_b_bq.cpp` + `d_a_b_bh.cpp` — enrage **L/R/L/R/BOTH** conductor (beat 5 → APPEAR)
- [ ] Door volume + door-zone reach (middle poison snap and/or conditional lunge extend)
- [ ] `d_albw_boss.h/cpp` — Diababa constants + `dAlbwBoss_diababaAttackIntervalMult()` (or inline 0.5f)
- [ ] `d_a_b_bh.cpp` — apply mult to HIO reads + hardcoded waits when Refinement on
- [ ] `d_albw_boss.cpp` — siphon + optional poison parry immunity
- [ ] `d_a_alink_damage.inc` — call Diababa hooks on `B_BQ` hits
- [ ] `d_a_b_bq.cpp` — optional distance retreat (Refinement only)
- [ ] Playtest: Boss Refinement ON, Boss Health Bars ON, Boss HP × as desired

### Open questions

- Enrage conductor: beat timing (~45–60f solo), dual-beat telegraph, interrupt policy?
- Door safe zone: volume coords, camp timer length, poison snap vs trap vs lunge-only?
- Siphon: cap per poison spray (max % per burst)?
- Rhythm parry: immunity window length vs bash-charge economy (each parry still costs/grants charges as today?)
- Distance retreat: **`RUNAWAY` wired for arrow test (v1)** — compare vs `RETURN02` for distance retreat; fallback = `RETURN02` (proven FX/SFX path)
- Middle-head poison frequency: separate from side-head 2× (event-driven today)?

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
| Diababa middle head | `d_a_b_bq.cpp`, `d_a_b_bq.h` |
| Diababa side tentacles | `d_a_b_bh.cpp`, `d_a_b_bh.h` |
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

**To continue work:** open a **new** chat and reference this file + specific task (e.g. “implement Diababa 2× attack interval in Boss Refinement”, “tune Armogohma bow chip from 4% to 2%”, or “wire Zant phase 1 tool lock”).

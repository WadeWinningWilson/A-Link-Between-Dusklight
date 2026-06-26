# Future performance leaning — current suspicions

Tracking doc for restoring **stable 144 FPS in all play scenarios**. **User-confirmed golden baseline:** merge commit **`76b715abc6`** (Dusklight **v1.4.0** + ALBW) held **144 FPS consistently** — title, file select, overworld, combat (including bokoblins on main Hyrule Field), and **Armogohma**. Trust this baseline; current sub-60 field drops are **unacceptable**.

**Status (2026-06-18):** Automated drive bisect **Tracks B–J complete** — see **[performance-leaning-2026-06-18.md](performance-leaning-2026-06-18.md)** and **[performance-handoff.md](performance-handoff.md)**. Mean FPS ~143.8–144 on good oracle sessions; **intermittent ~105 cliffs** (~17% of sessions) on both WIP and v1.4.1. **Next:** profiler on hitch frame. **Build discipline:** [build-fps-guidelines.md](build-fps-guidelines.md). **Build-analysis chat:** doc map in build-fps-guidelines § “Doc map”; HUD: [hud-performance-handoff.md](hud-performance-handoff.md).

**Correct test scenes (confirmed):** main Hyrule Field — `F_SP121` room `0` point `0` (save-editor default); Armogohma — `D_MN06A`. Do not substitute other warps.

**Ordon / Dawnlight upstream comparison:** **MISSING / DISREGARD** — unrelated prior work from much earlier builds; not a valid bisect axis.

**Armogohma shield mission (`DUSK_DRIVE=armogohma_shield`):** **Out of scope for FPS baselines** — that automation was for earlier agents to detect combat bugs, not to measure perf. Do not cite arena/shield-validation drive numbers as FPS data.

---

## Mission

Restore **144 FPS in all scenarios** under **natural user play** (saved AppData config / real preset). Menu FPS is a sanity check only; **field steady-state at `F_SP121`** is the primary automated observation target until methodology is aligned with the prior Cursor instance.

| Bisect point | SHA | Notes |
|--------------|-----|-------|
| Golden | `76b715abc6` | v1.4.0 merge — user-confirmed 144 everywhere |
| v1.4.1 merge | `a736ab564e` | Also had 144 at merge time per user |
| WIP | `a736ab564e` + uncommitted | Boss refinement, shield HUD, boss HP HUD, Aurora `22351fb`, etc. |

---

## Leading suspects (still valid — not ruled out)

Ranked hypotheses to A/B once methodology is fixed:

1. **Lost fix from deleted sessions** — regression believed fixed post–v1.4.1, no git commit.
2. **Aurora `f49d3c5` → `22351fb`** — `process_events()` per frame (H7).
3. **`moveKantera()` full ALBW sim block** in `d_meter2_draw.cpp` (H6).
4. **WIP stage hooks** beyond HUD draw stubs (`d_s_play`, `d_albw_boss`, etc.) — H1–H4 draw-only stubs did not move field FPS.
5. **Environmental / build factor** — something present during user's 144 play but absent from current agent drives (see ineffective methods below).

Draw-only stubs already tried without meaningful field gain: boss HP HUD draw, shield HUD draw, kantera draw (H1–H4).

---

## Drive research — ineffective methods (2026-06-17, this Cursor instance)

**Verdict:** The drive batches run in this chat **did not replicate the prior instance's methodology** and **cannot be used to disprove the 144 baseline** or the suspect list above. Treat all FPS numbers from this session as **directional only** until protocol is corrected.

### What went wrong

| Failure | Detail |
|---------|--------|
| **No code baseline restore** | Did not restore `src/` from `local_dev_backup/drive_research/snapshots/2026-06-17-baseline/` before drives. Session edits accumulated in `drive.cpp` (observe mode, settings branches, v1.4.1 compat patches) on top of an already-mutated overlay. |
| **Staged tree bloat (+~69k lines)** | Accidentally staged entire `local_dev_backup/` into git index via stash checkout — not a clean bisect tree. |
| **Wrong mission framing** | Ran `armogohma_shield` as FPS bisect pair; used forced Armogohma mission settings (`shield_parry=1`, `infinite_hearts=1`, `boss_refinement=1`) when `DUSK_DRIVE_USER_CONFIG` was missing or cleared between pair legs. |
| **Over-automated field mission** | `hyrule_field` with editor grant + combat pad injection is **not** the rudimentary warp-and-observe drive the prior instance used for FPS sampling. |
| **Log wipe only** | Wiped `%AppData%/TwilitRealm/Dusklight/drive/*` but not **source** baseline — conflated runtime log cleanup with protocol "restore after n sessions". |
| **Detached bisect without WIP clarity** | Tested on detached `a736ab564e` + overlay while full WIP remained in `stash@{1}` (`drive-userconfig-wip`) — compared apples to oranges vs user's actual WIP build. |

### Numbers recorded (directional — do not over-interpret)

| Label | Build | Field play/combat avg | Notes |
|-------|-------|----------------------|-------|
| U0-wip-userconfig | WIP + overlay | ~58 combat | Full automation mission |
| U1-golden-userconfig | `76b715abc6` + overlay | ~58 field | Invalid Armogohma leg (prep stall) |
| U2-v141-basic-userconfig | `a736ab564e` + overlay | **61.8 play** / min 49.8 | Observe-only warp `F_SP121`, user config; menu ~131 |

**Pattern:** Menu/title ~130–144 on all builds; **field `F_SP121` ~50–67** under every method tried here — including golden and v1.4.1. That contradicts user memory unless **test conditions differ from natural play** (see next section).

### Likely difference vs prior Cursor instance

Prior instance reportedly achieved credible automation with **minimal** drive: warp + `aurora_get_fps()` observe, natural user config, snapshot/restore discipline, and **no** Armogohma shield mission for FPS. This instance instead:

- Expanded drive.cpp substantially during research (not reverted per batch).
- Mixed shield-validation missions with FPS bisect.
- Forced bisect cvars when user-config path failed.
- Skipped snapshot → drive → **full restore** cycle.

**Next step:** Deliberate with user on what the prior instance did differently (runner scripts, env vars, build target, scene dwell time, combat vs calm field) before more drives.

---

## Drive protocol (required before next batch)

1. **Snapshot** touched `src/` files to `local_dev_backup/drive_research/snapshots/<id>/`.
2. **One hypothesis** per batch; rebuild; run drive; log FPS.
3. **Full restore** all snapshotted files after `n` sessions — **no** leftover stubs in `src/`.
4. **Natural play:** `DUSK_DRIVE_USER_CONFIG=1` only; no forced `--cvar game.*` unless named isolation run.
5. **Basic FPS drive:** `hyrule_field` + user config → warp `F_SP121` → observe (`phase=play` samples only). No editor grant, no combat injection, **no** `armogohma_shield`.
6. Wipe `%AppData%/TwilitRealm/Dusklight/drive/*` before each run.
7. Drive instrumentation lives in **`local_dev_backup/`** or is reverted — not permanent in main tree unless user approves merge.

Runners: `local_dev_backup/session/Invoke-BasicFpsDrive.ps1` (after restore), `run_perf_fps_drive.ps1` (title/load only).

---

## Addendum (2026-06-25)

**Reconciles** the June 17 “field ~50–67 everywhere” drive numbers with the June 25 investigation outcome:

- Early drive batches in § “Drive research — ineffective methods” remain **directionally valid** for **protocol mistakes** (no baseline restore, wrong mission framing, shield drives as FPS tests).
- A **later session** on WIP @ `5b0fdaf` showed that **~77 FPS** on the same commit was caused by **dirty investigation source** (`fps-probe-temp` stash) and/or a **bad main `build/` executable**, not inherent WIP slowness. Clean worktree + good exe → **~144**.
- **~32 FPS** on all exes followed **`reconfigure_build.bat`** on main — a **build-environment** failure, not a gameplay regression.

**Do not** use June 17 field ~50–67 numbers to conclude WIP @ `5b0fdaf` is permanently sub-60. **Do** use them to avoid repeating bad drive protocol.

Full symptom table, agent avoid/do, and recovery: [build-fps-guidelines.md § Addendum (2026-06-25)](build-fps-guidelines.md#addendum-build-artifact-failures-2026-06-25). Drive session rules: [performance-handoff.md § Drive session addendum (2026-06-25)](performance-handoff.md#addendum-drive-session-protocol-2026-06-25).

---

## Session log (2026-06-18)

**[performance-leaning-2026-06-18.md](performance-leaning-2026-06-18.md)** — Tracks B/C (WIP stubs), pre141/v141merge matrix, Track D binary bisect, infrastructure, contradictions, next steps. Primary raw log: `local_dev_backup/drive_research/track_abc_progress.log`.

---

## Related docs

| Doc | Topic |
|-----|--------|
| [performance-leaning-2026-06-18.md](performance-leaning-2026-06-18.md) | **2026-06-18 drive bisect evidence** |
| [performance-handoff.md](performance-handoff.md) | Agent handoff, hitch/profiler next steps |
| [build-fps-guidelines.md](build-fps-guidelines.md) | Canonical build & launch for ~144 |
| [albw-port.md](albw-port.md) | ALBW port index |
| [shield-combat.md](shield-combat.md) | Shield HUD isolation only |
| `local_dev_backup/README.md` | Drive restore, env vars, covisible protocol |
| `docs/boss-fights-handoff.md` | Boss refinement toggles (separate chat) |

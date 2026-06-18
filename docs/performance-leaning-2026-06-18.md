# Performance leaning — drive bisect session (2026-06-18)

Continuation of [future-performance-leaning.md](future-performance-leaning.md). Documents **automated drive research through Track D** so work survives an unexpected Cursor close. **No final fix yet** — this is an evidence log.

**User goal:** restore **stable ~144 FPS** everywhere (golden baseline `76b715abc6` / `a736ab564e`). Current symptom: **field ~144**, **Armogohma ~119–127** under some WIP drive conditions.

**Test scenes (confirmed):**

| Mission | Stage | Room | Point |
|---------|-------|------|-------|
| `hyrule_field` | `F_SP121` | 0 | 0 |
| `armogohma` | `D_MN06A` | 0 | 50 |

**Armogohma shield mission (`DUSK_DRIVE=armogohma_shield`):** out of scope for FPS baselines — combat-validation automation only.

---

## Status summary (2026-06-18 afternoon)

| Question | Answer so far |
|----------|----------------|
| Did pre141 hold 144 on Armogohma? | **Yes** — B0 `play_avg=143.7`, `play_min=127`, 227 samples, no 125 gate fail |
| Did v1.4.1 merge break mean FPS? | **Mostly no** — full runs ~143.7–144.1; B0 hitch worse (`play_min=123.8`, 125 gate at 41 samples) |
| Do post-v1.4.1 commits (4 to `8ff98c94fe`) regress? | **No under Track D** — `post141-merge` and `aurora-bump` both ~144 sustained |
| Does uncommitted WIP regress vs committed HEAD? | **Depends on drive layer** — observe drive ~144; shield-mission drive ~127 (see below) |
| Single stubbable env flag? | **Ruled out** — Track C stubs (S1–S5, A1, A3, A6) do not recover 144 on WIP |

**Open fork:** ~127 on WIP Armogohma (Track B/C) vs ~144 (Track D) on the **same tree** — likely **which `drive.cpp` is deployed** (shield mission ~1947 lines vs observe overlay ~597 lines), not a single git commit.

---

## Git reference

| Label | SHA | Notes |
|-------|-----|-------|
| pre141 / golden | `76b715abc6` | v1.4.0 + ALBW merge |
| v141merge | `a736ab564e` | v1.4.1 merge |
| aurora-bump | `5418b1831d` | Aurora & flake.nix bump |
| post141-merge / HEAD committed | `8ff98c94fe` | Merge upstream post-v1.4.1 |
| wip-head | `8ff98c94fe` + uncommitted | Boss HUD, shield, meter, etc. |

Commits after v1.4.1 merge to HEAD: **4** (graphics backend label, menu pointer, Aurora bump, merge commit).

---

## Drive protocol (current standard)

**Observe drive:** `local_dev_backup/drive_research/bisect_overlay/src/dusk/drive.cpp`

| Env / setting | Value |
|---------------|-------|
| `DUSK_DRIVE_USER_CONFIG` | `1` (user AppData preset) |
| `DUSK_DRIVE_SKIP_MENU_GATE` | `1` (skip menu FPS gate after file-select hitch) |
| Play floor gate | any `phase=play` sample &lt; 125 → `event=play_fps_gate_fail`, session ends |
| Timeout | 200s per session |
| Launch cvars | `skipPreLaunchUI`, `enableAdvancedSettings`, `enableFullscreen=false`, `pauseOnFocusLost=false`, `enableFpsOverlay=true`, `enableFrameInterpolation=2` |

**WIP restore after batches:** `local_dev_backup/drive_research/snapshots/wip-restore-20260618/` (shield-mission `drive.cpp`).

**Primary log:** `local_dev_backup/drive_research/track_abc_progress.log` (append-only; survives missing jsonl).

**Runners:**

| Script | Purpose |
|--------|---------|
| `local_dev_backup/session/run_track_c_suspicion_batch.ps1` | WIP stubs S1–S5, A1, A3, A6 on Armogohma |
| `local_dev_backup/session/run_pre141_v141_suspicion_batch.ps1` | pre141 + v141merge full matrix |
| `local_dev_backup/session/run_track_d_binary.ps1` | Track D: B0 on wip-head, post141-merge, aurora-bump |

**Historical-tree build:** deploy overlay → `Seed-WorktreeSubmodules` → `Clone-MainBuildToWorktree` (path rewrite) → ninja incremental. **Do not** raw `cmake configure` on worktrees without deps seed (FetchContent SSL / path mismatch failures).

**Worktrees:**

| Path | SHA |
|------|-----|
| `dusklight-bisect-golden` | `76b715abc6` |
| `dusklight-bisect-v141` | `a736ab564e` |
| `dusklight-bisect-post141` | `8ff98c94fe` |
| `dusklight-bisect-aurora` | `5418b1831d` |

---

## Track B — WIP stub matrix (field + Armogohma)

**Date:** 2026-06-18 early morning. WIP HEAD, menu gate skipped.

| Label | Field | Armogohma | Notes |
|-------|-------|-----------|-------|
| B0-baseline | ~144 (`play_n=3`) | **127.3 avg**, `play_min` ~90, `play_n=202` | Canonical WIP regression signal |
| A2-boss-hp-hud | (logged in batch) | — | Draw stub |
| A4-aurora-events | — | — | Env stub |
| A5-d_s_play-hooks | — | — | Hook stub |

**B0 rerun (07:52–07:56):** field `play_avg=144.1` n=3; armogohma `play_avg=127.3` n=202 exit=natural.

Uses **shield-mission / restored WIP `drive.cpp`** on main tree (not observe overlay).

---

## Track C — WIP suspicion stubs (Armogohma only, 125 gate)

**Date:** 2026-06-18 morning. Valid batch after env-order fix (set stub env **after** `USER_CONFIG`).

| Label | play_avg | play_min | play_n | 125 gate |
|-------|----------|----------|--------|----------|
| B0-baseline | 139.3 | 119.8 | 8 | **fail** (early) |
| S1-frame-interp | 29.9 | 29.8 | 4 | fail (interp required) |
| S2-moveKantera | 139.2 | 121.0 | 8 | fail |
| S3-shield-tracking | 139.2 | 120.8 | 8 | fail |
| S5-hp-mult | 139.3 | 121.2 | 7 | fail |
| A1-boss-refinement | 139.9 | 123.3 | 7 | fail |
| A3-boss-both | 139.8 | 122.1 | 8 | fail |
| A6-settings-pacing | 138.1 | 119.8 | 8 | fail |

**Pattern:** brief ~139 avg then dip ~120; stubs do **not** restore 144. Early gate exits (`play_n=7–8`) vs full runs elsewhere (`play_n=225+`).

Earlier invalid runs (menu_gate_skip, `play_n=0`) from overlapping batches and env cleared before launch — discarded.

---

## Track pre141 + v141merge — version comparison (125 gate)

**Date:** 2026-06-18 late morning. GoldenCompat overlay on historical trees.

### pre141 (`76b715abc6`)

| Label | Field play_avg | Armogohma play_avg | Armogohma play_min | play_n | Gate |
|-------|----------------|--------------------|--------------------|--------|------|
| B0 | 0 (invalid) | **143.7** | 127 | 227 | no |
| S1-frame-interp | 0 | 30 | 29.9 | 5 | yes |
| S2-moveKantera | 0 | 144.2 | 143.7 | 227 | no |
| S3-shield-tracking | 0 | 144.2 | 144.1 | 227 | no |
| S5-hp-mult | 0 | 144.1 | 142.1 | 227 | no |
| A5-d_s_play-hooks | 142.6 | 143.7 | 136.9 | 227 | no |
| A6-settings-pacing | 144.1 | 144.1 | 143.1 | 228 | no |

**Field `play_avg=0`:** early exit / no play samples on most pre141 field warps — worth investigating separately. Armogohma credible.

### v141merge (`a736ab564e`)

| Label | Field play_avg | Armogohma play_avg | Armogohma play_min | play_n | Gate |
|-------|----------------|--------------------|--------------------|--------|------|
| B0 | 139.6 | **140.0** | **123.8** | 41 | **yes** |
| S2-moveKantera | 144.2 | 143.7 | 135.1 | 229 | no |
| S3-shield-tracking | 144.3 | 143.9 | 138.4 | 226 | no |
| S5-hp-mult | 144.1 | 144.1 | 143.9 | 226 | no |
| A5-d_s_play-hooks | 144.3 | 144.1 | 144.0 | 226 | no |
| A6-settings-pacing | 144.3 | 143.9 | 130.4 | 226 | no |

**Conclusion:** v1.4.1 merge did not collapse sustained mean FPS but **worsened hitches** (lower mins, B0 fails 125 gate early). Full Armogohma runs still ~144 mean.

---

## Track D — binary bisect (B0 only, observe drive)

**Date:** 2026-06-18 midday. Compares committed clusters vs WIP using **observe overlay** `drive.cpp`.

### Final results (after fixes)

| Build | Field avg / min / n | Armogohma avg / min / n | 125 gate |
|-------|---------------------|-------------------------|----------|
| **wip-head** | 144.2 / 143.9 / 3 | **144.0 / 141.1 / 226** | no |
| **post141-merge** (`8ff98c94fe`) | 143.5 / 143.1 / 3 | **144.1 / 143.0 / 225** | no |
| **aurora-bump** (`5418b1831d`) | 144.2 / 144.1 / 3 | **144.1 / 143.9 / 226** | no |

### Infrastructure failures fixed during Track D

| Failure | Fix |
|---------|-----|
| `drive.h` self-copy on wip-head | Skip copy when `TargetRoot eq RepoRoot` |
| Aurora overlay: no `conavigate.h` anchor | `Install-DriveOverlay.ps1` fallbacks (`menu_pointer.h`, `ui::initialize()`) |
| wip-head `play_n=0` (stale link) | Force delete `drive.cpp.obj` + rebuild |
| `-OnlyIds` resume no-op | Split comma-separated ids |

### Track D vs Track B/C contradiction

| Protocol | WIP Armogohma B0 | drive.cpp |
|----------|------------------|-----------|
| Track D (observe, `USER_CONFIG` preserved) | ~144 sustained, min 141.1 | observe overlay ~597 lines |
| Track B overnight (no `Deploy-ObserveDrive`) | ~127 sustained | shield-mission ~1947 lines |
| B0 rerun (`run_b0_baseline_rerun.ps1`) | 127.3 avg, n=202 | observe overlay, but **`USER_CONFIG` cleared** |
| Track C B0 (observe + suspicion stubs in `src/`) | ~139 avg, 125 gate fail | observe + `Apply-AllSuspicionStubs` |

**Env-order bug (confirmed in code):** `TrackAbcCommon.ps1` `Invoke-ObserveDrive` sets `DUSK_DRIVE_USER_CONFIG=1` then calls `Set-ArmogohmaStubEnv`, which calls `Clear-ArmogohmaStubEnv` and **wipes `USER_CONFIG`**. Track C/D set `USER_CONFIG` **after** stub env. Track D B0 skips `Set-ArmogohmaStubEnv` entirely.

**Track E** (2026-06-18 12:37–12:48): same wip-head exe, Armogohma only.

| Leg | drive.cpp | env | play_avg | play_min | play_n | Gate |
|-----|-----------|-----|----------|----------|--------|------|
| E1-observe | observe overlay | correct (`USER_CONFIG` after clears) | **143.7** | 130.7 | 226 | no |
| E3-shield | shield-mission snapshot | correct | — | — | **0** | no |

**E3 `play_n=0` explained (not a drive crash):** shield-mission `drive.cpp` treats `DUSK_DRIVE=armogohma` as **`armogohma_shield`** — logs to `armogohma_shield.log` with `phase=arena`, not `phase=play`. `Invoke-DriveGame.ps1` reads `armogohma.log` and only counts `phase=play` lines. **E3 FPS data was never parsed**; rerun must point the parser at `armogohma_shield.log` and `phase=arena` (or use `DUSK_DRIVE=perf_fps` / observe overlay for apples-to-apples baselines).
| E2-observe-broken-env | observe overlay | broken (old `Invoke-ObserveDrive` order) | **142.5** | 126.6 | 226 | no |

**Track E conclusions:**

- **Observe + correct env → ~144** — reproduces Track D.
- **Broken env order alone does not reproduce ~127** — E2 still ~142.5 avg over 226 samples (min 126.6). The morning B0 rerun (127.3) likely had **additional factors** (stale build, different drive deploy, or session state) — not env order alone.
- **Shield-mission drive → `play_n=0`** on E3 — **parser mismatch** (wrong log file + `phase=arena` vs `phase=play`), not proof of failure. Re-run needed with shield-aware parsing before comparing to ~127 Track B numbers.

**Env-order bug** in `TrackAbcCommon.ps1` `Invoke-ObserveDrive` is **fixed** (set `USER_CONFIG` after `Set-ArmogohmaStubEnv`) but is **not sufficient** to explain the full WIP regression.

---

## Infrastructure built (2026-06-18)

| Script | Role |
|--------|------|
| `CloneMainBuild.ps1` | Robocopy main `build/` + case-insensitive path rewrite + `FETCHCONTENT_FULLY_DISCONNECTED` |
| `Apply-DriveFileSelectHooks.ps1` | `getFileSelectForDrive`, `driveCanLoadSlot`, `driveLoadSlot` for golden trees |
| `Apply-ArmogohmaStubHooks.ps1` | A4/A5 stub injection |
| `SuspicionStub.ps1` / `ArmogohmaStub.ps1` | S1–S5 / A1–A6 env stubs |
| `TrackAbcCommon.ps1` | Shared helpers, `Restore-WipBuild`, `Ensure-Worktree` |
| `DriveBuild.ps1` | Ninja build (`ErrorActionPreference Continue` around stderr) |
| `Invoke-DriveGame.ps1` | Play gate detection, session parsing |
| `Install-DriveOverlay.ps1` | Hook injection into `m_Do_main`, `m_Do_controller_pad`, `overlay.cpp` |

| `local_dev_backup/session/run_track_f_shield_rerun.ps1` | Track F: observe vs shield with correct log parsing |

---

## Track F — shield drive rerun (correct log parsing)

**Date:** 2026-06-18 13:35–13:45. Extended `Invoke-DriveGame.ps1` with `-LogFileName` and `-SamplePhase`.

| Leg | Log | Phase | avg | min | n | Gate | Exit |
|-----|-----|-------|-----|-----|---|------|------|
| F1-observe | `armogohma.log` | play | **143.9** | 133.2 | 225 | no | close_window |
| F2-shield | `armogohma_shield.log` | arena | — | — | **0** | no | **forced** |

**F2 post-mortem:** shield log contains only `start` + `settings_applied` (user_config=1) — **no `phase=menu` lines** in 330s. Implies `onMainLoopFrame` never ran or game hung before main loop. Not a parser bug this time; shield-mission drive **failed to progress past boot** under current automation. Early `bisect_results.jsonl` shield runs that reported `arena_avg` may have used different env (no `USER_CONFIG`) or an older hook state.

---

## Leading suspects (updated ranking)

1. **Shield-mission drive boot stall** — Track F F2 hung after `settings_applied`; observe drive on same binary still ~144 (F1). Morning ~127 B0 rerun used observe drive with 202 play samples — still unexplained vs F1/E1 ~144.
2. **Lost uncommitted perf fix** — still possible; may have lived in drive or game code never committed.
3. **v1.4.1 hitch regression** — confirmed worse mins on v141merge B0; not sustained mean collapse.
4. **Aurora `process_events()` per frame** — not implicated alone (aurora-bump ~144 on Track D).
5. **`moveKantera()` ALBW sim** — S2 stub does not recover WIP 144 (Track C); still worth profiler diff.
6. **WIP stage hooks** (`d_s_play`, `d_albw_boss`, meter) — stubs inconclusive on WIP; binary trees clean under observe drive.

**Ruled out as sole cause:** any single post-v1.4.1 commit cluster; Aurora bump alone; individual S/A env stubs on WIP.

---

## Next steps (ordered)

**Drives exhausted (Tracks H–J).** See [performance-handoff.md](performance-handoff.md) for profiler investigation detail.

1. **Profiler diff** — good log (`track_j_logs/j1_armogohma.log`) vs bad (`track_h_logs/wip_r3` or `v141_r1`) at cliff ~t=153–176s; Tracy already in tree.
2. **Fix candidate** — minimal merge-friendly change once hotspot identified; validate 3× Armogohma oracle.
3. **Agent build contract** — [build-fps-guidelines.md](build-fps-guidelines.md) so WIP builds stay ~144-capable across uncommitted work.
4. **Reconcile morning B0 rerun 127.3** — if profiler does not explain user ~127 feel.
5. **Shield drive boot diagnosis** (Track F F2) — only if shield automation needed; not user-play FPS.
6. **Stash/reflog archaeology** for lost uncommitted perf fix — lower priority if oracle stable.

---

## Raw log pointer

All timestamped lines: `local_dev_backup/drive_research/track_abc_progress.log` (through 2026-06-18 13:45, includes Track F).

Per-track jsonl files (`track_c_results.jsonl`, `track_d_binary_results.jsonl`) were intended but **may be absent** — progress log is the authoritative recovery source for this session.

---

## Related docs

| Doc | Topic |
|-----|--------|
| [future-performance-leaning.md](future-performance-leaning.md) | Original suspicions + 2026-06-17 ineffective-methods note |
| [performance-handoff.md](performance-handoff.md) | Agent handoff, hitch signature, profiler suspects |
| [build-fps-guidelines.md](build-fps-guidelines.md) | Canonical build & launch for ~144 |
| [albw-port.md](albw-port.md) | ALBW port index |
| [boss-fights-handoff.md](boss-fights-handoff.md) | Boss refinement toggles |

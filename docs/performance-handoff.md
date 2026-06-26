# Performance investigation — agent handoff

**Read first:** [performance-leaning-2026-06-18.md](performance-leaning-2026-06-18.md) (full evidence tables Tracks B–J).

**Build discipline:** [build-fps-guidelines.md](build-fps-guidelines.md) — canonical preset, launch hygiene, merge rules.

**New instance?** For **build review / FPS optimization** (not feature implementation), use the **doc map** in [build-fps-guidelines.md](build-fps-guidelines.md) § “Doc map — read in this order”. HUD work also needs [hud-performance-handoff.md](hud-performance-handoff.md). This file is for hitch/profiler research; the build-analysis chat usually does not need Tracks B–J unless investigating cliffs.

## User goal (non-negotiable)

Restore **constant ~144 FPS in standard user play** — saved AppData preset, natural load path. **Not** shield-mission drive FPS. `DUSK_DRIVE=armogohma_shield` is bug-test automation only; ignore for baselines.

**Scenes:** Field `F_SP121` r0 p0; Armogohma `D_MN06A` r0 p50.

**User tolerance:** **~143.8 avg is acceptable** on full runs. Problem is **hitches** (sudden cliffs), not mean collapse.

## What is settled vs open

| Settled | Open |
|---------|------|
| pre141 ~144 sustained Armogohma (227+ samples) | **Root cause** of ~105 FPS cliff during D_MN06A observe |
| Mean FPS ~143.8–144.2 on good oracle sessions (WIP + v141) | Why user/manual play sometimes feels ~119–127 (env/build vs hitch) |
| v1.4.1 worse hitches, not mean collapse | Early-session gate fails (Track G) vs late-session pass — session variance |
| Post-v1.4.1 commits cleared at ~144 mean (Track D) | Profiler diff on hitch frame |
| Single env stubs ruled out (Track C, Track I) | Lost uncommitted perf fix (still possible) |
| Hitch affects **both** WIP and v141 (~17% oracle sessions) | Shield-mission drive boot stall (Track F F2) — separate from user-play FPS |
| Drive matrices exhausted (Tracks D, H, I, J) | Fix candidate once profiler identifies hotspot |

## User-natural oracle (canonical protocol)

**Drive:** `local_dev_backup/drive_research/bisect_overlay/src/dusk/drive.cpp` (observe, ~597 lines). **Main tree has no drive code** — do not restore shield `drive.cpp` to `src/`.

```powershell
Get-Process dusklight -EA SilentlyContinue | Stop-Process -Force -EA SilentlyContinue
powershell -NoProfile -ExecutionPolicy Bypass -File `
  "<repo-root>\local_dev_backup\session\run_track_g_user_natural.ps1"
```

| Env | Value |
|-----|-------|
| `DUSK_DRIVE_USER_CONFIG` | `1` |
| `DUSK_DRIVE_SKIP_MENU_GATE` | `1` |
| `DUSK_DRIVE` | `armogohma` or `hyrule_field` (set by runner) |

**Success oracle:** Armogohma `play_avg` ≥ 140, `play_n` ≥ 200, no `play_fps_gate_fail` (floor 125). **Failure oracle:** sustained `play_avg` &lt; 130 or gate fail with `play_n` ≥ 50.

**After every batch:** runner restores `snapshots/wip-restore-20260618/` (shield-mission drive for dev) and rebuilds main.

## Key paths

| Item | Path |
|------|------|
| Progress log | `local_dev_backup/drive_research/track_abc_progress.log` |
| Observe drive | `local_dev_backup/drive_research/bisect_overlay/src/dusk/drive.cpp` |
| WIP restore | `local_dev_backup/drive_research/snapshots/wip-restore-20260618/` |
| Main exe | `build/windows-msvc-relwithdebinfo/dusklight.exe` |
| Drive logs | `%AppData%/TwilitRealm/Dusklight/drive/*.log` |
| Hitch report | `local_dev_backup/drive_research/track_j_hitch_report.txt` |
| Hitch log archive | `local_dev_backup/drive_research/track_j_logs/`, `track_h_logs/` |
| Hitch analyzer | `local_dev_backup/session/Analyze-DriveHitchLog.ps1` |
| Regen report | `local_dev_backup/session/regen_track_j_report.ps1` |

## Git / worktrees

| Label | SHA | Worktree |
|-------|-----|----------|
| pre141 | `76b715abc6` | `dusklight-bisect-golden` |
| v141merge | `a736ab564e` | `dusklight-bisect-v141` |
| post141 | `8ff98c94fe` | `dusklight-bisect-post141` |
| aurora-bump | `5418b1831d` | `dusklight-bisect-aurora` |
| wip-head | `8ff98c94fe` + uncommitted | main `dusklight` |

Historical builds: `CloneMainBuild.ps1` + `Seed-WorktreeSubmodules` — **never** raw cmake on worktrees without deps seed.

## Session scripts

| Script | Purpose |
|--------|---------|
| `run_track_g_user_natural.ps1` | 3× Armogohma + 1× field oracle |
| `run_track_h_compare.ps1` | WIP vs v141 intermittent compare |
| `run_track_i_wip_clusters.ps1` | WIP feature cluster FPS (exhausted) |
| `run_track_j_hitch_repro.ps1` | Hitch repro + log analysis |
| `run_track_d_binary.ps1` | B0 binary bisect (exhausted) |
| `Install-DriveOverlay.ps1` | Hooks for historical trees |
| `Invoke-DriveGame.ps1` | Session runner; `-LogFileName` / `-SamplePhase` |

## Drive results summary (Tracks H–J)

### Track H — WIP vs v141 (3× each)

| Tree | r1 | r2 | r3 |
|------|----|----|-----|
| **wip** | 143.7 pass | 143.8 pass | 143.8 avg, **min 105.2**, gate fail |
| **v141** | 142.3 avg, **min 105.9**, gate fail | 144 pass | 143.8 pass |

Gate fails: `reason=observe_below_floor` at **~105 FPS** during Armogohma observe (~88s `observe_enter`, fail ~65–87s into observe).

### Track I — WIP feature clusters (10 runs)

Stub env **disables** feature. All full runs ~143.5–143.9 except S5 (142.2, min 127) and A4 (gate fail when aurora stubbed). **No single cluster is the culprit.**

### Track J — hitch repro (6× wip, 2026-06-18)

All six passed: **144.1–144.2 avg**, arena_min 142.7–144.1, 226–227 samples.

**Combined H + J (12 sessions):** **10 pass / 2 fail** (~17% hitch rate). Fails: `wip_r3`, `v141_r1`.

**Hitch signature (from `Analyze-DriveHitchLog.ps1`):**

- **Type:** sudden single-sample **cliff** (~144 → **~105**), not gradual mean collapse.
- **When:** `D_MN06A` observe phase, typically **65–90s after `observe_enter`** (~88s after session start).
- **Examples:**
  - `wip_r3_armogohma.log`: t=175.6s, fps=105.2, prev=144.3, Δ39.1
  - `v141_r1_armogohma.log`: t=153.5s, fps=105.9, prev=137.9, Δ32.0
- **Good sessions:** arena_min ~131–144, dips &lt;130 = 0.

Regenerate report: `local_dev_backup/session/regen_track_j_report.ps1`

---

## Profiler investigation — next work (ordered)

**Drives are done.** Do not run more mean-FPS cluster/binary matrices unless methodology changes.

### Objective

Identify **what runs on the hitch frame** (~t=153–176s in failing logs, ~65–87s into Armogohma observe) and produce a minimal fix.

### Comparison pair

| Role | Log | Notes |
|------|-----|-------|
| **Good** | `track_j_logs/j1_armogohma.log` (or j2–j6) | 144.1 avg, arena_min 144.1 |
| **Bad** | `track_h_logs/wip_r3_armogohma.log` or `v141_r1_armogohma.log` | cliff ~105, gate fail |

Align profiler capture to **observe phase** around cliff timestamp; `observe_enter` is consistently ~88.3–88.5s after session start.

### Tooling

- **Tracy** already linked (`TracyClient` in `CMakeLists.txt`; `FrameMark` / `ZoneScoped` in `m_Do_main.cpp`, `f_ap_game.cpp`, audio, etc.).
- Run RelWithDebInfo build with Tracy capture enabled; compare frame budgets good vs hitch.
- Optional: enrich observe-drive logging around hitch second if Tracy alone is inconclusive.

### Leading suspects (evidence trail)

Ranked by what drive bisect showed — **none confirmed** as cliff root cause yet:

| Rank | Suspect | Evidence for | Evidence against |
|------|---------|--------------|------------------|
| 1 | **Unknown shared gameplay/render spike** | Cliff on **both** WIP and v141; not WIP-only; happens mid-observe idle | No CPU stack yet |
| 2 | **v1.4.1 hitch regression** | Worse mins on v141merge B0; gate fails on v141_r1 | Full-run means still ~144; Track D cleared post-v1.4.1 commits |
| 3 | **`moveKantera()` / ALBW meter sim** (`d_meter2_draw.cpp`) | Original H6 suspect; kantera draw path active in play | S2 stub did not recover WIP 144 (Track C) |
| 4 | **Aurora `process_events()` per frame** | Aurora bump H7 suspect | aurora-bump tree ~144 on Track D; A4 stub worse when disabled |
| 5 | **WIP stage hooks** (`d_s_play`, `d_albw_boss`, boss HP HUD, shield HUD) | Large uncommitted WIP surface | Track I cluster stubs did not isolate culprit |
| 6 | **Lost uncommitted perf fix** | User memory of post–v1.4.1 fix never committed | Observe drive sustains ~144 on current tree — fix may be env/build not code |
| 7 | **Agent build / launch factor** | Prior session ~50–67 field under bad protocol | [build-fps-guidelines.md](build-fps-guidelines.md); oracle ~144 when correct |

**Ruled out as sole cause:** single post-v1.4.1 commit cluster; Aurora bump alone; any single `DUSK_DRIVE_STUB_*` on WIP; WIP-only sustained mean regression; shield-mission drive as FPS baseline.

### Profiler success criteria

1. Hotspot identified on hitch frame with Tracy (or equivalent).
2. Minimal code fix in normal `src/` (merge-friendly).
3. 3× Armogohma oracle pass with **no** 125 gate fail and arena_min ≥ 130.

### Morning B0 rerun 127.3 (still unexplained)

Observe drive, 202 play samples, same binary layer later scored ~144 (F1/E1). Possible session/env variance or pre-fix protocol bug — reconcile if hitch profiler does not explain user ~127 feel.

### Shield drive boot stall (Track F — separate track)

F2 (`armogohma_shield.log`) hung after `settings_applied` with `play_n=0`. Observe drive on same binary ~144. Diagnose only if shield automation is needed; **not** user-play FPS.

---

## Env bug (fixed)

`TrackAbcCommon.ps1` `Invoke-ObserveDrive` must set `DUSK_DRIVE_USER_CONFIG` **after** `Set-ArmogohmaStubEnv` (stub clear wipes it).

## Do not

- Use shield-mission `drive.cpp` for FPS conclusions.
- Report FPS from Debug/ASAN builds or with `DUSK_DRIVE*` env set during manual play.
- Run more cluster/binary drive matrices without new hypothesis.
- Commit without user request.
- Raw `cmake configure` on bisect worktrees without `CloneMainBuild.ps1`.

## Playtest build (2026-06-18)

**Ready now** — same binary that passed Track J (6/6 oracle, 144.1–144.2 avg).

| Item | Value |
|------|-------|
| **Exe** | `build/windows-msvc-relwithdebinfo/dusklight.exe` |
| **Tree** | `8ff98c94fe` + uncommitted WIP; **no drive code in `src/`** |
| **Build** | `build_run.bat` or `cmake --build build/windows-msvc-relwithdebinfo --target dusklight` |
| **Expect** | ~140–144 mean on field + Armogohma; **possible** rare ~105 hitch (~17% in drives) |

### Launch for playtest (natural play)

```powershell
Get-Process dusklight -EA SilentlyContinue | Stop-Process -Force -EA SilentlyContinue
Remove-Item Env:DUSK_DRIVE* -EA SilentlyContinue
Remove-Item Env:DUSK_DRIVE_STUB_* -EA SilentlyContinue
& "<repo-root>\build\windows-msvc-relwithdebinfo\dusklight.exe"
```

Use saved AppData preset only — no `--cvar` overrides, no drive env.

### Suggested playtest scenes

1. **Field:** `F_SP121` r0 p0 — roam/combat several minutes; FPS overlay on.
2. **Armogohma:** `D_MN06A` r0 p50 — full fight or idle observe ~2 min after arena load.

**Pass bar (user judgment):** feels ~140–144 sustained; occasional single-frame dip acceptable if rare; sustained sub-130 unacceptable.

### If user approves — commit handoff (next instance)

**Only commit when user explicitly asks after playtest.**

1. `git status` / `git diff` — review scope.
2. **Stage:** `src/`, `include/`, `files.cmake`, `CMakeLists.txt`, `.gitignore`, `docs/` (performance + build guidelines + ALBW handoffs as user wants).
3. **Do not stage:** drive sources, `albw_*_debug.txt`, `build_stdout.txt`, `build_stderr.txt`, `build/`, `local_dev_backup/` (gitignored).
4. Draft commit message from actual diff; one commit unless user wants split (ALBW features vs docs).
5. `git status` after commit to verify.

Rebuild before commit if source changed during playtest: `build_run.bat`.

---

## Addendum: June 2026 WIP FPS incident (resolved)

**Date:** 2026-06-25. **Commit:** `5b0fdaf` (checkpoint parry rework, focused arts, HUD meter baseline).

### Outcome

- WIP at this commit is **144-capable** on natural `load_save` → `F_SP121` room 3 with saved AppData preset (RelWithDebInfo, no drive env).
- Regression to **~77 FPS** was traced to **dirty investigation source** (`fps-probe-temp` stash content) and/or a **bad main `build/` executable** — not WIP HUD/meter/FA code.
- **~32 FPS** on all exes followed **`reconfigure_build.bat`** on main — build-environment artifact; recovered via investigate worktree rebuild.

### Ruled out (this incident)

| Suspect | Verdict |
|---------|---------|
| WIP feature content @ `5b0fdaf` | **Not the cause** — clean worktree + same commit → ~144 |
| Upstream baseline permanently broken | **No** — upstream hit ~144 before bad reconfigure |
| Drive code in normal play | **No** — drive hooks reverted from main; drive only runs when `DUSK_DRIVE` is set |

Full symptom table and agent avoid/do: [build-fps-guidelines.md § Addendum (2026-06-25)](build-fps-guidelines.md#addendum-build-artifact-failures-2026-06-25).

---

## Addendum: Drive session protocol (2026-06-25)

**Purpose:** Automated drives are an **oracle** for repeatable `load_save` → field FPS — not a substitute for natural user play. See also [build-fps-guidelines.md §2 Launch hygiene](build-fps-guidelines.md#2-launch-hygiene).

### When to drive vs manual play

| Use drives | Use manual play |
|------------|-----------------|
| Regression after a **known-good build** | “Does it feel like 144?” user judgment |
| Repeatable `F_SP121` gate (observe s 6–12, floor 127) | Saved AppData preset, no drive env |
| Labeled A/B (config, exe source) | Final sign-off before commit |

**Do not** use `armogohma_shield` or heavy combat automation as FPS baselines (existing handoff rule still applies).

### Pre-flight (every batch)

1. Build RelWithDebInfo first — prefer investigate worktree if main `build/` is suspect ([build-fps-guidelines.md addendum](build-fps-guidelines.md#addendum-build-artifact-failures-2026-06-25)).
2. Do **not** start with `reconfigure_build.bat` on main.
3. Do **not** pop `fps-probe-temp` onto main.
4. Kill stray `dusklight.exe` before rebuild/drive.

### Drive code location

| Item | Rule |
|------|------|
| `src/dusk/drive.cpp`, `include/dusk/drive.h` | **Gitignored** — local automation only |
| Hooks in `m_Do_main`, pad, `files.cmake` | **Not for shipping** — session overlay only; revert before handoff unless user keeps tooling |
| Scripts | `local_dev_backup/session/` (gitignored) |
| **`deploy_drive.ps1`** | **Avoid** — pad-hook patterns have broken good hooks |

### Runners

| Script | Role |
|--------|------|
| `local_dev_backup/session/run_load_save_drive.ps1` | Single dive: slot 0, config swap + restore, JSON result |
| `local_dev_backup/session/fps_drive_loop.ps1` | Worktree build + drive until PASS; optional exe copy to main |

Always pass **`-Dive N -Label descriptive-name`**. Example config: `$env:TEMP\dusklight-golden-minimal2.json` (document which config was used).

### Expected log chain (`load_save`)

```
menu_nav → file_select_ready → menu_wait (12s) → save_load slot=0 → load_enter stage=F_SP121 → load_fps_gate_pass
```

**Automation gotchas (2026-06-25):**

- Opening cutscene ~75s — pad inject alone may not reach file select; **`force_name_scene`** after grace is the recovery path (in local `drive.cpp` overlay).
- **Do not** start the 12s menu FPS window at **t=0** — only after `file_select_ready`.

### Pass / fail interpretation

| Result | Meaning | Fix first |
|--------|---------|-----------|
| `gate_min ≥ 127`, `play_avg ~140–144` | **PASS** | — |
| ~70 field, clean env | Wrong exe or dirty source | Exe A/B, stash audit — not HUD bisect |
| ~32 everywhere | Bad build artifact | Worktree rebuild — stop driving |
| `navigate_timeout`, `save_sent=0` | Automation bug | Drive navigation — not FPS code |

### After a drive batch

1. Clear `DUSK_DRIVE*` and `DUSK_DRIVE_STUB_*`.
2. Confirm **`config.json` restored** (WIP toggles intact).
3. One **manual** field check with no drive env.
4. Revert temporary drive hooks from main if session added them.
5. Do **not** commit drive artifacts or `local_dev_backup/`.

### One-line agent mandate

> Build clean RelWithDebInfo → run labeled `load_save` drive with config restore → PASS means ~144 on F_SP121 after real save load; otherwise fix **build, source hygiene, or automation** — not “revert the HUD.”

---

## Related docs

| Doc | Topic |
|-----|--------|
| [build-fps-guidelines.md](build-fps-guidelines.md) | **Agent build & launch contract** |
| [build-fps-guidelines.md § Addendum (2026-06-25)](build-fps-guidelines.md#addendum-build-artifact-failures-2026-06-25) | Symptom → cause, exe A/B, recovery |
| [commit-and-push.md](commit-and-push.md) | Fork remotes, commit/push workflow |
| [performance-leaning-2026-06-18.md](performance-leaning-2026-06-18.md) | Full evidence tables |
| [future-performance-leaning.md](future-performance-leaning.md) | Original suspicions + ineffective-methods note |

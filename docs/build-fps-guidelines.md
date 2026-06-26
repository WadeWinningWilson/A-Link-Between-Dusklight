# Build & FPS maintenance guidelines

**Audience:** Cursor agents and anyone building uncommitted WIP. Goal: keep **~139–144 FPS mean** under natural user play without the **~50–127 tank** that often follows agent builds.

**Role:** This is the **build-analysis chat** doc set. Feature agents (HUD, bosses, combat) implement; **this chat** reviews builds, diagnoses FPS drift, and optimizes in place. If you are a new instance and the user sent you here after a build — read the map below before changing code.

---

## Doc map — read in this order

| Order | Doc | When to use |
|-------|-----|-------------|
| **1** | **[build-fps-guidelines.md](build-fps-guidelines.md)** (this file) | Every build review: preset, launch hygiene, validation checklist |
| **2** | **[hud-performance-handoff.md](hud-performance-handoff.md)** | HUD / meter / shield / `game.lopHud` changes — per-frame rules, LoP cache patterns |
| **3** | **[performance-handoff.md](performance-handoff.md)** | Intermittent hitches (~105 cliffs), profiler follow-up, drive oracle (research only) |
| **4** | **[commit-and-push.md](commit-and-push.md)** | Commit/push to `upstream` (ALBW-Dusklight fork); never-commit list |
| **5** | **[performance-leaning-2026-06-18.md](performance-leaning-2026-06-18.md)** | Historical drive evidence if you need SHAs / what was ruled out |
| **6** | **[albw-hud-lop-layout-brief.md](albw-hud-lop-layout-brief.md)** | LoP layout *behavior* spec (feature agent); cross-ref §2 for FPS |
| **7** | **§ [Addendum: Build artifact failures (2026-06-25)](#addendum-build-artifact-failures-2026-06-25)** (this file) | Symptom → cause, agent avoid/do, `build_run` vs `reconfigure`, recovery |

**Feature-specific briefs** (read only if the diff touches that area): `boss-fights-handoff.md`, `TrueALBWWorld.md` (TRUETEST / open-world bootstrap), `albw-boss-hp-hud-tuning-brief.md`, `albw-hud-lop-layout-v3-instructions.md`, `testing-parry-rework.md` (RB parry + parry VFX/SFX test toggle).

---

## Feature agent onboarding (HUD, bosses, combat, TRUETEST, etc.)

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
| Boss fights / refinement | [boss-fights-handoff.md](boss-fights-handoff.md) |
| Boss HP bar tuning | [albw-boss-hp-hud-tuning-brief.md](albw-boss-hp-hud-tuning-brief.md) |
| True ALBW world / TRUETEST bootstrap | [TrueALBWWorld.md](TrueALBWWorld.md) |

**After they build — hand off here:**

| Chat | Doc entry |
|------|-----------|
| Build-analysis / FPS review | This file → Doc map section, then [hud-performance-handoff.md](hud-performance-handoff.md) if HUD changed |

**Short prompt (paste into any new feature chat):**

> Before coding: read `docs/build-fps-guidelines.md` and `docs/commit-and-push.md`.  
> Build with `build_run.bat` only (RelWithDebInfo). No drive/conavigate code in `src/`.  
> If HUD/meter/shield: also read `docs/hud-performance-handoff.md`.  
> When done: git diff summary + build result → build-analysis chat for FPS check before commit.  
> Push to **upstream** (ALBW-Dusklight), not origin.

**Hard rules (one-liners for any chat):**

- **Build:** `build/windows-msvc-relwithdebinfo/dusklight.exe` via `build_run.bat`
- **Never commit:** `local_dev_backup/`, drive/conavigate sources, `albw_*_debug.txt`
- **Don't revert features to fix FPS** — optimize in place; use the build-analysis chat
- **Don't commit/push unless the user explicitly asks**

### Build-review workflow (same as the long-running analysis chat)

1. `git status` + `git diff --stat` — scope the change.
2. `build_run.bat` — confirm RelWithDebInfo compiles.
3. Grep for regressions: `dusk/drive`, `dusk/conavigate`, per-frame `findFirstPicture`, repeated `getSettings().game.*.getValue()` in `draw()`/`exec()`.
4. Ask user (or infer from diff): did FPS drift with **`lopHud` Off**? Off = shared hot path; On = LoP-only branches.
5. Optimize **in place** — do not revert features to fix FPS (see [hud-performance-handoff.md](hud-performance-handoff.md) hard constraint).
6. Report: build pass/fail, likely cause, what you changed, what to playtest.

**Do not** run full drive matrices unless the user asks — oracle lives under `local_dev_backup/` (gitignored).

**Not covered here:** intermittent ~105 FPS hitches during Armogohma observe (~17% of oracle sessions). See [performance-handoff.md](performance-handoff.md) for profiler follow-up.

---

## What “144-capable” means on current WIP

Under the **user-natural oracle** (saved AppData preset, no drive env, correct exe), WIP averages **~143.8–144.2** on good runs. User accepts **~143.8 avg**; the open problem is **constant** 144 (single-frame cliffs), not mean FPS.

---

## 1. Canonical build

| Rule | Detail |
|------|--------|
| **Preset** | `windows-msvc-relwithdebinfo` only for FPS validation |
| **Exe** | `build/windows-msvc-relwithdebinfo/dusklight.exe` |
| **Never** | `windows-msvc-debug`, plain `Debug`, or ASAN presets for perf checks |
| **Configure** | Reuse existing build dir; avoid wipe/reconfigure unless necessary |
| **Worktrees** | Historical bisect trees: `CloneMainBuild.ps1` + submodule seed — **not** raw cmake |

`cmake-variants.yaml` defaults to **Debug**. Debug enables `DUSK_GFX_DEBUG_GROUPS`, unoptimized game code, and very different runtime cost than RelWithDebInfo.

**Standard agent build:**

```powershell
cmake --preset windows-msvc-relwithdebinfo
cmake --build build/windows-msvc-relwithdebinfo --target dusklight
```

Default CMake build type when unset is `RelWithDebInfo` (`CMakeLists.txt`); the preset above is still the explicit contract.

---

## 2. Launch hygiene

Before **manual** play or reporting FPS, clear drive session state:

```powershell
Get-Process dusklight -EA SilentlyContinue | Stop-Process -Force -EA SilentlyContinue
Remove-Item Env:DUSK_DRIVE* -EA SilentlyContinue
Remove-Item Env:DUSK_DRIVE_STUB_* -EA SilentlyContinue
```

| Do not leave set | Why |
|------------------|-----|
| `DUSK_DRIVE` / `DUSK_DRIVE_USER_CONFIG` | Would activate automation — **drive code must not live in main tree** (use `local_dev_backup/` only) |
| `DUSK_DRIVE_STUB_A*` / `S*` | Alters ALBW code paths for bisect only |
| CLI `--cvar` overrides from drive scripts | Not the user's saved AppData preset |

`onMainLoopFrame()` returns immediately when drive is inactive, but **`DUSK_DRIVE=armogohma`** enables heavy automation — FPS will not match natural play.

**Manual play:** launch with **no** `DUSK_DRIVE*` env vars. Automated oracle drives are a separate, labeled test ([performance-handoff.md](performance-handoff.md)).

---

## 3. Source rules (merge-friendly)

**Do**

- Put gameplay/HUD changes in normal `src/` files.
- Gate **TEMP** diagnostics behind an env flag or remove before handoff (several `albw_*_debug.txt` writers exist in shield/shop/rupee code).
- Keep drive instrumentation in `local_dev_backup/`; restore snapshots after research batches.
- One focused change → rebuild → spot-check field + Armogohma.

**Don't**

- Add drive/automation code to `src/` — keep in `local_dev_backup/drive_research/` only.
- Add per-frame file I/O, logging, or ImGui in hot paths without a toggle.
- Leave `DUSK_DRIVE_STUB_*` checks in production paths long-term.
- Stage/commit `local_dev_backup/` or debug `*.txt` artifacts.

---

## 4. User settings oracle (for comparing builds)

Match the saved AppData preset used in drives:

| Setting | Value |
|---------|-------|
| `enableFrameInterpolation` | capped (mode `2`) |
| `maxFrameRate` | 144 |
| `fullscreen` | false (consistent testing) |
| `pauseOnFocusLost` | false |
| `enableFpsOverlay` | on when measuring |

Drives set `DUSK_DRIVE_USER_CONFIG=1` to load this. Manual play should use AppData alone.

---

## 5. Quick validation after any agent build

1. Confirm exe: `build/windows-msvc-relwithdebinfo/dusklight.exe`
2. Confirm **no** `DUSK_DRIVE*` env in shell / shortcut
3. Field `F_SP121` r0 p0 — steady play **~140–144**
4. Optional oracle: `local_dev_backup/session/run_track_g_user_natural.ps1` — pass = `play_avg` ≥ 140, `play_n` ≥ 200, no 125 gate fail

If field is **~50–70** on RelWithDebInfo with clean env, suspect **wrong exe, Debug build, or drive env still set** — not “WIP is inherently slow.”

---

## 6. Merge receptivity

- Perf fixes live in **normal game code** (meter draw, boss hooks, Aurora usage), not drive overlay.
- No permanent env-gated stubs in hot paths.
- Remove TEMP debug dumps before upstream merge.
- Document intentional perf tradeoffs in commit/PR text.

---

## Addendum: Build artifact failures (2026-06-25)

**Context:** WIP @ `5b0fdaf` (parry rework, focused arts, HUD meters) is **144-capable**. A June 2026 investigation session saw **~77 FPS** and **~32 FPS** on the same commit — caused by **build/source hygiene**, not WIP gameplay code. See also [performance-handoff.md § Addendum: June 2026 WIP FPS incident](performance-handoff.md#addendum-june-2026-wip-fps-incident-resolved) and [future-performance-leaning.md § Addendum (2026-06-25)](future-performance-leaning.md#addendum-2026-06-25).

### Symptom → cause (quick reference)

| Observed FPS | Likely cause | First action |
|--------------|--------------|--------------|
| ~50–70 steady | **Debug** preset, wrong exe, or `DUSK_DRIVE*` still set | Confirm RelWithDebInfo exe; clear drive env |
| ~77 on WIP commit | **Dirty investigation source** on main (`fps-probe-temp` stash) **or** bad main `build/` exe | Clean tree or worktree build; same-commit exe A/B |
| ~32 on all exes | **Broken build environment** after main `reconfigure_build.bat` / fresh CMake without seeded `_deps` | Recover via worktree (`build_fps_probe.bat`); do not bisect HUD |
| ~105 single-sample cliff | Intermittent hitch (WIP and v1.4.1) | [performance-handoff.md](performance-handoff.md) profiler follow-up |
| ~140–144 field | Healthy RelWithDebInfo + clean source + good exe | Baseline |

### Three-layer incident (plain language)

1. **~77 FPS — dirty source:** Temporary bisect stubs, profiling hooks, and pad inject helpers (later in stash `fps-probe-temp`) were mixed into the main working tree. Same commit built cleanly in a worktree → **~144**; dirty main tree → **~77**.
2. **~77 FPS — bad executable:** Same source, worktree-built exe **144**, main `build/` exe **77**. Copying the worktree exe into main’s build folder restored **144** — a **link/cache artifact**, not a gameplay regression.
3. **~32 FPS — broken reconfigure:** After `reconfigure_build.bat` on main, **all** binaries (worktree, upstream, WIP) reported ~32 FPS. Environmental/build-deps failure, not WIP features. Cleared after rebuilding via the investigate worktree path.

**Do not** revert WIP HUD/FA/parry features to “fix” these symptoms — optimize in place (§3, [hud-performance-handoff.md](hud-performance-handoff.md)).

### Avoid (future AI builds)

- Leave **investigation stubs** in main `src/` (bisect env checks, `game_clock` profiling, file-select stubs, etc.).
- **Pop `fps-probe-temp`** (or similar investigation stashes) onto main without a deliberate bisect plan.
- Use **`reconfigure_build.bat`** as the default build — only when CMake configure is genuinely broken.
- **Delete or full-reconfigure** main `build/` as the first FPS fix attempt.
- Conclude **“WIP is slow”** when `build_run.bat` exits 0 but FPS is still low — suspect **source or exe**, not compile failure.
- Trust FPS during **manual play** with `DUSK_DRIVE*` or `DUSK_DRIVE_STUB_*` still set.
- **Commit** drive hooks in `m_Do_main.cpp`, `files.cmake`, etc. — drive sources stay gitignored (see [performance-handoff.md § Drive session addendum](performance-handoff.md#addendum-drive-session-protocol-2026-06-25)).

### Do instead

- **Incremental build:** `build_run.bat` → `build/windows-msvc-relwithdebinfo/dusklight.exe` on an **existing** RelWithDebInfo dir.
- **If main build is suspect:** build in `dusklight-fps-investigate` via `build_fps_probe.bat` (seeds `_deps` from main), then copy exe if needed.
- **Same-commit exe A/B:** worktree exe vs main `build/` exe before blaming gameplay code.
- **Clear drive env** before manual FPS reports (§2).
- **Optimize in place** per [hud-performance-handoff.md](hud-performance-handoff.md) when HUD changed.

### When `build_run.bat` is not enough

| Situation | Why | Recovery |
|-----------|-----|----------|
| Fresh worktree / missing `build/` | `cmake --preset` tries FetchContent download → SSL/network failures | Seed `_deps` from main; use `build_fps_probe.bat` or `CloneMainBuild.ps1` ([performance-leaning-2026-06-18.md](performance-leaning-2026-06-18.md)) |
| `build.ninja` exists but cache stale | `build_run.bat` **does not** reconfigure | Prefer incremental ninja; if deps/submodules changed badly, use offline reconfigure **once**, then return to incremental |
| Link errors after drive overlay | `files.cmake` lists gitignored `drive.cpp` but file missing | Restore drive files from `local_dev_backup/` or revert hooks; build in investigate worktree |
| Compile OK, FPS still ~70 | Dirty source or wrong exe | Stash audit + exe A/B — not more reconfigure |

### Why agents abandoned `build_run.bat`

| What agents reported | What was usually true |
|----------------------|------------------------|
| “Configure failed” | Fresh CMake without `FETCHCONTENT_FULLY_DISCONNECTED` / seeded `_deps` |
| “Build didn’t fix FPS” | Ninja succeeded; **source or exe** still bad |
| “Link error on drive.h” | Drive overlay partial after stash/checkout |
| “Worktree has no build” | Needed cloned build dir, not bare `build_run.bat` |

### `reconfigure_build.bat` hazard

Script at repo root forces full `cmake --preset` with offline FetchContent paths. Reasonable **once** when configure/fetch fails; **not** routine. In the June 2026 session it correlated with **global ~32 FPS** on all exes. Prefer worktree recovery over repeated main reconfigure.

### `fps-probe-temp` stash

Named investigation stash containing bisect/profiling changes that caused **~77 FPS** on main when applied. **Do not pop** onto main unless intentionally bisecting — use a dedicated worktree instead.

### Drive `config.json` swap

`local_dev_backup/session/run_load_save_drive.ps1` temporarily replaces `%AppData%/TwilitRealm/Dusklight/config.json` with a minimal test JSON and restores a backup in `finally`. If a drive is **killed** mid-run, settings may look “reverted” — not an FPS cause. Re-check WIP toggles in AppData after drive batches.

### Recovery workflow (local, gitignored)

1. Sync source into `dusklight-fps-investigate` (or use existing worktree @ target commit).
2. Run `build_fps_probe.bat` (seeds `_deps`, incremental ninja).
3. Optional validation: `local_dev_backup/session/fps_drive_loop.ps1` (build + `load_save` drive until PASS).
4. Copy `dusklight.exe` to main `build/windows-msvc-relwithdebinfo/` only after PASS if main artifact is suspect.
5. Clear `DUSK_DRIVE*` before manual play (§2).

---

## Related docs

| Doc | Topic |
|-----|--------|
| [performance-handoff.md](performance-handoff.md) | Drive oracle, hitch investigation, profiler next steps |
| [performance-handoff.md § June 2026 incident](performance-handoff.md#addendum-june-2026-wip-fps-incident-resolved) | Resolved ~77/~32 FPS root causes |
| [performance-handoff.md § Drive session (2026-06-25)](performance-handoff.md#addendum-drive-session-protocol-2026-06-25) | `load_save` drive protocol, runners, pass/fail |
| [performance-leaning-2026-06-18.md](performance-leaning-2026-06-18.md) | Full drive evidence (Tracks B–J) |
| [future-performance-leaning.md](future-performance-leaning.md) | Original suspicions + ineffective-methods note |
| [commit-and-push.md](commit-and-push.md) | Fork remotes, commit/push workflow |
| [hud-performance-handoff.md](hud-performance-handoff.md) | **LoP HUD FPS — for Claude/feature agents** |
| [TrueALBWWorld.md](TrueALBWWorld.md) | TRUETEST / open-world bootstrap design handoff |

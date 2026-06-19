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

**Feature-specific briefs** (read only if the diff touches that area): `boss-fights-handoff.md`, `albw-boss-hp-hud-tuning-brief.md`, `albw-hud-lop-layout-v3-instructions.md`.

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

## Related docs

| Doc | Topic |
|-----|--------|
| [performance-handoff.md](performance-handoff.md) | Drive oracle, hitch investigation, profiler next steps |
| [performance-leaning-2026-06-18.md](performance-leaning-2026-06-18.md) | Full drive evidence (Tracks B–J) |
| [future-performance-leaning.md](future-performance-leaning.md) | Original suspicions + ineffective-methods note |
| [commit-and-push.md](commit-and-push.md) | Fork remotes, commit/push workflow |
| [hud-performance-handoff.md](hud-performance-handoff.md) | **LoP HUD FPS — for Claude/feature agents** |

# HUD performance handoff — for Claude / Cursor agents

**To:** Claude (HUD implementation) and other agents touching ALBW HUD  
**From:** Cursor build/FPS guidelines author (diagnosis pass, 2026-06-18)  
**User workflow:** Use the **build-analysis chat** (not the feature chat) to validate FPS after HUD changes.

**New instance taking over build review?** Start at [build-fps-guidelines.md](build-fps-guidelines.md) → **Doc map** section, then return here for LoP/HUD-specific rules. Do not implement features in the analysis chat unless the user asks; diagnose → optimize in place → rebuild.

---

## Hard constraint (non-negotiable)

**Do not revert or strip the Lies of Link HUD** (`game.lopHud`, `dLopHudOn()` / `mLopHudActive`) to “fix” FPS. This feature was **git checkout–wiped once** and required a full manual rebuild. Optimize **in place** only.

---

## What the diagnosis pass included (2026-06-18)

LoP HUD had drifted off the ~144 baseline. User confirmed **stable again** after these **in-place** optimizations (feature + gating unchanged):

### 1. Per-frame `lopHud` cache (`d_meter2_draw.cpp`)

**Problem:** `dLopHudOn()` / `game.lopHud.getValue()` was consulted ~12–15×/frame across `exec()`, `draw()`, rupee/button paths, and shield code — even with **`lopHud` Off** (default).

**Fix:**

- `dMeter2Draw_c::mLopHudActive` — set **once** at start of `exec()` and `draw()`.
- Member methods use `mLopHudActive` instead of repeated `dLopHudOn()`.
- `bool dLopHudOn()` (exported in `d_meter2_draw.h`) reads the cached flag from the live meter draw instance; falls back to `getValue()` only if meter/draw is unavailable.
- `bool isLopHudActive() const` on `dMeter2Draw_c`.

### 2. Shield anchor cache when LoP on (`d_meter2_draw.cpp`)

**Problem:** `getShieldHudAnchorCenter()` called `getGlobalVtxCenter()` per bash/wolf draw when LoP on.

**Fix:**

- `mLopShieldAnchor` + `mLopShieldAnchorValid` computed **once** at `draw()` start when `mLopHudActive`.
- `getShieldHudAnchorCenter()` returns cached anchor when valid.

### 3. Shield-Only picture cache restored (`d_albw_shield.cpp`)

**Problem:** LoP merge **lost** the white-box fix cache. Shield-Only mode ran `findFirstPicture()` **per charge slot per frame** — pane tree walk × `maxCharges`, **even with `lopHud` Off**.

**Fix (re-applied):**

- `sHudShieldApplied`, `sHudShieldOffPic` — set in `applyParryIcons()`, cleared in `deleteBashHud()`.
- Shield-Only draw loop uses cached `sHudShieldOffPic`; skips draw until texture + starburst hide are applied (no spur flash).
- `computeBashHudLayout()` / row layout use `dLopHudOn()` (cached), not raw `getValue()`.

### 4. Build fixes from the same pass

- `dLopHudOn()` must have **external linkage** (not inside anonymous namespace) — `d_albw_shield.cpp` links against it.
- `#include "d/d_meter2.h"` in `d_meter2_draw.cpp` for meter draw cache lookup.

---

## Diagnosis method (use every HUD build)

1. **Build:** `build_run.bat` → `windows-msvc-relwithdebinfo` only ([build-fps-guidelines.md](build-fps-guidelines.md) §1).
2. **Launch:** no `DUSK_DRIVE*` env ([build-fps-guidelines.md](build-fps-guidelines.md) §2).
3. **Isolate LoP:**
   - Test with **`game.lopHud` Off** first (shipping default). If FPS is bad here, suspect **shared HUD hot paths** (shield draw, meter draw), not LoP-only branches.
   - Test with **`game.lopHud` On** second. If only On is bad, suspect ring trim / anchor / durability relocation.
4. **Pass bar:** field `F_SP121` ~140–144 sustained; optional oracle per guidelines §5.

### Ruled out as primary Off-default drift

- `ConfigVar::getValue()` itself — trivial inline read; the issue was **call frequency** and **heavy work behind guards**, not the getter.
- LoP `cont_n` child sweep — runs only when **`lopHud` On**.

### Confirmed Off-default regression source

- Per-frame `findFirstPicture()` in Shield-Only `dShield_drawBashCharges()` loop (lost cache after revert).

---

## Rules for further HUD work

### Per-frame hot path (`draw()`, `exec()`, `dShield_drawBashCharges()`)

| Do | Don't |
|----|-------|
| Cache settings flags **once per frame** (`mLopHudActive` pattern) | Call `dusk::getSettings().game.*.getValue()` repeatedly in the same frame |
| Cache `J2DPicture*` / pane lookups on **apply or rebuild** | `findFirstPicture()` / pane tree walks every frame × slot count |
| Cache `getGlobalVtxCenter()` when anchor is stable for the frame | Re-query global verts multiple times per draw for the same anchor |
| Gate heavy layout behind `mLopHudActive` / `dLopHudOn()` | Add unconditional per-frame work “because default is Off” — Off still pays for **calls and shared paths** |

### LoP-specific (when `lopHud` On)

- **Ring trim** (`cont_n` child sweep in `draw()`): still **required every frame** while LoP is on — vanilla move phase re-shows button children before draw. Do not remove without replacing vanilla visibility behavior.
- **Toggle detectors** in `d_meter2.cpp` (rupee / cross / buttons): one `getValue()` per move function is fine; they only force redraw on **edge** change.
- One-shot lift caches (`sLopBtnLift`, `sLopRupeeYOffset`) are good — keep that pattern for new reposition logic.

### Diagnostics

- Do **not** add per-frame `fopen` / `albw_*_debug.txt` writes in draw paths. Existing TEMP logs are change-gated or settings-gated only.
- Drive / CoNavigate code stays out of `src/` ([commit-and-push.md](commit-and-push.md)).

### After you build

Hand off to the **build-analysis chat** with:

- `git diff --stat` (or list of files touched)
- Build log if failed
- Whether drift was seen with `lopHud` Off, On, or both

That chat will compare against this doc and [build-fps-guidelines.md](build-fps-guidelines.md) — not re-run full drive matrices unless asked.

---

## Key files (LoP HUD surface)

| File | Role |
|------|------|
| `src/d/d_meter2_draw.cpp` | LoP layout, `mLopHudActive`, ring trim, anchors, rupee/button reposition |
| `include/d/d_meter2_draw.h` | `dLopHudOn()`, `isLopHudActive()`, member cache fields |
| `src/d/d_albw_shield.cpp` | Bash/shield row, Shield-Only cache, `computeBashHudLayout` LoP branch |
| `src/d/d_meter2.cpp` | Toggle invalidation for rupee / cross / button redraw |
| `include/dusk/settings.h` | `game.lopHud` (default **Off**) |

---

## Related docs

| Doc | Topic |
|-----|--------|
| [build-fps-guidelines.md](build-fps-guidelines.md) | Canonical build, launch, validation |
| [albw-hud-lop-layout-v3-instructions.md](albw-hud-lop-layout-v3-instructions.md) | LoP layout design spec |
| [commit-and-push.md](commit-and-push.md) | Push to `upstream` (ALBW-Dusklight fork) |

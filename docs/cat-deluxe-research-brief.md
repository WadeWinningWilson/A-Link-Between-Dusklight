# Cat Deluxe shield durability — Claude research brief (R4b)

**Status: complete (C1–C10).** Summary in [shield-combat.md](shield-combat.md) § R4b. **No cherry-pick possible** — implement Phase 2 from spec + R5 hooks.

## Results (integrated)

| C# | Answer |
|----|--------|
| C1 | [Captainkittyca2/duskLight](https://github.com/Captainkittyca2/duskLight) `main` `7a77d48954847330a5ab55f5c7f2e64cdd25d846` — no mod code |
| C2 | `D:\Decomps\Dusk Mods\CatDeluxe\source code\duskLight-1.1.1\` = stale vanilla; mod = `dusk.exe` only |
| C3 | Inline `d_a_alink.cpp` / guard+damage `.inc`; settings in `dusk/settings.*` |
| C4 | `enableShieldDurability`; max/drain unknown; toast "Your shield broke" |
| C5 | HUD unknown — ALBW: gauge index 3 or custom draw |
| C6 | See settings table in shield-combat.md |
| C7 | Zero `field_0x2fd5` when durability on; new counter → `procGuardBreakInit` at 0 |
| C8 | R5 hook map + settings + toast |
| C9 | Cat Deluxe manual = R-only; ALBW Phase 1 = Dawnlight L+R |
| C10 | Write from spec, M effort, credit Captain Kitty Cat |

---

**Original brief** (for archival):

**Context:** [shield-combat.md](shield-combat.md) — Dawnlight manual shield (R1–R7) is done. Shield **durability** is **not** in Dawnlight; it is in **Cat Deluxe** (exe-only).

**ALBW target repo:** `C:\Users\<user>\Documents\dusklight` (ALBW-Dusklight).

---

## Hard constraints

- **No implementation** in ALBW-Dusklight.
- **No commits** unless user asks to merge findings into `docs/shield-combat.md`.
- Distinguish three artifacts:
  1. **Captainkittyca2/duskLight** (Git — primary)
  2. **Local** `D:\Decomps\Dusk Mods\CatDeluxe\`
  3. **ALBW-Dusklight** (integration target — read-only for hooks)

---

## Local layout (verified)

```
D:\Decomps\Dusk Mods\CatDeluxe\
  dusk.exe              # Mod binary (~33,812,992 bytes) — ship artifact
  Setup.txt             # Install: replace TwilitRealm dusk v1.0.0 exe; F1 → Advanced → Shift+F1 → "Cat Deluxe" ImGui tab
  Contributors.txt      # Captain Kitty Cat + credits
  source code\
    duskLight-1.1.1\    # Full CMake tree (folder name "duskLight-1.1.1")
    duskLight-1.1.1 (1).zip
```

**Important:** Initial grep of `source code\duskLight-1.1.1\` found **no** `Cat Deluxe` / `manualShielding`-style markers in `src\dusk\`. Treat bundled source as **possibly stale or vanilla** until proven to match `dusk.exe`. If source ≠ exe, prioritize **Git repo** and **binary/string diff**.

**Exe string scan (done on `CatDeluxe\dusk.exe`):**

| Literal | Implication |
|---------|-------------|
| `game.enableShieldDurability` | Dusk `ConfigVar` — durability toggle |
| `game.enableManualShielding` | Same family as Dawnlight manual shield |
| `Cat Deluxe` / `Mod by Captain Kitty Cat` | ImGui mod tab |
| `Manual Shielding` / `Hold R to guard with your shield while targeting.` | UI copy (R = right trigger / ZR) |
| `Shield Durability` | Feature name in settings |
| `C:\duskTest\duskMayhem\dusk\src\d\actor\...` | Author build path hints (`duskMayhem` fork name) |

Also present: generic `Shield`, `Guard`, `Captain`, `Kitty`. Use Git **[Captainkittyca2/duskLight](https://github.com/Captainkittyca2/duskLight)** and ripgrep `enableShieldDurability` / `enableManualShielding` in `settings.h` to find implementation files.

---

## Research workflow (follow in order)

### Step 1 — GitHub (most up to date)

1. Clone or browse **[Captainkittyca2/duskLight](https://github.com/Captainkittyca2/duskLight)**.
2. Record **default branch**, **latest commit SHA**, and **latest release/tag** (if any).
3. Search repo (case-insensitive) for:
   - `Cat Deluxe`, `CatDeluxe`, `durab`, `shield hp`, `shieldHp`, `ShieldDur`, `mShield`, `guard meter`, `field_0x2fd5`
   - ImGui tab registration (`Cat Deluxe` menu label)
   - `manualShield`, `NG+`, `Z(R)`, armor swap D-pad (note if present — out of scope but helps find mod files)
4. List **every file** that implements shield durability (not vanilla `mShieldModel` / `PLATFORM_SHIELD`).

### Step 2 — Reconcile with local folder

1. Compare `Captainkittyca2/duskLight` @ latest commit vs `D:\Decomps\Dusk Mods\CatDeluxe\source code\duskLight-1.1.1\`.
   - Same tree? Same mod markers? If local zip is older, **Git wins**.
2. If local source lacks mod code, state clearly: **“Cat Deluxe mod lives in built exe and/or Git only.”**

### Step 3 — Binary diff (if source unclear)

Compare **one** vanilla baseline vs Cat Deluxe mod exe:

| Build | Suggested path |
|-------|----------------|
| Cat Deluxe mod | `D:\Decomps\Dusk Mods\CatDeluxe\dusk.exe` |
| Vanilla / upstream | TwilitRealm [dusklight releases](https://github.com/TwilitRealm/dusklight/releases) **or** user’s `build\...\dusklight.exe` @ known version |

Methods (pick what you have):

- `strings dusk.exe | findstr /i shield durab guard Cat`
- Export symbols if PDB exists (unlikely for distributed exe)
- If author publishes source only on Git, skip deep RE — document gap

Goal: confirm **runtime-only** features vs compiled-in source.

### Step 4 — In-game / ImGui discovery (optional, user-assisted)

Per `Setup.txt`:

1. Enable **Advanced Settings** (F1 → Settings → Interface).
2. **Shift+F1** → ImGui → **Cat Deluxe** tab.
3. List toggles related to **shield**, **durability**, **guard**, **manual shield** (screenshot or exact label strings helps match exe literals).

### Step 5 — Map durability mechanics (required detail)

For each mechanic found, document:

| Question | Answer needed |
|----------|----------------|
| State variable name(s) | e.g. `sShieldDurability`, member on `daAlink_c`, save field? |
| Max / initial value | Fixed or per shield type (wood / hylian)? |
| Drain trigger | `procGuardSlipInit`, `setSmallGuard`, per-frame while guarding, on hit only? |
| Break behavior | `procGuardBreakInit`, remove shield item, temporary disable? |
| Interaction with vanilla `field_0x2fd5` (4 slips) | Disabled, parallel, or replaced? |
| HUD | Which gauge index, BLO pane, ImGui overlay, custom draw? |
| Recovery | Passive regen out of combat? |
| Settings | ConfigVar name, default, menu path |

### Step 6 — Map to ALBW-Dusklight (read-only)

Using [shield-combat.md](shield-combat.md) R5 hooks, produce **Cat Deluxe path → ALBW path** table:

| Cat Deluxe | ALBW-Dusklight (current) |
|------------|---------------------------|
| (fill) | `d_a_alink_damage.inc` ~683–720 |
| (fill) | `d_a_alink_guard.inc` `setShieldGuard` |
| (fill) | `d_meter2.cpp` recovery ~1583 |
| (fill) | New `d_albw_shield.*` if no 1:1 file |

Note version skew: Cat Deluxe may target **dusk 1.0.0–1.1.x**; ALBW tree is **newer dusklight** — call out merge conflicts.

### Step 7 — Port recommendation

- **Cherry-pick** vs **rewrite** vs **ideas-only** (if code is exe-only and no Git match).
- Effort **S/M/L** for Phase 2 durability port after Dawnlight Phase 1 manual shield.
- License / credit line for Captain Kitty Cat.

---

## Required deliverables (return filled table)

| ID | Deliverable |
|----|-------------|
| **C1** | Git: repo URL, branch, **commit SHA**, date |
| **C2** | Does bundled local `duskLight-1.1.1` match Git + exe? (yes/no + why) |
| **C3** | **File list** + function names for shield durability |
| **C4** | State variables, max, drain formula, break condition |
| **C5** | HUD implementation (gauge index / draw function / UI strings) |
| **C6** | Settings: ConfigVar / ImGui label / default |
| **C7** | Relation to vanilla `field_0x2fd5` and `field_0x2fcb` |
| **C8** | **Hook map** → ALBW-Dusklight files (for Phase 2) |
| **C9** | Overlap with Dawnlight `9ff9d35` manual shield (same author ecosystem?) |
| **C10** | Port strategy + effort + risks |

---

## Search commands (quick reference)

**Git checkout:**

```powershell
git clone https://github.com/Captainkittyca2/duskLight.git
cd duskLight
git log -1 --oneline
```

**Ripgrep (repo root):**

```powershell
rg -i "cat deluxe|durab|shield.*hp|shieldhp|guard.*meter|2fd5" src include
rg -i "Cat Deluxe" src/dusk
```

**Local Cat Deluxe source:**

```powershell
rg -i "cat deluxe|durab|shield" "D:\Decomps\Dusk Mods\CatDeluxe\source code\duskLight-1.1.1\src"
```

**Local exe strings (PowerShell):**

```powershell
$f = "D:\Decomps\Dusk Mods\CatDeluxe\dusk.exe"
Select-String -Path $f -Pattern "durab","Cat Deluxe","Shield" -Encoding byte -SimpleMatch
# or: strings.exe from Sysinternals if installed
```

---

## Success criteria

A coding agent can start **Phase 2 (shield durability)** without re-investigating Cat Deluxe: clear variables, hooks, HUD slot, and whether to port code or reimplement from spec.

Update **`docs/shield-combat.md`** § R4b with a one-paragraph summary + link to this report when done (if user requests doc update).

---

## Credits to cite

- **Captain Kitty Cat** / [Captainkittyca2](https://github.com/Captainkittyca2) — Cat Deluxe
- **kamilink7** — manual shield (Dawnlight; separate port)
- **ALBW-Dusklight** — parry → ALBW meter, recovery pause while guarding

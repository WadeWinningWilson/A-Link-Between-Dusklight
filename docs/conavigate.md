# CoNavigate (dev-only agent trace)

CoNavigate is an **opt-in, session-only** trace for debugging input and ALBW combat routing while a developer (or agent, with user approval) observes gameplay.

It is **not** for player builds.

## Enable at compile time

```bash
cmake --preset windows-msvc-relwithdebinfo -DDUSK_ENABLE_CONAVIGATE=ON
ninja -C build/windows-msvc-relwithdebinfo dusklight
```

Release / CI presets leave `DUSK_ENABLE_CONAVIGATE` **OFF** (default).

## Arm at runtime

Set before launch:

```bash
set DUSK_CONAVIGATE=1
dusklight.exe
```

Without this env var, no CoNavigate code runs even in dev builds.

## Log location

```
%AppData%/TwilitRealm/Dusklight/conavigate/session.log
```

(Uses `dusk::CachePath / "conavigate" / "session.log"`.)

The log is truncated at each armed launch. Session end writes a footer when the game exits cleanly.

## What is logged

- **Session config** — `hiddenSkillRework`, `shieldParry`, `manualShield` at launch
- **Pad edges / meaningful stick changes** — hold/trig masks, stick vector, Link proc, Do status, lock-on, ALBW meter (numeric)
- **Combat events** — Jump Strike / charge inits, charge release, cancel, `checkDoCutAction` fallthrough
- **Expanded cut snapshots (`[cutframe]`, `[cut]`, `[doCut]`, `[jsready]`)** — per-frame while charging/turn-moving/slamming; full context: proc, under-frame, do/sword, lock, rework flag, charge-ready flag, turn-mode, sword BTK frame, `varR4`, branch taken

## Repository hygiene

- Log output lives under AppData, **not** the git tree.
- `.gitignore` includes `**/conavigate/` patterns.
- Never commit `session.log` or other CoNavigate artifacts.

## Agent protocol

1. Ask user before arming CoNavigate or launching the game.
2. Start trace when the game launches (`DUSK_CONAVIGATE=1`).
3. User plays; agent reads `session.log`.
4. When Dusklight closes, verify no `dusklight.exe` remains, then report **"CoNavigating has stopped"**.

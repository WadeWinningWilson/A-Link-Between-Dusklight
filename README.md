# ALBW-Dusklight

A work-in-progress mod for [Dusklight](https://github.com/TwilitRealm/dusklight) — the open-source PC reimplementation of *The Legend of Zelda: Twilight Princess* — that brings an *A Link Between Worlds*-style energy meter system to the game.

> **⚠️ Work in Progress** — Core systems are playable and stable, but a few features are still being finalised. See [Current Status](#current-status) below.

---

## What This Mod Does

Instead of the lantern oil meter, Link uses an **ALBW-style energy meter** that drains with combat and movement, then automatically recharges over time. You start with a base meter and can expand it by collecting items at the in-game **Rental Shop**.

### Features

| Feature | Status |
|---|---|
| ALBW Energy Meter HUD (fades with the rest of the HUD) | ✅ |
| Rental Shop (buy/rent items, ????? slots for unearned items) | ✅ |
| Sword attacks drain meter (standing, running, jumping, spin, horseback) | ✅ |
| Agility moves drain meter (sidestep, backflip, roll) | ✅ |
| Hidden Skills require a fully charged meter (Jump Strike, Mortal Draw, Great Spin) | ✅ |
| Meter expands with upgrades; recovery rate scales up with each unlock | ✅ |
| Wolf Link sections are fully unaffected by the meter | ✅ |
| Wolf Link game-over warp to Ordon blocked until Master Sword obtained | ✅ |
| Postman route and rental shop tied to story progression flags | ✅ |
| Colossal Wallet reward (Cave of Ordeals) | ⏳ Pending |
| Final rental shop pricing pass | ⏳ Pending |
| Full end-to-end playtest | ⏳ Pending |

### Meter Details

- **Base capacity:** 10,900 units
- **Recovery:** ~10 seconds from empty at base level; ~6.6s at upgrade 1; ~5.0s at upgrade 2
- **Sword cost:** 1/6 of base meter per swing (~1,817 units)
- **Sidestep/roll cost:** 1/5 to 1/3 of base meter depending on move
- **Hidden Skill cost:** 1/2 of base meter — requires a *full* meter to initiate

---

## Using the Prebuilt Executable (Windows)

> You will need a legal copy of *Twilight Princess* (Wii or GameCube) to provide the game's data files. The executable does not include any copyrighted game content.

1. **Download** `dusklight-albw-mod.zip` from the [Releases](../../releases) page.
2. **Extract** the zip to a folder of your choice (e.g. `C:\Games\Dusklight`).
3. **Set up your game files** by following the [Dusklight setup guide](https://github.com/TwilitRealm/dusklight/wiki) — place your disc image or extracted files where Dusklight expects them.
4. **Run** `dusklight.exe` inside the extracted folder.
5. The ALBW Meter and Rental Shop will be active automatically.

---

## Building from Source (for developers / tweakers)

These steps are for people who want to modify the mod or build it themselves.

### Prerequisites

- [Dusklight](https://github.com/TwilitRealm/dusklight) cloned locally (follow their build instructions first and confirm a vanilla build works)
- Git
- CMake 3.20+
- MSVC (Visual Studio 2022) or Clang on Windows

### Applying the Mod

1. **Clone this repository** next to your Dusklight folder:
   ```
   git clone https://github.com/WadeWinningWilson/ALBW-Dusklight.git
   ```

2. **Copy the mod files** over your Dusklight clone. From inside the `ALBW-Dusklight` folder, copy everything *except* `README.md` and `LICENSE` into your `dusklight` folder, overwriting when prompted:

   **Windows (File Explorer):** Select all files and folders, drag them into your `dusklight` folder, choose *Replace*.

   **Windows (Command Prompt):**
   ```cmd
   xcopy /E /Y "C:\path\to\ALBW-Dusklight\*" "C:\path\to\dusklight\"
   ```

3. **Build Dusklight** using the normal build process:
   ```
   cd dusklight
   cmake --preset windows-msvc
   cmake --build build/windows-msvc --config RelWithDebInfo
   ```

4. The compiled executable will appear in `build/windows-msvc/RelWithDebInfo/`.

### File Overview

All modified files mirror their exact path in the Dusklight repository. Key files:

| File | What it does |
|---|---|
| `src/d/d_meter2.cpp` | Core ALBW meter logic — drain, recovery, locking |
| `src/d/d_meter2_draw.cpp` | HUD rendering and fade-in/out animation |
| `include/d/d_meter2_info.h` | Public API declarations for the meter |
| `src/d/d_albw_rental.cpp` | Rental Shop system (new file) |
| `include/d/d_albw_rental.h` | Rental Shop header (new file) |
| `src/d/actor/d_a_alink*.cpp/.inc` | Player actor gates for drain on attacks/movement |
| `src/d/d_gameover.cpp` | Game-over warp logic and wolf-form gate |
| `src/d/actor/d_a_npc_post.cpp` | Postman route tied to story flag |

---

## Credits

- **Mod created by** [WadeWinningWilson](https://github.com/WadeWinningWilson)
- **Inspired by** [CaptainKittyCa2](https://github.com/CaptainKittyCa2) and their original ALBW meter work
- **Dusklight PC reimplementation by** [TwilitRealm](https://github.com/TwilitRealm)
- *A Link Between Worlds* meter concept © Nintendo — this mod is a fan creation and is not affiliated with or endorsed by Nintendo

---

## License

The Dusklight codebase is released under [CC0 1.0 Universal](LICENSE) (public domain). This mod's additions follow the same licence.

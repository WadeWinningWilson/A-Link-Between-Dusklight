# ALBW Port (PC)

Dusklight includes an experimental **A Link Between Worlds–style item rental** flow for the PC build. It reuses Twilight Princess native UI (J2D letter-select screens and message windows) instead of ImGui when enabled.

Part of **[ALBW-Dusklight](https://github.com/WadeWinningWilson/ALBW-Dusklight)** — clone that repo and follow its **[README](../README.md)** for build steps. Based on [Dusklight](https://github.com/TwilitRealm/dusklight).

## Build flag

Enable at configure time:

```sh
cmake --preset windows-msvc-relwithdebinfo -DTARGET_PC_NATIVE_UI=ON
cmake --build --preset windows-msvc-relwithdebinfo
```

When `TARGET_PC_NATIVE_UI` is **OFF** (default), the rental Postman still works on PC but uses **ImGui toasts** for greeting/farewell and an **ImGui shop table** for browsing items.

When **ON**, greeting, shop, and farewell use in-game BLO screens and fonts.

## How to find it in-game

1. Play as **human Link** (not wolf).
2. Progress until the **Ordon Village rental Postman** is active (`getBitSW() == 0x42` on the special Postman actor in F_SP103).
3. Talk to him to open **Postman's Lending Service**.

Items appear in the shop only if they were **stripped on death** (ALBW rental eligibility) and are **not currently owned**.

## Features

### Death / game over integration

- On death, eligible inventory items can be **stripped** (same slot mapping as the ALBW design).
- Save bits track which items were ever eligible for rental.
- **Magic Armor** and **Deity Armor** entries use extra eligibility rules (wallet / prior armor strip).

### Rental shop (native UI)

- **13 rentable items** in ascending price order (Slingshot through Deity Armor).
- **Six visible rows** with scroll; selection follows D-pad / stick (rental state machine).
- **Letter-select layout** (`zelda_letter_select_6menu.blo` + `select_base.blo`):
  - Left: item icon, name (`?????` when locked), **rupee price** on the right of each row.
  - Right: parchment description (stock message or item text + price when rentable).
- **Rent** with A when the row is purchasable and you have enough rupees; **Leave** with B.
- Row prices use `fenu_t6`–`fenu_t11`; footer work must not hide those panes (see maintainer notes below).

### Native dialogue (greeting / farewell)

- Uses `zelda_message_window_text.blo` + `zelda_message_window_new.blo` (shared message archive).
- **First visit:** greeting split across **three** message boxes to avoid overflow.
- **Returning customer:** single greeting page.
- **Farewell** text depends on whether you rented anything this session.
- Pagination and dismiss with A/B; vanilla “No response…” bar is suppressed **only while the rental talk event is open**.

### Postman actor (PC)

- Custom voice SFX arc and optional BGM during the interaction.
- `evtTalk()` intercept for the rental Postman; keeps the talk event alive during shop (locks Link like vanilla NPC talk).
- Wolf Link gets a dismissal toast and immediate event end (no shop).

### ImGui fallback (TARGET_PC_NATIVE_UI=OFF)

- `dALBWRental_imguiDraw()` — shop window in the main loop.
- `dusk::ui::push_toast()` — greeting and farewell.

## Key source files

| Area | Files |
|------|--------|
| Rental state, catalog, input | `src/d/d_albw_rental.cpp`, `include/d/d_albw_rental.h` |
| Native shop UI | `src/d/d_albw_shop.cpp`, `include/d/d_albw_shop.h` — see [albw-shop-icon-alignment.md](albw-shop-icon-alignment.md) |
| Death rupee orb | `src/d/d_albw_death_rupee.cpp` — see [albw-death-recovery-orb-brief.md](albw-death-recovery-orb-brief.md) |
| Native talk box | `src/d/d_albw_dialogue.cpp`, `include/d/d_albw_dialogue.h` |
| Shared text helpers | `src/d/d_albw_ui_text.cpp`, `include/d/d_albw_ui_text.h` |
| Postman hook | `src/d/actor/d_a_npc_post.cpp` |
| Vanilla talk suppress | `src/d/d_msg_object.cpp`, `src/d/d_msg_scrn_talk.cpp` |
| HUD while shop open | `src/d/d_meter2.cpp` |
| Death strip | `src/d/d_gameover.cpp` |

## Maintainer notes — shop footer (in progress)

Footer customization (analog stick icon, tagline, “A Rent / B Leave”) is **only** partially done. To avoid regressions:

- **Do not** hide `fenu_t6`–`fenu_t11` on `mpMenuScreen` (row prices).
- **Do not** `hidePaneTag` / subtree-hide `menu_f6`…`menu_t11` or `menu_10n` on 6menu without `suppressFifthRowFooterOverlap()` — the **5th on-screen row** (`fenu_t10`) shares layout with footer hints.
- **Do not** `hideFooterSlotTree(wi_btn_n)` (breaks wallet rupee HUD).
- Row **4** price may use `J2DPrint` overlay in `drawRowListText`; keep that path when touching draw order.

**Future goal:** scope vanilla talk suppression to the ALBW Postman only (local suppress), not a global flag in `d_msg_object`.

## Maintainer notes — vanilla dialogue suppress

- `dALBWRental_armVanillaTalkSuppress()` / `clearVanillaTalkSuppress()` guard the global message UI during Postman talk.
- **Must** clear suppress on `advanceToClosed()`, `STATE_CLOSED`, and Postman `Delete()` — otherwise **all NPC dialogue** stops rendering after one rental session.

## Debug

With the shop open, the game may write `albw_shop_debug.txt` under the project or user Documents path (catalog + viewport rows). This file is for local debugging and is not required for release builds.

## Local build script (optional)

Some developers use a repo-root `build_run.bat` to configure with `TARGET_PC_NATIVE_UI=ON` and run ninja; it is not part of the upstream build docs and may be kept locally only.

## Planned: shield combat

**Manual shielding** (Dawnlight `9ff9d35`) and **ALBW recovery pause while guarding** are implemented — see **[shield-combat.md](shield-combat.md)**. **Next:** Phase 4 parry + bash charge economy (spec in shield-combat § Phase 4). Shield durability (Phase 2) follows.

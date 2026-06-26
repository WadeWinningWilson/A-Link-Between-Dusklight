# ALBW Port (PC)

**ALBW-Dusklight** adds an _A Link Between Worlds_–style energy meter, death item strip, Postman rental economy, and optional combat/economy systems to the PC build of [Dusklight](https://github.com/TwilitRealm/dusklight). All mod code is gated on `#if TARGET_PC`.

Clone **[ALBW-Dusklight](https://github.com/WadeWinningWilson/A-Link-Between-Dusklight)** and follow the **[README](../README.md)** for build steps.

## Build flag

Enable native in-game UI at configure time:

```sh
cmake --preset windows-msvc-relwithdebinfo -DTARGET_PC_NATIVE_UI=ON
cmake --build --preset windows-msvc-relwithdebinfo
```

When `TARGET_PC_NATIVE_UI` is **OFF** (default), the rental Postman still works on PC but uses **ImGui toasts** for greeting/farewell and an **ImGui shop table** for browsing items.

When **ON**, greeting, shop, and farewell use in-game BLO screens and fonts.

---

## Settings

Pause menu → **Quality of Life** → **ALBW Settings** (single section):

| Setting | Default | What it does |
|---------|---------|--------------|
| **Enemy HP ×** (Normal / Mid-Boss / Boss / Final Boss) | 1× each | Scales effective enemy HP by dividing incoming attack power in `d_cc_uty.cpp` (1–16× per category). *Planned: true max-HP scaling — see [Next on the docket](#next-on-the-docket).* |
| **No Ammo Drops** | On | Bombs, arrows, and seeds no longer drop from enemies; magic pickups replace them. |
| **Manual Shielding** | Off | Hold **ZR** to guard without Z-target lock-on; **ZR+B** shield bash. Off = vanilla auto-guard on Z-target. |
| **Shield Parry & Bash Charges** | On | Perfect-guard timing earns bash charges and ALBW meter; failed blocks cost meter and charges. Off = traditional TP guard. |
| **Shield Durability** | Off | Shield HP by tier; failed blocks drain it. Hylian repairs on parry. Break at 0 triggers guard break. |
| **Death Recovery Orb** | Off | After Talo is rescued, dying halves your wallet and leaves a Tear of Light at the death spot to recover part of it. Item strip and meter refill on death are unaffected. |
| **Wolf Link Combat** | Off | ALBW wolf form: bite charges for Midna field attacks, twilight/non-twilight damage split, non-twilight stun, low-HP bite healing. Off = vanilla TP wolf combat. |
| **Enemy Death Rupees** | Off | Credit rupees directly to your wallet when enemies die and when boss fights end. Vanilla drop tables (hearts, jars, ground rupees) are unchanged. |
| **Extra Item Slot** | Off | **Off** / **Extra Only** (Midna on left D-pad, Z item) / **Extra + Quick Swap** (also Up = cycle sword, Right = cycle shield, Down = transform; map defaults to M / Tab). See `docs/d-pad-reworking.md`. |

Design details for shield systems: **[shield-combat.md](shield-combat.md)**.

Release history: **[patch-notes-v0.55.md](patch-notes-v0.55.md)**.

---

## Gameplay

### ALBW energy meter

- Replaces the lantern-oil HUD with an ALBW-style stamina bar for **human Link**.
- Drains on sword swings, agility actions, and hidden-skill use; passive recovery when not guarding.
- **Wolf Link is unaffected** — wolf combat uses its own optional overhaul (below), not the meter.
- While actively guarding, passive ALBW recovery (including idle boost) is paused; parry rewards refill via explicit meter grants.

### Death — item strip

- On death, eligible inventory items can be **stripped** (13 rentable slots; same mapping as the ALBW design).
- Save bits track which items were ever eligible for rental.
- **Magic Armor** and **Deity Armor** use extra eligibility rules (wallet / prior armor strip).
- Meter refills on death regardless of optional economy settings.

### Death — Recovery Orb (optional)

- Gated on **Death Recovery Orb** and progress after **Talo is rescued** (event bit F_0625).
- On real player death: wallet halved (round up), Tear of Light spawns at death location when you reload that room.
- Picking up the orb returns **50% of what was lost** (see **[albw-death-recovery-orb-brief.md](albw-death-recovery-orb-brief.md)**).

### Death — Oocoo warp (Postman service)

- After a **stripped dungeon death** where you respawn in Ordon, the Postman can sell **Cuckoo's Return** — a paid warp back to that dungeon entrance.
- Not an inventory item; appears as a special shop row when eligible.

### Postman rental shop

**How to find it:**

1. Play as **human Link** (not wolf).
2. Progress until the **Ordon Village rental Postman** is active (`getBitSW() == 0x42` on the special Postman actor in F_SP103).
3. Talk to him to open **Postman's Lending Service**.

Items appear in the shop only if they were **stripped on death** and are **not currently owned**.

**Native UI** (`TARGET_PC_NATIVE_UI=ON`):

- **13 rentable items** in ascending price order (Slingshot through Deity Armor).
- **Six visible rows** with scroll; selection follows D-pad / stick.
- **Letter-select layout** (`zelda_letter_select_6menu.blo` + `select_base.blo`): icon, name (`?????` when locked), rupee price per row; parchment description on the right.
- **Rent** with A when purchasable; **Leave** with B.
- **Native dialogue:** first visit uses three greeting pages; returning customers get one page; farewell depends on whether you rented this session.

**ImGui fallback** (`TARGET_PC_NATIVE_UI=OFF`):

- `dALBWRental_imguiDraw()` — shop window in the main loop.
- `dusk::ui::push_toast()` — greeting and farewell.

**Postman actor:** custom voice SFX, optional BGM, `evtTalk()` intercept keeps Link locked during shop. Wolf Link gets a dismissal toast and no shop.

Shop footer polish (analog-stick hint, tagline) is still WIP — see **[albw-shop-icon-alignment.md](albw-shop-icon-alignment.md)** and maintainer notes below.

### Shield combat (optional)

Three independent toggles under **ALBW Settings** (see table above):

- **Manual shielding** — Dawnlight-style guard chord (hold ZR); no auto-guard from Z-target alone.
- **Shield Parry & Bash Charges** — perfect-block window, charge bank by shield tier, bash spend rules, ALBW meter rewards/penalties.
- **Shield Durability** — separate shield-HP meter and HUD; mid-boss/boss hits scale durability loss via HP category.

ALBW recovery pause while guarding is always active on PC when the mod is built in.

### Wolf Link combat (optional)

When **Wolf Link Combat** is on:

- Bite charge system for Midna field attacks (with dedicated charge HUD).
- Twilight vs non-twilight damage split, non-twilight enemy stun, low-HP bite healing.
- Wolf form remains **outside** the ALBW energy meter economy.

### Enemy Death Rupees (optional)

When **Enemy Death Rupees** is on:

- **Additive only** — vanilla enemy drop tables (hearts, jars, ground rupees) are unchanged.
- Field kills credit the wallet via per-enemy lookup tables; boss/mid-boss **fight victories** grant once per profile name per session.
- A native **"+n"** popup beside the rupee counter shows each grant (`d_albw_rupee_popup.cpp`, drawn from `d_meter2_draw.cpp`).
- Many enemies hook death in their actor files in addition to the central `d_cc_uty.cpp` path (Deku Baba segments, Skulltulas, Beamos, etc.).

---

## Key source files

| Area | Files |
|------|--------|
| Rental state, catalog, input | `src/d/d_albw_rental.cpp`, `include/d/d_albw_rental.h` |
| Native shop UI | `src/d/d_albw_shop.cpp`, `include/d/d_albw_shop.h` |
| Native talk box | `src/d/d_albw_dialogue.cpp`, `include/d/d_albw_dialogue.h` |
| Shared UI text helpers | `src/d/d_albw_ui_text.cpp`, `include/d/d_albw_ui_text.h` |
| Oocoo warp service | `src/d/d_albw_oocoo.cpp`, `include/d/d_albw_oocoo.h` |
| Death Recovery Orb | `src/d/d_albw_death_rupee.cpp`, `include/d/d_albw_death_rupee.h` |
| Enemy HP multipliers | `src/d/d_albw_hp_mult.cpp`, `include/d/d_albw_hp_mult.h` |
| Shield parry / durability / bash | `src/d/d_albw_shield.cpp`, `include/d/d_albw_shield.h` |
| Wolf combat + stun | `src/d/d_albw_wolf_stun.cpp`, `include/d/d_albw_wolf_stun.h` |
| Wolf charge HUD | `src/d/d_albw_wolf_charge_hud.cpp`, `include/d/d_albw_wolf_charge_hud.h` |
| Enemy Death Rupees | `src/d/d_albw_enemy_rupee.cpp`, `include/d/d_albw_enemy_rupee.h` |
| Rupee grant HUD popup | `src/d/d_albw_rupee_popup.cpp`, `include/d/d_albw_rupee_popup.h` |
| ALBW meter HUD + recovery | `src/d/d_meter2.cpp`, `src/d/d_meter2_draw.cpp` |
| Collision / kill hooks | `src/d/d_cc_uty.cpp` |
| Death strip + orb hook | `src/d/d_gameover.cpp` |
| Orb room spawn | `src/d/d_s_room.cpp` |
| Postman hook | `src/d/actor/d_a_npc_post.cpp` |
| Link guard / manual shield | `src/d/actor/d_a_alink.cpp`, `src/d/actor/d_a_alink_guard.inc` |
| Vanilla talk suppress | `src/d/d_msg_object.cpp`, `src/d/d_msg_scrn_talk.cpp` |
| Settings UI | `src/dusk/ui/settings.cpp`, `src/dusk/settings.cpp` |
| Per-enemy kill hooks | `src/d/actor/d_a_e_*.cpp`, `src/d/actor/d_a_b_*.cpp`, `src/d/actor/d_a_obj_bemos.cpp`, … |

## Related docs

| Doc | Topic |
|-----|--------|
| [combat-refinements-handoff.md](combat-refinements-handoff.md) | Field combat backlog: enemy windup targeting, hidden skills × ALBW meter |
| [shield-combat.md](shield-combat.md) | Shield design, Dawnlight port notes, playtest checklist |
| [albw-death-recovery-orb-brief.md](albw-death-recovery-orb-brief.md) | Death orb state machine and spawn rules |
| [albw-shop-icon-alignment.md](albw-shop-icon-alignment.md) | Shop row icons, footer layout constraints |

---

## Next on the docket

Planned work after v0.55, in rough priority order:

### 1. Merge upstream Dusklight v1.3.1

Rebase / submodule sync with **[TwilitRealm/dusklight v1.3.1](https://github.com/TwilitRealm/dusklight/releases)** (2026-05-27 hotfix on 1.3.0): config save/load fixes, Arbiter's Grounds / Hyrule Castle chain geometry, text-input clear-with-Enter, and other engine fixes.

**Touch points:** git submodule / merge base, conflict pass on shared files (`m_Do_main`, actor infra, settings, build presets), full rebuild and smoke test (Postman shop, meter HUD, wolf combat, Darknut parry).

### 2. Postman heart & stamina upgrades — ✅ finalized (2026-06)

**Shipped** on the Postman **Upgrades & Services** page when **Master Quest** is on (`d_albw_master_quest.cpp`, `d_albw_rental.cpp`):

- Escalating-price **heart** purchases (half-heart / container progression).
- Escalating-price **ALBW stamina meter** capacity purchases.

**Remaining polish:** swap the **stamina upgrade row icon** in shop UI (heart icon is correct).

**Touch points:** `d_albw_master_quest.cpp`, `d_albw_rental.cpp` (`CAT_UPGRADES`), `d_meter2.cpp` (meter max), save event regs 100–102.

### 3. Actual enemy HP multiplier (deferred)

Replace (or offer alongside) today's **attack-power division** intercept in `dAlbwHP_applyMult()` with **real max-HP scaling** on enemies at spawn or init — so 16× means 16× HP without integer-division plateaus or the `max(1, …)` floor distorting time-to-kill.

**Why:** Current divide-by-mult approach saturates early on normal sword hits (see maintainer discussion: damage hits 1 per hit once multiplier ≥ ⌊attackPower/2⌋ + 1). True HP mult keeps high slider values meaningful across all attack strengths. `dAlbwHP_tryApplyTrueMaxHp()` exists but the divide path in `d_cc_uty.cpp` is still active — finish migration after Dusklight merge.

**Touch points:** `d_albw_hp_mult.cpp`, enemy init paths (`health` / `mMaxHp` fields per actor), `d_cc_uty.cpp` (remove or gate divide path), wolf charge scaling in `d_cc_uty.cpp` (today uses `dAlbwHP_getRawMult()` to undo the divide).

### 4. Lockout boomerang — visual “ranged open” feedback (future)

Gameplay debuff (`dAlbwLockout_onBoomerangHit`, 4s window) is in; **no dedicated VFX/HUD yet**. Later: enemy shimmer / status icon / lock-on tint while `dAlbwLockout_isRangedOpened()` so players know bow / bomb / iron ball will connect before the timer expires.

### 5. Zant & Ganon boss changes

ALBW-specific fight tuning for the **Zant** and **Ganon / Ganondorf** finale — phase scripts, damage expectations, and durability rules that still treat Zant as excluded from wolf combat overrides (`fpcNm_B_ZANT_e` in `d_cc_uty.cpp`).

**Scope TBD:** document desired phase behavior, HP category overrides, and any bespoke parry/durability rules before implementation. Final-boss category already exists in `d_albw_hp_mult.cpp` (`sFinalBoss` list).

---

## Maintainer notes — shop footer

Footer customization (analog stick icon, tagline, “A Rent / B Leave”) is partially done. To avoid regressions:

- **Do not** hide `fenu_t6`–`fenu_t11` on `mpMenuScreen` (row prices).
- **Do not** `hidePaneTag` / subtree-hide `menu_f6`…`menu_t11` or `menu_10n` on 6menu without `suppressFifthRowFooterOverlap()` — the **5th on-screen row** (`fenu_t10`) shares layout with footer hints.
- **Do not** `hideFooterSlotTree(wi_btn_n)` (breaks wallet rupee HUD).
- Row **4** price may use `J2DPrint` overlay in `drawRowListText`; keep that path when touching draw order.

**Future goal:** scope vanilla talk suppression to the ALBW Postman only (local suppress), not a global flag in `d_msg_object`.

## Maintainer notes — vanilla dialogue suppress

- `dALBWRental_armVanillaTalkSuppress()` / `clearVanillaTalkSuppress()` guard the global message UI during Postman talk.
- **Must** clear suppress on `advanceToClosed()`, `STATE_CLOSED`, and Postman `Delete()` — otherwise **all NPC dialogue** stops rendering after one rental session.

# Ordon shield tier — research instructions (for Claude / playtest agent)

**Status (2026-05-31):** Fixes applied: `item_func_SHIELD()` now sets `COLLECT_ORDON_SHIELD` + equip (was empty stub); `setShieldArcName()` uses Ordon item bit for **SWShd**; spur tier prefers **`mShieldArcName`** (shield on Link's back).

**Use when:** Player has the **Ordon Shield** visually equipped but sees **4** bash spurs (Wooden tier) instead of **2**, or bash economy does not match Ordon rules (2 max, bash at 2/2).

**Repo:** ALBW-Dusklight, `TARGET_PC` only.  
**Module:** `src/d/d_albw_shield.cpp`, hooks in `d_a_alink_guard.inc`, `d_a_alink_damage.inc`.

---

## Expected behavior (authoritative)

| Player equip (`dComIfGs_getSelectEquipShield()`) | Item enum | Max spurs | Bash opens at |
|--------------------------------------------------|-----------|-----------|----------------|
| Ordon Shield (Rusl / first shield) | `dItemNo_SHIELD_e` (0x2B) | **2** | 2 charges |
| Wooden Shield (Goron carving) | `dItemNo_WOOD_SHIELD_e` (0x2A) | **4** | 4 charges |
| Hylian Shield | `dItemNo_HYLIA_SHIELD_e` (0x2C) | **6** | 4+ charges |

**Tier source order** in `shieldTierFromLink()` (`d_albw_shield.cpp`):

1. **Equipped item ID** (Items → gear slot) — primary  
2. `mShieldArcName` (`SWShd` / `CWShd` / `HyShd`) — fallback  
3. `checkCarvingWoodShieldEquip()` / `checkShopWoodShieldEquip()` — last resort  

**Do not** use asset-browser English names on `.bmd` rows alone; they are often inverted vs item IDs.

---

## Naming traps (common false leads)

| Label location | Says | Actually maps to |
|----------------|------|------------------|
| `daAlink_c::setShieldArcName()` | `checkShopWoodShieldEquip()` → **SWShd** | **Ordon** item (`dItemNo_SHIELD_e`) |
| Same | `checkCarvingWoodShieldEquip()` → **CWShd** | **Wooden** item (`dItemNo_WOOD_SHIELD_e`) |
| Asset table (user report) | SWShd = “Wooden Shield” model | Game still uses SWShd for **Ordon item** |
| Asset table | CWShd = “Ordon Shield” model | Game still uses CWShd for **Wooden item** |
| `src/dusk/ui/editor.cpp` ~271 | `dItemNo_SHIELD_e` labeled `"Wooden Shield"` | **Wrong UI string only** — ID is still Ordon |
| `src/dusk/imgui/ImGuiSaveEditor.cpp` | `dItemNo_SHIELD_e` = `"Ordon Shield"` | Correct for save editor |

**Conclusion:** Four spurs almost always means `getSelectEquipShield() == dItemNo_WOOD_SHIELD_e` (0x2A), not a wrong arc constant in `d_albw_shield.cpp`.

---

## Playtest checklist (human, ~5 min)

1. Settings: **Manual Shielding** ON, **Shield Parry & Bash Charges** ON.  
2. Pause → **Items** → equip **Ordon** on the shield gear slot (not Wooden / Hylian).  
3. Enter combat (Z-target enemy, or block once).  
4. HUD: **2** spur icons above rupees (not 4).  
5. Console / log: `[dShield]` lines should show `equip=43` (0x2B) and `tier=1` (Ordon enum).  
6. Drain ALBW below full → shield bash **hit** enemy → meter rises ~2% (`dMeter2_addALBWFraction(1, 50)` on `ChkAtHit` in `procGuardAttack`).  
7. Repeat with **Wooden** equipped → `equip=42`, `tier=2`, **4** icons.

Report back: screenshot + one `[dShield]` log line + equipped item name from pause menu.

---

## Log interpretation

`logChargeEvent()` format:

```text
[dShield] <event>: arc=<SWShd|CWShd|HyShd> equip=<u16> tier=<u8> charges=<n>/<max> thresh=<t> chain=<0|1>
```

| `tier` | Meaning |
|--------|---------|
| 0 | None / unknown |
| 1 | Ordon |
| 2 | Wooden |
| 3 | Hylian |

| `equip` (hex) | Item |
|---------------|------|
| 0x2A (42) | Wooden |
| 0x2B (43) | Ordon |
| 0x2C (44) | Hylian |

**Failure signature:** Ordon on back + `equip=42` or `tier=2` → equip slot desync; fix equip flow, not spur art.

---

## Hypothesis tree (if Ordon still wrong after equip-first fix)

### H1 — Wrong item equipped (most likely)

- **Check:** `dComIfGs_getSelectEquipShield()` while reproducing.  
- **Files:** `d_meter2_info.cpp` `dMeter2Info_setShield()`, `d_menu_collect.cpp` shield select cases, `d_s_menu.cpp` debug equip.  
- **Fix:** None in `d_albw_shield` if value is 0x2A; user must equip Ordon, or fix menu writing wrong ID.

### H2 — Equip slot empty / invalid → arc fallback gives Wooden

- **Check:** `equip=255` or `0` in log; `mShieldArcName` when `!checkShieldGet()`.  
- **Code:** `setShieldArcName()` forces **CWShd** when `!checkShieldGet()` (`d_a_alink_swindow.inc` ~28–30).  
- **Fix:** If equip invalid but model visible, call `setShieldArcName()` after equip set; or map CWShd only when `WOOD_SHIELD` item owned.

### H3 — Equip write path passes wrong ID (confirmed for Dusk editor only)

- **Check:** Trace shield pick to `dMeter2Info_setShield(i_itemId)` (`d_meter2_info.cpp` ~1708).  
- **Collect menu:** `d_menu_collect.cpp` `changeShield()` ~1247–1264 — **correct** (`SHIELD_e` / `WOOD_SHIELD_e` / `HYLIA_SHIELD_e`).  
- **Dusk editor:** `src/dusk/ui/editor.cpp` item table had **swapped display names** (fixed) — picking "Ordon Shield" in editor used to grant `dItemNo_WOOD_SHIELD_e` (0x2A) → 4 spurs.

### H4 — Icon count not tied to tier (unlikely)

- **Check:** `dShield_drawBashCharges()` loop `for (i < maxCharges)` and `dShield_getMaxBashCharges()`.  
- **grep:** hardcoded `4` in shield draw path.

### H5 — Stale session charges (cosmetic only)

- `sBashCharges` can exceed new max until `clampChargesToTier()`; icon **count** uses `maxCharges`, not stored charges. Four **slots** = wrong tier, not stale charge value.

---

## Code anchors (read in this order)

| Step | File | What to verify |
|------|------|----------------|
| 1 | `src/d/d_albw_shield.cpp` | `shieldTierFromEquipSlot()`, `shieldTierFromLink()`, `tierConfig()` |
| 2 | `include/d/actor/d_a_player.h` ~1172–1177 | `checkCarvingWoodShieldEquip` / `checkShopWoodShieldEquip` |
| 3 | `src/d/actor/d_a_alink_swindow.inc` ~28–35 | `setShieldArcName()` arc assignment |
| 4 | `include/d/d_item_data.h` ~137–139 | `dItemNo_WOOD_SHIELD_e`, `dItemNo_SHIELD_e`, `dItemNo_HYLIA_SHIELD_e` |
| 5 | `src/d/d_meter2_info.cpp` ~1708 | `dMeter2Info_setShield` |
| 6 | `src/d/actor/d_a_alink_guard.inc` ~385–388, ~460–463 | bash gate + ALBW refund on hit |

---

## Minimal code experiments (if H1–H3 confirmed)

1. **Debug overlay (temporary):** draw `equip=%u tier=%u` near spur row using `getSelectEquipShield()` + `shieldTierFromLink(link)` — remove before merge.  
2. **On equip change:** in `dShield_updateGuardTracking`, detect `equip != sLastEquip` and `clampChargesToTier` + log `equip-change`.  
3. **Assert in dev:** if `checkShopWoodShieldEquip()` and `maxCharges != 2`, `OS_REPORT` warning with arc + equip.

**Do not** swap `SHIELD_ARC_ORDON` / `SHIELD_ARC_WOODEN` constants to match asset-browser labels without also changing `setShieldArcName()` — that would break Wooden players.

---

## Related docs

- `docs/shield-combat.md` — Phase 4 economy, bash refund, HUD gate  
- `docs/shield-hud-research-brief.md` — spur layout / pikari (R-H1–R-H8)  

---

## Out of scope

- Parry window tuning  
- Persisting bash charges to save  
- Re-exporting shield `.bmd` / texture names (`ttdekushield`, `ni_woodenshield`, etc.)

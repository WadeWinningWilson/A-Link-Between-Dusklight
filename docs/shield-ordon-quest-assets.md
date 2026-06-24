# Ordon Village shield quest — asset & item ID research

**Quest:** Wolf Link bashes the wall in Jaggle's house; shield falls; Midna sequence; Link obtains the first shield.  
**Stage object:** `wshield` → actor `Obj_Shield` (`d_a_obj_shield.cpp`).

---

## Authoritative code path (wall drop → pickup)

| Step | File | What happens |
|------|------|----------------|
| 1 | `d_stage.cpp` | `OBJNAME("wshield", fpcNm_Obj_Shield_e)` places the actor |
| 2 | `d_a_obj_shield.cpp:111` | **`m_itemNo = dItemNo_WOOD_SHIELD_e` (0x2A)** — hardcoded |
| 3 | `d_a_obj_shield.cpp:122` | Loads field model `dItem_data::getFieldArc(0x2A)` |
| 4 | `d_a_obj_shield.cpp:185-198` | Wolf `checkWolfAttackReverse()` / roll crash → shake → `Z2SE_OBJ_WOODSHIELD_*` SFX |
| 5 | `d_a_obj_shield.cpp:244` | Pickup spawns item **`dItemNo_WOOD_SHIELD_e`** via `fopAcM_createItemForTrBoxDemo` |
| 6 | `d_item.cpp:768` | `item_func_WOOD_SHIELD()` → `COLLECT_WOODEN_SHIELD` + **`setSelectEquipShield(WOOD_SHIELD)`** |
| 7 | `d_a_alink_swindow.inc` | `checkCarvingWoodShieldEquip()` true → **`mShieldArcName = "CWShd"`** (PC: always from **`getSelectEquipShield()`** first — required for save-editor multi-shield quick swap; see `docs/d-pad-reworking.md` § Implementation progress) |
| 8 | `include/d/d_save_bit_labels.inc` | Event **`M_072`** = "Get **wooden** shield" (Ordon Village) |

**Player-facing name:** Ordon Shield (UI, story, Dusk start menu).  
**Engine name:** `WOOD_SHIELD` / 木の盾 — same item.

---

## Item IDs vs Link model arcs

| Item enum | Hex | Collect flag | `setShieldArcName()` when equipped | Typical English UI |
|-----------|-----|--------------|-----------------------------------|-------------------|
| `dItemNo_WOOD_SHIELD_e` | **0x2A** | `COLLECT_WOODEN_SHIELD` | **`CWShd`** (`al_shb`) | Ordon Shield (quest / first shield) |
| `dItemNo_SHIELD_e` | **0x2B** | `COLLECT_ORDON_SHIELD` | **`SWShd`** (`al_shc`) | Shop / replacement shield slot |
| `dItemNo_HYLIA_SHIELD_e` | **0x2C** | `COLLECT_HYLIAN_SHIELD` | **`HyShd`** | Hylian Shield |

Helper names in `d_a_player.h` are **misleading**:

- `checkShopWoodShieldEquip()` → tests **`dItemNo_SHIELD_e` (0x2B)**, not WOOD.
- `checkCarvingWoodShieldEquip()` → tests **`dItemNo_WOOD_SHIELD_e` (0x2A)** — this is the **Ordon quest shield**.

Asset browser labels (user table) often call **`CWShd` = "Ordon Shield"** and **`SWShd` = "Wooden Shield"** — that matches **what Link wears** after the quest, not the `checkShop*` function names.

---

## Field / object archives (pickup & world model)

From `d_item_data.cpp` `field_item_res[]` / `item_resource[]` (indexed by `dItemNo`):

| Item | Field arc (world drop) | Notes |
|------|------------------------|--------|
| `0x2A` WOOD_SHIELD | **`T_g_SHB`** (field), **`O_g_SWA`** nearby in table | Obj_Shield loads field arc for 0x2A |
| `0x2B` SHIELD | **`T_g_SHB`** at adjacent index | Separate inventory item |

Dungeon / object arcs seen in item table: **`O_gD_SWA`**, **`T_gD_SHB`**, **`O_gD_SWB`** (shield A/B/C family).

Link body arcs (from `d_a_alink.cpp`):

- `l_cWShdArcName[] = "CWShd"` — used when **`WOOD_SHIELD` equipped**
- `l_sWShdArcName[] = "SWShd"` — used when **`SHIELD` equipped**

Texture names from user notes (Ordon / demo vs final):

- Ordon family: `ttdekushield`, `ttdelunotate_s3_tc` ↔ **SWShd path in some builds; quest equip uses CWShd in this decomp**
- Wooden family: `ni_woodenshield`, `ni_kinotate_48` ↔ often **SWShd** slot in file labels

**Do not rename `.bmd` / archive strings on disc** — only fix tier tables in `d_albw_shield.cpp` to match **item ID + arc** above.

---

## Bash spur tier (mod)

| Shield | Item / arc | Max spurs |
|--------|------------|-----------|
| **Ordon (quest shield)** | `WOOD_SHIELD` + **`CWShd`** | **2** |
| **Shop / replacement** | `SHIELD` + **`SWShd`** | **4** (Goron-style upgrade path in mod) |
| **Hylian** | `HYLIA` + **`HyShd`** | **6** |

Earlier mod logic treated `checkShopWoodShieldEquip` as "Ordon" and `WOOD_SHIELD` as "Wooden 4" — **inverted** relative to the quest.

---

## Event / area flags (debug)

From `ImGuiEventFlags.hpp` / Ordon house:

- `0x0D01` — ordon shield fell down cutscene  
- `0x0804` — got ordon shield  
- `0x0E20` — started midna jump to ordon shield inside house  

---

## How to verify in play

After rebuild, `[dShield]` log with Ordon quest shield equipped should show:

```text
equip=42(0x2a) arc=CWShd tier=1 max=2
```

If you see `equip=43 arc=SWShd max=4`, you have the **other** shield item selected (0x2B path).

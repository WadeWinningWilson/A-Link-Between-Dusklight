# ALBW Death Recovery Orb — handoff brief

**For:** orb + death rupee penalty work (shop UI is separate — see [albw-shop-icon-alignment.md](albw-shop-icon-alignment.md))  
**Repo:** Dusklight / ALBW-Dusklight (`TARGET_PC`)  
**Build:** `cmake --build --preset windows-msvc-relwithdebinfo`

---

## Locked design (user requirements)

| Requirement | Detail |
|-------------|--------|
| **Gate** | Same as ALBW item strip: only after Talo rescued (F_0625 / event bit 625). Before that, no rupee loss and no orb. |
| **When loss applies** | On real player death (game-over type 0), regardless of warp choice (dungeon entrance vs Ordon). |
| **Where** | **Dungeons and overworld** — any stage after F_0625 (e.g. `F_SP108`, `D_MN01`). No dungeon-only gate. |
| **Loss formula** | Halve wallet, round up (player-friendly): `lost = (wallet + 1) / 2`, `kept = wallet - lost`. |
| **Orb location** | Spawn at Link's XYZ at death, in the death stage + room. |
| **Orb pickup reward** | Return 50% of what was lost (integer halve): `recovery = lost / 2`. Example: 100 rupees → lose 50 → orb gives 25 back. |
| **Visual** | Reuse Tear of Light look (glowing floating tear), **not** a rupee field item (`daItem` green/blue rupee). |
| **Gameplay feel** | ALBW-style risk: die → lose half wallet → recover a quarter of original wallet if you return to death spot. |
| **UX** | Rupees lost immediately on death. Orb appears when player **loads the death room again** (e.g. re-enter dungeon after Ordon respawn). **Not** delivered to respawn point. |

---

## Implemented code (`#if TARGET_PC`)

| File | Role |
|------|------|
| `include/d/d_albw_death_rupee.h` | Public API |
| `src/d/d_albw_death_rupee.cpp` | State machine + spawn + grant logic |
| `src/d/d_gameover.cpp` (~205–208) | Death hook after F_0625 |
| `src/d/d_s_room.cpp` (~253) | Spawn on room load |
| `src/d/actor/d_a_obj_drop.cpp` | `daObjDrop_c` integration (Tear of Light actor) |
| `src/d/actor/d_a_obj_item.cpp` (~900) | Legacy `dALBWDeathRupees_onOrbItemGet` (unused spawn path) |

### Static state (RAM only — not saved)

- `sLastLostRupees` — amount removed on last death  
- `sOrbRecovery` — orb grant amount (`lost / 2`)  
- `sOrbPending` / `sOrbSpawned` — spawn queue  
- `sOrbStage[32]`, `sOrbRoom`, `sOrbPos` — death location  
- `sOrbActorId` — tracks spawned actor  
- `pushTearRenderFlags()` / `popTearRenderFlags()` — sets all 3 `LightDropGetFlags` so tear particles render outside Twilight  

### Death flow (`dALBWDeathRupees_applyHalvingOnDeath`)

1. Clears prior orb state (and deletes prior orb actor).  
2. If wallet > 0: halve (round up), `setRupee(kept)`.  
3. `sOrbRecovery = lost / 2` (skip orb if 0).  
4. Saves Link `current.pos`, room, stage name (`getStartStageName()` fallback `getLastPlayStageName()`).  
5. Sets `sOrbPending = true`.

### Spawn flow (`dALBWDeathRupees_trySpawnOrbInRoom` + `dALBWDeathRupees_tickSpawn`)

Runs when the death stage/room is active:

- **`d_s_room.cpp`** — on room load (re-entering dungeon death room after Ordon respawn)
- **`d_s_play.cpp`** — every gameplay frame while orb pending (same-room “Continue Here” / dungeon entrance respawn without a full room reload)

Spawn when:

- Orb pending, not spawned, `sOrbRecovery > 0`  
- `stageName == sOrbStage` and `roomNo == sOrbRoom`  

Creates `fpcNm_Obj_Drop_e` with params `0x0000FF00` (`ym_swbit = 0xFF` → floating tear, no insect parent). Scale 1.35, tear render flags, +100 Y lift on create.

### Pickup flow (`daObjDrop_c` patches)

- **create:** bypass tbox gate during orb spawn; `createBodyEffect()`  
- **modeInit:** appear → float sequence + appear particles + sound  
- **checkGetArea:** collectible within 250 units without Twilight flags  
- **dropGet / checkCompleteDemo:** `dALBWDeathRupees_onOrbDropGet` (not vanilla heart/tear story)  
- **execute:** re-creates body effects if emitters died (`mModeAction < 3`)

---

## Status

| Area | Status |
|------|--------|
| Rupee halving on death | Works |
| Rupee grant on pickup | Works |
| **Orb visibility** | **Main open problem** — orb not visible / not “real tear” enough |
| Save persistence | Not implemented — static RAM; save/load likely loses pending orb |
| Old orb actor cleanup | Implemented in `clearOrbState()` via `destroySpawnedOrbActor()` |
| `daItem` path | Unused; user rejected rupee actors for visuals |

### Why visibility is hard (`daObjDrop_c`)

- No 3D mesh — tear is **particle-only** (`createBodyEffect` → JPA IDs `0x838B`–`0x842B`, appear `0x8388`/`0x8389`).  
- Vanilla tears assume Twilight context; `pushTearRenderFlags()` is a hack for overworld.  
- If particles fail to bind or emitters die early, actor is invisible but pickup may still work in range.  
- Spawn is at death site — player must **return** to that room; orb is not at Ordon respawn.

---

## Suggested implementation order

1. **Repro checklist** — Die with rupees after F_0625 → note wallet before/after → return to exact death room → check console/log for spawn.  
2. **Path A debug pass** — Visibility + effect emitters + position (current path).  
3. **Pickup polish** — `Z2SE_SY_LIGHT_DROP_GET` on grant; optional float text.  
4. **Path E** — Save persistence if user wants orb across reloads.  
5. **Path B** — Custom `daALBWRecoveryOrb_c` only if Path A cannot get credible tear in overworld.

### Path options

| Path | Summary |
|------|---------|
| **A** (recommended first) | Stay on `daObjDrop_c`; debug emitters, render flags, appear sequence |
| **B** | New actor with explicit `_draw` (billboard / glow BMD) |
| **C** | Particle-only pickup zone, no actor |
| **D** | Different vanilla actor with visible model (user rejected rupee items) |
| **E** | Save persistence for pending orb |
| **F** | Spawn timing alternatives if stage/room matching fails |

---

## Key code references

**Apply on death** — `src/d/d_gameover.cpp`:

```cpp
if (dComIfGs_isEventBit(dSv_event_flag_c::saveBitLabels[625])) {
    dALBWDeathRupees_applyHalvingOnDeath();
    dMeter2_stripAllALBWInventoryOnDeath();
    dMeter2_fillALBWMeter();
}
```

**Spawn on room load** — `src/d/d_s_room.cpp`:

```cpp
dALBWDeathRupees_trySpawnOrbInRoom(dComIfGp_getStartStageName(), roomNo);
```

**Math + spawn** — `src/d/d_albw_death_rupee.cpp` (`applyHalvingOnDeath`, `trySpawnOrbInRoom`).

**Drop actor** — `src/d/actor/d_a_obj_drop.cpp` (create gate, modeInit, checkGetArea, dropGet).

---

## Test notes

- Gate: rescue Talo first (F_0625), die with rupees, **return to death location** (not just Ordon respawn).  
- Do **not** use `daItem` rupee actors for visuals unless user changes mind.

## Debug log

After a death/orb test, check **`%USERPROFILE%\Documents\dusklight\albw_orb_debug.txt`** (fallback: repo-root `albw_orb_debug.txt`). Append-only trace:

- `death:` — wallet halving, saved stage/room/pos (`lastStage` vs `startStage` at gameover)
- `spawn:` — room match, create success/failure, respawn after room unload
- `created:` — actor id + lifted Y
- `grant:` — rupee pickup confirmed

If `spawn:` never appears after returning to the death room, stage/room name mismatch. If `spawn: created` but no `grant:`, walk to exact death XYZ (within 250 units).

---

## Out of scope

- Rental shop UI  
- Oocoo's Return shop row (`d_albw_oocoo.*`)  
- ALBW meter dungeon-clear upgrades  
- Shield / HUD work  

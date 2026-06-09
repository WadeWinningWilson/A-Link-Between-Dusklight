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
| `src/d/d_particle.cpp` + `include/d/d_particle.h` | Supplemental tear archive + fallback routing (see add-on) |
| `src/m_Do/m_Do_machine.cpp` | Archive heap +0xA0000 for the tear heap (see add-on) |

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

## Remaining / future work

Path A (stay on `daObjDrop_c`, fix the particle pipeline) shipped — see the add-on
section below. Alternatives B/C/D (custom actor, particle zone, rupee items) are
obsolete. Left for the future:

1. **Save persistence (old Path E)** — pending orb is RAM-only; save/quit while an
   orb is pending loses it. Related: `pushTearRenderFlags()` forces all three
   `LightDropGetFlag` save flags while pending (restored on pickup/next death) —
   a save during that window could persist them.
2. **Tears-quest map dot** — orb `execute()` calls `dTres_c::setLightDropPostion`
   like any tear; an orb pending during an active Tears of Light quest might draw
   a phantom dot on the Twilight map HUD. Cosmetic; not observed in testing.
3. **Pickup polish** — optional float text on grant.

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

After a death/orb test, check **`%USERPROFILE%\Documents\dusklight\albw_orb_debug.txt`** (fallback: repo-root `albw_orb_debug.txt`). Truncated once per game session, then appends — multi-death tests keep every run:

- `death:` — wallet halving, saved stage/room/pos (`lastStage` vs `startStage` at gameover)
- `spawn:` — room match, create success/failure, respawn after room unload
- `created:` — actor id, final pos, `groundY`, `bodyFx=N/6` (tear emitters bound), `sceneJpc=PsceneNNN` (stage's particle archive), `hasTearRes` (archive contains tear FX), `suppRes` (supplemental archive ready)
- `grant:` — rupee pickup confirmed

Triage: no `spawn:` after returning to the death room → stage/room mismatch. `created:` but no `grant:` → walk to exact death XYZ (within 250 units). `bodyFx=0/6` with `suppRes=0` → supplemental archive failed to load (check `OS_REPORT` console for `ALBW tear res:` lines).

---

## Out of scope

- Rental shop UI  
- Oocoo's Return shop row (`d_albw_oocoo.*`)  
- ALBW meter dungeon-clear upgrades  
- Shield / HUD work  

---

# Add-on: dungeon rendering fix + crash fixes (June 2026)

Follow-up to the original orb work. The "orb visibility" open problem is solved:
the **real Tear of Light now renders in dungeons and overworld**, for human and
Wolf Link, in and out of Twilight. All changes `#if TARGET_PC`.

## Root cause of dungeon invisibility

The tear has no draw method — visuals are six JPA body emitters
(`0x838B`–`0x838F`, `0x842B`) plus appear FX (`0x8388`/`0x8389`) and draw-in
lines (`0x838A`). Every one of those IDs has the `0x8000` bit set, which routes
them to the **scene particle resource manager** (`dPa_control_c::getRM_ID`):

- Scene particles load from a **per-stage archive** `/res/Particle/Pscene###.jpc`
  (number from the stage's STAG info; loaded by `dPa_control_c::readScene`).
- Faron Woods (`F_SP108`) loads **Pscene011**, which contains the full tear FX
  set (vanilla Twilight tears live there) → orb rendered fine.
- Forest Temple (`D_MN05`) loads **Pscene100**, which has none of them →
  `dComIfGp_particle_set` returned NULL for all six emitters → `bodyFx=0/6` →
  invisible orb with working sound/pickup.

This was never a positioning, render-flag, or spawn-timing problem. Earlier
experiments along those lines were reverted to upstream behavior.

## The fix: resident supplemental tear archive

| File | Change |
|------|--------|
| `src/d/d_particle.cpp` | `ensureTearSceneRes()`, supplemental resource manager, fallback routing in `set()`, dedicated heap, emitter-manager slot count 2 → 3 |
| `include/d/d_particle.h` | `getScenePrtclNo()`, `hasSceneParticleRes(u16)`, `ensureTearSceneRes()` |
| `src/d/d_albw_death_rupee.cpp` | Kick + poll `ensureTearSceneRes()`; diagnostics in `created:` log |
| `src/m_Do/m_Do_machine.cpp` | Archive heap +0xA0000 to host the tear heap |

Mechanism:

1. **On death** (`applyHalvingOnDeath`), `ensureTearSceneRes()` queues an async
   DVD-thread read of `/res/Particle/Pscene011.jpc`. `tickSpawn` polls it each
   frame while an orb is pending.
2. When the read lands, a `JPAResourceManager` is built over the data and
   registered in **emitter-manager slot 2** (`JPAEmitterManager` is now created
   with `ridMax = 3` on PC; vanilla uses 2: common + scene). Loaded once,
   resident for the whole session.
3. **Fallback routing** in `dPa_control_c::set` (`dPa_tearResFallbackRM`): a
   scene-particle request (`rmID == 1`) whose ID is missing from the current
   stage's archive but present in the supplemental archive is served from
   slot 2. Stages that natively contain the tear FX (Faron, Twilight areas) are
   untouched — native archive wins.
4. If the orb spawns before the read completes, the existing per-frame
   `createBodyEffect()` retry in `daObjDrop_c::execute` picks the FX up a few
   frames later.

## Crash fixes along the way

Both crashes were heap exhaustion, not logic bugs:

1. **Crash on death in dungeon** — first version loaded Pscene011 into
   `m_resHeap` (vanilla budget `0x96000` ≈ 614 KB, sized for `common.jpc` +
   exactly one scene archive). The extra archive overflowed it the moment the
   read landed (right after death).
2. **Crash leaving dungeon → Faron** — interim fix (+512 KB on `m_resHeap`)
   survived the dungeon but not the transition: during `D_MN05 → F_SP108` the
   heap briefly held common + supplemental Pscene011 + Faron's own Pscene011
   (one of the largest archives) → overflow again.

Final layout: the supplemental archive lives on its **own dedicated
`JKRExpHeap` (`sTearHeap`, 0xA0000 ≈ 640 KB)**, created at boot in the
`dPa_control_c` constructor (avoids mid-game fragmentation), child of the
archive heap (which grew by the same 0xA0000 in `m_Do_machine.cpp`).
`m_resHeap` is back to its exact vanilla budget, so stage transitions have the
same memory profile as stock Dusklight.

## Other changes in this pass

- **Positioning reverted to upstream** (`finishRecoveryDropSetup`): `dBgS_GndChk`
  ground snap, fallback `deathY + offset`, min Y 80. Session experiments
  (per-frame position pin, exact-XZ lock, pickup grace period, unused
  `sOrbFloorY`) were removed.
- **Float height raised**: `kOrbFloatAboveGround` 90 → **135** (slightly above
  Link's head; pickup radius is 250 so it stays trivially collectible).
- **Debug log**: truncates once per session instead of per death; `created:`
  line gained `bodyFx`, `sceneJpc`, `hasTearRes`, `suppRes` fields.

## Verified by testing

| Scenario | Result |
|----------|--------|
| Human death, Faron Woods (`F_SP108`, Pscene011) | Tear visible, pickup + grant OK |
| Human death, Forest Temple (`D_MN05`, Pscene100) | Tear visible via supplemental (`bodyFx=6/6 hasTearRes=0 suppRes=1`) |
| Collect in dungeon → exit to Faron | No crash (post heap fix) |
| Wolf death, non-Twilight | Works |
| Wolf death, Twilight overworld | Works |
| Wolf death during Tears of Light quest | Works; vessel count unaffected (custom grant early-returns before light-drop counters in `dropGet` / `checkCompleteDemo`) |

## Wolf Link notes

- Death hook is form-agnostic (game-over type 0 + F_0625 only). The wolf gate in
  `d_gameover.cpp` only hides the **Ordon warp choice** pre-Master-Sword; the
  orb is queued regardless.
- Pickup is form-agnostic (250-unit distance check). `onWolfLightDropGet()` (the
  wolf tear-collect glow shader) is intentionally skipped for the recovery orb.
- In Twilight, stage archives contain the tear FX natively — the supplemental
  archive is only consulted where they don't (dungeons, most light-world stages).

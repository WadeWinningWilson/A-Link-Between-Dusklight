#ifndef D_ALBW_DEATH_RUPEE_H
#define D_ALBW_DEATH_RUPEE_H

// ============================================
// NEW CODE — ALBW Port
// Death rupee penalty (half wallet, round up) and recovery orb at death location.
// Gated on F_0625 (Talo rescued), same as ALBW item strip.
// ============================================
#if TARGET_PC

class fopAc_ac_c;

// Apply (wallet + 1) / 2 rupee loss; records amount lost and queues orb spawn.
void dALBWDeathRupees_applyHalvingOnDeath();

// Rupees removed on the last death (0 if halving did not run).
u16 dALBWDeathRupees_getLastLostAmount();

// Orb pickup returns half of what was lost on that death (e.g. lost 50 → orb 25).
u16 dALBWDeathRupees_getOrbRecoveryAmount();

// Spawn pending orb when the player enters the death stage/room (dScnRoom load).
void dALBWDeathRupees_trySpawnOrbInRoom(const char* stageName, int roomNo);

// Retry spawn each frame while pending (same-room respawn / room already loaded).
void dALBWDeathRupees_tickSpawn();

// True when this field item actor is the ALBW recovery orb (custom rupee grant).
bool dALBWDeathRupees_onOrbItemGet(fopAc_ac_c* itemActor);

// Tear of Light (daObjDrop_c) pickup — grants recovery rupees, not a story tear.
bool dALBWDeathRupees_onOrbDropGet(fopAc_ac_c* dropActor);

// daObjDrop_c::create — skip tbox gate while spawning the recovery orb.
bool dALBWDeathRupees_allowOrbDropCreate();

// daObjDrop_c::create — record actor id and lift the orb off the ground.
void dALBWDeathRupees_onOrbDropCreated(fopAc_ac_c* dropActor);

// daObjDrop_c::create — snap to ground, start floating immediately (no rise-in).
void dALBWDeathRupees_finishRecoveryDropSetup(class daObjDrop_c* drop);

// daObjDrop_c::create — clear pending flag when actor create fails.
void dALBWDeathRupees_onOrbDropCreateAborted();

// daObjDrop_c::checkGetArea — human Link can collect the recovery orb in any field.
bool dALBWDeathRupees_isRecoveryOrbDrop(const fopAc_ac_c* dropActor);

#endif
// ============================================
// NEW CODE ENDS HERE
// ============================================

#endif /* D_ALBW_DEATH_RUPEE_H */

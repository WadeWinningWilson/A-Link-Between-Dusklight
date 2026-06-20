#ifndef D_ALBW_SHADE_REFUGE_H
#define D_ALBW_SHADE_REFUGE_H

// ============================================
// NEW CODE — ALBW Port — "Shade's Refuge"
// Single active death-respawn slot, set when Link rests at a Shade Watcher.
// Resting at a new watcher overwrites the previous slot (one point at a time).
// Read by the game-over flow (d_gameover.cpp) to offer "respawn at the last
// Shade Watcher". Volatile session RAM — not written to the save card.
// ============================================
#if TARGET_PC

#include "SSystem/SComponent/c_xyz.h"

void dShadeRefuge_setRespawn(const char* i_stage, s8 i_roomNo, const cXyz& i_pos, s16 i_angleY);
void dShadeRefuge_clearRespawn();
bool dShadeRefuge_hasRespawn();
const char* dShadeRefuge_getStage();
s8 dShadeRefuge_getRoom();
const cXyz& dShadeRefuge_getPos();
s16 dShadeRefuge_getAngleY();

// Camera snap on respawn — armed by the game-over respawn branch, consumed for
// a few frames by the Shade Watcher's Execute to force the camera behind Link.
// A point -1 (coords) respawn carries no camera data, so without this the
// camera stays wherever it was at death.
void dShadeRefuge_armRespawnCamera();
bool dShadeRefuge_consumeRespawnCamera();

// Resolve where Link should respawn for the watcher at (stage,room): a custom
// override point if one is registered, otherwise the fallback (the wolf's own
// pos/angle). Lets a watcher land Link beside/facing the wolf instead of on it.
void dShadeRefuge_resolveLinkSpawn(const char* i_stage, s8 i_roomNo,
                                   const cXyz& i_fallbackPos, s16 i_fallbackAngleY,
                                   cXyz* o_pos, s16* o_angleY);

// ============================================
// NEW CODE — ALBW Port (Shop "Return to Last Shade Watcher" service)
// Surfaced as a Postman shop row on the Upgrades & Services page, directly
// above Oocoo's Return.  Available whenever a respawn slot is set.  Purchase
// queues a warp to the saved watcher, executed on shop close (mirrors Oocoo).
// ============================================
bool        dShadeRefuge_canShowInShop();
bool        dShadeRefuge_tryPurchaseReturn();
void        dShadeRefuge_executePendingWarp();
const char* dShadeRefuge_getServiceName();
const char* dShadeRefuge_getServiceDesc();
int         dShadeRefuge_getServicePrice();

#endif  // TARGET_PC

#endif /* D_ALBW_SHADE_REFUGE_H */

// ============================================
// NEW CODE — ALBW Port — "Shade's Refuge"
// Single active death-respawn slot set by resting at a Shade Watcher.
// Overwrites the previous watcher's slot (one respawn point at a time).
// ============================================
#include "d/d_albw_shade_refuge.h"

#if TARGET_PC

#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_player.h"  // daPy_py_c::setParamData (getup warp-in spawn param)
#include "d/d_com_inf_game.h"
#include "dusk/settings.h"  // master enable toggle (game.shadeRefuge)
#include <cstring>

namespace {
bool sHasRespawn = false;
char sStage[8]   = {};
s8   sRoom       = -1;
cXyz sPos        = { 0.0f, 0.0f, 0.0f };
s16  sAngleY     = 0;
// Frames remaining to force the camera behind Link after a respawn. Counts a
// few frames so the snap overrides the room's initial camera, not just one.
int  sRespawnCameraFrames = 0;
}  // namespace

void dShadeRefuge_setRespawn(const char* i_stage, s8 i_roomNo, const cXyz& i_pos, s16 i_angleY) {
    // One active respawn point: overwrite whatever the previous watcher set.
    int i = 0;
    if (i_stage != NULL) {
        for (; i < 7 && i_stage[i] != '\0'; i++) {
            sStage[i] = i_stage[i];
        }
    }
    sStage[i]   = '\0';
    sRoom       = i_roomNo;
    sPos        = i_pos;
    sAngleY     = i_angleY;
    sHasRespawn = true;
}

// Master toggle for the whole Shade's Refuge system. When off, no watchers
// spawn (d_s_room.cpp), the death screen never offers "Last Shade Watcher"
// (hasRespawn returns false below), and the shop service is hidden
// (canShowInShop returns false below). Default off — feature is WIP.
bool dShadeRefuge_isEnabled() {
    return dusk::getSettings().game.shadeRefuge.getValue();
}

void dShadeRefuge_clearRespawn() {
    sHasRespawn = false;
}

bool dShadeRefuge_hasRespawn() {
    return dShadeRefuge_isEnabled() && sHasRespawn;
}

const char* dShadeRefuge_getStage() {
    return sStage;
}

s8 dShadeRefuge_getRoom() {
    return sRoom;
}

const cXyz& dShadeRefuge_getPos() {
    return sPos;
}

void dShadeRefuge_armRespawnCamera() {
    sRespawnCameraFrames = 4;
}

bool dShadeRefuge_consumeRespawnCamera() {
    if (sRespawnCameraFrames > 0) {
        sRespawnCameraFrames--;
        return true;
    }
    return false;
}

s16 dShadeRefuge_getAngleY() {
    return sAngleY;
}

// ============================================
// NEW CODE — ALBW Port — "Shade's Refuge"
// Per-watcher custom Link respawn point. Most watchers respawn Link at the
// wolf's own spot (the fallback); a watcher listed here respawns Link at a
// chosen offset/facing instead (e.g. beside and facing the wolf). Keyed by
// (stage, room) like the spawn table in d_s_room.cpp.
// ============================================
namespace {
struct LinkSpawnOverride {
    const char* stage;
    s8          room;
    cXyz        pos;
    s16         angleY;
};
const LinkSpawnOverride kLinkSpawnOverrides[] = {
    // Outside the Diababa boss room (D_MN05 r12): land Link beside/facing the
    // wolf, not on top of it.
    { "D_MN05", 12, { 7160.7280f, 3309.4785f, -15697.7324f }, (s16)-17084 },
};
}  // namespace

void dShadeRefuge_resolveLinkSpawn(const char* i_stage, s8 i_roomNo,
                                   const cXyz& i_fallbackPos, s16 i_fallbackAngleY,
                                   cXyz* o_pos, s16* o_angleY) {
    for (int i = 0; i < (int)(sizeof(kLinkSpawnOverrides) / sizeof(kLinkSpawnOverrides[0])); i++) {
        const LinkSpawnOverride& o = kLinkSpawnOverrides[i];
        if (i_stage != NULL && strcmp(i_stage, o.stage) == 0 && i_roomNo == o.room) {
            *o_pos    = o.pos;
            *o_angleY = o.angleY;
            return;
        }
    }
    *o_pos    = i_fallbackPos;
    *o_angleY = i_fallbackAngleY;
}

// ============================================
// NEW CODE — ALBW Port (Shop "Return to Last Shade Watcher" service)
// Mirrors the Oocoo service: a purchasable shop row that queues a warp to the
// saved Shade Watcher, executed on shop close.  The warp itself reuses the
// proven game-over refuge sequence (setRestartRoom + setNextStage, point -1).
// ============================================
namespace {
// TODO(user): confirm the rupee price for this shop row (placeholder).
constexpr int kReturnPrice     = 50;
bool          sPendingReturnWarp = false;
}  // namespace

bool dShadeRefuge_canShowInShop() {
    return dShadeRefuge_isEnabled() && sHasRespawn;
}

bool dShadeRefuge_tryPurchaseReturn() {
    if (!sHasRespawn || sStage[0] == '\0') {
        return false;
    }
    const u16 rupees = dComIfGs_getRupee();
    if (rupees < (u16)kReturnPrice) {
        return false;
    }
    dComIfGs_setRupee(rupees - (u16)kReturnPrice);
    sPendingReturnWarp = true;
    return true;
}

void dShadeRefuge_executePendingWarp() {
    if (!sPendingReturnWarp) {
        return;
    }
    sPendingReturnWarp = false;
    if (!sHasRespawn || sStage[0] == '\0') {
        return;
    }
    // Same getup + camera respawn the game-over flow uses (d_gameover.cpp):
    // setNextStage lastMode 0x45 = mode 5 | (fall-damage 4 << 4) drives Link's
    // fall-recovery getup; setParamData key 0xC9 matches the void-restart; the
    // watcher snaps the camera behind Link and tops the fall damage back to full
    // on arrival.
    dComIfGs_setLife(dComIfGs_getMaxLife());
    dComIfGs_setRestartRoom(sPos, sAngleY, sRoom);
    dComIfGs_setRestartRoomParam(daPy_py_c::setParamData(sRoom, 0, 0xC9, 0));
    dComIfGp_setNextStage(sStage, -1, sRoom, -1, 0.0f, 0x45, 0, 0, 0, 1, 0);
    dShadeRefuge_armRespawnCamera();
}

const char* dShadeRefuge_getServiceName() {
    return "Return to Last Shade Watcher";
}

const char* dShadeRefuge_getServiceDesc() {
    return "A faint ember marks where you last rested. Let it carry you back to "
           "that Shade Watcher's side.";
}

int dShadeRefuge_getServicePrice() {
    return kReturnPrice;
}

#endif  // TARGET_PC

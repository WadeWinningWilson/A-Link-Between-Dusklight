/**
 * d_albw_wolf_stun.cpp — ALBW Port
 * Wolf combat system — generic pause-based stun module.
 *
 * On a Midna field attack hit against a non-twilight, non-boss enemy:
 *   dAlbwWolfStun_apply() is called from cc_at_check.
 *   fpcM_PauseEnable(enemy, 1) skips the enemy's execute() each frame
 *   while fpcEx_Execute still returns early, but draw() is unaffected —
 *   the enemy freezes mid-animation in its last pose for the stun duration.
 *
 * dAlbwWolfStun_update() is called every frame from daAlink_c::execute()
 * to decrement timers and call fpcM_PauseDisable when each expires.
 */

#if TARGET_PC

#include "d/d_albw_wolf_stun.h"
#include "d/dolzel.h"
#include "dusk/settings.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "f_op/f_op_actor.h"

namespace {

struct WolfStunEntry {
    fpc_ProcID mId;
    s16        mTimer;
};

constexpr int WOLF_STUN_MAX = 16;

static WolfStunEntry sStunList[WOLF_STUN_MAX];
static int           sStunCount = 0;

} // anonymous namespace

// ============================================
// NEW CODE — ALBW Port
// ============================================

bool dAlbwWolfCombat_isEnabled() {
    return dusk::getSettings().game.wolfLinkCombat.getValue();
}

bool dAlbwWolfStun_isTwilightEnemy(s16 i_name) {
    switch (i_name) {
    case fpcNm_E_MD_e:       // Shadow Beast (twilight messenger)
    case fpcNm_E_YD_e:       // Shadow (Dusk Puppet)
    case fpcNm_E_YH_e:       // Shadow Bug
    case fpcNm_E_YD_LEAF_e:  // Shadow Leaf
    case fpcNm_E_YMB_e:      // Shadow Bulblin
    case fpcNm_E_YK_e:       // Shadow Keese
    case fpcNm_E_YR_e:       // Shadow Rat
    case fpcNm_E_YG_e:       // Shadow Vermin
        return true;
    default:
        return false;
    }
}

void dAlbwWolfStun_apply(fopAc_ac_c* i_enemy) {
    if (!dAlbwWolfCombat_isEnabled() || i_enemy == NULL) {
        return;
    }

    const fpc_ProcID id = i_enemy->id;

    // Already in list — refresh the timer rather than adding a duplicate.
    for (int i = 0; i < sStunCount; i++) {
        if (sStunList[i].mId == id) {
            sStunList[i].mTimer = static_cast<s16>(WOLF_STUN_FRAMES);
            OS_REPORT("[dAlbwWolfStun] stun refreshed id=%u\n", id);
            return;
        }
    }

    if (sStunCount >= WOLF_STUN_MAX) {
        OS_REPORT("[dAlbwWolfStun] stun list full, dropping id=%u\n", id);
        return;
    }

    fpcM_PauseEnable(i_enemy, 1);
    sStunList[sStunCount].mId    = id;
    sStunList[sStunCount].mTimer = static_cast<s16>(WOLF_STUN_FRAMES);
    sStunCount++;

    OS_REPORT("[dAlbwWolfStun] stun applied id=%u (%d active)\n", id, sStunCount);
}

void dAlbwWolfStun_update() {
    if (!dAlbwWolfCombat_isEnabled()) {
        for (int i = 0; i < sStunCount; i++) {
            base_process_class* proc = fpcM_SearchByID(sStunList[i].mId);
            if (proc != NULL) {
                fpcM_PauseDisable(proc, 1);
            }
        }
        sStunCount = 0;
        return;
    }

    int writeIdx = 0;
    for (int i = 0; i < sStunCount; i++) {
        sStunList[i].mTimer--;
        if (sStunList[i].mTimer > 0) {
            sStunList[writeIdx++] = sStunList[i];
        } else {
            // Timer expired — look up the actor and release the pause.
            base_process_class* proc = fpcM_SearchByID(sStunList[i].mId);
            if (proc != NULL) {
                fpcM_PauseDisable(proc, 1);
                OS_REPORT("[dAlbwWolfStun] stun expired id=%u\n", sStunList[i].mId);
            }
        }
    }
    sStunCount = writeIdx;
}

// ============================================
// NEW CODE ENDS HERE
// ============================================

#endif // TARGET_PC

#if TARGET_PC

#include "d/d_albw_combat.h"
#include "d/actor/d_a_b_tn.h"
#include "dusk/trace_noop.h"
#include "dusk/settings.h"
#include "f_op/f_op_actor.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"

bool dAlbw_isHiddenSkillReworkEnabled() {
    return dusk::getSettings().game.hiddenSkillRework.getValue();
}

static bool s_jumpStrikeChargeReady = false;

void dAlbw_resetJumpStrikeChargeReady() {
    s_jumpStrikeChargeReady = false;
    CONAV_LOG("jsready", "reset");
}

void dAlbw_setJumpStrikeChargeReady() {
    if (!s_jumpStrikeChargeReady) {
        s_jumpStrikeChargeReady = true;
        CONAV_LOG("jsready", "set ready=1");
    }
}

bool dAlbw_isJumpStrikeChargeReady() {
    if (!dAlbw_isHiddenSkillReworkEnabled()) {
        return true;
    }

    return s_jumpStrikeChargeReady;
}

bool dAlbw_peekJumpStrikeChargeReady() {
    return s_jumpStrikeChargeReady;
}

bool dAlbw_tryConsumeJumpStrikeChargeReady() {
    if (!dAlbw_isHiddenSkillReworkEnabled()) {
        return true;
    }

    if (!s_jumpStrikeChargeReady) {
        CONAV_LOG("jsready", "consume denied ready=0");
        return false;
    }

    s_jumpStrikeChargeReady = false;
    CONAV_LOG("jsready", "consume ok");
    return true;
}

dAlbwHelmBashTier dAlbwCombat_getHelmBashTier(fopAc_ac_c* i_actor) {
    if (i_actor == NULL || fopAcM_GetGroup(i_actor) != fopAc_ENEMY_e) {
        return dAlbwHelmBash_THRESHOLD;
    }

    if (fopAcM_GetName(i_actor) == fpcNm_B_TN_e) {
        const daB_TN_c* darknut = (const daB_TN_c*)i_actor;
        if (darknut->albwIsUnarmoredPhase()) {
            return dAlbwHelmBash_THRESHOLD;
        }

        return dAlbwHelmBash_MAX;
    }

    return dAlbwHelmBash_THRESHOLD;
}

#endif // TARGET_PC

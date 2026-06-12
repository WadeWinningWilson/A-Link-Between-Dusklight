// ============================================
// NEW CODE — ALBW Port
// Enemy death rupees — wallet credit on kill + fight victory payout.
// ============================================
#include "d/d_albw_enemy_rupee.h"
#include "d/d_albw_rupee_popup.h"

#if TARGET_PC

#include "d/d_com_inf_game.h"
#include "dusk/settings.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"
#include <cstring>

namespace {

constexpr u16 kNormalPoeRupees = 20;
constexpr u16 kImpPoeRupees    = 100;

static s16 sVictoryGranted[32];
static int   sVictoryGrantedCount = 0;

static constexpr int kGrantedKillIdCap = 64;
static u32 sGrantedKillActorIds[kGrantedKillIdCap];
static int   sGrantedKillIdCount = 0;

static bool wasVictoryGranted(s16 profName) {
    for (int i = 0; i < sVictoryGrantedCount; ++i) {
        if (sVictoryGranted[i] == profName) {
            return true;
        }
    }
    return false;
}

static void markVictoryGranted(s16 profName) {
    if (sVictoryGrantedCount >= (int)(sizeof(sVictoryGranted) / sizeof(sVictoryGranted[0]))) {
        return;
    }
    sVictoryGranted[sVictoryGrantedCount++] = profName;
}

static bool wasKillGranted(fopAc_ac_c* enemy) {
    if (enemy == NULL) {
        return false;
    }
    const u32 actorId = fopAcM_GetID(enemy);
    for (int i = 0; i < sGrantedKillIdCount; ++i) {
        if (sGrantedKillActorIds[i] == actorId) {
            return true;
        }
    }
    return false;
}

static void markKillGranted(fopAc_ac_c* enemy) {
    if (enemy == NULL) {
        return;
    }
    const u32 actorId = fopAcM_GetID(enemy);
    for (int i = 0; i < sGrantedKillIdCount; ++i) {
        if (sGrantedKillActorIds[i] == actorId) {
            return;
        }
    }
    if (sGrantedKillIdCount >= kGrantedKillIdCap) {
        sGrantedKillIdCount = 0;
    }
    sGrantedKillActorIds[sGrantedKillIdCount++] = actorId;
}

static void grantRupees(u16 amount) {
    if (amount == 0) {
        return;
    }
    dComIfGp_setItemRupeeCount(static_cast<s32>(amount));
    // ============================================
    // NEW CODE — ALBW Port
    // Native "+n" HUD popup above the rupee counter (J2D meter stack,
    // not a Dusk toast).  Single notify point keeps the HUD in sync
    // with whatever amount the tables above resolved to.
    // ============================================
    dAlbwEnemyRupeesHud_onGrant(amount);
    // ============================================
    // NEW CODE ENDS HERE
    // ============================================
}

static bool isBulblinKingSpawn(fopAc_ac_c* enemy) {
    const u8 arg0 = static_cast<u8>(fopAcM_GetParam(enemy) & 0xFF);
    return arg0 == 4 || arg0 == 5 || arg0 == 11 || arg0 == 12;
}

static u16 resolveBulblinKillRupees(fopAc_ac_c* enemy) {
    if (isBulblinKingSpawn(enemy)) {
        return 0;
    }
    const u8 weaponType = static_cast<u8>((fopAcM_GetParam(enemy) & 0xF00) >> 8);
    if (weaponType >= 2) {
        return 5;
    }
    return 1;
}

static u16 resolvePoeKillRupees(fopAc_ac_c* enemy) {
    u8 arg0 = static_cast<u8>(fopAcM_GetParam(enemy) & 0xFF);
    if (arg0 == 0xFF) {
        arg0 = 1;
    }
    if (arg0 == 0) {
        return kImpPoeRupees;
    }
    return kNormalPoeRupees;
}

static bool isKillExcludedProfile(s16 profName) {
    switch (profName) {
    case fpcNm_B_BQ_e:
    case fpcNm_B_BH_e:
    case fpcNm_B_DS_e:
    case fpcNm_B_DR_e:
    case fpcNm_B_DRE_e:
    case fpcNm_B_GM_e:
    case fpcNm_B_GND_e:
    case fpcNm_B_MGN_e:
    case fpcNm_B_OB_e:
    case fpcNm_B_OH_e:
    case fpcNm_B_OH2_e:
    case fpcNm_B_YO_e:
    case fpcNm_B_YOI_e:
    case fpcNm_B_ZANT_e:
    case fpcNm_B_ZANTS_e:
    case fpcNm_B_ZANTZ_e:
    case fpcNm_B_ZANTM_e:
    case fpcNm_B_TN_e:
    case fpcNm_B_GG_e:
    case fpcNm_E_FM_e:
    case fpcNm_E_GOB_e:
    case fpcNm_E_TH_e:
    case fpcNm_E_VT_e:
    case fpcNm_E_DT_e:
    case fpcNm_E_MK_e:
    case fpcNm_E_RDB_e:
    case fpcNm_E_PZ_e:
    case fpcNm_E_YMB_e:
    case fpcNm_E_HZELDA_e:
    case fpcNm_E_MB_e:
    case fpcNm_E_GS_e:
    case fpcNm_E_IS_e:
    case fpcNm_E_MM_MT_e:
    case fpcNm_E_DB_LEAF_e:
    case fpcNm_E_HB_LEAF_e:
    case fpcNm_E_YD_LEAF_e:
    case fpcNm_E_BI_LEAF_e:
    case fpcNm_E_PH_e:
    case fpcNm_E_SB_e:
    case fpcNm_E_WW_e:
    case fpcNm_E_MD_e:
    case fpcNm_E_CR_EGG_e:
    case fpcNm_E_TK_BALL_e:
    case fpcNm_E_DF_e:
    case fpcNm_E_BEE_e:
    case fpcNm_E_GA_e:
        return true;
    default:
        return false;
    }
}

static u16 lookupKillRupees(s16 profName, fopAc_ac_c* enemy) {
    if (isKillExcludedProfile(profName)) {
        return 0;
    }

    switch (profName) {
    case fpcNm_E_AI_e:
        return 15;
    case fpcNm_E_GM_e:
        return 1;
    case fpcNm_E_DK_e:
        return 5;
    case fpcNm_E_GB_e:
        return 15;
    case fpcNm_E_HB_e:
        return 5;
    case fpcNm_E_HM_e:
        return 3;
    case fpcNm_E_KR_e:
        return 15;
    case fpcNm_E_ST_e:
        return 5;
    case fpcNm_E_OC_e:
        return 5;
    case fpcNm_E_BG_e:
        return 3;
    case fpcNm_E_CR_e:
        return 3;
    case fpcNm_E_BU_e:
        return 1;
    case fpcNm_E_RD_e:
        return resolveBulblinKillRupees(enemy);
    case fpcNm_E_KK_e:
        return 25;
    case fpcNm_E_SM2_e:
        return 3;
    case fpcNm_E_SM_e:
        return 3;
    case fpcNm_E_DB_e:
        return 1;
    case fpcNm_E_MF_e:
        return 50;
    case fpcNm_E_FB_e:
    case fpcNm_E_FZ_e:
        return 10;
    case fpcNm_E_NZ_e:
        return 1;
    case fpcNm_E_GE_e:
        return 3;
    case fpcNm_E_MM_e:
        return 3;
    case fpcNm_E_PO_e:
        return resolvePoeKillRupees(enemy);
    case fpcNm_E_HP_e:
        return kImpPoeRupees;
    case fpcNm_E_BA_e:
        return 1;
    case fpcNm_E_RB_e:
        return 1;
    case fpcNm_E_DN_e:
        return 25;
    case fpcNm_E_SW_e:
        return 1;
    case fpcNm_E_FK_e:
        return 20;
    case fpcNm_E_BUG_e:
        return 1;
    case fpcNm_E_FS_e:
        return 1;
    case fpcNm_E_MS_e:
        return 1;
    case fpcNm_E_GI_e:
        return 10;
    case fpcNm_E_S1_e:
        return 15;
    case fpcNm_E_RDY_e:
        return 5;
    case fpcNm_E_YD_e:
        return 1;
    case fpcNm_E_YH_e:
        return 1;
    case fpcNm_E_YM_e:
        return 1;
    case fpcNm_E_YC_e:
        return 15;
    case fpcNm_E_YK_e:
        return 1;
    case fpcNm_E_YG_e:
        return 1;
    case fpcNm_E_PM_e:
        return 64;
    case fpcNm_E_SG_e:
        return 1;
    case fpcNm_E_WS_e:
        return 1;
    case fpcNm_E_BS_e:
        return 1;
    case fpcNm_E_SF_e:
        return 25;
    case fpcNm_E_SH_e:
        return 10;
    case fpcNm_E_ZS_e:
        return 5;
    case fpcNm_E_TT_e:
        return 3;
    case fpcNm_E_OT_e:
        return 1;
    case fpcNm_E_TK_e:
    case fpcNm_E_TK2_e:
        return 3;
    case fpcNm_E_ZM_e:
        return 3;
    case fpcNm_E_KG_e:
        return 1;
    case fpcNm_E_BI_e:
        return 1;
    case fpcNm_E_HZ_e:
        return 4;
    default:
        return 0;
    }
}

static u16 lookupFightVictoryRupees(s16 profName) {
    switch (profName) {
    case fpcNm_B_BQ_e:
    case fpcNm_E_FM_e:
    case fpcNm_B_OB_e:
        return 200;
    case fpcNm_B_DS_e:
    case fpcNm_B_YO_e:
    case fpcNm_B_GM_e:
    case fpcNm_B_DR_e:
        return 300;
    case fpcNm_B_ZANT_e:
        return 500;
    case fpcNm_B_MGN_e:
        return 500;
    case fpcNm_E_HZELDA_e:
        return 200;
    case fpcNm_B_TN_e:
        return 200;
    case fpcNm_E_GOB_e:
    case fpcNm_E_TH_e:
    case fpcNm_E_VT_e:
    case fpcNm_E_DT_e:
    case fpcNm_E_RDB_e:
    case fpcNm_E_MK_e:
    case fpcNm_E_PZ_e:
    case fpcNm_B_GG_e:
    case fpcNm_E_YMB_e:
        return 100;
    default:
        return 0;
    }
}

}  // namespace

bool dAlbwEnemyRupees_isEnabled() {
    return dusk::getSettings().game.enemyDeathRupees.getValue();
}

void dAlbwEnemyRupees_onEnemyKill(fopAc_ac_c* enemy) {
    if (!dAlbwEnemyRupees_isEnabled() || enemy == NULL) {
        return;
    }
    if (fopAcM_GetGroup(enemy) != fopAc_ENEMY_e) {
        return;
    }
    if (wasKillGranted(enemy)) {
        return;
    }

    const s16 profName = fopAcM_GetName(enemy);
    const u16 amount   = lookupKillRupees(profName, enemy);
    if (amount == 0) {
        return;
    }

    markKillGranted(enemy);
    grantRupees(amount);
}

bool dAlbwEnemyRupees_tryKillAfterDamage(fopAc_ac_c* enemy, s32 attackPower) {
    if (!dAlbwEnemyRupees_isEnabled() || enemy == NULL || attackPower == 0) {
        return false;
    }
    if (fopAcM_GetGroup(enemy) != fopAc_ENEMY_e) {
        return false;
    }

    if (enemy->health > 0) {
        return false;
    }

    dAlbwEnemyRupees_onEnemyKill(enemy);
    return true;
}

void dAlbwEnemyRupees_tryGrantFightVictory(s16 profName) {
    if (!dAlbwEnemyRupees_isEnabled()) {
        return;
    }
    if (wasVictoryGranted(profName)) {
        return;
    }

    const u16 amount = lookupFightVictoryRupees(profName);
    if (amount == 0) {
        return;
    }

    markVictoryGranted(profName);
    grantRupees(amount);
}

void dAlbwEnemyRupees_onBeamosDestroyed() {
    if (!dAlbwEnemyRupees_isEnabled()) {
        return;
    }
    grantRupees(5);
}

#endif

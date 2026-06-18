#if TARGET_PC

#include "d/d_albw_boss.h"

#include "Z2AudioLib/Z2Instances.h"
#include "d/actor/d_a_b_gm.h"
#include "d/d_albw_hp_mult.h"
#include "d/d_cc_uty.h"
#include "d/actor/d_a_player.h"
#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "dusk/settings.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>

// ============================================
// NEW CODE — ALBW Boss Refinement
// ============================================

static AlbwBossArenaId s_warpBootstrapArena = ALBW_BOSS_ARENA_INVALID;
static bool s_armogohmaWarpBootstrap = false;

static constexpr int kAlbwArmogohmaEggGateCount = 4;
static constexpr int kAlbwArmogohmaEggGatePct[kAlbwArmogohmaEggGateCount] = {85, 75, 65, 20};
static constexpr int kAlbwArmogohmaOpeningPct[3] = {75, 45, 15};
static constexpr int kAlbwArmogohmaPostStatueSnapPct[2] = {60, 35};
static constexpr int kAlbwArmogohmaBowChipPct = 4;

static bool s_armogohmaInitialized = false;
static s16 s_armogohmaMaxHp = 0;
static s16 s_armogohmaPrevHp = 0;
static bool s_armogohmaEggQueued[kAlbwArmogohmaEggGateCount] = {};
static bool s_armogohmaEggUsed[kAlbwArmogohmaEggGateCount] = {};
static bool s_armogohmaOpeningCrossed[3] = {};
static bool s_armogohmaPendingCeilingDrop = false;
static bool s_armogohmaBarSuppressed = false;

static const char* const s_bossArenaStageNames[kAlbwBossWarpStageCount] = {
    "D_MN05A",
    "D_MN04A",
    "D_MN01A",
    "D_MN10A",
    "D_MN11A",
    "D_MN06A",
    "D_MN07A",
    "D_MN08A",
    "D_MN01A",
};

bool dAlbwBossRefinement_isEnabled() {
    return dusk::getSettings().game.bossRefinement.getValue();
}

bool dAlbwBoss_armogohmaShouldSuppressVanillaArrowDamage(fopAc_ac_c* i_boss,
                                                         dCcU_AtInfo* i_atInfo) {
    if (!dAlbwBossRefinement_isEnabled() || i_boss == NULL || i_atInfo == NULL) {
        return false;
    }

    if (fopAcM_GetName(i_boss) != fpcNm_B_GM_e) {
        return false;
    }

    if (i_atInfo->mpCollider == NULL || !i_atInfo->mpCollider->ChkAtType(AT_TYPE_ARROW)) {
        return false;
    }

    return true;
}

bool dAlbwBossRefinement_playerHasBossSword() {
    if (!dAlbwBossRefinement_isEnabled()) {
        return daPy_py_c::checkMasterSwordEquip() != FALSE;
    }

    return daPy_py_c::checkSwordGet() != FALSE;
}

bool dAlbwBossRefinement_colliderCountsAsMasterSword(dCcD_GObjInf* i_collider) {
    if (i_collider == NULL) {
        return false;
    }

    if (i_collider->ChkAtType(AT_TYPE_MASTER_SWORD)) {
        return true;
    }

    if (dAlbwBossRefinement_isEnabled() && i_collider->ChkAtType(AT_TYPE_NORMAL_SWORD)) {
        return daPy_py_c::checkSwordGet() != FALSE;
    }

    return false;
}

AlbwBossArenaId dAlbwBoss_stageNameToArenaId(const char* i_stageName) {
    if (i_stageName == NULL) {
        return ALBW_BOSS_ARENA_INVALID;
    }

    for (int i = 0; i < kAlbwBossWarpStageCount; i++) {
        if (strcmp(i_stageName, s_bossArenaStageNames[i]) == 0) {
            return static_cast<AlbwBossArenaId>(i);
        }
    }

    return ALBW_BOSS_ARENA_INVALID;
}

static void clearWarpBootstrapSession() {
    s_warpBootstrapArena = ALBW_BOSS_ARENA_INVALID;
    s_armogohmaWarpBootstrap = false;
}

static void* resetArmogohmaCstatueSwitches(void* i_actor, void* i_data) {
    (void)i_data;
    fopAc_ac_c* actor = (fopAc_ac_c*)i_actor;
    if (!fopAcM_IsActor(actor) || fopAcM_GetName(actor) != fpcNm_CSTATUE_e) {
        return NULL;
    }

    for (int sw = 0; sw <= 6; sw++) {
        if (fopAcM_isSwitch(actor, sw)) {
            fopAcM_offSwitch(actor, sw);
        }
    }

    return NULL;
}

static void resetArmogohmaCstatueActors() {
    fpcM_Search(resetArmogohmaCstatueSwitches, NULL);
}

static void applyArmogohmaStageFixes() {
    // Reset boss-room state so Dominion Rod statues can spawn/use mid-fight after warp-in.
    for (int sw = 0; sw <= 6; sw++) {
        dComIfGs_offOneZoneSwitch(sw, -1);
    }

    resetArmogohmaCstatueActors();
}

bool dAlbwBoss_isArmogohmaWarpBootstrap() {
    return dAlbwBossRefinement_isEnabled() && s_armogohmaWarpBootstrap;
}

void dAlbwBoss_armogohmaResetFightState() {
    s_armogohmaInitialized = false;
    s_armogohmaMaxHp = 0;
    s_armogohmaPrevHp = 0;

    for (int i = 0; i < kAlbwArmogohmaEggGateCount; i++) {
        s_armogohmaEggQueued[i] = false;
        s_armogohmaEggUsed[i] = false;
    }

    for (int i = 0; i < 3; i++) {
        s_armogohmaOpeningCrossed[i] = false;
    }

    s_armogohmaPendingCeilingDrop = false;
    s_armogohmaBarSuppressed = false;
}

static s16 armogohmaHpFromPercent(int i_percent) {
    if (s_armogohmaMaxHp <= 0) {
        return 1;
    }

    const int hp = static_cast<int>(s_armogohmaMaxHp) * i_percent / 100;
    return static_cast<s16>(std::max(1, std::min(hp, static_cast<int>(s_armogohmaMaxHp))));
}

static void armogohmaSetHp(fopAc_ac_c* i_boss, s16 i_hp) {
    const s16 clamped = static_cast<s16>(std::max(1, std::min(static_cast<int>(i_hp), static_cast<int>(s_armogohmaMaxHp))));
    i_boss->health = clamped;
}

static void armogohmaProcessThresholdCrossings(s16 i_prevHp, s16 i_newHp) {
    if (s_armogohmaMaxHp <= 0 || i_prevHp <= i_newHp) {
        return;
    }

    for (int i = 0; i < kAlbwArmogohmaEggGateCount; i++) {
        const s16 gate = armogohmaHpFromPercent(kAlbwArmogohmaEggGatePct[i]);
        if (i_prevHp > gate && i_newHp <= gate) {
            s_armogohmaEggQueued[i] = true;
        }
    }

    for (int i = 0; i < 3; i++) {
        const s16 gate = armogohmaHpFromPercent(kAlbwArmogohmaOpeningPct[i]);
        if (i_prevHp > gate && i_newHp <= gate) {
            s_armogohmaOpeningCrossed[i] = true;
            s_armogohmaPendingCeilingDrop = true;
        }
    }
}

static void armogohmaApplyHpChange(fopAc_ac_c* i_boss, s16 i_newHp) {
    const s16 prev = i_boss->health;
    armogohmaSetHp(i_boss, i_newHp);
    armogohmaProcessThresholdCrossings(prev, i_boss->health);
    s_armogohmaPrevHp = i_boss->health;
}

void dAlbwBoss_armogohmaEnsureInitialized(fopAc_ac_c* i_boss) {
    if (!dAlbwBossRefinement_isEnabled() || i_boss == NULL) {
        return;
    }

    const s16 maxFromActor = i_boss->field_0x560 > 0 ? i_boss->field_0x560 : i_boss->health;

    if (!s_armogohmaInitialized) {
        s_armogohmaMaxHp = maxFromActor > 0 ? maxFromActor : i_boss->health;
        if (s_armogohmaMaxHp <= 0) {
            s_armogohmaMaxHp = 500;
        }

        if (i_boss->health <= 0 || i_boss->health > s_armogohmaMaxHp) {
            i_boss->health = s_armogohmaMaxHp;
        }

        s_armogohmaPrevHp = i_boss->health;
        s_armogohmaInitialized = true;
        return;
    }

    if (maxFromActor > s_armogohmaMaxHp) {
        s_armogohmaMaxHp = maxFromActor;
        s_armogohmaPrevHp = i_boss->health;
    }
}

void dAlbwBoss_armogohmaOnBowCoreHit(fopAc_ac_c* i_boss) {
    if (!dAlbwBossRefinement_isEnabled() || i_boss == NULL) {
        return;
    }

    dAlbwBoss_armogohmaEnsureInitialized(i_boss);

    const int chip = static_cast<int>(s_armogohmaMaxHp) * kAlbwArmogohmaBowChipPct / 100;
    if (chip <= 0) {
        return;
    }

    const s16 newHp = static_cast<s16>(std::max(1, static_cast<int>(i_boss->health) - chip));
    armogohmaApplyHpChange(i_boss, newHp);
}

bool dAlbwBoss_armogohmaIsOnCeiling(fopAc_ac_c* i_boss) {
    if (i_boss == NULL) {
        return false;
    }

    return i_boss->current.pos.y > 1000.0f;
}

bool dAlbwBoss_armogohmaTakeCeilingDropPending() {
    if (!s_armogohmaPendingCeilingDrop) {
        return false;
    }

    s_armogohmaPendingCeilingDrop = false;
    return true;
}

void dAlbwBoss_armogohmaFillDisplayHp(fopAc_ac_c* i_boss, s16* o_current, s16* o_max) {
    if (i_boss == NULL || o_current == NULL || o_max == NULL) {
        return;
    }

    *o_current = i_boss->health;
    *o_max = i_boss->field_0x560 > 0 ? i_boss->field_0x560 : i_boss->health;

    if (!dAlbwBossRefinement_isEnabled()) {
        return;
    }

    dAlbwBoss_armogohmaEnsureInitialized(i_boss);
    if (s_armogohmaMaxHp > 0) {
        *o_max = s_armogohmaMaxHp;
    }
    *o_current = i_boss->health;
}

static void* searchArmogohmaPhase2Eye(void* i_actor, void* i_data) {
    (void)i_data;
    if (fopAcM_IsActor(i_actor) && fopAcM_GetName(i_actor) == fpcNm_E_GM_e &&
        fopAcM_GetParam(i_actor) == 3)
    {
        return i_actor;
    }

    return NULL;
}

static fopAc_ac_c* findArmogohmaPhase2Eye() {
    return (fopAc_ac_c*)fpcM_Search(searchArmogohmaPhase2Eye, NULL);
}

bool dAlbwBoss_armogohmaResolveBarTarget(fopAc_ac_c** o_actor) {
    if (o_actor == NULL) {
        return false;
    }

    *o_actor = NULL;

    if (s_armogohmaBarSuppressed) {
        return false;
    }

    fopAc_ac_c* gmActor = fopAcM_SearchByName(fpcNm_B_GM_e);
    if (gmActor == NULL || !fopAcM_IsActor(gmActor)) {
        return false;
    }

    const b_gm_class* boss = (const b_gm_class*)gmActor;

    if (boss->mDemoMode >= 50) {
        return false;
    }

    if (boss->mIsDisappear != 0) {
        if (boss->mDemoMode >= 40) {
            return false;
        }

        fopAc_ac_c* eye = findArmogohmaPhase2Eye();
        if (eye == NULL || !fopAcM_IsActor(eye) || eye->health <= 0) {
            return false;
        }

        *o_actor = eye;
        return true;
    }

    if (gmActor->health <= 0) {
        return false;
    }

    *o_actor = gmActor;
    return true;
}

bool dAlbwBoss_armogohmaQueryHealthBar(dAlbwBoss_ArmogohmaBarState* o_state) {
    if (o_state == NULL) {
        return false;
    }

    o_state->visible = false;
    o_state->phase = 0;
    o_state->current = 0;
    o_state->max = 0;

    fopAc_ac_c* target = NULL;
    if (!dAlbwBoss_armogohmaResolveBarTarget(&target) || target == NULL) {
        return true;
    }

    o_state->visible = true;
    o_state->phase = (fopAcM_GetName(target) == fpcNm_E_GM_e) ? 2 : 1;

    if (o_state->phase == 1) {
        dAlbwBoss_armogohmaFillDisplayHp(target, &o_state->current, &o_state->max);
    } else {
        const dAlbwHP_LockonDisplay hp = dAlbwHP_getLockonDisplayHp(target);
        o_state->current = hp.current;
        o_state->max = hp.max;
    }

    return true;
}

void dAlbwBoss_armogohmaOnRodHit(fopAc_ac_c* i_boss, s8 i_hitCount) {
    if (!dAlbwBossRefinement_isEnabled() || i_boss == NULL) {
        return;
    }

    dAlbwBoss_armogohmaEnsureInitialized(i_boss);

    if (i_hitCount == 1) {
        armogohmaApplyHpChange(i_boss, armogohmaHpFromPercent(kAlbwArmogohmaPostStatueSnapPct[0]));
    } else if (i_hitCount == 2) {
        armogohmaApplyHpChange(i_boss, armogohmaHpFromPercent(kAlbwArmogohmaPostStatueSnapPct[1]));
    }
}

bool dAlbwBoss_armogohmaTryBeginEggPhase(b_gm_class* i_boss) {
    if (!dAlbwBossRefinement_isEnabled() || i_boss == NULL) {
        return false;
    }

    dAlbwBoss_armogohmaEnsureInitialized((fopAc_ac_c*)i_boss);

    for (int i = 0; i < kAlbwArmogohmaEggGateCount; i++) {
        if (s_armogohmaEggQueued[i] && !s_armogohmaEggUsed[i]) {
            s_armogohmaEggUsed[i] = true;
            s_armogohmaEggQueued[i] = false;
            i_boss->field_0x1ad5 = 1;
            return true;
        }
    }

    return false;
}

void dAlbwBoss_onArmogohmaVictory() {
    if (dAlbwBossRefinement_isEnabled()) {
        dAlbwBoss_armogohmaResetFightState();
        clearWarpBootstrapSession();
    }

    s_armogohmaBarSuppressed = true;
}

void dAlbwBoss_onStageLoad() {
    if (!dAlbwBossRefinement_isEnabled()) {
        return;
    }

    const AlbwBossArenaId stageArena = dAlbwBoss_stageNameToArenaId(dComIfGp_getStartStageName());

    if (s_warpBootstrapArena != ALBW_BOSS_ARENA_INVALID && stageArena != s_warpBootstrapArena) {
        clearWarpBootstrapSession();
        return;
    }

    if (s_warpBootstrapArena == ALBW_BOSS_ARENA_TEMPLE_OF_TIME &&
        stageArena == ALBW_BOSS_ARENA_TEMPLE_OF_TIME)
    {
        applyArmogohmaStageFixes();
    }
}

void dAlbwBoss_requestWarpBootstrap(const char* i_stageName) {
    if (!dAlbwBossRefinement_isEnabled()) {
        return;
    }

    const AlbwBossArenaId arena = dAlbwBoss_stageNameToArenaId(i_stageName);
    if (arena == ALBW_BOSS_ARENA_INVALID) {
        return;
    }

    s_warpBootstrapArena = arena;
    s_armogohmaWarpBootstrap = false;
    dComIfGs_offStageBossEnemy();

    if (arena == ALBW_BOSS_ARENA_TEMPLE_OF_TIME) {
        applyArmogohmaStageFixes();
    }
}

void dAlbwBoss_applyPendingStageBootstrap() {
    dAlbwBoss_onStageLoad();
}

static void applyArmogohmaActorBootstrap(b_gm_class* i_gm) {
    s_armogohmaWarpBootstrap = true;

    i_gm->field_0x566 = 1;
    i_gm->mDemoMode = 0;
    i_gm->mAction = 1;
    i_gm->mMode = 4;
    i_gm->field_0x1ad5 = 0;
    i_gm->field_0x1cfc = 2;
    i_gm->field_0x6f5 = 0;
    i_gm->mTimers[2] = 250;

    Z2GetAudioMgr()->subBgmStop();
    Z2GetAudioMgr()->bgmStart(Z2BGM_GOMA_BTL01, 0, 0);

    resetArmogohmaCstatueActors();
}

bool dAlbwBoss_tryApplyActorBootstrap(s16 i_procName, fopAc_ac_c* i_actor) {
    if (!dAlbwBossRefinement_isEnabled() || s_warpBootstrapArena == ALBW_BOSS_ARENA_INVALID ||
        i_actor == NULL)
    {
        return false;
    }

    if (i_procName == fpcNm_B_GM_e && s_warpBootstrapArena == ALBW_BOSS_ARENA_TEMPLE_OF_TIME) {
        applyArmogohmaActorBootstrap((b_gm_class*)i_actor);
        return true;
    }

    return false;
}

// ============================================
// NEW CODE ENDS HERE
// ============================================

#endif

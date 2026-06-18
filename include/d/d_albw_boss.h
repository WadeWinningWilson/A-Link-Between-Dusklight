/**

 * d_albw_boss.h

 * ALBW boss warp bootstrap and Boss Refinement helpers.

 */



#pragma once



#if TARGET_PC



#include "SSystem/SComponent/c_cc_d.h"

#include "f_pc/f_pc_name.h"

#include "f_op/f_op_actor.h"

struct dCcU_AtInfo;

// Boss Refinement: Armogohma eye hits use 4% chip only (see dAlbwBoss_armogohmaOnBowCoreHit).
bool dAlbwBoss_armogohmaShouldSuppressVanillaArrowDamage(fopAc_ac_c* i_boss,
                                                         dCcU_AtInfo* i_atInfo);

// ============================================

// NEW CODE — ALBW Boss Refinement

// ============================================



static constexpr int kAlbwBossWarpStageCount = 9;



enum AlbwBossArenaId {

    ALBW_BOSS_ARENA_INVALID = -1,

    ALBW_BOSS_ARENA_FOREST = 0,

    ALBW_BOSS_ARENA_GORON_MINES,

    ALBW_BOSS_ARENA_LAKEBED,

    ALBW_BOSS_ARENA_ARBITER,

    ALBW_BOSS_ARENA_SNOWPEAK,

    ALBW_BOSS_ARENA_TEMPLE_OF_TIME,

    ALBW_BOSS_ARENA_CITY_IN_SKY,

    ALBW_BOSS_ARENA_PALACE_OF_TWilight,

    ALBW_BOSS_ARENA_LAKEBED_WARP = 8,

};



bool dAlbwBossRefinement_isEnabled();



// When Boss Refinement is off, matches vanilla Master Sword checks.

bool dAlbwBossRefinement_playerHasBossSword();

bool dAlbwBossRefinement_colliderCountsAsMasterSword(dCcD_GObjInf* i_collider);



AlbwBossArenaId dAlbwBoss_stageNameToArenaId(const char* i_stageName);



void dAlbwBoss_requestWarpBootstrap(const char* i_stageName);

void dAlbwBoss_applyPendingStageBootstrap();

bool dAlbwBoss_tryApplyActorBootstrap(s16 i_procName, fopAc_ac_c* i_actor);



// Armogohma (Boss Refinement only): %-of-max HP pacing, 3 statue hits to win, 4 egg waves.

class b_gm_class;



void dAlbwBoss_armogohmaResetFightState();

void dAlbwBoss_armogohmaEnsureInitialized(fopAc_ac_c* i_boss);

void dAlbwBoss_armogohmaOnBowCoreHit(fopAc_ac_c* i_boss);

bool dAlbwBoss_armogohmaIsOnCeiling(fopAc_ac_c* i_boss);

// True once when an opening gate (75/45/15 %) was crossed; clears the pending flag.
bool dAlbwBoss_armogohmaTakeCeilingDropPending();

void dAlbwBoss_armogohmaFillDisplayHp(fopAc_ac_c* i_boss, s16* o_current, s16* o_max);

struct dAlbwBoss_ArmogohmaBarState {
    bool visible;
    u8 phase;  // 1 = B_GM body, 2 = E_GM floor eye (TYPE_GOMA / param 3)
    s16 current;
    s16 max;
};

// Fills bar visibility + HP for the HUD (phase 1 refinement pool or phase 2 eye).
bool dAlbwBoss_armogohmaQueryHealthBar(dAlbwBoss_ArmogohmaBarState* o_state);

void dAlbwBoss_armogohmaOnRodHit(fopAc_ac_c* i_boss, s8 i_hitCount);

bool dAlbwBoss_armogohmaTryBeginEggPhase(b_gm_class* i_boss);



// Boss-warp revisit session (room switches reset for mid-fight Dominion Rod).

bool dAlbwBoss_isArmogohmaWarpBootstrap();

void dAlbwBoss_onArmogohmaVictory();

void dAlbwBoss_onStageLoad();



// ============================================

// NEW CODE ENDS HERE

// ============================================



#endif


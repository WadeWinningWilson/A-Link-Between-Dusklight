/**
 * d_albw_lockout.cpp — ALBW meter lockout item perks and session counters.
 */

#if TARGET_PC

#include "d/d_albw_lockout.h"
#include "d/d_albw_shield.h"
#include "d/d_com_inf_game.h"
#include "d/d_meter2_info.h"
#include "SSystem/SComponent/c_cc_d.h"
#include "f_pc/f_pc_manager.h"
#include <algorithm>

namespace {

constexpr u8 kLockoutBowShotsMax           = 3;
constexpr u8 kLockoutSlingShotsMax         = 3;
constexpr u8 kLockoutHookshotHitsHeal      = 10;
constexpr u16 kLockoutHeartHealUnits       = 12; // 3 hearts (quarter-heart units)
constexpr int kLockoutBoomerangDebuffFrames = 120; // 4 seconds @ 30fps

constexpr int kLockoutTaggedMax = 16;

struct LockoutTaggedEnemy {
    fpc_ProcID mId;
    s16        mFrames;
    bool       mPauseActive;
};

u8 sLockoutBowShotsRemaining  = kLockoutBowShotsMax;
u8 sLockoutSlingShotsRemaining = kLockoutSlingShotsMax;
u8 sLockoutHookshotHits       = 0;
bool sLockoutDoubleClawUsed   = false;

LockoutTaggedEnemy sTaggedEnemies[kLockoutTaggedMax];
int              sTaggedCount = 0;

int lockoutMeterThresholdPct(int i_percent) {
    const int maxVal = dMeter2_getALBWMaxValue();
    if (maxVal <= 0) {
        return 0;
    }
    return (maxVal * i_percent) / 100;
}

LockoutTaggedEnemy* findTaggedEntry(fpc_ProcID i_id) {
    for (int i = 0; i < sTaggedCount; i++) {
        if (sTaggedEnemies[i].mId == i_id) {
            return &sTaggedEnemies[i];
        }
    }
    return NULL;
}

void clearTaggedEntryAt(int i_index) {
    LockoutTaggedEnemy& entry = sTaggedEnemies[i_index];
    if (entry.mPauseActive) {
        base_process_class* proc = fpcM_SearchByID(entry.mId);
        if (proc != NULL) {
            fpcM_PauseDisable(proc, 1);
        }
    }

    sTaggedCount--;
    for (int i = i_index; i < sTaggedCount; i++) {
        sTaggedEnemies[i] = sTaggedEnemies[i + 1];
    }
}

void clearAllTaggedEnemies() {
    while (sTaggedCount > 0) {
        clearTaggedEntryAt(0);
    }
}

void tagEnemyForLockoutBoomerang(fopAc_ac_c* i_enemy, bool i_nativeStunOnly) {
    if (!dMeter2_isALBWLocked() || i_enemy == NULL ||
        fopAcM_GetGroup(i_enemy) != fopAc_ENEMY_e) {
        return;
    }

    const fpc_ProcID id = i_enemy->id;
    LockoutTaggedEnemy* existing = findTaggedEntry(id);
    if (existing != NULL) {
        existing->mFrames = static_cast<s16>(kLockoutBoomerangDebuffFrames);
        return;
    }

    if (sTaggedCount >= kLockoutTaggedMax) {
        return;
    }

    LockoutTaggedEnemy& entry = sTaggedEnemies[sTaggedCount++];
    entry.mId           = id;
    entry.mFrames       = static_cast<s16>(kLockoutBoomerangDebuffFrames);
    entry.mPauseActive  = false;

    if (!i_nativeStunOnly) {
        fpcM_PauseEnable(i_enemy, 1);
        entry.mPauseActive = true;
    }
}

bool taggedEnemyHasFrames(fopAc_ac_c* i_enemy) {
    if (i_enemy == NULL || !dMeter2_isALBWLocked()) {
        return false;
    }

    const LockoutTaggedEnemy* entry = findTaggedEntry(i_enemy->id);
    return entry != NULL && entry->mFrames > 0;
}

} // namespace

void dAlbwLockout_onBegin() {
    sLockoutBowShotsRemaining   = kLockoutBowShotsMax;
    sLockoutSlingShotsRemaining = kLockoutSlingShotsMax;
    sLockoutHookshotHits        = 0;
    sLockoutDoubleClawUsed      = false;
    clearAllTaggedEnemies();
}

void dAlbwLockout_onEnd() {
    sLockoutBowShotsRemaining   = kLockoutBowShotsMax;
    sLockoutSlingShotsRemaining = kLockoutSlingShotsMax;
    sLockoutHookshotHits        = 0;
    sLockoutDoubleClawUsed      = false;
    clearAllTaggedEnemies();
}

void dAlbwLockout_update() {
    if (sTaggedCount == 0) {
        return;
    }

    int writeIdx = 0;
    for (int i = 0; i < sTaggedCount; i++) {
        sTaggedEnemies[i].mFrames--;
        if (sTaggedEnemies[i].mFrames > 0) {
            sTaggedEnemies[writeIdx++] = sTaggedEnemies[i];
        } else if (sTaggedEnemies[i].mPauseActive) {
            base_process_class* proc = fpcM_SearchByID(sTaggedEnemies[i].mId);
            if (proc != NULL) {
                fpcM_PauseDisable(proc, 1);
            }
        }
    }
    sTaggedCount = writeIdx;
}

void dAlbwLockout_onArrowFired() {
    if (!dMeter2_isALBWLocked() || sLockoutBowShotsRemaining == 0) {
        return;
    }

    sLockoutBowShotsRemaining--;
}

void dAlbwLockout_onSlingFired() {
    if (!dMeter2_isALBWLocked() || sLockoutSlingShotsRemaining == 0) {
        return;
    }

    sLockoutSlingShotsRemaining--;
}

void dAlbwLockout_onHookshotFired() {
    if (!dMeter2_isALBWLocked()) {
        return;
    }

    sLockoutHookshotHits++;
    if (sLockoutHookshotHits < kLockoutHookshotHitsHeal) {
        return;
    }

    sLockoutHookshotHits = 0;
    const u16 life = dComIfGs_getLife();
    const u16 maxLife = dComIfGs_getMaxLife();
    dComIfGs_setLife((u16)std::min<u32>(maxLife, life + kLockoutHeartHealUnits));
}

void dAlbwLockout_onDoubleHookshotFired() {
    if (!dMeter2_isALBWLocked() || sLockoutDoubleClawUsed) {
        return;
    }

    sLockoutDoubleClawUsed = true;
    dMeter2_restoreALBWMeterToFull();
}

bool dAlbwLockout_canFireBow() {
    return dMeter2_isALBWLocked() && sLockoutBowShotsRemaining > 0;
}

bool dAlbwLockout_canFireSling() {
    return dMeter2_isALBWLocked() && sLockoutSlingShotsRemaining > 0;
}

bool dAlbwLockout_canUseDoubleHookshot() {
    return dMeter2_isALBWLocked() && !sLockoutDoubleClawUsed &&
           dMeter2_getALBWMeterValue() >= lockoutMeterThresholdPct(50);
}

void dAlbwLockout_onBoomerangHit(fopAc_ac_c* i_enemy) {
    tagEnemyForLockoutBoomerang(i_enemy, false);
}

void dAlbwLockout_onBoomerangHitNative(fopAc_ac_c* i_enemy) {
    tagEnemyForLockoutBoomerang(i_enemy, true);
}

bool dAlbwLockout_isRangedOpened(fopAc_ac_c* i_enemy) {
    return taggedEnemyHasFrames(i_enemy);
}

bool dAlbwLockout_isBoomerangStunActive(fopAc_ac_c* i_enemy) {
    return taggedEnemyHasFrames(i_enemy);
}

int dAlbwLockout_getBoomerangStunFrames(int i_baseFrames) {
    if (!dMeter2_isALBWLocked() || i_baseFrames <= 0) {
        return i_baseFrames;
    }

    if (i_baseFrames >= kLockoutBoomerangDebuffFrames) {
        return i_baseFrames;
    }

    return kLockoutBoomerangDebuffFrames;
}

void dAlbwLockout_applyAttackPowerBoost(u16& io_attackPower, u32 i_atType) {
    if (io_attackPower == 0 || !dMeter2_isALBWLocked()) {
        return;
    }

    if (i_atType == AT_TYPE_ARROW || i_atType == AT_TYPE_BOMB) {
        io_attackPower = (io_attackPower * 3) / 2;
    } else if (i_atType == AT_TYPE_IRON_BALL) {
        io_attackPower = (io_attackPower * 5) / 2;
    }
}

#endif // TARGET_PC

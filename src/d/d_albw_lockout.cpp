/**
 * d_albw_lockout.cpp — ALBW meter lockout item perks and session counters.
 */

#if TARGET_PC

#include "d/d_albw_lockout.h"
#include "d/d_albw_shield.h"
#include "d/d_com_inf_game.h"
#include "d/d_meter2_info.h"
#include "SSystem/SComponent/c_cc_d.h"
#include <algorithm>

namespace {

constexpr u8 kLockoutBowShotsMax      = 3;
constexpr u8 kLockoutHookshotHitsHeal = 10;
constexpr u16 kLockoutHeartHealUnits  = 12; // 3 hearts (quarter-heart units)

u8 sLockoutBowShotsRemaining = kLockoutBowShotsMax;
u8 sLockoutHookshotHits      = 0;
bool sLockoutDoubleClawUsed  = false;

int lockoutMeterThresholdPct(int i_percent) {
    const int maxVal = dMeter2_getALBWMaxValue();
    if (maxVal <= 0) {
        return 0;
    }
    return (maxVal * i_percent) / 100;
}

} // namespace

void dAlbwLockout_onBegin() {
    sLockoutBowShotsRemaining = kLockoutBowShotsMax;
    sLockoutHookshotHits      = 0;
    sLockoutDoubleClawUsed    = false;
}

void dAlbwLockout_onEnd() {
    sLockoutBowShotsRemaining = kLockoutBowShotsMax;
    sLockoutHookshotHits      = 0;
    sLockoutDoubleClawUsed    = false;
}

void dAlbwLockout_onArrowFired() {
    if (!dMeter2_isALBWLocked() || sLockoutBowShotsRemaining == 0) {
        return;
    }

    sLockoutBowShotsRemaining--;
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

bool dAlbwLockout_canUseDoubleHookshot() {
    return dMeter2_isALBWLocked() && !sLockoutDoubleClawUsed &&
           dMeter2_getALBWMeterValue() >= lockoutMeterThresholdPct(70);
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

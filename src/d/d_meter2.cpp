#include <cstdlib>
/**
 * d_meter2.cpp
 * Main UI handler
 */

#include "d/dolzel.h" // IWYU pragma: keep

#include "d/d_meter2.h"
#include "JSystem/JKernel/JKRExpHeap.h"
#include "d/d_camera.h"
#include "d/d_demo.h"
#include "d/d_scope.h"
#include "d/d_timer.h"
#include "d/d_menu_window_HIO.h"
#include "d/d_meter2_draw.h"
#include "d/d_meter2_info.h"
#include "d/d_meter_HIO.h"
#include "d/d_meter_button.h"
#include "d/d_meter_haihai.h"
#include "d/d_meter_hakusha.h"
#include "d/d_meter_map.h"
#include "d/d_meter_string.h"
#include "f_op/f_op_msg_mng.h"
#include "d/actor/d_a_horse.h"
#include <cstring>

#if TARGET_PC
#include "d/d_albw_lockout.h"
#include "d/d_albw_master_quest.h"
#include "d/d_albw_rental.h"
#include "d/d_albw_shield.h"
#include "d/d_attention.h"
#include "d/actor/d_a_player.h"
#include "dusk/action_bindings.h"
#include "dusk/memory.h"
#include "dusk/settings.h"
#include <chrono>
#include <unordered_set>
#include <vector>
#include "f_op/f_op_actor_mng.h"
#endif
// ============================================
// MODIFIED CODE — ALBW Port
// Source: global variables in ALBW main.cpp
// Using independent sALBWMeter variable instead
// of the game's oil system to avoid conflicts
// with lantern, save system, and other oil writers
// ============================================
#if TARGET_PC
static bool sArrowMade    = false;
static bool sBombAmmo     = false;
static bool sBoomThrow    = false;
static bool sSlingMade    = false;
static bool sIronballThrow = false;
// sOilMaxVar is the current meter ceiling — grows when the meter expands.
// sOilBaseMax is the fixed original meter size used for per-target cost
// calculations (e.g. boomerang: each lock-on costs sOilBaseMax/5 = 2180).
// Keeping them separate ensures per-target costs stay constant even after
// meter expansion (5 lock-ons always costs one full base meter, not one
// full expanded meter).
static int sOilBaseMax = 10900;
static int sOilMaxVar  = 10900;
static bool sMeterinc = false;
static int sALBWMeter = 10900;
static u8 s_zSlotItemCache = 0xFF;
static bool s_extraItemSlotCache = false;
// ============================================
// MODIFIED CODE — ALBW Port (fix: framerate-dependent recovery)
// sRecoveryTimer counted frames, making recovery scale with FPS.
// Replaced with wall-clock time so recovery is framerate-independent.
// Rate: 36 units per 100ms ≈ 15s from half, 30s from empty (testing values).
// ============================================
static std::chrono::steady_clock::time_point sLastRecoveryTime = std::chrono::steady_clock::now();
// ============================================
// NEW CODE — ALBW Port
// Continuous-drain items (spinner, dominion rod) use independent
// 100ms timers so their drain ticks don't interfere with each other
// or with recovery timing.
// sSpinnerActive / sDomRodActive: set each frame by the item proc,
//   consumed in moveKantera(). Reset to false after each drain check.
// Meter expansion:
//   sInitialized    — set on first frame; silently sizes sOilMaxVar
//                     with no sound or animation.
//   sALBWExpanding  — true while filling toward a new (larger) ceiling
//                     after a mid-session unlock.
//   sPrevUnlockCount — number of unlocks detected so far; used to
//                      detect when a new one is earned.
// ============================================
static std::chrono::steady_clock::time_point sLastSpinnerDrainTime = std::chrono::steady_clock::now();
static std::chrono::steady_clock::time_point sLastDomRodDrainTime  = std::chrono::steady_clock::now();
static bool sSpinnerActive        = false;
static bool sDomRodActive         = false;
static bool sHookshotFire         = false;
static bool sDoubleHookshotFire   = false;
static bool sALBWArmorDepleted    = false;
// ============================================
// NEW CODE — ALBW Port
// Drain flags for sword attacks and agility moves.
// Set by the corresponding procInit gate at the start of each action;
// consumed once per moveKantera() tick then cleared.
// sSwordSwing:  normal cuts + non-hidden finishers   (1/6 base = 1817 units)
// sSidestep:    sidestep left/right                  (1/5 base = 2180 units)
// sBackJump:    backward sidestep / back jump        (1/3 base = 3633 units)
// sRollJump:    roll/spin jump                       (1/3 base = 3633 units)
// sHiddenSkill: Mortal Draw, Jump Strike, Helm Splitter, Great Spin
//                                                    (1/2 base = 5450 units)
// ============================================
static bool sSwordSwing    = false;
static bool sSidestep      = false;
static bool sBackJump      = false;
static bool sRollJump      = false;
static bool sHiddenSkill   = false;
// ============================================
// NEW CODE — ALBW Port
// Idle recovery flag.
// Set each frame by procWait() in d_a_alink.cpp while Link is standing
// still (wolf excluded). Cleared by the 100ms recovery tick after it is
// read, so procWait() must re-assert it on subsequent frames if Link is
// still idle. Used to apply a small recovery boost during the exhausted
// lockout state, rewarding the player for resting rather than moving.
// ============================================
static bool sALBWPlayerIdle = false;
// ============================================
// NEW CODE ENDS HERE
// ============================================
// ============================================
// NEW CODE ENDS HERE
// ============================================
// ============================================
// NEW CODE — ALBW Port
// Meter lockout latch.
// Set when internal sALBWMeter reaches zero or below (debt); cleared at sOilMaxVar.
// Spending may push the meter negative on the action that caused depletion; HUD
// shows max(0, internal) until debt is repaid. Lockout blocks new sword/agility/
// hidden-skill actions; the proc already in progress is not interrupted.
// Spinner and Dominion Rod are blocked while internal meter <= 0.
// sALBWMovementExhausted clears at sOilBaseMax; lockout perks still apply until full.
// ============================================
static bool sALBWLocked = false;
static bool sALBWMovementExhausted = false;
// ============================================
// NEW CODE — ALBW Port
// Prevents MAGIC_METER_RECOVER from retriggering every 100ms tick.
// Set true on the first recovery tick; cleared when MAGIC_METER_FINISH
// plays (full) or when MAGIC_METER_DEC plays (new depletion event),
// so each depletion→recovery cycle gets exactly one RECOVER sound.
// ============================================
static bool sALBWRecoverSoundActive = false;
// ============================================
// NEW CODE ENDS HERE
// ============================================
// ============================================
// NEW CODE — ALBW Port
// Clean-encounter reward tracking.
// sArmorEncounterIDs: actor IDs that have attacked Link while armor is
//   equipped — they stay tracked until killed (or room changes).
// sArmorTaintedIDs: subset of encounter IDs that dealt real HP damage.
//   If ANY tainted actor is killed in the encounter, no reward.
// sArmorRewardBlocked: latched true the moment a taint is registered
//   so the block survives even if the tainted actor is killed before
//   all encounter actors are dead.
// sArmorEncounterKillCount: ensures at least one kill happened before
//   the reward fires (prevents triggering on a bare room-change flush).
// sArmorLastRoomNo: current room number; changes mean stale actor IDs —
//   flush tracking without rewarding on mismatch.
// ============================================
static std::unordered_set<fpc_ProcID> sArmorEncounterIDs;
static std::unordered_set<fpc_ProcID> sArmorTaintedIDs;
static bool sArmorRewardBlocked    = false;
static int  sArmorEncounterKillCount = 0;
static s16  sArmorLastRoomNo       = -1;
// ============================================
// NEW CODE ENDS HERE
// ============================================
static bool sInitialized       = false;
static bool sALBWExpanding     = false;
static int  sPrevUpgradeSteps  = 0;

// Meter capacity: heart/armor tiers + one sword-swing (1817) per main dungeon clear.
static constexpr int kALBWHeartArmorTierBonus = 8480;
static constexpr int kALBWDungeonClearBonus   = 1817;
// Normal passive recovery (pre-lockout): scales with meter upgrades (~10s base at 109/100ms).
static constexpr int kALBWRecoveryBase    = 109;
static constexpr int kALBWRecoveryPerStep = 164;  // 10 steps → 2.5× base (273/100ms)
// Lockout-only passive recovery: 7s debt/empty → sOilBaseMax, then 3s for expanded portion.
static constexpr int kALBWLockoutBaseRecoveryTicks   = 70;  // 70 × 100ms = 7.0s
static constexpr int kALBWLockoutExpandRecoveryTicks = 30;  // 30 × 100ms = 3.0s

static const int kALBWDungeonClearBitIdx[] = {
    55,  64,  78, 265, 266, 267, 268, 570,
};

static int countALBWDungeonClears() {
    int count = 0;
    for (int bitIdx : kALBWDungeonClearBitIdx) {
        if (dComIfGs_isEventBit(dSv_event_flag_c::saveBitLabels[bitIdx])) {
            count++;
        }
    }
    return count;
}

static int countALBWMeterUpgradeSteps() {
    int steps = 0;
    if (dAlbwMQ_isEnabled()) {
        steps = dAlbwMQ_getMeterShopTier();
    } else {
        steps = countALBWDungeonClears();
    }
    if (dComIfGs_getMaxLife() >= 50) {
        steps++;
    }
    if (dComIfGs_isItemFirstBit(dItemNo_ARMOR_e)) {
        steps++;
    }
    return steps;
}

static int computeALBWMeterMax() {
    int maxVal = sOilBaseMax;
    if (dComIfGs_getMaxLife() >= 50) {
        maxVal += kALBWHeartArmorTierBonus;
    }
    if (dComIfGs_isItemFirstBit(dItemNo_ARMOR_e)) {
        maxVal += kALBWHeartArmorTierBonus;
    }
    if (dAlbwMQ_isEnabled()) {
        maxVal += dAlbwMQ_getMeterShopTier() * kAlbwMQMeterUnitsPerBuy;
    } else {
        maxVal += countALBWDungeonClears() * kALBWDungeonClearBonus;
    }
    return maxVal;
}

static int albwDisplayMeter() {
    return sALBWMeter > 0 ? sALBWMeter : 0;
}

static void albwRefreshLockoutState(bool i_playDecSound) {
    if (sALBWMeter <= 0) {
        if (i_playDecSound && !sALBWLocked) {
            Z2GetAudioMgr()->seStart(
                Z2SE_MAGIC_METER_DEC, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
            sALBWRecoverSoundActive = false;
            dAlbwLockout_onBegin();
        }
        sALBWLocked = true;
        sALBWMovementExhausted = true;
    }

    if (sALBWMovementExhausted && sALBWMeter >= sOilBaseMax) {
        sALBWMovementExhausted = false;
    }
    if (sALBWLocked && sALBWMeter >= sOilMaxVar) {
        sALBWLocked = false;
        dAlbwLockout_onEnd();
    }
    if (sALBWArmorDepleted && sALBWMeter >= sOilBaseMax) {
        sALBWArmorDepleted = false;
    }

    if (sALBWMeter > sOilMaxVar) {
        sALBWMeter = sOilMaxVar;
    }
}

static void albwDrainMeter(int amount, const std::chrono::steady_clock::time_point& i_now) {
    sALBWMeter -= amount;
    sLastRecoveryTime = i_now;
    albwRefreshLockoutState(true);
}

static int computeALBWRecoveryRate() {
    // Scale passive recovery with current meter capacity, not how it was earned.
    const int expansion    = sOilMaxVar - sOilBaseMax;
    const int maxExpansion = 8 * kALBWDungeonClearBonus + 2 * kALBWHeartArmorTierBonus;
    if (maxExpansion <= 0) {
        return kALBWRecoveryBase;
    }
    constexpr int kVanillaMaxSteps = 10;
    return kALBWRecoveryBase +
           (expansion * kALBWRecoveryPerStep * kVanillaMaxSteps) / (maxExpansion * 10);
}

static int albwLockoutRecoveryRate() {
    if (sALBWMeter < sOilBaseMax) {
        return sOilBaseMax / kALBWLockoutBaseRecoveryTicks;
    }

    const int expansion = sOilMaxVar - sOilBaseMax;
    if (expansion <= 0) {
        return sOilBaseMax / kALBWLockoutBaseRecoveryTicks;
    }

    return expansion / kALBWLockoutExpandRecoveryTicks;
}
// ============================================
// NEW CODE ENDS HERE
// ============================================
// ============================================
// MODIFIED CODE ENDS HERE
// ============================================
#endif
// ============================================
// MODIFIED CODE ENDS HERE
// ============================================

// ============================================
// NEW CODE — ALBW Port
// Source: global drain flag setters, ALBW main.cpp
// Free functions called from d_a_alink_bow.inc (and
// future item files) to signal a drain event this frame.
// Kept as non-inline definitions so the static flags
// stay private to this translation unit.
// ============================================
#if TARGET_PC
void dMeter2_onALBWSling() {
    sSlingMade = true;
    if (sALBWLocked) {
        dAlbwLockout_onSlingFired();
    }
}

void dMeter2_onALBWBoom() {
    sBoomThrow = true;
}
void dMeter2_onALBWArrow() {
    sArrowMade = true;
    dAlbwLockout_onArrowFired();
}
void dMeter2_onALBWBomb()     { sBombAmmo     = true; }
void dMeter2_onALBWIronball() { sIronballThrow = true; }
// ============================================
// NEW CODE — ALBW Port
// Pre-fire affordability checks for ranged/item actions.
// Returns true if sALBWMeter has enough for one use AND
// the lockout latch is clear.  Thresholds must stay in
// sync with the drain values in the moveKantera() block.
// ============================================
bool dMeter2_isALBWLocked() { return sALBWLocked; }

bool dMeter2_isALBWMovementExhausted() { return sALBWMovementExhausted; }

void dMeter2_restoreALBWMeterToFull() {
    sALBWMeter = sOilMaxVar;
    sALBWLocked = false;
    sALBWMovementExhausted = false;
    sALBWRecoverSoundActive = false;
    dAlbwLockout_onEnd();
}

static int albwLockoutMeterPct(int i_percent) {
    if (sOilMaxVar <= 0) {
        return 0;
    }
    return (sOilMaxVar * i_percent) / 100;
}

static bool dMeter2_isALBWLockoutZTargetRecovery() {
    // Lockon() is true for free Z-target (no enemy) and enemy lock-on.
    // daPy_py_c::checkAttentionLock() only tracks RFLG0_ATTENTION_LOCK (enemy).
    dAttention_c* attn = dComIfGp_getAttention();
    return attn != NULL && attn->Lockon();
}

// Continuous-drain items are forced off when internal meter is at or below zero.
bool dMeter2_canALBWSpinner() { return sALBWMeter > 0; }
bool dMeter2_canALBWDomRod() { return sALBWMeter > 0; }

bool dMeter2_canALBWSling() {
    if (sALBWLocked) {
        return dAlbwLockout_canFireSling();
    }
    return true;
}

bool dMeter2_canALBWBoom() {
    return true;
}

bool dMeter2_canALBWArrow() {
    if (sALBWLocked) {
        return dAlbwLockout_canFireBow();
    }
    return true;
}

bool dMeter2_canALBWBomb() {
    if (sALBWLocked) {
        return true;
    }
    return true;
}

bool dMeter2_canALBWIronball() {
    if (sALBWLocked) {
        return sALBWMeter >= albwLockoutMeterPct(93);
    }
    return true;
}
// ============================================
// NEW CODE — ALBW Port
// Continuous-drain item signals.
// Called every frame from the item proc while the item is active;
// moveKantera() processes the 100ms tick and resets the flag.
// dMeter2_isALBWDepleted(): queried by item procs to force-eject
// the player from the item when the meter reaches zero.
// ============================================
void dMeter2_onALBWSpinner()  { sSpinnerActive = true; }
void dMeter2_onALBWDomRod()   { sDomRodActive  = true; }
bool dMeter2_isALBWDepleted() { return sALBWMeter <= 0; }
// ============================================
// NEW CODE — ALBW Port
// Clawshot drain signals and pre-fire gates.
// Single clawshot: 4 shots = one full base meter (2725 each).
// Double clawshot: 8 shots = one full base meter (1362 each).
// Costs are fixed to sOilBaseMax so shot count is constant
// regardless of meter expansion level.
// ============================================
void dMeter2_onALBWHookshot() {
    sHookshotFire = true;
    dAlbwLockout_onHookshotFired();
}

void dMeter2_onALBWDoubleHookshot() {
    sDoubleHookshotFire = true;
    dAlbwLockout_onDoubleHookshotFired();
}

bool dMeter2_canALBWHookshot() {
    if (sALBWLocked) {
        return true;
    }
    return true;
}

bool dMeter2_canALBWDoubleHookshot() {
    if (sALBWLocked) {
        return dAlbwLockout_canUseDoubleHookshot();
    }
    return true;
}
// ============================================
// NEW CODE — ALBW Port
// Sword and agility drain signals and gates.
// Signal functions set the flag for moveKantera() to consume this frame.
// Gate functions let the procInit caller decide whether to proceed:
//   canALBWSword/Sidestep/BackJump/RollJump/HiddenSkill: blocked while sALBWLocked
//     (internal meter at or below zero). No minimum meter while unlocked — a
//     started action may push the meter into debt; lockout blocks the next action.
//   Ranged/items: no minimum while unlocked; lockout perks apply while locked.
// ============================================
void dMeter2_onALBWSword()       { sSwordSwing  = true; }
void dMeter2_onALBWSidestep()    { sSidestep    = true; }
void dMeter2_onALBWBackJump()    { sBackJump    = true; }
void dMeter2_onALBWRollJump()    { sRollJump    = true; }
void dMeter2_onALBWHiddenSkill() {
    if (sHiddenSkill) {
        return;
    }

    sHiddenSkill = true;
}

static void commitALBWHiddenSkillDrain(const std::chrono::steady_clock::time_point& i_now) {
    if (!sHiddenSkill) {
        return;
    }

    albwDrainMeter(5450, i_now);
    sHiddenSkill = false;
}

void dMeter2_commitALBWHiddenSkillIfPending() {
    commitALBWHiddenSkillDrain(std::chrono::steady_clock::now());
}
// Sword, agility, and hidden skills: no minimum meter while lockout is clear
// (drain may push internal meter into debt). Once internal hits zero, lockout
// latches and NEW actions are blocked — the current animation still finishes.
bool dMeter2_canALBWSword()       { return !sALBWLocked; }
bool dMeter2_canALBWSidestep()    { return !sALBWLocked; }
bool dMeter2_canALBWBackJump()    { return !sALBWLocked; }
bool dMeter2_canALBWRollJump()    { return !sALBWLocked; }
bool dMeter2_canALBWHiddenSkill() { return !sALBWLocked; }
// ============================================
// NEW CODE — ALBW Port
// Wolf-form query used by the game-over warp gate.
// Routes through d_meter2 so d_gameover.cpp does not need to include
// the heavy player actor header. daPy_getPlayerActorClass() is already
// called elsewhere in this translation unit (alphaAnimeKantera).
// ============================================
bool dMeter2_isWolfForm() { return daPy_getPlayerActorClass()->checkWolf() != 0; }
// ============================================
// NEW CODE — ALBW Port
// Idle state setter — called from procWait() each frame while Link is
// standing still. Cleared by the recovery tick; see sALBWPlayerIdle.
// ============================================
void dMeter2_setALBWPlayerIdle(bool i_idle) { sALBWPlayerIdle = i_idle; }
// ============================================
// NEW CODE — ALBW Port (Phase 3: no passive recovery while guarding)
// Uses virtual checkPlayerGuard() on daAlink — no d_a_alink.h include.
// ============================================
static bool dMeter2_isALBWRecoveryPausedByGuard() {
    return daPy_getPlayerActorClass()->checkPlayerGuard() != 0;
}
// ============================================
// NEW CODE ENDS HERE
// ============================================
// ============================================
// NEW CODE ENDS HERE
// ============================================
// ============================================
// NEW CODE — ALBW Port
// Magic armor on-hit drain and query functions.
// dMeter2_onALBWArmorHit(): called from setDamagePoint() the moment the
//   armor absorbs a blow. Instantly zeros the meter and arms the recovery
//   timer so normal per-100ms refill starts immediately.
// dMeter2_isALBWArmorDepleted(): queried by checkMagicArmorHeavy() and the
//   per-frame hue updater to drive the gray/normal visual state.
// dMeter2_canALBWArmorBlock(): queried by checkMagicArmorNoDamage(); true
//   when the meter is non-zero and the armor is not in recovery. The
//   rupee >= 500 check is handled at the call site in d_a_alink_damage.inc.
// sALBWArmorDepleted is cleared in moveKantera() once the meter is full
//   again AND the player has >= 500 rupees to fund the next activation.
// ============================================
void dMeter2_onALBWArmorHit() {
    sALBWMeter         = 0;
    sALBWArmorDepleted = true;
    sLastRecoveryTime  = std::chrono::steady_clock::now();
    albwRefreshLockoutState(true);
}
bool dMeter2_isALBWArmorDepleted() { return sALBWArmorDepleted; }
bool dMeter2_canALBWArmorBlock()   { return !sALBWArmorDepleted && sALBWMeter > 0; }
// ============================================
// NEW CODE — ALBW Port
// Called from checkDamageAction() whenever an enemy attacks Link
// while the magic armor is equipped (blocking OR not).
//   actorID      — fopAcM_GetID() of the attacking actor
//   dealtHPDamage — true when armor was NOT blocking (hit went through
//                   to health); false when armor absorbed the blow
// Adding to sArmorEncounterIDs is unconditional so the tracker knows
// who is "in this encounter". Taint is only set on real HP damage.
// ============================================
void dMeter2_onArmorEncounterHit(fpc_ProcID actorID, bool dealtHPDamage) {
    if (actorID == 0) return;
    sArmorEncounterIDs.insert(actorID);
    if (dealtHPDamage) {
        sArmorTaintedIDs.insert(actorID);
        sArmorRewardBlocked = true;
    }
}
// ============================================
// NEW CODE — ALBW Port
// Attack-side encounter registration.
// Called from setSwordHitVibration() whenever Link's sword/wolf attack
// connects with an enemy actor. Adds the enemy to sArmorEncounterIDs
// so the clean-encounter reward can fire even when enemies never land a
// hit on Link (and therefore were never added from the defense side in
// dMeter2_onArmorEncounterHit). Does NOT set sArmorRewardBlocked —
// Link hitting an enemy is never a taint event.
// ============================================
void dMeter2_onArmorAttackHit(fpc_ProcID actorID) {
    if (actorID == 0) return;
    sArmorEncounterIDs.insert(actorID);
}

bool dMeter2_isInCombatEncounter() {
    return !sArmorEncounterIDs.empty();
}
// ============================================
// NEW CODE — ALBW Port
// Fractional meter refill used by magic pickup items (S_MAGIC / L_MAGIC).
// Amount is computed as (sOilMaxVar * numerator / denominator) so the
// refill scales with any meter upgrades the player has collected.
// The result is clamped to sOilMaxVar so the meter cannot overflow.
// ============================================
void dMeter2_addALBWFraction(int numerator, int denominator) {
    int amount = (sOilMaxVar * numerator) / denominator;
    sALBWMeter += amount;
    albwRefreshLockoutState(false);
}

void dMeter2_subALBWFraction(int numerator, int denominator) {
    int amount = (sOilMaxVar * numerator) / denominator;
    albwDrainMeter(amount, std::chrono::steady_clock::now());
}

int dMeter2_getALBWMeterValue() {
    return albwDisplayMeter();
}

int dMeter2_getALBWMaxValue() {
    return sOilMaxVar;
}
// ============================================
// NEW CODE ENDS HERE
// ============================================
// ============================================
// NEW CODE — ALBW Port
// Game-over item reset and rental eligibility tracking.
// dMeter2_fillALBWMeter():          Called from d_gameover on player death
//   (type 0). Resets meter to full sOilMaxVar, clears lockout so Link
//   respawns with full stamina. Meter capacity upgrades are NOT reset.
// sALBWItemNos[]:                   Item-number lookup for the 12 rentable
//   items. Order: Boomerang, Spinner, Bow, Ball&Chain, DomRod, Clawshot,
//   DClawshot, Normal Bombs, Water Bombs, Bomblings, Slingshot, Magic Armor.
// kRentalEligibleBase:              First saveBitLabels index reserved for
//   rental eligibility. saveBitLabels[673+i] tracks whether sALBWItemNos[i]
//   was ever stripped from inventory. Indices 673-684 are confirmed free in
//   the TP save layout (large unused gap 673-784). Storing eligibility in
//   the save file ensures it survives game restarts, continues, and reloads —
//   the volatile static array it replaces was lost on any game exit or reload.
// dMeter2_onALBWRentalEligible():   Sets saveBitLabels[673+i] for the item.
//   Must be called BEFORE offItemFirstBit so the bit is written before strip.
// dMeter2_isALBWRentalEligible():   Queries saveBitLabels[673+i]; used by
//   the rental shop to show the real item name vs "Not in stock — Come Back Soon!".
// ============================================
static const u8 sALBWItemNos[12] = {
    (u8)dItemNo_BOOMERANG_e,    (u8)dItemNo_SPINNER_e,
    (u8)dItemNo_BOW_e,          (u8)dItemNo_IRONBALL_e,
    (u8)dItemNo_COPY_ROD_e,     (u8)dItemNo_HOOKSHOT_e,
    (u8)dItemNo_W_HOOKSHOT_e,   (u8)dItemNo_BOMB_BAG_LV1_e,
    (u8)dItemNo_BOMB_BAG_LV2_e, (u8)dItemNo_POKE_BOMB_e,
    (u8)dItemNo_PACHINKO_e,     (u8)dItemNo_ARMOR_e,
};
static constexpr int kRentalEligibleBase = 673; // saveBitLabels[673..684] — confirmed free

void dMeter2_fillALBWMeter() {
    sALBWMeter              = sOilMaxVar;
    sALBWLocked             = false;
    sALBWMovementExhausted  = false;
    sALBWExpanding          = false;
    sALBWRecoverSoundActive = false;
    dAlbwLockout_onEnd();
}

void dMeter2_onALBWMeterShopPurchase() {
    sALBWExpanding = true;
}

void dMeter2_onALBWRentalEligible(u8 itemNo) {
    for (int i = 0; i < 12; i++) {
        if (sALBWItemNos[i] == itemNo) {
            dComIfGs_onEventBit(dSv_event_flag_c::saveBitLabels[kRentalEligibleBase + i]);
            return;
        }
    }
}

bool dMeter2_isALBWRentalEligible(u8 itemNo) {
    for (int i = 0; i < 12; i++) {
        if (sALBWItemNos[i] == itemNo) {
            return dComIfGs_isEventBit(dSv_event_flag_c::saveBitLabels[kRentalEligibleBase + i]);
        }
    }
    return false;
}

static void dMeter2_clearItemFromAllSlots(u8 itemNo) {
    for (int slot = SLOT_0; slot <= SLOT_23; slot++) {
        if (dComIfGs_getItem(slot, false) == itemNo) {
            dComIfGs_setItem(slot, dItemNo_NONE_e);
        }
    }
}

static bool dMeter2_slotHasPossessionForm(u8 rentalItemNo) {
    if (dComIfGs_isItemFirstBit(rentalItemNo)) {
        return true;
    }
    for (int slot = SLOT_0; slot <= SLOT_23; slot++) {
        if (dComIfGs_getItem(slot, false) == rentalItemNo) {
            return true;
        }
    }
    if (rentalItemNo == (u8)dItemNo_BOMB_BAG_LV1_e) {
        for (int slot = SLOT_0; slot <= SLOT_23; slot++) {
            if (dComIfGs_getItem(slot, false) == (u8)dItemNo_NORMAL_BOMB_e) {
                return true;
            }
        }
    } else if (rentalItemNo == (u8)dItemNo_BOMB_BAG_LV2_e) {
        for (int slot = SLOT_0; slot <= SLOT_23; slot++) {
            if (dComIfGs_getItem(slot, false) == (u8)dItemNo_WATER_BOMB_e) {
                return true;
            }
        }
    } else if (rentalItemNo == (u8)dItemNo_COPY_ROD_e) {
        for (int slot = SLOT_0; slot <= SLOT_23; slot++) {
            if (dComIfGs_getItem(slot, false) == (u8)dItemNo_COPY_ROD_2_e) {
                return true;
            }
        }
    }
    return false;
}

static void dMeter2_clearAllPossessionForms(u8 rentalItemNo) {
    dMeter2_clearItemFromAllSlots(rentalItemNo);
    dComIfGs_offItemFirstBit(rentalItemNo);
    switch (rentalItemNo) {
    case (u8)dItemNo_BOMB_BAG_LV1_e:
        dMeter2_clearItemFromAllSlots((u8)dItemNo_NORMAL_BOMB_e);
        dComIfGs_offItemFirstBit((u8)dItemNo_NORMAL_BOMB_e);
        break;
    case (u8)dItemNo_BOMB_BAG_LV2_e:
        dMeter2_clearItemFromAllSlots((u8)dItemNo_WATER_BOMB_e);
        dComIfGs_offItemFirstBit((u8)dItemNo_WATER_BOMB_e);
        break;
    case (u8)dItemNo_COPY_ROD_e:
        dMeter2_clearItemFromAllSlots((u8)dItemNo_COPY_ROD_2_e);
        dComIfGs_offItemFirstBit((u8)dItemNo_COPY_ROD_2_e);
        break;
    default:
        break;
    }
}

bool dMeter2_playerOwnsRentalItem(u8 itemNo) {
    if (dMeter2_isShieldItem(itemNo)) {
        return dComIfGs_getSelectEquipShield() == itemNo;
    }
    // ============================================
    // MODIFIED CODE — ALBW Port (Outfit rental)
    // Clothes items (Magic Armor, Ordon/casual wear) have no inventory slot —
    // they count as "owned" only while currently equipped, so the shop hides
    // them while worn and re-offers them after the player switches outfits.
    // ============================================
    if ((itemNo == (u8)dItemNo_ARMOR_e || itemNo == (u8)dItemNo_WEAR_CASUAL_e) &&
        dComIfGs_getSelectEquipClothes() == itemNo) {
        return true;
    }
    // ============================================
    // MODIFIED CODE ENDS HERE
    // ============================================
    return dMeter2_slotHasPossessionForm(itemNo);
}

void dMeter2_stripRentalItemOnDeath(u8 itemNo) {
    if (!dMeter2_playerOwnsRentalItem(itemNo)) {
        return;
    }
    dMeter2_onALBWRentalEligible(itemNo);
    if (itemNo == (u8)dItemNo_ARMOR_e) {
        dComIfGs_offItemFirstBit(itemNo);
        if (dComIfGs_getSelectEquipClothes() == itemNo) {
            dComIfGs_setSelectEquipClothes((u8)dItemNo_WEAR_KOKIRI_e);
        }
        return;
    }
    dMeter2_clearAllPossessionForms(itemNo);
}

void dMeter2_stripAllALBWInventoryOnDeath() {
    for (int i = 0; i < 12; i++) {
        dMeter2_stripRentalItemOnDeath(sALBWItemNos[i]);
    }

    if (dComIfGs_isItemFirstBit((u8)dItemNo_DEITY_ARMOR_e)) {
        dComIfGs_offItemFirstBit((u8)dItemNo_DEITY_ARMOR_e);
    }
}

static const u8 sShieldRentalItemNos[3] = {
    (u8)dItemNo_WOOD_SHIELD_e,
    (u8)dItemNo_SHIELD_e,
    (u8)dItemNo_HYLIA_SHIELD_e,
};
static constexpr int kShieldRentalEligibleBase = 685; // saveBitLabels[685..687]

bool dMeter2_isShieldItem(u8 itemNo) {
    return itemNo == (u8)dItemNo_WOOD_SHIELD_e || itemNo == (u8)dItemNo_SHIELD_e ||
           itemNo == (u8)dItemNo_HYLIA_SHIELD_e;
}

bool dMeter2_playerHasAnyShield() {
    if (dComIfGs_getSelectEquipShield() != dItemNo_NONE_e) {
        return true;
    }
    return dComIfGs_isItemFirstBit((u8)dItemNo_WOOD_SHIELD_e) ||
           dComIfGs_isItemFirstBit((u8)dItemNo_SHIELD_e) ||
           dComIfGs_isItemFirstBit((u8)dItemNo_HYLIA_SHIELD_e);
}

static void dMeter2_clearAllShieldPossession() {
    dComIfGs_offItemFirstBit((u8)dItemNo_WOOD_SHIELD_e);
    dComIfGs_offItemFirstBit((u8)dItemNo_SHIELD_e);
    dComIfGs_offItemFirstBit((u8)dItemNo_HYLIA_SHIELD_e);
    dMeter2Info_setShield(dItemNo_NONE_e, false);
}

bool dMeter2_canAcquireShield(u8 itemNo) {
    if (!dMeter2_isShieldItem(itemNo)) {
        return true;
    }
    if (!dMeter2_playerHasAnyShield()) {
        return true;
    }

    const u8 equipped = dComIfGs_getSelectEquipShield();
    if (equipped == itemNo) {
        return true;
    }

    // One shield at a time until a future upgrade path allows multiples.
    return false;
}

void dMeter2_onShieldDestroyedForRental(u8 itemNo) {
    for (int i = 0; i < 3; i++) {
        if (sShieldRentalItemNos[i] == itemNo) {
            dComIfGs_onEventBit(dSv_event_flag_c::saveBitLabels[kShieldRentalEligibleBase + i]);
            OS_REPORT("[dMeter2] shield rental eligible item=%u bit=%d\n", itemNo,
                      kShieldRentalEligibleBase + i);
            return;
        }
    }
}

bool dMeter2_isShieldRentalEligible(u8 itemNo) {
    for (int i = 0; i < 3; i++) {
        if (sShieldRentalItemNos[i] == itemNo) {
            return dComIfGs_isEventBit(dSv_event_flag_c::saveBitLabels[kShieldRentalEligibleBase + i]);
        }
    }
    return false;
}

void dMeter2_grantRentalShield(u8 itemNo) {
    if (!dMeter2_isShieldItem(itemNo) || !dMeter2_canAcquireShield(itemNo)) {
        return;
    }

    if (dMeter2_playerHasAnyShield()) {
        dMeter2_clearAllShieldPossession();
    }

    switch (itemNo) {
    case (u8)dItemNo_WOOD_SHIELD_e:
        dComIfGs_setCollectShield(COLLECT_WOODEN_SHIELD);
        break;
    case (u8)dItemNo_SHIELD_e:
        dComIfGs_setCollectShield(COLLECT_ORDON_SHIELD);
        break;
    case (u8)dItemNo_HYLIA_SHIELD_e:
        dComIfGs_setCollectShield(COLLECT_HYLIAN_SHIELD);
        break;
    default:
        break;
    }

    dComIfGs_onItemFirstBit(itemNo);
    dMeter2Info_setShield(itemNo, false);
}

// ============================================
// NEW CODE — ALBW Port (Outfit / clothes rental)
// "Worn once" gate for the Ordon outfit: the player wears the casual clothes
// throughout the intro until escaping the Hyrule Castle Sewers (saveBitLabels[47]
// = "Escaped Sewers", after which the game forces Hero's clothes — see
// daAlink_c::initClothes).  So that bit being set means the Ordon outfit has been
// worn at least once.  True ALBW bypasses this gate (entry is not alwaysGated).
// ============================================
bool dMeter2_isCasualWearEligible() {
    return dComIfGs_isEventBit(dSv_event_flag_c::saveBitLabels[47]);
}

// Grant + auto-equip a clothes item and trigger the model swap, mirroring the
// vanilla collection-menu equip path (setSelectEquipClothes + setClothesChange).
void dMeter2_grantRentalClothes(u8 itemNo) {
    dComIfGs_onItemFirstBit(itemNo);
    dComIfGs_setSelectEquipClothes(itemNo);
    dComIfGp_setSelectEquipClothes(itemNo);
    if (daPy_getPlayerActorClass() != NULL) {
        daPy_getPlayerActorClass()->setClothesChange(0);
    }
}
// ============================================
// NEW CODE ENDS HERE
// ============================================

#if TARGET_PC
static bool dMeter2_hideHudForALBWShop() {
    return dALBWRental_isOpen();
}
#endif
// ============================================
// NEW CODE ENDS HERE
// ============================================
// ============================================
// NEW CODE ENDS HERE
// ============================================
#endif
// ============================================
// NEW CODE ENDS HERE
// ============================================

int dMeter2_c::_create() {
    stage_stag_info_class* stag_info = dComIfGp_getStageStagInfo();
    if (dStage_stagInfo_GetUpButton(stag_info) == 1) {
        mpHeap = fopMsgM_createExpHeap(HEAP_SIZE(0x5A400, 0xA0000), NULL);
    } else {
        mpHeap = fopMsgM_createExpHeap(HEAP_SIZE(0x60800, 0xC1000), NULL);
    }
    JKRHEAP_NAME(mpHeap, "dMeter2_c");

    JKRHeap* heap = mDoExt_setCurrentHeap(mpHeap);
    mpHeap->getTotalFreeSize();

    if (!strcmp(dComIfGp_getStartStageName(), "F_SP00")) {
        dMeter2Info_setNowCount(0);
        dMeter2Info_setMaxCount(0);
    }

    field_0x128 = 0;
    field_0x12c = field_0x128;
    mStatus = 0;
    mSubContentType = 0;
    mSubContentsStringType = 0;
    field_0x1e6 = 0;
    field_0x1e7 = 0;

    mItemMaxNum[ARROW_MAX] = dComIfGs_getArrowMax();
    mArrowNum = dComIfGs_getArrowNum();
    mItemMaxNum[PACHINKO_MAX] = dComIfGs_getPachinkoMax();
    mPachinkoNum = dComIfGs_getPachinkoNum();

    for (int i = 0; i < 3; i++) {
        mBombMax[i] = dComIfGs_getBombMax(dComIfGs_getItem((u8)(i + SLOT_15), 1));
        mBombNum[i] = dComIfGs_getBombNum(i);
    }

    for (int i = 0; i < 2; i++) {
        mItemMaxNum[i] = dComIfGs_getSelectItemIndex(i);
    }

    for (int i = 0; i < 4; i++) {
        mBottleNum[i] = dComIfGs_getBottleNum(i);
    }

    field_0x1e8 = 0;
    field_0x1e9 = 0;
    mRupeeSound = 0;
    mArrowSound = 0;

    for (int i = 0; i < 5; i++) {
        field_0x1b8[i] = 0;
    }

    field_0x1ec = 0;
    field_0x1ed = 0;
    mLifeCountType = 0;

    mNowLifeGauge = dComIfGs_getLife();
    dComIfGp_setItemNowLife((u8)mNowLifeGauge);

    mMaxLife = dComIfGs_getMaxLife();

    mNowMagic = dComIfGs_getMagic();
    dComIfGp_setItemNowMagic(mNowMagic);

    mMaxMagic = dComIfGs_getMaxMagic();

    mNowOil = (s16)dComIfGs_getOil();
    dComIfGp_setItemNowOil(mNowOil);

    mMaxOil = (s16)dComIfGs_getMaxOil();

    mNowOxygen = dComIfGp_getOxygen();
    dComIfGp_setNowOxygen(mNowOxygen);

    mMaxOxygen = dComIfGp_getMaxOxygen();

    field_0x130 = 0.0f;

    u8 dark_area = dComIfGp_getStartStageDarkArea();
    mLightDropNum = dComIfGs_getLightDropNum(dark_area);

    mNeedLightDropNum = dComIfGp_getNeedLightDropNum();

    mRupeeNum = dComIfGs_getRupee();
    mKeyNum = dComIfGs_getKeyNum();

    mDoStatus = dComIfGp_getDoStatus();
    mDoSetFlag = dComIfGp_isDoSetFlag(2);

    for (int i = 0; i < 2; i++) {
        mButtonATalkPosX[i] = 0.0f;
        mButtonATalkPosY[i] = 0.0f;
    }
    field_0x144 = 1.0f;

    mAStatus = dComIfGp_getAStatus();
    field_0x1c6 = 0;
    mASetFlag = dComIfGp_isASetFlag(2);

    for (int i = 0; i < 2; i++) {
        field_0x148[i] = 0.0f;
        field_0x150[i] = 0.0f;
    }
    field_0x158 = 1.0f;

    field_0x1e4 = 0;
    mEquipSword = dComIfGs_getSelectEquipSword();
    field_0x1e3 = 0;

    field_0x1fe = 0;
    field_0x1fd = 0;
    field_0x1ff = 0;
    field_0x200 = 0;
    field_0x201 = 0;

    mCollectSmell = dComIfGs_getCollectSmell();
    mRStatus = dComIfGp_getRStatus();
    mRSetFlag = dComIfGp_isRSetFlag(2);
    mXSetFlag = dComIfGp_isXSetFlag(2);
    mYSetFlag = dComIfGp_isYSetFlag(2);

    for (int i = 0; i < 2; i++) {
        dComIfGp_setSelectItem(i);
    }

#if TARGET_PC
    s_zSlotItemCache = 0xFF;
    s_extraItemSlotCache = dusk::isExtraItemSlotEnabled();
    if (s_extraItemSlotCache) {
        dComIfGp_setSelectItem(SELECT_ITEM_DOWN);
        s_zSlotItemCache = dComIfGp_getSelectItem(SELECT_ITEM_DOWN);
    }
#endif

    mItemStatus[X_ITEM] = dComIfGp_getSelectItem(0);
    mItemStatus[Y_ITEM] = dComIfGp_getSelectItem(1);
    mItemStatus[X_STATUS] = dComIfGp_getXStatus();
    mItemStatus[Y_STATUS] = dComIfGp_getYStatus();
    field_0x188 = 0.0f;
    field_0x18c = 0.0f;

    for (int i = 0; i < 2; i++) {
        field_0x1d6[i] = dMeter2Info_isDirectUseItem(i);
        field_0x1d8[i] = dComIfGp_getSelectItemNum(i);
    }

    field_0x1e1 = 0;
    field_0x1b4 = 0;
    field_0x15c = 0.0f;

    for (int i = 0; i < 4; i++) {
        field_0x160[i] = 0.0f;
        field_0x174[i] = 0.0f;
    }
    field_0x190 = 0;

    mZStatus = dComIfGp_getZStatus();
    m3DStatus = dComIfGp_get3DStatus();
    mCStickStatus = dComIfGp_getCStickStatus();
    mSButtonStatus = dComIfGp_getSButtonStatus();
    mNunStatus = dComIfGp_getNunStatus();
    mRemoConStatus = dComIfGp_getRemoConStatus();
    mNunZStatus = dComIfGp_getNunZStatus();
    mNunCStatus = dComIfGp_getNunCStatus();
    mBottleStatus = dComIfGp_getBottleStatus();

    field_0x1ac = dMeter2Info_isUseButton(16);
    field_0x19a = 0;

    mpMeterDraw = JKR_NEW dMeter2Draw_c(mpHeap);

    field_0x130 = mpMeterDraw->getNowLightDropRateCalc();
    mpHeap->getTotalFreeSize();

    for (int i = 0; i < 2; i++) {
        if (field_0x128 == 0) {
            if (mItemStatus[i * 2] == dItemNo_BOMB_BAG_LV1_e || mItemStatus[i * 2] == dItemNo_NORMAL_BOMB_e ||
                mItemStatus[i * 2] == dItemNo_WATER_BOMB_e || mItemStatus[i * 2] == dItemNo_POKE_BOMB_e)
            {
                mpMeterDraw->setItemNum(i, dComIfGp_getSelectItemNum(i),
                                        dComIfGp_getSelectItemMaxNum(i));
            } else if (mItemStatus[i * 2] == dItemNo_BEE_CHILD_e) {
                mpMeterDraw->setItemNum(i, dComIfGp_getSelectItemNum(i),
                                        dComIfGp_getSelectItemMaxNum(i));
            } else if (mItemStatus[i * 2] == dItemNo_BOW_e || mItemStatus[i * 2] == dItemNo_LIGHT_ARROW_e ||
                       mItemStatus[i * 2] == dItemNo_ARROW_LV1_e || mItemStatus[i * 2] == dItemNo_ARROW_LV2_e ||
                       mItemStatus[i * 2] == dItemNo_ARROW_LV3_e || mItemStatus[i * 2] == dItemNo_HAWK_ARROW_e)
            {
                mpMeterDraw->setItemNum(i, mArrowNum, dComIfGs_getArrowMax());
            } else if (mItemStatus[i * 2] == dItemNo_PACHINKO_e) {
                mpMeterDraw->setItemNum(i, mPachinkoNum, dComIfGs_getPachinkoMax());
            } else if (mItemStatus[i * 2] == dItemNo_BOMB_ARROW_e) {
                u8 item_num = dComIfGp_getSelectItemNum(i);
                u8 item_max = dComIfGp_getSelectItemMaxNum(i);
                if (item_num > mArrowNum) {
                    item_num = mArrowNum;
                }
                u8 temp = dComIfGs_getArrowMax() & 0xFF;
                if (item_max < temp) {
                    item_max = temp;
                }
                mpMeterDraw->setItemNum(i, item_num, item_max);
            }
        }
    }
   
    mpMap = NULL;
    if (dMeterMap_c::isEnableDispMapAndMapDispSizeTypeNo()) {
        mpMap = JKR_NEW dMeterMap_c(mpMeterDraw->getMainScreenPtr());
    } else {
        if (g_meter2_info.mMapStatus == 2) {
            g_meter2_info.mMapStatus = 0;
        }
    }
    dMeter2Info_setMeterMapClass(mpMap);

    mpHeap->getTotalFreeSize();
    mpSubHeap = fopMsgM_createExpHeap(HEAP_SIZE(0x5000, 0x6500), mpHeap);
    JKRHEAP_NAME(mpSubHeap, "dMeter2_c mpSubHeap");
    field_0x108 = NULL;
    mpSubContents = NULL;
    mpSubSubContents = NULL;
    mpEmpButton = NULL;

    mpHeap->getTotalFreeSize();
    field_0x11c = NULL;
    mDoExt_setCurrentHeap(heap);
    return cPhs_COMPLEATE_e;
}

int dMeter2_c::_execute() {
    JKRHeap* heap = mDoExt_setCurrentHeap(mpHeap);

    if (!dComIfGs_isCollectMirror(0)
           /* dSv_event_flag_c::F_0685 - Cutscene - (Cutscene 32) Sage appears, get first Mirror of Twilight shard */
        && dComIfGs_isEventBit(dSv_event_flag_c::F_0685)) {
        dComIfGs_onCollectMirror(0);
    }

    if (!dComIfGs_isCollectCrystal(3)
           /* dSv_event_flag_c::F_0686 - Palace of Twilight - Get fused shadow piece (final mask) */
        && dComIfGs_isEventBit(dSv_event_flag_c::F_0686)) {
        dComIfGs_onCollectCrystal(3);
    }

    checkStatus();
    mpMeterDraw->exec(mStatus);

    moveLife();
    moveKantera();
    moveOxygen();
    moveLightDrop();
    moveRupee();
    moveKey();
#if TARGET_PC
    // ============================================
    // NEW CODE — ALBW Port (Lies of Link HUD)
    // The A/B/R/Z and X/Y button groups only redraw on a status change, so flipping
    // the layout toggle wouldn't restore the vanilla ring (nor re-apply the LoP
    // trim). Invalidate their cached statuses on toggle change to force one redraw;
    // each move function self-corrects its cache the same frame.
    // ============================================
    {
        static bool sButtonsLopWasOn = false;
        const bool lopOn = dusk::getSettings().game.lopHud.getValue();
        if (sButtonsLopWasOn != lopOn) {
            sButtonsLopWasOn = lopOn;
            // Show every ring child first (restores the vine + decorations the LoP
            // sweep hid — vanilla's button redraw doesn't manage those), then force
            // the button groups to redraw so they re-apply correct visibility.
            mpMeterDraw->lopRestoreButtonRing();
            mDoStatus = 0xFF;
            mAStatus = 0xFF;
            mRStatus = 0xFF;
            mZStatus = 0xFF;
            mItemStatus[0] = 0xFF;
            mItemStatus[1] = 0xFF;
            mItemStatus[2] = 0xFF;
            mItemStatus[3] = 0xFF;
        }
    }
    // ============================================
    // NEW CODE ENDS HERE
    // ============================================
#endif
    moveButtonXY();
    moveButtonA();
    moveButtonB();
    moveButtonR();
    moveButtonZ();
    moveButton3D();
    moveButtonC();
    moveButtonS();
    moveButtonCross();
    mpMeterDraw->setButtonIconMidonaAlpha(mStatus);
    moveTouchSubMenu();
    moveBombNum();
    moveArrowNum();
    movePachinkoNum();
    moveBottleNum();

    if (mpMap != NULL) {
        mpMap->_move(mStatus);
    } else {
        dMeterMap_c::meter_map_move(mStatus);
    }

    moveSubContents();
    move2DContents();

    if (!dComIfGp_isPauseFlag() && !dComIfGp_event_runCheck()) {
        dMeter2Info_decHotSpringTimer();
    }

    dMeter2Info_allUseButton();
    dMeter2Info_offUseButton(0x800);
#if TARGET_PC
    if (dusk::isExtraItemSlotEnabled() && !daPy_getPlayerActorClass()->checkWolf()) {
        const u8 zItem = dComIfGp_getSelectItem(SELECT_ITEM_DOWN);
        if (zItem != dItemNo_NONE_e && zItem != 0) {
            dMeter2Info_onUseButton(METER2_USEBUTTON_Z);
        }
    }
#endif
    dMeter2Info_resetGameStatus();
    dComIfGp_setNunStatus(0, 0, 0);
    dComIfGp_setRemoConStatus(0, 0, 0);
    dComIfGp_setNunZStatus(0, 0);
    dComIfGp_setNunCStatus(0, 0);
    dComIfGp_setBottleStatus(0, 0);
    dComIfGp_setCStickStatus(0, 0, 0);

    mDoExt_setCurrentHeap(heap);
    return 1;
}

int dMeter2_c::_draw() {
    #if TARGET_PC
    if (dusk::getSettings().game.recordingMode || dusk::getSettings().game.minimalHUD ||
        dusk::getSettings().game.debugFlyCam)
    {
        return 1;
    }
    #endif

    if (mpMap != NULL) {
        mpMap->_draw();
    }

    if (mpSubContents != NULL) {
        dComIfGd_set2DOpaTop(mpSubContents);
    }

    if (mpSubSubContents != NULL) {
        if (mSubContentType == 5) {
            if (mSubContentsStringType != 0) {
                dComIfGd_set2DOpaTop(mpSubSubContents);
            }
        } else {
            dComIfGd_set2DOpaTop(mpSubSubContents);
        }
    }

    if (dMeter2Info_getWindowStatus() == 2) {
        dComIfGd_set2DOpa(mpMeterDraw);
    } else {
        dComIfGd_set2DOpaTop(mpMeterDraw);
    }

    if (mpEmpButton != NULL) {
        dComIfGd_set2DOpaTop(mpEmpButton);
    }

    return 1;
}

int dMeter2_c::_delete() {
    mpHeap->getTotalFreeSize();
    JKRHeap* heap = mDoExt_setCurrentHeap(mpHeap);

    if (isRupeeSoundBit(2)) {
        mDoAud_seStart(Z2SE_LUPY_INC_CNT_2, 0, 0, 0);
    }

    if (isRupeeSoundBit(3)) {
        mDoAud_seStart(Z2SE_LUPY_DEC_CNT_2, 0, 0, 0);
    }

    if (isArrowSoundBit(2) && (isArrowEquip() || isPachinkoEquip()) &&
        mpMeterDraw->isButtonVisible())
    {
        mDoAud_seStart(Z2SE_CONSUM_INC_CNT_2, 0, 0, 0);
    }

    mpHeap->getTotalFreeSize();
    if (mpMap != NULL) {
        JKR_DELETE(mpMap);
        mpMap = NULL;
    }

    mpHeap->getTotalFreeSize();
    mDoExt_setCurrentHeap(mpSubHeap);
    if (mpSubContents != NULL) {
        JKR_DELETE(mpSubContents);
        mpSubContents = NULL;
    }

    if (mpSubSubContents != NULL) {
        JKR_DELETE(mpSubSubContents);
        mpSubSubContents = NULL;
    }

    mpHeap->getTotalFreeSize();
    if (field_0x11c != NULL) {
        JKR_DELETE(field_0x11c);
        field_0x11c = NULL;
    }

    mDoExt_setCurrentHeap(heap);
    JKR_DELETE(mpMeterDraw);
    mpMeterDraw = NULL;

    fopMsgM_destroyExpHeap(mpSubHeap);
    fopMsgM_destroyExpHeap(mpHeap);
    emphasisButtonDelete();
    return 1;
}

int dMeter2_c::emphasisButtonDelete() {
    if (mpEmpButton != NULL) {
        JKRExpHeap* heap = dComIfGp_getSubHeap2D(8);
        mpEmpButton->hideAll();

        if (heap != NULL) {
            JKR_DELETE(mpEmpButton);
            mpEmpButton = NULL;
            heap->freeAll();
            dComIfGp_offHeapLockFlag(8);
        }
    }

    return 1;
}

void dMeter2_c::setLifeZero() {
    dComIfGs_setLife(1);
    setNowLifeGauge(1);
    dComIfGp_setItemLifeCount(-1.0f, 0);
}

void dMeter2_c::checkStatus() {
    mStatus = 0;

    field_0x12c = field_0x128;

    field_0x128 = daPy_py_c::checkNowWolf();

    if (!dComIfGp_2dShowCheck() || dMsgObject_getMsgObjectClass()->isPlaceMessage()) {
        mStatus |= 0x4000;
    } else if (dComIfGp_checkPlayerStatus1(0, 1) && dComIfGp_getAStatus() == 0x12) {
        mStatus |= 0x200000;
    } else if (dComIfGp_event_runCheck()) {
        mStatus |= 0x40;

        if (dDemo_c::getMode() != 1 && dComIfGp_isHeapLockFlag() != 6 &&
            dMsgObject_isTalkNowCheck() && !dMeter2Info_isFloatingMessageVisible())
        {
            mStatus |= 0x100;
            dComIfGp_setAStatus(0, 0);
            dComIfGp_setRStatus(0, 0);
        }
    }

    if (dComIfGp_isHeapLockFlag() != 6 && !(mStatus & 0x100) && dMsgObject_isTalkNowCheck() &&
        !dMeter2Info_isFloatingMessageVisible())
    {
        mStatus |= 0x100;
        dComIfGp_setAStatus(0, 0);
        dComIfGp_setRStatus(0, 0);
    }

    if (!dComIfGp_isPauseFlag()) {
        if (mpMeterDraw->getCameraSubject()) {
            mStatus |= 0x40000000;
        } else if (mpMeterDraw->getItemSubject()) {
            mStatus |= 0x1000;
        }

        if (mpMeterDraw->getPlayerSubject()) {
            mStatus |= 0x100000;
        }

        if (dComIfGp_getCameraAttentionStatus(0) & 0x40000) {
            mStatus |= 0x80000000;
            dComIfGp_setDoStatusForce(0x12, 2);
        }

        s16 sp8;
        s16 spA;

        if (dComIfGp_checkCameraAttentionStatus(0, 8)) {
            mStatus |= 0x80;
        } else if (dComIfGp_checkCameraAttentionStatus(dComIfGp_getPlayerCameraID(0), 0x10) &&
                   dCam_getBody()->CalcSubjectAngle(&sp8, &spA))
        {
            mStatus |= 0x20000000;
        } else if (daPy_getPlayerActorClass()->checkCanoeRide()) {
            mStatus |= 0x400;
        } else if (dComIfGp_checkPlayerStatus0(0, 0x8000000) ||
                   daPy_getPlayerActorClass()->checkSpinnerRide())
        {
            mStatus |= 0x200;
        } else if (dComIfGp_checkPlayerStatus0(0, 0x800000)) {
            mStatus |= 0x800;
        } else if (dComIfGp_checkPlayerStatus0(0, 0x100000)) {
            mStatus |= 0x2000;
        } else if (dComIfGp_checkPlayerStatus0(0, 0x2000108)) {
            mStatus |= 0x8000;
        } else if (dComIfGp_checkPlayerStatus0(0, 0x4000000)) {
            mStatus |= 0x10000;
        } else if (daPy_getPlayerActorClass()->checkHorseRideNotReady() &&
                   dComIfGp_getHorseActor() != NULL &&
                   !dComIfGp_getHorseActor()->checkRodeoMode())
        {
            mStatus |= 0x2000000;
        }
    }

    switch (dMeter2Info_getWindowStatus()) {
    case 1:
        mStatus |= 0x8;
        break;
    case 3:
    case 10:
        mStatus |= 0x10;
        break;
    case 2:
        mStatus |= 0x1000000;
        dComIfGp_setAStatus(0x12, 0);
        dComIfGp_setDoStatus(0, 0);
        dComIfGp_setRStatus(0, 0);
        break;
    case 4:
    case 6:
        mStatus |= 0x20;
        dComIfGp_setAStatus(0x12, 0);
        dComIfGp_setDoStatus(0, 0);
        dComIfGp_setRStatus(0, 0);
        break;
    case 5:
        mStatus |= 0x4000000;
        dComIfGp_setAStatus(0x12, 0);
        dComIfGp_setDoStatus(0, 0);
        dComIfGp_setRStatus(0, 0);
        break;
    case 7:
        mStatus |= 0x8000000;
        dComIfGp_setAStatus(0x12, 0);
        dComIfGp_setDoStatus(0x22, 0);
        dComIfGp_setRStatus(0, 0);
        break;
    case 8:
        mStatus |= 0x10000000;
        dComIfGp_setAStatus(0, 0);
        dComIfGp_setDoStatus(0, 0);
        dComIfGp_setRStatus(0, 0);
        break;
    }

    switch (dStage_stagInfo_GetSTType(dComIfGp_getStage()->getStagInfo())) {
    case ST_DUNGEON:
        mStatus |= 0x4;
        break;
    case ST_ROOM:
        mStatus |= 0x2;
        break;
    case ST_FIELD:
    default:
        mStatus |= 0x1;
        break;
    }
}

void dMeter2_c::moveLife() {
    s16 life_count = 0;
    bool draw_life = false;

    if (dComIfGp_getItemMaxLifeCount() != 0) {
        s16 max_count = dComIfGs_getMaxLife() + dComIfGp_getItemMaxLifeCount();
        if (max_count > 100) {
            max_count = 100;
        } else if (max_count < 15) {
            max_count = 15;
        }

        dComIfGs_setMaxLife(max_count);
#if TARGET_PC
        if (dAlbwMQ_isEnabled()) {
            life_count = static_cast<s16>(dComIfGs_getMaxLifeGauge());
        } else
#endif
        {
            life_count = (max_count / 5) * 4;
        }

        s16 current_life = life_count - dComIfGs_getLife();
        dComIfGp_setItemLifeCount(current_life, 0);
        dComIfGp_clearItemMaxLifeCount();
        draw_life = true;
    }

    f32 item_life_count = dComIfGp_getItemLifeCount();
    f32 tmp = 0.0f;
    if (item_life_count != tmp) {
        mLifeCountType = dComIfGp_getItemLifeCountType();
        if (!draw_life) {
#if TARGET_PC
            if (dAlbwMQ_isEnabled()) {
                life_count = static_cast<s16>(dComIfGs_getMaxLifeGauge());
            } else
#endif
            {
                life_count = (dComIfGs_getMaxLife() / 5) * 4;
            }
        }

        s16 new_life = dComIfGs_getLife() + dComIfGp_getItemLifeCount();
        if (new_life > life_count) {
            new_life = life_count;
        } else if (new_life < 0) {
            new_life = 0;
        }

        dComIfGs_setLife((u8)new_life);
        dComIfGp_clearItemLifeCount();

        if (mNowLifeGauge == dComIfGs_getLife() && mLifeCountType != 0) {
            mLifeCountType = 0;
        }
        draw_life = true;
    }

    u16 max_life = dComIfGs_getMaxLife();
#if TARGET_PC
    if (dAlbwMQ_isEnabled()) {
        max_life = dAlbwMQ_getDisplayMaxLifeInternal();
    }
#endif
    if (mMaxLife != max_life) {
        if (mMaxLife < max_life) {
            mMaxLife++;
            dMeter2Info_onLifeGaugeSE();
            draw_life = true;
        } else if (mMaxLife > max_life) {
            mMaxLife--;
            draw_life = true;
        }
    }

    u16 current_life = dComIfGs_getLife();
    if (mNowLifeGauge != current_life) {
        if (mNowLifeGauge < current_life) {
            mNowLifeGauge++;
            if (dComIfGp_checkPlayerStatus1(0, 0x2000) ||
                dComIfGp_checkPlayerStatus0(0, 0x20000000) || dMeter2Info_getLifeGaugeSE())
            {
                if (mNowLifeGauge % 4 == 0) {
                    mDoAud_seStart(Z2SE_HP_GAUGE_INC, 0, 0, 0);
                }
            } else if (mLifeCountType == 1) {
                mDoAud_seStart(Z2SE_HP_GAUGE_INC, 0, 0, 0);
            }

            u16 life = dComIfGs_getLife();
            if (mNowLifeGauge == life && mLifeCountType != 0) {
                mLifeCountType = 0;
            }
            draw_life = true;
        } else if (mNowLifeGauge > current_life) {
            mNowLifeGauge--;
            if (mLifeCountType != 0) {
                mLifeCountType = 0;
            }
            draw_life = true;
        }
    } else if (dMeter2Info_getLifeGaugeSE()) {
        dMeter2Info_offLifeGaugeSE();
    }

    f32 life_gauge_x = g_drawHIO.mLifeGaugePosX;
    f32 life_gauge_y = g_drawHIO.mLifeGaugePosY;

    if (mLifeGaugePosX != g_drawHIO.mLifeGaugePosX) {
        mLifeGaugePosX = g_drawHIO.mLifeGaugePosX;
        draw_life = true;
    }

    if (mLifeGaugePosY != g_drawHIO.mLifeGaugePosY) {
        mLifeGaugePosY = g_drawHIO.mLifeGaugePosY;
        draw_life = true;
    }

#if TARGET_PC
    const f32 lifeGaugeScale =
        g_drawHIO.mLifeParentScale *
        std::clamp(dusk::getSettings().game.hudScale.getValue(), 0.5f, 2.0f);
#else
    const f32 lifeGaugeScale = g_drawHIO.mLifeParentScale;
#endif
    if (mLifeGaugeScale != lifeGaugeScale) {
        mLifeGaugeScale = lifeGaugeScale;
        draw_life = true;
    }

    if (mHeartScale != g_drawHIO.mHeartMarkScale) {
        mHeartScale = g_drawHIO.mHeartMarkScale;
        draw_life = true;
    }

    if (mLargeHeartScale != g_drawHIO.mBigHeartScale) {
        mLargeHeartScale = g_drawHIO.mBigHeartScale;
        draw_life = true;
    }

    if (draw_life == true) {
        mpMeterDraw->drawLife(mMaxLife, mNowLifeGauge, life_gauge_x, life_gauge_y);
    }

    alphaAnimeLife();
    dComIfGp_setItemNowLife((u8)mNowLifeGauge);
}

void dMeter2_c::moveKantera() {
    s32 var_r0;
    s32 var_r7;
    bool draw_kantera;

    s32 max_oil = dComIfGs_getMaxOil();
    var_r7 = 0;
    draw_kantera = false;

    if (dComIfGp_getItemMaxOilCount() != 0) {
        var_r7 = dComIfGs_getMaxOil() + dComIfGp_getItemMaxOilCount();
        if (var_r7 > max_oil) {
            var_r7 = max_oil;
        } else if (var_r7 < 0) {
            var_r7 = 0;
        }

        dComIfGs_setMaxOil(var_r7);
        dComIfGp_setItemOilCount(var_r7 - dComIfGs_getOil());
        dComIfGp_clearItemMaxOilCount();
        draw_kantera = true;
    }

    if (dComIfGp_getItemOilCount() != 0) {
        if (draw_kantera == 0) {
            var_r7 = dComIfGs_getMaxOil();
        }

        var_r0 = dComIfGs_getOil() + dComIfGp_getItemOilCount();
        if (var_r0 > var_r7) {
            var_r0 = var_r7;
        } else if (var_r0 < 0) {
            var_r0 = 0;
        }

        dComIfGs_setOil(var_r0);
#if TARGET_PC
        if (dComIfGp_getItemOilCount() < 0) {
            dShield_repairDurabilityFraction(1, 5);
        }
#endif
        dComIfGp_clearItemOilCount();
        draw_kantera = true;
    }

    if (mMaxOil != dComIfGs_getMaxOil()) {
        if (mMaxOil < dComIfGs_getMaxOil()) {
            mMaxOil += 200;
            if (mMaxOil > dComIfGs_getMaxOil()) {
                mMaxOil = dComIfGs_getMaxOil();
            }
            draw_kantera = true;
        } else if (mMaxOil > dComIfGs_getMaxOil()) {
            mMaxOil -= 200;
            if (mMaxOil < dComIfGs_getMaxOil()) {
                mMaxOil = dComIfGs_getMaxOil();
            }
            draw_kantera = true;
        }
    }

#if !TARGET_PC
    // ============================================
    // ORIGINAL CODE — Console only
    // On PC this block is bypassed entirely because
    // mNowOil is controlled by sALBWMeter in our
    // TARGET_PC block below. Keeping this running
    // on PC caused the recovery sound to trigger
    // and overwrote our meter value every frame.
    // ============================================
    if (mNowOil != dComIfGs_getOil()) {
        if (mNowOil < dComIfGs_getOil()) {
            mNowOil += 200;
            if (mNowOil > dComIfGs_getOil()) {
                mNowOil = dComIfGs_getOil();
            }

            if (!dComIfGp_getOxygenShowFlag() && mpMeterDraw->getMeterGaugeAlphaRate(1) > 0.0f) {
                Z2GetAudioMgr()->seStartLevel(
                    Z2SE_OIL_METER_RECOVER, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
            }
            draw_kantera = true;
        } else if (mNowOil > dComIfGs_getOil()) {
            mNowOil -= 200;
            if (mNowOil < dComIfGs_getOil()) {
                mNowOil = dComIfGs_getOil();
            }

            if (mNowOil == 0) {
                if (mpMeterDraw->getMeterGaugeAlphaRate(1) > 0.0f) {
                    Z2GetAudioMgr()->seStart(
                        Z2SE_OIL_METER_FINISH, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
                }
            } else if (((f32)dComIfGs_getOil() / (f32)dComIfGs_getMaxOil()) <= 0.1f &&
                       mpMeterDraw->getMeterGaugeAlphaRate(1) > 0.0f)
            {
                Z2GetAudioMgr()->seStartLevel(
                    Z2SE_OIL_METER_LESS, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
            }

            draw_kantera = true;
        }
    }
#endif
    // ============================================
    // ORIGINAL CODE ENDS HERE
    // ============================================

    f32 x_pos = g_drawHIO.mLanternMeterPosX;
    f32 y_pos = g_drawHIO.mLanternMeterPosY;

    if (field_0x246 != mMaxLife) {
        field_0x246 = mMaxLife;
        draw_kantera = true;
    }

    if (mLanternMeterScale != g_drawHIO.mLanternMeterScale) {
        mLanternMeterScale = g_drawHIO.mLanternMeterScale;
        draw_kantera = true;
    }

    if (mLanternMeterPosX != g_drawHIO.mLanternMeterPosX) {
        mLanternMeterPosX = g_drawHIO.mLanternMeterPosX;
        draw_kantera = true;
    }

    if (mLanternMeterPosY != g_drawHIO.mLanternMeterPosY) {
        mLanternMeterPosY = g_drawHIO.mLanternMeterPosY;
        draw_kantera = true;
    }

#if TARGET_PC
    // ============================================
    // NEW CODE — ALBW Port
    // Source: meterValuee function in ALBW main.cpp
    // Moved before drawKantera so mNowOil is correct
    // when the meter visual is updated this frame
    // ============================================
    {
        auto sNow = std::chrono::steady_clock::now();
        // ============================================
        // NEW CODE — ALBW Port
        // Freeze recovery/drain timers while paused or during the game-over
        // screen (heap lock flag 6). Wall-clock time keeps advancing even
        // when gameplay is suspended, so without this reset the meter would
        // silently refill and drain events would fire on unpause.
        // ============================================
        if (dComIfGp_isPauseFlag() || dComIfGp_isHeapLockFlag() == 6) {
            sLastRecoveryTime     = sNow;
            sLastSpinnerDrainTime = sNow;
            sLastDomRodDrainTime  = sNow;
        }
        // ============================================
        // NEW CODE ENDS HERE
        // ============================================

        // ============================================
        // NEW CODE — ALBW Port
        // Meter expansion: 10+ hearts, magic armor, and eight main dungeon
        // clears. Each heart/armor tier adds kALBWHeartArmorTierBonus; each
        // dungeon adds kALBWDungeonClearBonus (one normal sword swing).
        // New steps trigger sALBWExpanding (celebration fill + border growth).
        // ============================================
        {
            const int upgradeSteps = countALBWMeterUpgradeSteps();
            const int newMax       = computeALBWMeterMax();

            if (!sInitialized) {
                sOilMaxVar          = newMax;
                sALBWMeter          = newMax;
                sPrevUpgradeSteps   = upgradeSteps;
                sInitialized        = true;
            } else if (upgradeSteps > sPrevUpgradeSteps) {
                sOilMaxVar        = newMax;
                sPrevUpgradeSteps = upgradeSteps;
                sALBWExpanding    = true;
            } else if (newMax != sOilMaxVar) {
                sOilMaxVar = newMax;
            }
        }
        // ============================================
        // NEW CODE ENDS HERE
        // ============================================

        if (sArrowMade) {
            albwDrainMeter(5450, sNow);
            sArrowMade = false;
        } else if (sBombAmmo) {
            albwDrainMeter(5450, sNow);
            sBombAmmo = false;
        } else if (sBoomThrow) {
            if (sALBWLocked) {
                dMeter2_addALBWFraction(3, 20);
            } else {
                albwDrainMeter(2725, sNow);
            }
            sBoomThrow = false;
        }
        // ============================================
        // NEW CODE — ALBW Port
        // Source: slingshot drain check, ALBW main.cpp
        // Drain value 3633 (1/3 meter, 3 shots to drain) — testing value.
        // ============================================
        else if (sSlingMade) {
            albwDrainMeter(3633, sNow);
            sSlingMade = false;
        } else if (sIronballThrow) {
            albwDrainMeter(sOilMaxVar / 2, sNow);
            sIronballThrow = false;
        } else if (sHookshotFire) {
            albwDrainMeter(2725, sNow);
            sHookshotFire = false;
        } else if (sDoubleHookshotFire) {
            albwDrainMeter(1362, sNow);
            sDoubleHookshotFire = false;
        }
        // ============================================
        // NEW CODE — ALBW Port
        // Sword attack and agility drain blocks.
        // All costs are fixed to sOilBaseMax fractions so per-action
        // consumption stays constant even when the meter is expanded.
        // Each block mirrors the else-if pattern above: drain, clear flag,
        // reset sLastRecoveryTime to defer recovery until the next interval.
        // ============================================
        else if (sSwordSwing) {
            albwDrainMeter(1817, sNow);
            sSwordSwing = false;
        } else if (sSidestep) {
            albwDrainMeter(2180, sNow);
            sSidestep = false;
        } else if (sBackJump) {
            albwDrainMeter(3633, sNow);
            sBackJump = false;
        } else if (sRollJump) {
            albwDrainMeter(3633, sNow);
            sRollJump = false;
        } else if (sHiddenSkill) {
            commitALBWHiddenSkillDrain(sNow);
        }
        // ============================================
        // NEW CODE ENDS HERE
        // ============================================

        // ============================================
        // NEW CODE — ALBW Port
        // Continuous drain: Spinner
        // sSpinnerActive is set every frame by procSpinnerWait()
        // while Link is riding. Drain is processed at most once per
        // 100ms tick (sLastSpinnerDrainTime), so the total ride time
        // before depletion scales with meter level:
        //   Level 0 (sOilMaxVar = 10900): 156 units/100ms →  7.0s
        //   Level 1 (sOilMaxVar = 16350): 164 units/100ms → 10.0s
        //   Level 2 (sOilMaxVar = 21800):  88 units/100ms → 24.7s
        // After the drain tick, sLastRecoveryTime is updated to prevent
        // the recovery timer from running while the spinner is active.
        // ============================================
        if (sSpinnerActive) {
            auto msSpin = std::chrono::duration_cast<std::chrono::milliseconds>(
                sNow - sLastSpinnerDrainTime).count();
            if (msSpin >= 100) {
                int rate = (sOilMaxVar <= 10900) ? 156 : (sOilMaxVar <= 16350) ? 164 : 88;
                albwDrainMeter(rate, sNow);
                sLastSpinnerDrainTime = sNow;
            }
            sSpinnerActive = false;
        }
        // ============================================
        // NEW CODE ENDS HERE
        // ============================================

        // ============================================
        // NEW CODE — ALBW Port
        // Continuous drain: Dominion Rod
        // sDomRodActive is set every frame by procCopyRodSubject() /
        // procCopyRodMove() while a statue is under active control.
        // Flat rate: 36 units per 100ms (same as recovery) so the
        // meter drains at ≈ 30s from full regardless of meter level.
        // ============================================
        if (sDomRodActive) {
            auto msDom = std::chrono::duration_cast<std::chrono::milliseconds>(
                sNow - sLastDomRodDrainTime).count();
            if (msDom >= 100) {
                albwDrainMeter(36, sNow);
                sLastDomRodDrainTime = sNow;
            }
            sDomRodActive = false;
        }
        // ============================================
        // NEW CODE ENDS HERE
        // ============================================

        // ============================================
        // NEW CODE — ALBW Port
        // Passive recovery: normal rates while unlocked; faster 7s/3s rates during lockout.
        // Internal meter may be negative (debt); HUD shows max(0, internal).
        // ============================================
        const int sNormalRecovery = computeALBWRecoveryRate();
        const bool sALBWGuarding = dMeter2_isALBWRecoveryPausedByGuard();
        bool allowPassiveRecovery = !sALBWGuarding;
        int sRecoveryRate = sALBWLocked ? albwLockoutRecoveryRate() : sNormalRecovery;

        if (sALBWLocked) {
            allowPassiveRecovery = allowPassiveRecovery && dMeter2_isALBWLockoutZTargetRecovery();
        } else if (sALBWPlayerIdle) {
            sRecoveryRate = (sNormalRecovery * 105) / 100;
        }

        sALBWPlayerIdle = false;
        auto msElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            sNow - sLastRecoveryTime).count();
        if (sALBWExpanding) {
            if (allowPassiveRecovery && msElapsed >= 100) {
                sALBWMeter += 36;
                sLastRecoveryTime = sNow;
                albwRefreshLockoutState(false);
                if (sALBWMeter >= sOilMaxVar) {
                    sALBWMeter     = sOilMaxVar;
                    sALBWExpanding = false;
                    sALBWRecoverSoundActive = false;
                    Z2GetAudioMgr()->seStart(
                        Z2SE_MAGIC_METER_FINISH, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
                } else if (mpMeterDraw->getMeterGaugeAlphaRate(1) > 0.0f && !sALBWRecoverSoundActive &&
                           sALBWMeter >= 0)
                {
                    sALBWRecoverSoundActive = true;
                    if (sALBWLocked) {
                        Z2GetAudioMgr()->seStart(
                            Z2SE_MAGIC_METER_RECOVER, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
                    }
                }
            }
            if (sALBWMeter >= sOilMaxVar) {
                sALBWMeter     = sOilMaxVar;
                sALBWExpanding = false;
            }
        } else if (allowPassiveRecovery && msElapsed >= 100 && sALBWMeter < sOilMaxVar && !sMeterinc) {
            sALBWMeter += sRecoveryRate;
            sLastRecoveryTime = sNow;
            albwRefreshLockoutState(false);
            if (sALBWMeter >= sOilMaxVar) {
                sALBWMeter = sOilMaxVar;
                sALBWRecoverSoundActive = false;
                Z2GetAudioMgr()->seStart(
                    Z2SE_MAGIC_METER_FINISH, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
            } else if (!sALBWRecoverSoundActive && sALBWMeter >= 0) {
                sALBWRecoverSoundActive = true;
                Z2GetAudioMgr()->seStartLevel(
                    Z2SE_MAGIC_METER_RECOVER, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
            }
        }
        // ============================================
        // MODIFIED CODE ENDS HERE
        // ============================================

        // ============================================
        // MODIFIED CODE — ALBW Port (fix: fill bar stuck at 50%)
        // drawKantera divides by dComIfGs_getMaxOil() directly, not
        // by i_max. The game's native oil max is 21600 but our internal
        // ALBW scale is sOilMaxVar=10900, so passing sALBWMeter raw
        // produced 10900/21600 ≈ 50% every frame with no recovery.
        // We now scale [0, sOilMaxVar] → [0, nativeMax] before passing.
        // ============================================
        // === Clean-encounter reward check ===
        // Each frame: scan sArmorEncounterIDs for actors that are gone (killed).
        // Room change: detected via sArmorLastRoomNo mismatch — flush without reward
        // so stale actor IDs from a different room never trigger a false positive.
        // Reward fires (via the normal rupee queue) when all encounter actors are
        // killed, at least one kill happened, and sArmorRewardBlocked is false.
        {
            s16 curRoom = dComIfGp_roomControl_getStayNo();
            if (sArmorLastRoomNo != -1 && curRoom != sArmorLastRoomNo) {
                sArmorEncounterIDs.clear();
                sArmorTaintedIDs.clear();
                sArmorRewardBlocked   = false;
                sArmorEncounterKillCount = 0;
            }
            sArmorLastRoomNo = curRoom;

            if (!sArmorEncounterIDs.empty()) {
                std::vector<fpc_ProcID> killed;
                for (fpc_ProcID id : sArmorEncounterIDs) {
                    if (fopAcM_SearchByID(id) == NULL) {
                        killed.push_back(id);
                    }
                }
                for (fpc_ProcID id : killed) {
                    sArmorEncounterIDs.erase(id);
                    sArmorTaintedIDs.erase(id);
                    sArmorEncounterKillCount++;
                }
                if (sArmorEncounterIDs.empty() && sArmorEncounterKillCount > 0) {
                    if (!sArmorRewardBlocked) {
                        // All enemies in the encounter killed with no damage — reward
                        dComIfGp_setItemRupeeCount(300);
                    }
                    sArmorRewardBlocked      = false;
                    sArmorEncounterKillCount = 0;
                }
            }
        }

        // ============================================
        // NEW CODE ENDS HERE
        // ============================================

        const int displayMeter = albwDisplayMeter();
        s32 nativeMax = dComIfGs_getMaxOil();
        mNowOil = (nativeMax > 0) ? (s32)((f32)displayMeter / (f32)sOilMaxVar * (f32)nativeMax) : 0;
        mMaxOil = nativeMax;
        draw_kantera = true;
        // ============================================
        // MODIFIED CODE ENDS HERE
        // ============================================
    }
moveKantera_skip_albw_pc:
#endif
    // ============================================
    // NEW CODE ENDS HERE
    // ============================================

    if (draw_kantera == true) {
        mpMeterDraw->drawKantera(mMaxOil, mNowOil, x_pos, y_pos);
#if TARGET_PC
        // ============================================
        // NEW CODE — ALBW Port
        // Feed ALBW meter values into the cut magic meter widget.
        // drawMagic uses a 0–32 reference width at base capacity (sOilBaseMax).
        // Bar length and fill both scale with sOilMaxVar so upgrades visibly
        // lengthen the meter, not just its internal unit pool.
        // ============================================
        {
            static constexpr s16 kVisualMeterBaseWidth = 32;
            const s16 barMax = (sOilMaxVar > 0)
                ? static_cast<s16>((f32)kVisualMeterBaseWidth * (f32)sOilMaxVar /
                                   (f32)sOilBaseMax)
                : kVisualMeterBaseWidth;
            const s16 barCur = (sOilMaxVar > 0)
                ? static_cast<s16>((f32)albwDisplayMeter() / (f32)sOilMaxVar * (f32)barMax)
                : 0;
            mpMeterDraw->drawMagic(barMax, barCur, x_pos, y_pos);
            mpMeterDraw->setAlphaMagicChange(false);
        }
        // ============================================
        // NEW CODE ENDS HERE
        // ============================================
#endif
    }

    alphaAnimeKantera();
    dComIfGp_setItemNowOil(mNowOil);
}

void dMeter2_c::moveOxygen() {
    s32 var_r0;
    s32 var_r7;
    bool draw_oxygen;

    s32 var_r4 = dComIfGp_getMaxOxygen();
    var_r7 = 0;
    draw_oxygen = false;

    if (dComIfGp_getMaxOxygenCount() != 0) {
        var_r7 = dComIfGp_getMaxOxygen() + dComIfGp_getMaxOxygenCount();
        if (var_r7 > var_r4) {
            var_r7 = var_r4;
        } else if (var_r7 < 0) {
            var_r7 = 0;
        }

        dComIfGp_setMaxOxygen((u16)var_r7);
        dComIfGp_setOxygenCount(var_r7 - dComIfGp_getOxygen());
        dComIfGp_clearMaxOxygenCount();
        draw_oxygen = true;
    }

    if (dComIfGp_getOxygenCount() != 0) {
        if (draw_oxygen == 0) {
            var_r7 = dComIfGp_getMaxOxygen();
        }

        var_r0 = dComIfGp_getOxygen() + dComIfGp_getOxygenCount();
        if (var_r0 > var_r7) {
            var_r0 = var_r7;
        } else if (var_r0 < 0) {
            var_r0 = 0;
        }

        if (dComIfGp_getOxygenShowFlag() && var_r0 == dComIfGp_getMaxOxygen()) {
            if (dComIfGp_getOxygen() < dComIfGp_getMaxOxygen() &&
                mpMeterDraw->getMeterGaugeAlphaRate(2) > 0.0f)
            {
                Z2GetAudioMgr()->seStart(Z2SE_SWIM_TIMER_RECOVER, NULL, 0, 0, 1.0f, 1.0f, -1.0f,
                                         -1.0f, 0);
            }
        }

        dComIfGp_setOxygen((u16)var_r0);
        dComIfGp_clearOxygenCount();
        draw_oxygen = true;
    }

    if (mMaxOxygen != dComIfGp_getMaxOxygen()) {
        if (mMaxOxygen < dComIfGp_getMaxOxygen()) {
            mMaxOxygen += 50;
            if (mMaxOxygen > dComIfGp_getMaxOxygen()) {
                mMaxOxygen = dComIfGp_getMaxOxygen();
            }
            draw_oxygen = true;
        } else if (mMaxOxygen > dComIfGp_getMaxOxygen()) {
            mMaxOxygen -= 50;
            if (mMaxOxygen < dComIfGp_getMaxOxygen()) {
                mMaxOxygen = dComIfGp_getMaxOxygen();
            }
            draw_oxygen = true;
        }
    }

    if (mNowOxygen != dComIfGp_getOxygen()) {
        if (mNowOxygen < dComIfGp_getOxygen()) {
            mNowOxygen += 50;
            if (mNowOxygen > dComIfGp_getOxygen()) {
                mNowOxygen = dComIfGp_getOxygen();
            }

            draw_oxygen = true;
        } else if (mNowOxygen > dComIfGp_getOxygen()) {
            mNowOxygen -= 50;
            if (mNowOxygen < dComIfGp_getOxygen()) {
                mNowOxygen = dComIfGp_getOxygen();
            }

            if ((f32)dComIfGp_getOxygen() / (f32)dComIfGp_getMaxOxygen() > 0.5f &&
                mpMeterDraw->getMeterGaugeAlphaRate(2) > 0.0f)
            {
                Z2GetAudioMgr()->seStartLevel(Z2SE_SWIM_TIMER_DEC, NULL, 0, 0, 1.0f, 1.0f, -1.0f,
                                              -1.0f, 0);
            }

            draw_oxygen = true;
        }
    }

    f32 x_pos = g_drawHIO.mOxygenMeterPosX;
    f32 y_pos = g_drawHIO.mOxygenMeterPosY;

    if (field_0x248 != mMaxLife) {
        field_0x248 = mMaxLife;
        draw_oxygen = true;
    }

    if (mOxygenMeterScale != g_drawHIO.mOxygenMeterScale) {
        mOxygenMeterScale = g_drawHIO.mOxygenMeterScale;
        draw_oxygen = true;
    }

    if (mOxygenMeterPosX != g_drawHIO.mOxygenMeterPosX) {
        mOxygenMeterPosX = g_drawHIO.mOxygenMeterPosX;
        draw_oxygen = true;
    }

    if (mOxygenMeterPosY != g_drawHIO.mOxygenMeterPosY) {
        mOxygenMeterPosY = g_drawHIO.mOxygenMeterPosY;
        draw_oxygen = true;
    }

    if (draw_oxygen == true) {
        mpMeterDraw->drawOxygen(mMaxOxygen, mNowOxygen, x_pos, y_pos);
    }

    alphaAnimeOxygen();
    dComIfGp_setNowOxygen(mNowOxygen);
}

void dMeter2_c::moveLightDrop() {
    f32 scale;
    f32 alpha;
    f32 pos_x;
    f32 pos_y;
    bool draw_lightdrop;
    u8 var_r28;

    draw_lightdrop = false;
    var_r28 = 0;
    if (g_drawHIO.mLightDrop.mAnimDebug) {
        u8 dark_level = dComIfGp_getStartStageDarkArea();
        if (dComIfGp_getNeedLightDropNum() == dComIfGs_getLightDropNum(dark_level)) {
            var_r28 = 2;
        }
    }

    if (mLightDropNum != dComIfGs_getLightDropNum(dComIfGp_getStartStageDarkArea())) {
        if (dComIfGp_getNeedLightDropNum() ==
            dComIfGs_getLightDropNum(dComIfGp_getStartStageDarkArea()))
        {
            var_r28 = 2;
            Z2GetAudioMgr()->seStart(Z2SE_SY_LIGHT_DROP_COMPLETE, NULL, 0, 0, 1.0f, 1.0f, -1.0f,
                                     -1.0f, 0);
        } else if (mLightDropNum < dComIfGs_getLightDropNum(dComIfGp_getStartStageDarkArea())) {
            var_r28 = 1;
            Z2GetAudioMgr()->seStart(Z2SE_SY_LIGHT_DROP_GET, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f,
                                     0);
        }

        mLightDropNum = dComIfGs_getLightDropNum(dComIfGp_getStartStageDarkArea());
        draw_lightdrop = true;
    }

    if (mNeedLightDropNum != dComIfGp_getNeedLightDropNum()) {
        mNeedLightDropNum = dComIfGp_getNeedLightDropNum();
        draw_lightdrop = true;
    }

    if ((mStatus & 0x100) || daPy_getPlayerActorClass()->checkHawkWait() ||
        daPy_getPlayerActorClass()->checkGrassWhistle() ||
        g_meter2_info.mItemExplainWindowStatus != 0)
    {
        pos_x = g_drawHIO.mLightDrop.mVesselTalkPosX;
        pos_y = g_drawHIO.mLightDrop.mVesselTalkPosY;
        scale = g_drawHIO.mLightDrop.mVesselTalkScale;
        alpha = g_drawHIO.mLightDrop.mVesselAlpha[1];
    } else {
        pos_x = g_drawHIO.mLightDrop.mVesselPosX;
        pos_y = g_drawHIO.mLightDrop.mVesselPosY;
        scale = g_drawHIO.mLightDrop.mVesselScale;
        alpha = g_drawHIO.mLightDrop.mVesselAlpha[0];
    }

    if (mVesselPosX != pos_x) {
        cLib_addCalc2(&mVesselPosX, pos_x, 1.0f, 10.0f);
        draw_lightdrop = true;
        if ((f32)fabs(mVesselPosX - pos_x) < 0.1f) {
            mVesselPosX = pos_x;
        }
    }

    if (mVesselPosY != pos_y) {
        cLib_addCalc2(&mVesselPosY, pos_y, 1.0f, 10.0f);
        draw_lightdrop = true;
        if ((f32)fabs(mVesselPosY - pos_y) < 0.1f) {
            mVesselPosY = pos_y;
        }
    }

    if (mVesselScale != scale) {
        cLib_addCalc2(&mVesselScale, scale, 0.2f, 1.0f);
        draw_lightdrop = true;
        if ((f32)fabs(mVesselScale - scale) < 0.1f) {
            mVesselScale = scale;
        }
    }

    if (mVesselAlpha != alpha) {
        cLib_addCalc2(&mVesselAlpha, alpha, 0.2f, 1.0f);
        draw_lightdrop = true;
        if ((f32)fabs(mVesselAlpha - alpha) < 0.1f) {
            mVesselAlpha = alpha;
        }
    }

    if (draw_lightdrop == true) {
        mpMeterDraw->drawLightDrop(mLightDropNum, mNeedLightDropNum, mVesselPosX, mVesselPosY,
                                   mVesselScale, mVesselAlpha, var_r28);
    }

    alphaAnimeLightDrop();
}

void dMeter2_c::moveRupee() {
    s16 temp_r5;
    s32 temp_r0;

    temp_r5 = dComIfGs_getRupeeMax();
    s16 r29 = 0;
    bool draw_rupee = false;

    if (dComIfGp_getItemRupeeCount() != 0) {
        r29 = dComIfGs_getRupee() + dComIfGp_getItemRupeeCount();
        if (r29 > temp_r5) {
            r29 = temp_r5;
        } else if (r29 < 0) {
            r29 = 0;
        }

        dComIfGs_setRupee(r29);
        dComIfGp_clearItemRupeeCount();

        if (dComIfGs_getRupee() - mRupeeNum >= 5) {
            onRupeeSoundBit(2);
            if (isRupeeSoundBit(3)) {
                offRupeeSoundBit(3);
                offRupeeSoundBit(1);
            }
        } else if (dComIfGs_getRupee() - mRupeeNum <= -5) {
            onRupeeSoundBit(3);
            if (isRupeeSoundBit(2)) {
                offRupeeSoundBit(2);
                offRupeeSoundBit(0);
            }
        }
    }

    if (mRupeeNum != dComIfGs_getRupee()) {
        if (mRupeeNum < dComIfGs_getRupee()) {
            mRupeeNum++;
            draw_rupee = true;

            if (isRupeeSoundBit(2) & 1) {
                if (mRupeeNum != dComIfGs_getRupee()) {
                    if (!isRupeeSoundBit(0)) {
                        onRupeeSoundBit(0);
                        mDoAud_seStart(Z2SE_LUPY_INC_CNT_1, NULL, 0, 0);
                    } else {
                        offRupeeSoundBit(0);
                    }
                } else {
                    mDoAud_seStart(Z2SE_LUPY_INC_CNT_2, NULL, 0, 0);
                    offRupeeSoundBit(2);
                    offRupeeSoundBit(0);
                }
            }
        } else if (mRupeeNum > dComIfGs_getRupee()) {
            mRupeeNum--;
            draw_rupee = true;

            if (isRupeeSoundBit(3) & 1) {
                if (mRupeeNum != dComIfGs_getRupee()) {
                    if (!isRupeeSoundBit(1)) {
                        onRupeeSoundBit(1);
                        mDoAud_seStart(Z2SE_LUPY_DEC_CNT_1, NULL, 0, 0);
                    } else {
                        offRupeeSoundBit(1);
                    }
                } else {
                    mDoAud_seStart(Z2SE_LUPY_DEC_CNT_2, NULL, 0, 0);
                    offRupeeSoundBit(3);
                    offRupeeSoundBit(1);
                }
            }
        }
    }

    if (mRupeeKeyScale != g_drawHIO.mRupeeKeyScale) {
        mRupeeKeyScale = g_drawHIO.mRupeeKeyScale;
        draw_rupee = true;
    }

    if (mRupeeKeyPosX != g_drawHIO.mRupeeKeyPosX) {
        mRupeeKeyPosX = g_drawHIO.mRupeeKeyPosX;
        draw_rupee = true;
    }

    if (mRupeeKeyPosY != g_drawHIO.mRupeeKeyPosY) {
        mRupeeKeyPosY = g_drawHIO.mRupeeKeyPosY;
        draw_rupee = true;
    }

    if (mRupeeScale != g_drawHIO.mRupeeScale) {
        mRupeeScale = g_drawHIO.mRupeeScale;
        draw_rupee = true;
    }

    if (mRupeePosX != g_drawHIO.mRupeePosX) {
        mRupeePosX = g_drawHIO.mRupeePosX;
        draw_rupee = true;
    }

    if (mRupeePosY != g_drawHIO.mRupeePosY) {
        mRupeePosY = g_drawHIO.mRupeePosY;
        draw_rupee = true;
    }

    if (mRupeeFramePosY != g_drawHIO.mRupeeFramePosY) {
        mRupeeFramePosY = g_drawHIO.mRupeeFramePosY;
        draw_rupee = true;
    }

    if (mRupeeFrameScale != g_drawHIO.mRupeeFrameScale) {
        mRupeeFrameScale = g_drawHIO.mRupeeFrameScale;
        draw_rupee = true;
    }

    if (mRupeeFramePosX != g_drawHIO.mRupeeFramePosX) {
        mRupeeFramePosX = g_drawHIO.mRupeeFramePosX;
        draw_rupee = true;
    }

    if (mRupeeCountScale != g_drawHIO.mRupeeCountScale) {
        mRupeeCountScale = g_drawHIO.mRupeeCountScale;
        draw_rupee = true;
    }

    if (mRupeeCountPosX != g_drawHIO.mRupeeCountPosX) {
        mRupeeCountPosX = g_drawHIO.mRupeeCountPosX;
        draw_rupee = true;
    }

    if (mRupeeCountPosY != g_drawHIO.mRupeeCountPosY) {
        mRupeeCountPosY = g_drawHIO.mRupeeCountPosY;
        draw_rupee = true;
    }

    if (mWalletSize != dComIfGs_getWalletSize()) {
        mWalletSize = dComIfGs_getWalletSize();
        draw_rupee = true;
    }

#if TARGET_PC
    // ============================================
    // NEW CODE — ALBW Port (Lies of Link HUD)
    // drawRupee() (which carries the LoP top-right reposition) only runs when the
    // wallet is animating. Force a re-layout when the layout toggle flips so the
    // wallet snaps to / from the LoP corner without needing a rupee change.
    // ============================================
    {
        static bool sRupeeLopWasOn = false;
        const bool lopOn = dusk::getSettings().game.lopHud.getValue();
        if (sRupeeLopWasOn != lopOn) {
            sRupeeLopWasOn = lopOn;
            draw_rupee = true;
        }
    }
    // ============================================
    // NEW CODE ENDS HERE
    // ============================================
#endif

    if (draw_rupee == true) {
        mpMeterDraw->drawRupee(mRupeeNum);
    }

    alphaAnimeRupee();
}

void dMeter2_c::moveKey() {
    s16 var_r5;
    bool draw_key;

    draw_key = false;
    if (dComIfGp_getItemKeyNumCount() != 0) {
        var_r5 = dComIfGs_getKeyNum() + dComIfGp_getItemKeyNumCount();
        if (var_r5 > 99) {
            var_r5 = 99;
        } else if (var_r5 < 0) {
            var_r5 = 0;
        }

        dComIfGs_setKeyNum(var_r5);
        dComIfGp_clearItemKeyNumCount();
    }

    if (mKeyNum != dComIfGs_getKeyNum()) {
        if (mKeyNum < dComIfGs_getKeyNum()) {
            mKeyNum++;
            draw_key = true;
        } else if (mKeyNum > dComIfGs_getKeyNum()) {
            mKeyNum--;
            draw_key = true;
        }
    }

    if (mKeyScale != g_drawHIO.mKeyScale) {
        mKeyScale = g_drawHIO.mKeyScale;
        draw_key = true;
    }

    if (mKeyPosX != g_drawHIO.mKeyPosX) {
        mKeyPosX = g_drawHIO.mKeyPosX;
        draw_key = true;
    }

    if (mKeyPosY != g_drawHIO.mKeyPosY) {
        mKeyPosY = g_drawHIO.mKeyPosY;
        draw_key = true;
    }

    if (mKeyNumScale != g_drawHIO.mKeyNumScale) {
        mKeyNumScale = g_drawHIO.mKeyNumScale;
        draw_key = true;
    }

    if (mKeyNumPosX != g_drawHIO.mKeyNumPosX) {
        mKeyNumPosX = g_drawHIO.mKeyNumPosX;
        draw_key = true;
    }

    if (mKeyNumPosY != g_drawHIO.mKeyNumPosY) {
        mKeyNumPosY = g_drawHIO.mKeyNumPosY;
        draw_key = true;
    }

    if (draw_key == true) {
        mpMeterDraw->drawKey(mKeyNum);
    }

    alphaAnimeKey();
}

void dMeter2_c::moveButtonA() {
    f32 var_f31;
    f32 pos_x[2];
    f32 pos_y[2];

    bool draw_buttonA;
    bool var_r29;
    bool emphasis_a;
    u8 temp_r0;

    draw_buttonA = false;
    var_r29 = 0;
    emphasis_a = false;

    if (dComIfGp_getDoStatusForce() != 0) {
        dComIfGp_setDoStatus(dComIfGp_getDoStatusForce(), dComIfGp_getDoSetFlagForce());
        dComIfGp_setDoStatusForce(0, 0);
    }

    if (daPy_getPlayerActorClass()->getSumouMode()) {
        if (dComIfGp_getDoStatus() == 0) {
            dComIfGp_setDoStatus(0x15, 0);
            emphasis_a = true;
        } else if (dComIfGp_getDoStatus() == 0x15 && !mpMeterDraw->isEmphasisA()) {
            var_r29 = 1;
            draw_buttonA = true;
        }
    }

    if (mDoStatus != dComIfGp_getDoStatus()) {
        mDoStatus = dComIfGp_getDoStatus();

        if (mDoStatus == 0x2D || mDoStatus == 0x2E) {
            dComIfGp_setDoStatus(mDoStatus, 1);
        }

        var_r29 = 1;
        draw_buttonA = true;
    }

    if (mDoSetFlag != dComIfGp_isDoSetFlag(2)) {
        mDoSetFlag = dComIfGp_isDoSetFlag(2);
        var_r29 = 1;
        draw_buttonA = true;
    }

    if (emphasis_a && mpMeterDraw->isEmphasisA()) {
        if (field_0x1fd == 0) {
            var_r29 = 1;
            draw_buttonA = true;
            field_0x1fd = 1;
        }
    } else if (!emphasis_a && !mpMeterDraw->isEmphasisA() && field_0x1fd == 1) {
        var_r29 = 1;
        draw_buttonA = true;
        field_0x1fd = 0;
    }

    if (mButtonAScale != g_drawHIO.mButtonAScale) {
        mButtonAScale = g_drawHIO.mButtonAScale;
        draw_buttonA = true;
    }

    for (int i = 0; i < 2; i++) {
        if (mButtonATalkScale[i] != g_drawHIO.mButtonATalkScale[i]) {
            mButtonATalkScale[i] = g_drawHIO.mButtonATalkScale[i];
            draw_buttonA = true;
        }
    }

    if (mButtonAPosX != g_drawHIO.mButtonAPosX) {
        mButtonAPosX = g_drawHIO.mButtonAPosX;
        draw_buttonA = true;
    }

    if (mButtonAPosY != g_drawHIO.mButtonAPosY) {
        mButtonAPosY = g_drawHIO.mButtonAPosY;
        draw_buttonA = true;
    }

    if (mButtonATextScale != g_drawHIO.mButtonATextScale) {
        mButtonATextScale = g_drawHIO.mButtonATextScale;
        draw_buttonA = true;
    }

    if (mButtonATextPosX != g_drawHIO.mButtonATextPosX) {
        mButtonATextPosX = g_drawHIO.mButtonATextPosX;
        draw_buttonA = true;
    }

    if (mButtonATextPosY != g_drawHIO.mButtonATextPosY) {
        mButtonATextPosY = g_drawHIO.mButtonATextPosY;
        draw_buttonA = true;
    }

    if (isShowLightDrop()) {
        pos_x[0] = g_drawHIO.mButtonAVesselPosX;
        pos_y[0] = g_drawHIO.mButtonAVesselPosY;
        pos_x[1] = 0.0f;
        pos_y[1] = 0.0f;
        var_f31 = 1.0f;
    } else if (isShowFlag(0)) {
        if (isShowFlag(1)) {
            for (int i = 0; i < 2; i++) {
                pos_x[i] = g_drawHIO.mButtonATalkPosX[i];
                pos_y[i] = g_drawHIO.mButtonATalkPosY[i];
            }
        } else {
            for (int i = 0; i < 2; i++) {
                pos_x[i] = g_drawHIO.mButtonATalkAPosX[i];
                pos_y[i] = g_drawHIO.mButtonATalkAPosY[i];
            }
        }
        var_f31 = 1.0f;
    } else {
        pos_x[0] = g_drawHIO.mButtonAPosX;
        pos_y[0] = g_drawHIO.mButtonAPosY;
        pos_x[1] = 0.0f;
        pos_y[1] = 0.0f;
        var_f31 = 1.0f;
    }

    for (int i = 0; i < 2; i++) {
        if (mButtonATalkPosX[i] != pos_x[i]) {
            cLib_addCalc2(&mButtonATalkPosX[i], pos_x[i], 1.0f, 10.0f);
            draw_buttonA = true;
            if ((f32)fabs(mButtonATalkPosX[i] - pos_x[i]) < 0.1f) {
                mButtonATalkPosX[i] = pos_x[i];
            }
        }

        if (mButtonATalkPosY[i] != pos_y[i]) {
            cLib_addCalc2(&mButtonATalkPosY[i], pos_y[i], 1.0f, 10.0f);
            draw_buttonA = true;
            if ((f32)fabs(mButtonATalkPosY[i] - pos_y[i]) < 0.1f) {
                mButtonATalkPosY[i] = pos_y[i];
            }
        }
    }

    if (field_0x144 != var_f31) {
        cLib_addCalc2(&field_0x144, var_f31, 1.0f, 10.0f);
        draw_buttonA = true;
        if ((f32)fabs(field_0x144 - var_f31) < 0.01f) {
            field_0x144 = var_f31;
        }
    }

    if (field_0x200 != dMsgObject_isTalkNowCheck()) {
        field_0x200 = dMsgObject_isTalkNowCheck();
        draw_buttonA = true;
    }

    if (draw_buttonA) {
        mpMeterDraw->drawButtonA(mDoStatus, mButtonATalkPosX[0], mButtonATalkPosY[0],
                                 mButtonATalkPosX[1], mButtonATalkPosY[1], field_0x144, var_r29,
                                 ((mStatus & 0x100) ||
                                  daPy_getPlayerActorClass()->checkHawkWait() ||
                                  daPy_getPlayerActorClass()->checkGrassWhistle()) != false);

        if (emphasis_a) {
            mpMeterDraw->setEmphasisA(0);
        }
    }
}

void dMeter2_c::moveButtonB() {
    f32 var_f31;
    f32 pos_x[2];
    f32 pos_y[2];

    bool draw_buttonB;
    bool var_r29;
    bool emphasis_b;
    u8 temp_r0;

    draw_buttonB = false;
    var_r29 = 0;
    emphasis_b = false;

    if (dComIfGp_getAStatusForce() != 0) {
        dComIfGp_setAStatus(dComIfGp_getAStatusForce(), dComIfGp_getASetFlagForce());
        dComIfGp_setAStatusForce(0, 0);
    }

    if (daPy_getPlayerActorClass()->getSumouMode()) {
        if (dComIfGp_getAStatus() == 0 || dComIfGp_getAStatus() == 0x26) {
            dComIfGp_setAStatus(0x44, 0);
            emphasis_b = true;
        } else if (dComIfGp_getAStatus() == 0x44 && !mpMeterDraw->isEmphasisB()) {
            var_r29 = 1;
            draw_buttonB = true;
        }
    } else if (daPy_getPlayerActorClass()->checkGoatStopGame() ||
               ((mStatus & 2) && dComIfGp_getAStatus() == 0x26))
    {
        dComIfGp_setAStatus(0, 0);
    }

    if (mAStatus != dComIfGp_getAStatus()) {
        mAStatus = dComIfGp_getAStatus();

        if (mAStatus == 0x2D || mAStatus == 0x2E) {
            dComIfGp_setAStatus(mAStatus, 1);
        }

        var_r29 = 1;
        draw_buttonB = true;
    }

    if (mASetFlag != dComIfGp_isASetFlag(2)) {
        mASetFlag = dComIfGp_isASetFlag(2);
        var_r29 = 1;
        draw_buttonB = true;
    }

    if (mEquipSword != dComIfGs_getSelectEquipSword()) {
        mEquipSword = dComIfGs_getSelectEquipSword();
        var_r29 = 1;
        draw_buttonB = true;
    }

    if (emphasis_b && mpMeterDraw->isEmphasisB()) {
        if (field_0x1fe == 0) {
            var_r29 = 1;
            draw_buttonB = true;
            field_0x1fe = 1;
        }
    } else if (!emphasis_b && !mpMeterDraw->isEmphasisB() && field_0x1fe == 1) {
        var_r29 = 1;
        draw_buttonB = true;
        field_0x1fe = 0;
    }

    if (field_0x2cc != g_drawHIO.mButtonBScale) {
        field_0x2cc = g_drawHIO.mButtonBScale;
        draw_buttonB = true;
    }

    for (int i = 0; i < 2; i++) {
        if (field_0x2ec[i] != g_drawHIO.mButtonBTalkScale[i]) {
            field_0x2ec[i] = g_drawHIO.mButtonBTalkScale[i];
            draw_buttonB = true;
        }
    }

    if (mButtonBPosX != g_drawHIO.mButtonBPosX) {
        mButtonBPosX = g_drawHIO.mButtonBPosX;
        draw_buttonB = true;
    }

    if (mButtonBPosY != g_drawHIO.mButtonBPosY) {
        mButtonBPosY = g_drawHIO.mButtonBPosY;
        draw_buttonB = true;
    }

    if (g_drawHIO.mButtonDebug[3]) {
        draw_buttonB = true;
    }

    for (int i = 0; i < 2; i++) {
        if (mItemBBaseScale[i] != g_drawHIO.mItemBBaseScale[i]) {
            mItemBBaseScale[i] = g_drawHIO.mItemBBaseScale[i];
            draw_buttonB = true;
        }

        if (mItemBBasePosX[i] != g_drawHIO.mItemBBasePosX[i]) {
            mItemBBasePosX[i] = g_drawHIO.mItemBBasePosX[i];
            draw_buttonB = true;
        }

        if (mItemBBasePosY[i] != g_drawHIO.mItemBBasePosY[i]) {
            mItemBBasePosY[i] = g_drawHIO.mItemBBasePosY[i];
            draw_buttonB = true;
        }
    }

    if (mButtonBFontScale != g_drawHIO.mButtonBFontScale) {
        mButtonBFontScale = g_drawHIO.mButtonBFontScale;
        draw_buttonB = true;
    }

    if (mButtonBFontPosX != g_drawHIO.mButtonBFontPosX) {
        mButtonBFontPosX = g_drawHIO.mButtonBFontPosX;
        draw_buttonB = true;
    }

    if (mButtonBFontPosY != g_drawHIO.mButtonBFontPosY) {
        mButtonBFontPosY = g_drawHIO.mButtonBFontPosY;
        draw_buttonB = true;
    }

    if (isShowLightDrop()) {
        pos_x[0] = g_drawHIO.mButtonBVesselPosX;
        pos_y[0] = g_drawHIO.mButtonBVesselPosY;
        pos_x[1] = 0.0f;
        pos_y[1] = 0.0f;
        var_f31 = 1.0f;
    } else if (isShowFlag(1)) {
        for (int i = 0; i < 2; i++) {
            pos_x[i] = g_drawHIO.mButtonBTalkPosX[i];
            pos_y[i] = g_drawHIO.mButtonBTalkPosY[i];
        }
        var_f31 = 1.0f;
    } else {
        for (int i = 0; i < 2; i++) {
            if (field_0x128 != 0 && i == 0) {
                pos_x[i] = g_drawHIO.mButtonBWolfPosX;
                pos_y[i] = g_drawHIO.mButtonBWolfPosY;
            } else {
                pos_x[i] = 0.0f;
                pos_y[i] = 0.0f;
            }
        }
        var_f31 = 1.0f;
    }

    if (mpMeterDraw->isBButtonShow(false)) {
        if (field_0x1ff == 0) {
            field_0x1ff = 1;
            draw_buttonB = true;
        }
    } else if (field_0x1ff == 1) {
        field_0x1ff = 0;
        draw_buttonB = true;
    }

    for (int i = 0; i < 2; i++) {
        if (field_0x148[i] != pos_x[i]) {
            cLib_addCalc2(&field_0x148[i], pos_x[i], 1.0f, 10.0f);
            draw_buttonB = true;
            if ((f32)fabs(field_0x148[i] - pos_x[i]) < 0.1f) {
                field_0x148[i] = pos_x[i];
            }
        }

        if (field_0x150[i] != pos_y[i]) {
            cLib_addCalc2(&field_0x150[i], pos_y[i], 1.0f, 10.0f);
            draw_buttonB = true;
            if ((f32)fabs(field_0x150[i] - pos_y[i]) < 0.1f) {
                field_0x150[i] = pos_y[i];
            }
        }
    }

    if (field_0x158 != var_f31) {
        cLib_addCalc2(&field_0x158, var_f31, 1.0f, 10.0f);
        draw_buttonB = true;
        if ((f32)fabs(field_0x158 - var_f31) < 0.01f) {
            field_0x158 = var_f31;
        }
    }

    if (g_drawHIO.mItemScaleAdjustON && field_0x4bc != g_drawHIO.mItemScalePercent) {
        field_0x4bc = g_drawHIO.mItemScalePercent;
        draw_buttonB = true;
    }

    if (field_0x1c6 != daPy_getPlayerActorClass()->checkGrassWhistle()) {
        field_0x1c6 = daPy_getPlayerActorClass()->checkGrassWhistle();
        draw_buttonB = true;
    }

    if (draw_buttonB) {
        mpMeterDraw->drawButtonB(mAStatus, field_0x128 == 0, field_0x148[0], field_0x150[0],
                                 field_0x148[1], field_0x150[1], field_0x158, var_r29);

        if (emphasis_b) {
            mpMeterDraw->setEmphasisB(0);
        }
    }
}

void dMeter2_c::moveButtonR() {
    bool var_r7;
    bool draw_buttonR;

    draw_buttonR = false;
    var_r7 = 0;

    if (field_0x128 != 0 &&
        (mCollectSmell != dComIfGs_getCollectSmell() || field_0x128 != field_0x12c))
    {
        mCollectSmell = dComIfGs_getCollectSmell();
        var_r7 = 1;
        draw_buttonR = true;
    }

    if (dComIfGp_getRStatusForce() != 0) {
        dComIfGp_setRStatus(dComIfGp_getRStatusForce(), dComIfGp_getRSetFlagForce());
        dComIfGp_setRStatusForce(0, 0);
    }

    if (mRStatus != dComIfGp_getRStatus()) {
        mRStatus = dComIfGp_getRStatus();

        if (mRStatus == 0x2D || mRStatus == 0x2E) {
            dComIfGp_setRStatus(mRStatus, 1);
        }

        var_r7 = 1;
        draw_buttonR = true;
    }

    if (mRSetFlag != dComIfGp_isRSetFlag(2)) {
        mRSetFlag = dComIfGp_isRSetFlag(2);
        var_r7 = 1;
        draw_buttonR = true;
    }

    if (draw_buttonR) {
        mpMeterDraw->drawButtonR(mCollectSmell, mRStatus, field_0x128 == 0, var_r7);
    }
}

void dMeter2_c::moveButtonZ() {
    bool draw_buttonZ = false;

    if (field_0x324 != g_drawHIO.mButtonZScale) {
        field_0x324 = g_drawHIO.mButtonZScale;
        draw_buttonZ = true;
    }

    if (field_0x328 != g_drawHIO.mButtonZPosX) {
        field_0x328 = g_drawHIO.mButtonZPosX;
        draw_buttonZ = true;
    }

    if (field_0x32c != g_drawHIO.mButtonZPosY) {
        field_0x32c = g_drawHIO.mButtonZPosY;
        draw_buttonZ = true;
    }

    if (field_0x384 != g_drawHIO.mButtonZItemScale) {
        field_0x384 = g_drawHIO.mButtonZItemScale;
        draw_buttonZ = true;
    }

    if (field_0x388 != g_drawHIO.mButtonZItemPosX) {
        field_0x388 = g_drawHIO.mButtonZItemPosX;
        draw_buttonZ = true;
    }

    if (field_0x38c != g_drawHIO.mButtonZItemPosY) {
        field_0x38c = g_drawHIO.mButtonZItemPosY;
        draw_buttonZ = true;
    }

    if (field_0x3e4 != g_drawHIO.mButtonZItemBaseScale) {
        field_0x3e4 = g_drawHIO.mButtonZItemBaseScale;
        draw_buttonZ = true;
    }

    if (field_0x3e8 != g_drawHIO.mButtonZItemBasePosX) {
        field_0x3e8 = g_drawHIO.mButtonZItemBasePosX;
        draw_buttonZ = true;
    }

    if (field_0x3ec != g_drawHIO.mButtonZItemBasePosY) {
        field_0x3ec = g_drawHIO.mButtonZItemBasePosY;
        draw_buttonZ = true;
    }

    if (field_0x330 != g_drawHIO.mButtonZFontScale) {
        field_0x330 = g_drawHIO.mButtonZFontScale;
        draw_buttonZ = true;
    }

    if (field_0x334 != g_drawHIO.mButtonZFontPosX) {
        field_0x334 = g_drawHIO.mButtonZFontPosX;
        draw_buttonZ = true;
    }

    if (field_0x338 != g_drawHIO.mButtonZFontPosY) {
        field_0x338 = g_drawHIO.mButtonZFontPosY;
        draw_buttonZ = true;
    }

    if (dComIfGp_getZStatusForce() != 0) {
        dComIfGp_setZStatus(dComIfGp_getZStatusForce(), dComIfGp_getZSetFlagForce());
        dComIfGp_setZStatusForce(0, 0);
    }

#if TARGET_PC
    if (s_extraItemSlotCache != dusk::isExtraItemSlotEnabled()) {
        s_extraItemSlotCache = dusk::isExtraItemSlotEnabled();
        s_zSlotItemCache = 0xFF;
        draw_buttonZ = true;
    }

    if (dusk::isExtraItemSlotEnabled() && !daPy_getPlayerActorClass()->checkWolf()) {
        const u8 zItem = dComIfGp_getSelectItem(SELECT_ITEM_DOWN);
        if (s_zSlotItemCache != zItem) {
            s_zSlotItemCache = zItem;
        }
        // Redraw every frame: drawButtonR hides item_r_n when R status changes
        // (e.g. boomerang lock-on), and the Z icon shares that pane tree.
        draw_buttonZ = true;
    }
#endif

    if (mZStatus != dComIfGp_getZStatus() || draw_buttonZ) {
        mZStatus = dComIfGp_getZStatus();

        if (mZStatus == 0x2D || mZStatus == 0x2E) {
            dComIfGp_setZStatus(mZStatus, 1);
        }

        mpMeterDraw->drawButtonZ(mZStatus);
    }

    dComIfGp_setZStatus(0, 0);

    if (dComIfGp_getBottleStatusForce() != 0) {
        dComIfGp_setBottleStatus(dComIfGp_getBottleStatusForce(),
                                   dComIfGp_getBottleSetFlagForce());
        dComIfGp_setBottleStatusForce(0, 0);
    }

    if (mBottleStatus != dComIfGp_getBottleStatus()) {
        mBottleStatus = dComIfGp_getBottleStatus();
        mpMeterDraw->drawButtonBin(mBottleStatus);
    }
}

void dMeter2_c::moveButton3D() {
    u8 var_r6;

    if (dComIfGp_get3DStatusForce() != 0) {
        var_r6 = dComIfGp_get3DSetFlagForce();
        if (dComIfGp_get3DStatusForce() == 0x6A) {
            var_r6 = 1;
        }

        dComIfGp_set3DStatus(dComIfGp_get3DStatusForce(), dComIfGp_get3DDirectionForce(), var_r6);
        dComIfGp_set3DStatusForce(0, 0, 0);
    }

    if (m3DStatus != dComIfGp_get3DStatus()) {
        m3DStatus = dComIfGp_get3DStatus();

        if (m3DStatus == 0x2D || m3DStatus == 0x2E) {
            dComIfGp_set3DStatus(m3DStatus, 5, 1);
        }

        mpMeterDraw->drawButton3D(m3DStatus);
    }
}

void dMeter2_c::moveButtonC() {
    if (dComIfGp_getCStickStatusForce()) {
        u8 setFlag = dComIfGp_getCStickSetFlagForce();
        u8 dirForce = dComIfGp_getCStickDirectionForce();
        u8 statusForce = dComIfGp_getCStickStatusForce();
        dComIfGp_setCStickStatus(statusForce, dirForce, setFlag);
        dComIfGp_setCStickStatusForce(0, 0, 0);
    }

    if (mCStickStatus == dComIfGp_getCStickStatus() && field_0x1ac == dMeter2Info_isUseButton(0x10))
    {
        if ((!mpMeterDraw->isEmphasisC() || dComIfGp_isCStickSetFlag(2)) &&
            (mpMeterDraw->isEmphasisC() || !dComIfGp_isCStickSetFlag(2)) &&
            field_0x19a == mpMeterDraw->getButtonTimer())
        {
            return;
        }
    }

    field_0x19a = mpMeterDraw->getButtonTimer();
    mCStickStatus = dComIfGp_getCStickStatus();
    field_0x1ac = dMeter2Info_isUseButton(0x10);
    mpMeterDraw->drawButtonC(mCStickStatus, field_0x128 == 0);
}

void dMeter2_c::moveButtonS() {
    if (dComIfGp_getSButtonStatusForce()) {
        u8 setFlag = dComIfGp_getSButtonSetFlagForce();
        u8 statusForce = dComIfGp_getSButtonStatusForce();
        dComIfGp_setSButtonStatus(statusForce, setFlag);
        dComIfGp_setSButtonStatusForce(0, 0);
    } else {
        dComIfGp_setSButtonStatus(0, 0);
    }

    if (mSButtonStatus != dComIfGp_getSButtonStatus()) {
        mSButtonStatus = dComIfGp_getSButtonStatus();
        mpMeterDraw->drawButtonS(mSButtonStatus);
    }
}

void dMeter2_c::moveButtonXY() {
    bool sp8[2];
    bool spC[2];

    for (int i = 0; i < 2; i++) {
        sp8[i] = 0;
        spC[i] = 0;
    }

    if (field_0x33c != g_drawHIO.mButtonXScale) {
        field_0x33c = g_drawHIO.mButtonXScale;
        sp8[0] = 1;
    }

    if (field_0x340 != g_drawHIO.mButtonXPosX) {
        field_0x340 = g_drawHIO.mButtonXPosX;
        sp8[0] = 1;
    }

    if (field_0x344 != g_drawHIO.mButtonXPosY) {
        field_0x344 = g_drawHIO.mButtonXPosY;
        sp8[0] = 1;
    }

    if (field_0x348 != g_drawHIO.mButtonYScale) {
        field_0x348 = g_drawHIO.mButtonYScale;
        sp8[1] = 1;
    }

    if (field_0x34c != g_drawHIO.mButtonYPosX) {
        field_0x34c = g_drawHIO.mButtonYPosX;
        sp8[1] = 1;
    }

    if (field_0x350 != g_drawHIO.mButtonYPosY) {
        field_0x350 = g_drawHIO.mButtonYPosY;
        sp8[1] = 1;
    }

    if (field_0x360 != g_drawHIO.mButtonXItemScale) {
        field_0x360 = g_drawHIO.mButtonXItemScale;
        sp8[0] = 1;
    }

    if (field_0x364 != g_drawHIO.mButtonXItemPosX) {
        field_0x364 = g_drawHIO.mButtonXItemPosX;
        sp8[0] = 1;
    }

    if (field_0x368 != g_drawHIO.mButtonXItemPosY) {
        field_0x368 = g_drawHIO.mButtonXItemPosY;
        sp8[0] = 1;
    }

    if (field_0x36c != g_drawHIO.mButtonYItemScale) {
        field_0x36c = g_drawHIO.mButtonYItemScale;
        sp8[1] = 1;
    }

    if (field_0x370 != g_drawHIO.mButtonYItemPosX) {
        field_0x370 = g_drawHIO.mButtonYItemPosX;
        sp8[1] = 1;
    }

    if (field_0x374 != g_drawHIO.mButtonYItemPosY) {
        field_0x374 = g_drawHIO.mButtonYItemPosY;
        sp8[1] = 1;
    }

    for (int i = 0; i < 2; i++) {
        if (field_0x39c[i] != g_drawHIO.mButtonXItemBaseScale[i]) {
            field_0x39c[i] = g_drawHIO.mButtonXItemBaseScale[i];
            sp8[0] = 1;
        }

        if (field_0x3a4[i] != g_drawHIO.mButtonXItemBasePosX[i]) {
            field_0x3a4[i] = g_drawHIO.mButtonXItemBasePosX[i];
            sp8[0] = 1;
        }

        if (field_0x3ac[i] != g_drawHIO.mButtonXItemBasePosY[i]) {
            field_0x3ac[i] = g_drawHIO.mButtonXItemBasePosY[i];
            sp8[0] = 1;
        }

        if (field_0x3b4[i] != g_drawHIO.mButtonYItemBaseScale[i]) {
            field_0x3b4[i] = g_drawHIO.mButtonYItemBaseScale[i];
            sp8[1] = 1;
        }

        if (field_0x3bc[i] != g_drawHIO.mButtonYItemBasePosX[i]) {
            field_0x3bc[i] = g_drawHIO.mButtonYItemBasePosX[i];
            sp8[1] = 1;
        }

        if (field_0x3c4[i] != g_drawHIO.mButtonYItemBasePosY[i]) {
            field_0x3c4[i] = g_drawHIO.mButtonYItemBasePosY[i];
            sp8[1] = 1;
        }
    }

    if (g_drawHIO.mItemScaleAdjustON && field_0x4bd != g_drawHIO.mItemScalePercent) {
        field_0x4bd = g_drawHIO.mItemScalePercent;

        for (int i = 0; i < 2; i++) {
            sp8[i] = 1;
        }
    }

    if (field_0x3f0 != g_drawHIO.mButtonXYTextScale) {
        field_0x3f0 = g_drawHIO.mButtonXYTextScale;

        for (int i = 0; i < 2; i++) {
            sp8[i] = 1;
        }
    }

    if (field_0x3f4 != g_drawHIO.mButtonXYTextPosX) {
        field_0x3f4 = g_drawHIO.mButtonXYTextPosX;

        for (int i = 0; i < 2; i++) {
            sp8[i] = 1;
        }
    }

    if (field_0x3f8 != g_drawHIO.mButtonXYTextPosY) {
        field_0x3f8 = g_drawHIO.mButtonXYTextPosY;

        for (int i = 0; i < 2; i++) {
            sp8[i] = 1;
        }
    }

    for (int i = 0; i < 3; i++) {
        if (field_0x390[i] != g_drawHIO.mButtonItemRotation[i]) {
            field_0x390[i] = g_drawHIO.mButtonItemRotation[i];
            sp8[i] = 1;
        }

        if (g_drawHIO.mButtonDebug[i]) {
            sp8[i] = 1;
        }

        if (field_0x430 != g_drawHIO.field_0x54c) {
            field_0x430 = g_drawHIO.field_0x54c;
            sp8[i] = 1;
        }
    }

    for (int i = 0; i < 2; i++) {
        if (field_0x128 != 0) {
            if (field_0x128 != field_0x12c) {
                spC[i] = 1;
                sp8[i] = 1;
            }

            if (i == 0) {
                if (dComIfGp_getXStatusForce() != 0) {
                    dComIfGp_setXStatus(dComIfGp_getXStatusForce(), dComIfGp_getXSetFlagForce());
                    dComIfGp_setXStatusForce(0, 0);
                }

                if (mItemStatus[i * 2 + 1] != dComIfGp_getXStatus()) {
                    mItemStatus[i * 2 + 1] = dComIfGp_getXStatus();

                    if (mItemStatus[i * 2 + 1] == 0x2D || mItemStatus[i * 2 + 1] == 0x2E) {
                        dComIfGp_setXStatus(mItemStatus[i * 2 + 1], 1);
                    }

                    spC[i] = 1;
                    sp8[i] = 1;
                }

                if ((&mXSetFlag)[i] != dComIfGp_isXSetFlag(2)) {
                    (&mXSetFlag)[i] = dComIfGp_isXSetFlag(2);
                    spC[i] = 1;
                    sp8[i] = 1;
                }
            } else if (i == 1) {
                if (dComIfGp_getYStatusForce() != 0) {
                    dComIfGp_setYStatus(dComIfGp_getYStatusForce(), dComIfGp_getYSetFlagForce());
                    dComIfGp_setYStatusForce(0, 0);
                }

                if (mItemStatus[i * 2 + 1] != dComIfGp_getYStatus()) {
                    mItemStatus[i * 2 + 1] = dComIfGp_getYStatus();

                    if (mItemStatus[i * 2 + 1] == 0x2D || mItemStatus[i * 2 + 1] == 0x2E) {
                        dComIfGp_setYStatus(mItemStatus[i * 2 + 1], 1);
                    }

                    spC[i] = 1;
                    sp8[i] = 1;
                }

                if ((&mXSetFlag)[i] != dComIfGp_isYSetFlag(2)) {
                    (&mXSetFlag)[i] = dComIfGp_isYSetFlag(2);
                    spC[i] = 1;
                    sp8[i] = 1;
                }
            }
        } else {
            if (mItemStatus[2 * i] != dComIfGp_getSelectItem(i) || field_0x128 != field_0x12c) {
                mItemStatus[2 * i] = dComIfGp_getSelectItem(i);
                spC[i] = 1;
                sp8[i] = 1;
            }

            if (field_0x1d6[i] != dMeter2Info_isDirectUseItem(i)) {
                field_0x1d6[i] = dMeter2Info_isDirectUseItem(i);
                spC[i] = 1;
                sp8[i] = 1;
            }

            if (mItemStatus[2 * i] == 0x48) {
                if (field_0x1ec == 0) {
                    if (dComIfGs_getOil() != 0) {
                        field_0x1ec = 1;
                        sp8[i] = 1;
                    }
                } else {
                    if (dComIfGs_getOil() == 0) {
                        field_0x1ec = 0;
                        sp8[i] = 1;
                    }
                }
            } else if (mItemStatus[2 * i] == 0x46) {
                if (field_0x1ed == 0) {
                    if (daPy_getPlayerActorClass()->checkCopyRodTopUse()) {
                        field_0x1ed = 1;
                        sp8[i] = 1;
                    }
                } else {
                    if (!daPy_getPlayerActorClass()->checkCopyRodTopUse()) {
                        field_0x1ed = 0;
                        sp8[i] = 1;
                    }
                }
            } else if (mItemStatus[2 * i] == 0x50 || mItemStatus[2 * i] == 0x70 ||
                       mItemStatus[2 * i] == 0x71 || mItemStatus[2 * i] == 0x72)
            {
                if (sp8[i] != 0) {
                    mpMeterDraw->setItemNum(i, dComIfGp_getSelectItemNum(i),
                                            dComIfGp_getSelectItemMaxNum(i));
                }
            } else if (mItemStatus[2 * i] == 0x76) {
                if (field_0x1d8[i] != dComIfGp_getSelectItemNum(i)) {
                    field_0x1d8[i] = dComIfGp_getSelectItemNum(i);
                    sp8[i] = 1;
                }

                if (sp8[i] != 0) {
                    mpMeterDraw->setItemNum(i, dComIfGp_getSelectItemNum(i),
                                            dComIfGp_getSelectItemMaxNum(i));
                }
            } else if (mItemStatus[2 * i] == 0x43 || mItemStatus[2 * i] == 0x53 ||
                       mItemStatus[2 * i] == 0x54 || mItemStatus[2 * i] == 0x55 ||
                       mItemStatus[2 * i] == 0x56 || mItemStatus[2 * i] == 0x5A)
            {
                if (sp8[i] != 0) {
                    mpMeterDraw->setItemNum(i, mArrowNum, dComIfGs_getArrowMax());
                }
            } else if (mItemStatus[2 * i] == 0x4B) {
                if (sp8[i] != 0) {
                    mpMeterDraw->setItemNum(i, mPachinkoNum, dComIfGs_getPachinkoMax());
                }
            } else if (mItemStatus[2 * i] == 0x59) {
                u8 var_r25 = dComIfGp_getSelectItemNum(i);
                u8 var_r6_2 = dComIfGp_getSelectItemMaxNum(i);

                if (var_r25 > mArrowNum) {
                    var_r25 = mArrowNum;
                }

                if (var_r6_2 < dComIfGs_getArrowMax()) {
                    var_r6_2 = dComIfGs_getArrowMax();
                }

                mpMeterDraw->setItemNum(i, var_r25, var_r6_2);
            }
        }

        if (sp8[i] != 0) {
            mpMeterDraw->drawButtonXY(i, mItemStatus[2 * i], mItemStatus[i * 2 + 1],
                                      field_0x128 == 0 ? true : false, spC[i]);
        }
    }

    alphaAnimeButton();
    dMeter2Info_resetDirectUseItem();
}

void dMeter2_c::moveButtonCross() {
    f32 temp_f1;
    f32 temp_f30;
    f32 temp_f31;
    f32 var_f31;
    bool draw_cross;

    draw_cross = false;
    var_f31 = 0.0f;

    if (mButtonCrossOFFPosX != g_drawHIO.mButtonCrossOFFPosX) {
        mButtonCrossOFFPosX = g_drawHIO.mButtonCrossOFFPosX;
        draw_cross = true;
    }

    if (mButtonCrossOFFPosY != g_drawHIO.mButtonCrossOFFPosY) {
        mButtonCrossOFFPosY = g_drawHIO.mButtonCrossOFFPosY;
        draw_cross = true;
    }

    if (mButtonCrossONPosX != g_drawHIO.mButtonCrossONPosX) {
        mButtonCrossONPosX = g_drawHIO.mButtonCrossONPosX;
        draw_cross = true;
    }

    if (mButtonCrossONPosY != g_drawHIO.mButtonCrossONPosY) {
        mButtonCrossONPosY = g_drawHIO.mButtonCrossONPosY;
        draw_cross = true;
    }

    if (mButtonCrossScale != g_drawHIO.mButtonCrossScale) {
        mButtonCrossScale = g_drawHIO.mButtonCrossScale;
        draw_cross = true;
    }

    if (mButtonCrossTextScale != g_drawHIO.mButtonCrossTextScale) {
        mButtonCrossTextScale = g_drawHIO.mButtonCrossTextScale;
        draw_cross = true;
    }

    if (mpMap != NULL) {
        temp_f31 = mpMap->getMapDispEdgeTop();
        temp_f1 = (temp_f31 - mpMeterDraw->getButtonCrossParentInitTransY()) - 15.0f;

        if (mpMap->isDispPosInsideFlg()) {
            if (field_0x1b4 < g_drawHIO.mButtonCrossMoveFrame) {
                field_0x1b4++;
                draw_cross = true;
            } else {
                field_0x1b4 = g_drawHIO.mButtonCrossMoveFrame;
            }

            var_f31 = mButtonCrossONPosY + temp_f1;
        } else {
            if (field_0x1b4 > 0) {
                field_0x1b4--;
                draw_cross = true;
            } else {
                field_0x1b4 = 0;
            }

            var_f31 = mButtonCrossOFFPosY;
        }
    }

    temp_f30 = mButtonCrossOFFPosX + (((f32)field_0x1b4 / (f32)g_drawHIO.mButtonCrossMoveFrame) *
                                     (mButtonCrossONPosX - mButtonCrossOFFPosX));
    if (field_0x15c != var_f31) {
        cLib_addCalc2(&field_0x15c, var_f31, 0.5f, 50.0f);
        if ((f32)fabs(field_0x15c - var_f31) < 0.5f) {
            field_0x15c = var_f31;
        }
        draw_cross = true;
    }

#if TARGET_PC
    // ============================================
    // NEW CODE — ALBW Port (Lies of Link HUD)
    // drawButtonCross() carries the LoP show/hide of the whole cross cluster but
    // only runs while the cross is animating. Force a re-layout on toggle change so
    // the cross hides/restores without needing an item/map open animation.
    // ============================================
    {
        static bool sCrossLopWasOn = false;
        const bool lopOn = dusk::getSettings().game.lopHud.getValue();
        if (sCrossLopWasOn != lopOn) {
            sCrossLopWasOn = lopOn;
            draw_cross = true;
        }
    }
    // ============================================
    // NEW CODE ENDS HERE
    // ============================================
#endif

    if (draw_cross == true) {
        mpMeterDraw->drawButtonCross(temp_f30, field_0x15c);
    }

    alphaAnimeButtonCross();
}

void dMeter2_c::moveTouchSubMenu() {}

void dMeter2_c::moveSubContents() {
    JKRHeap* heap = mDoExt_setCurrentHeap(mpSubHeap);
    checkSubContents();

    if (mpSubContents != NULL) {
        mpSubContents->_execute(mStatus);
    }

    if (mpSubSubContents != NULL) {
        if (mSubContentType == 5 && mSubContentsStringType != dMeter2Info_getMeterStringType()) {
            mSubContentsStringType = dMeter2Info_getMeterStringType();
            if (mSubContentsStringType != 0) {
                mpSubSubContents->createString(mSubContentsStringType);
            }
        }
        mpSubSubContents->_execute(mStatus);
    }

    mDoExt_setCurrentHeap(heap);
}

void dMeter2_c::move2DContents() {
    bool var_r19;
    bool var_r27;
    bool var_r20;
    bool var_r28;
    bool var_r21;
    bool var_r22;
    bool var_r23;
    bool var_r24;
    bool var_r25;
    bool var_r29;
    bool var_r26;
    u8 var_r30;

    field_0x201 = 0;
    field_0x108 = NULL;

    JKRHeap* temp_r3 = dComIfGp_getSubHeap2D(8);
    if (temp_r3 != NULL) {
        field_0x108 = mDoExt_setCurrentHeap(temp_r3);
    }

    check2DContents();

    if (mpEmpButton != NULL) {
        var_r19 = 0;
        var_r27 = 0;
        var_r20 = 0;
        var_r28 = 0;
        var_r21 = 0;
        var_r22 = 0;
        var_r23 = 0;
        var_r24 = 0;
        var_r25 = 0;
        var_r29 = 0;
        var_r26 = 0;
        var_r30 = 2;

        if (mpMeterDraw->isEmphasisS() && mpEmpButton->isSetButton(6)) {
            mpEmpButton->setString(mpMeterDraw->getActionString(mSButtonStatus, 0, NULL), 6, 0, 0);
            var_r23 = 1;
            var_r30 = 0;
        }

        if (mpMeterDraw->isEmphasisB() && mpMeterDraw->isEmphasis3D() && var_r30 != 0) {
            if (mAStatus == m3DStatus) {
                mpEmpButton->setString(mpMeterDraw->getActionString(mAStatus, 0, NULL), 13,
                                       2 - var_r30, 0);
                var_r29 = 1;
                var_r30 -= 2;
            }
        }

        if (var_r29 == 0) {
            if (mpMeterDraw->isEmphasis3D() && var_r30 != 0 && mpEmpButton->isSetButton(4)) {
                mpEmpButton->setString(mpMeterDraw->getActionString(m3DStatus, 0, NULL), 4,
                                       2 - var_r30, 0);
                var_r21 = 1;
                var_r30 -= 1;
            }
        }

        if (mpMeterDraw->isEmphasisC() && var_r30 != 0 && mpEmpButton->isSetButton(5)) {
            mpEmpButton->setString(mpMeterDraw->getActionString(mCStickStatus, 0, NULL), 5,
                                   2 - var_r30, 0);
            var_r22 = 1;
            var_r30 -= 1;
        }

        if (var_r29 == 0) {
            if (mpMeterDraw->isEmphasisB() && var_r30 != 0 && mpEmpButton->isSetButton(1)) {
                mpEmpButton->setString(mpMeterDraw->getActionString(mAStatus, 0, NULL), 1,
                                       2 - var_r30, mAStatus == 0x4F ? true : false);
                var_r27 = 1;
                var_r30 -= 1;
            }
        }

        if (mpMeterDraw->isEmphasisY() && var_r30 != 0 && mpEmpButton->isSetButton(8)) {
            mpEmpButton->setString(mpMeterDraw->getActionString(mItemStatus[3], 0, NULL), 8,
                                   2 - var_r30, 0);
            var_r25 = 1;
            var_r30 -= 1;
        }

        if (mpMeterDraw->isEmphasisA() && var_r30 != 0 && mpEmpButton->isSetButton(0)) {
            mpEmpButton->setString(mpMeterDraw->getActionString(mDoStatus, 0, NULL), 0, 2 - var_r30,
                                   0);
            var_r19 = 1;
            var_r30 -= 1;
        }

        if (mpMeterDraw->isEmphasisZ() && var_r30 != 0 && mpEmpButton->isSetButton(3)) {
            if (mZStatus == 8) {
                mpEmpButton->setString(mpMeterDraw->getActionString(100, 0, NULL), 3, 2 - var_r30,
                                       0);
            } else {
                mpEmpButton->setString(mpMeterDraw->getActionString(mZStatus, 0, NULL), 3,
                                       2 - var_r30, 0);
            }

            var_r28 = 1;
            var_r30 -= 1;
        }

        if (mpMeterDraw->isEmphasisR() && var_r30 != 0 && mpEmpButton->isSetButton(2)) {
            mpEmpButton->setString(mpMeterDraw->getActionString(mRStatus, 0, NULL), 2, 2 - var_r30,
                                   0);
            var_r20 = 1;
            var_r30 -= 1;
        }

        if (mpMeterDraw->isEmphasisX() && var_r30 != 0 && mpEmpButton->isSetButton(7)) {
            mpEmpButton->setString(mpMeterDraw->getActionString(mItemStatus[1], 0, NULL), 7,
                                   2 - var_r30, 0);
            var_r24 = 1;
            var_r30 -= 1;
        }

        if (mpMeterDraw->isEmphasisBin() && var_r30 != 0 && mpEmpButton->isSetButton(21) &&
            !dMeter2Info_is2DActiveTouchArea())
        {
            mpEmpButton->setString(mpMeterDraw->getActionString(mBottleStatus, 0, NULL), 21,
                                   2 - var_r30, 0);
            var_r26 = 1;
        }

        mpEmpButton->_execute(mStatus, var_r19, var_r27, var_r20, var_r28, var_r21, var_r22,
                              var_r23, var_r24, var_r25, false, false, false, false, var_r29, false,
                              false, false, false, false, false, false, var_r26);

        if ((var_r19 != 0) || (var_r27 != 0) || (var_r20 != 0) || (var_r28 != 0) ||
            (var_r21 != 0) || (var_r22 != 0) || (var_r23 != 0) || (var_r24 != 0) ||
            (var_r25 != 0) || (var_r29 != 0) || (var_r26 != 0))
        {
            field_0x201 = 1;
        }
    }

    if (field_0x108 != NULL) {
        mDoExt_setCurrentHeap(field_0x108);
    }
}

void dMeter2_c::checkSubContents() {
    if (mStatus & 0x80) {
        killSubContents(4);

        if (mSubContentType == 0) {
            mpSubContents = JKR_NEW dScope_c(0);
            mSubContentType = 4;
        }
        return;
    } else if (mStatus & 0x2000000) {
        if (strcmp(dComIfGp_getStartStageName(), "F_SP103") &&
            (strcmp(dComIfGp_getStartStageName(), "F_SP00") || dComIfG_play_c::getLayerNo(0) != 5))
        {
            killSubContents(1);

            if (mSubContentType == 0) {
                mpSubHeap->getTotalFreeSize();
                mpSubContents = JKR_NEW dMeterHakusha_c(mpMeterDraw->getMainScreenPtr());
                mSubContentType = 1;
            }
            return;
        }
    }

    if (daPy_getPlayerActorClass()->getSumouMode() != 0) {
        killSubContents(5);

        if (mSubContentType == 0) {
            mpSubContents = JKR_NEW dMeterHaihai_c(0);
            mpSubSubContents = JKR_NEW dMeterString_c(dMeter2Info_getMeterStringType());
            mSubContentType = 5;
            mSubContentsStringType = dMeter2Info_getMeterStringType();
        }
    } else if (mStatus & 0x200) {
        killSubContents(2);

        if (mSubContentType == 0) {
            mpSubContents = JKR_NEW dMeterHaihai_c(0);
            mSubContentType = 2;
        }
    } else if (dMeter2Info_getMeterStringType() != 0) {
        killSubContents(3);

        if (mSubContentType == 0) {
            mpSubContents = JKR_NEW dMeterString_c(dMeter2Info_getMeterStringType());
            mSubContentType = 3;
        }
    } else if (mSubContentType == 4) {
        if (mpSubContents != NULL || mpSubSubContents != NULL) {
            bool free_heap = false;

            if (mpSubContents != NULL && mpSubContents->isDead()) {
                JKR_DELETE(mpSubContents);
                mpSubContents = NULL;
                free_heap = true;
            }

            if (mpSubSubContents != NULL && mpSubSubContents->isDead()) {
                JKR_DELETE(mpSubSubContents);
                mpSubSubContents = NULL;
                free_heap = true;
            }

            if (free_heap) {
                mpSubHeap->freeAll();
                mSubContentType = 0;
                mSubContentsStringType = 0;
            }
        } else {
            mSubContentType = 0;
        }
    } else if (mSubContentType != 0) {
        bool free_heap = false;

        if (mpSubContents != NULL) {
            JKR_DELETE(mpSubContents);
            mpSubContents = NULL;
            free_heap = true;
        }

        if (mpSubSubContents != NULL) {
            JKR_DELETE(mpSubSubContents);
            mpSubSubContents = NULL;
            free_heap = true;
        }

        if (free_heap) {
            mpSubHeap->freeAll();
        }

        mSubContentType = 0;
        mSubContentsStringType = 0;
    }
}

void dMeter2_c::check2DContents() {
    if (mpEmpButton == NULL) {
        if ((dComIfGp_isHeapLockFlag() == 0 || dComIfGp_isHeapLockFlag() == 5) &&
            (dMeter2Info_isFloatingMessageVisible() || mpMeterDraw->isEmphasisA() ||
             mpMeterDraw->isEmphasisB() || mpMeterDraw->isEmphasisR() ||
             mpMeterDraw->isEmphasisZ() || mpMeterDraw->isEmphasis3D() ||
             mpMeterDraw->isEmphasisC() || mpMeterDraw->isEmphasisS() ||
             mpMeterDraw->isEmphasisX() || mpMeterDraw->isEmphasisY() ||
             mpMeterDraw->isEmphasisBin()))
        {
            dComIfGp_setHeapLockFlag(8);

            if (field_0x108 == NULL) {
                field_0x108 = mDoExt_setCurrentHeap(dComIfGp_getSubHeap2D(8));
            }

            mpEmpButton = JKR_NEW dMeterButton_c();
        }
    } else if (dComIfGp_isHeapLockFlag() == 5 && !dMeter2Info_isFloatingMessageVisible()) {
        if (!mpMeterDraw->isEmphasisA() && !mpMeterDraw->isEmphasisB() &&
            !mpMeterDraw->isEmphasisR() && !mpMeterDraw->isEmphasisZ() &&
            !mpMeterDraw->isEmphasis3D() && !mpMeterDraw->isEmphasisC() &&
            !mpMeterDraw->isEmphasisS() && !mpMeterDraw->isEmphasisX() &&
            !mpMeterDraw->isEmphasisY() && !mpMeterDraw->isEmphasisBin() && mpEmpButton->isClose())
        {
            JKR_DELETE(mpEmpButton);
            mpEmpButton = NULL;
            dComIfGp_getSubHeap2D(8)->freeAll();

            if (field_0x108 != NULL) {
                mDoExt_setCurrentHeap(field_0x108);
                field_0x108 = NULL;
            }

            dComIfGp_offHeapLockFlag(8);
        }
    }
}

void dMeter2_c::moveBombNum() {
    u8 temp_r28;
    u8 temp_r31;

    for (int i = 0; i < 3; i++) {
        s16 var_r22;
        temp_r31 = dComIfGs_getItem((u8)(i + SLOT_15), true);
        temp_r28 = dComIfGs_getItem((u8)(i + SLOT_15), false);

        if (temp_r31 != dItemNo_NONE_e && temp_r31 != dItemNo_BOMB_BAG_LV1_e) {
            if (g_mwHIO.getBombFlag() ||
                (dMeter2Info_getMiniGameItemSetFlag() == 1 && i == dMeter2Info_getRentalBombBag()))
            {
                if (dComIfGs_getBombMax(temp_r28) != dComIfGs_getBombNum(i)) {
                    dComIfGp_setItemBombNumCount(i, dComIfGs_getBombMax(temp_r28));
                }

                if (dComIfGp_getItemBombNumCount(i) < 0) {
                    dComIfGp_clearItemBombNumCount(i);
                }
            }

            if (dComIfGp_getItemBombNumCount(i) != 0 || mBombNum[i] != dComIfGs_getBombNum(i) ||
                mBombMax[i] != dComIfGs_getBombMax(temp_r28))
            {
                var_r22 = dComIfGs_getBombNum(i) + dComIfGp_getItemBombNumCount(i);
                dComIfGp_clearItemBombNumCount(i);

                if (var_r22 < 0) {
                    var_r22 = 0;
                }

                if (var_r22 > dComIfGs_getBombMax(temp_r28)) {
                    var_r22 = dComIfGs_getBombMax(temp_r28);
                }

                if (var_r22 == 0) {
                    if (temp_r31 == dItemNo_BOMB_ARROW_e) {
                        for (int j = 0; j < 2; j++) {
                            if (i + SLOT_15 == dComIfGs_getSelectItemIndex(j) ||
                                i + SLOT_15 == dComIfGs_getMixItemIndex(j))
                            {
                                dComIfGs_setMixItemIndex(j, 0xFF);
                                dComIfGs_setSelectItemIndex(j, 4);
                                dComIfGp_setSelectItem(j);
                            }
                        }
                    }
                    dComIfGs_setItem(i + SLOT_15, dItemNo_BOMB_BAG_LV1_e);
                    dComIfGp_setItem(i + SLOT_15, dItemNo_BOMB_BAG_LV1_e);

                    for (int j = 0; j < 2; j++) {
                        if (i + SLOT_15 == dComIfGs_getSelectMixItemNoArrowIndex(j)) {
                            dComIfGp_setSelectItem(j);
                        }
                    }
                }

                dComIfGs_setBombNum(i, var_r22);
                mBombMax[i] = dComIfGs_getBombMax(temp_r28);

                if (temp_r31 != dItemNo_BOMB_ARROW_e) {
                    for (int j = 0; j < 2; j++) {
                        if (i + SLOT_15 == dComIfGs_getSelectMixItemNoArrowIndex(j)) {
                            mpMeterDraw->setItemNum(j, dComIfGp_getSelectItemNum(j),
                                                    dComIfGp_getSelectItemMaxNum(j));
                        }
                    }
                }
            }
        } else {
            if (temp_r31 != dItemNo_BOMB_BAG_LV1_e) {
                dComIfGs_setBombNum(i, 0);
            }

            if (dComIfGp_getItemBombNumCount(i) != 0) {
                dComIfGp_clearItemBombNumCount(i);
            }
        }

        if (mBombNum[i] != dComIfGs_getBombNum(i)) {
            mBombNum[i] = dComIfGs_getBombNum(i);
        }
    }

    for (int i = 0; i < 2; i++) {
        if (mItemMaxNum[i] != dComIfGs_getSelectItemIndex(i)) {
            for (int j = 0; j < 3; j++) {
                if (j + SLOT_15 == dComIfGs_getSelectItemIndex(i)) {
                    mpMeterDraw->setItemNum(i, dComIfGp_getSelectItemNum(i),
                                            dComIfGp_getSelectItemMaxNum(i));
                }
            }

            mItemMaxNum[i] = dComIfGs_getSelectItemIndex(i);
        }
    }
}

void dMeter2_c::moveBottleNum() {
    for (int i = 0; i < 4; i++) {
        if (dComIfGs_getItem((u8)(i + SLOT_11), true) == dItemNo_BEE_CHILD_e) {
            if (mBottleNum[i] != dComIfGs_getBottleNum(i)) {
                for (int j = 0; j < 2; j++) {
                    if (i + SLOT_11 == dComIfGs_getSelectItemIndex(j)) {
                        mpMeterDraw->setItemNum(j, dComIfGp_getSelectItemNum(j),
                                                dComIfGp_getSelectItemMaxNum(j));
                        mBottleNum[i] = dComIfGs_getBottleNum(i);
                    }
                }
            }
        }
    }
}

void dMeter2_c::moveArrowNum() {
    s16 var_r6;
    int var_r28;
    int i;
    u8 var_r27;
    u8 var_r6_2;

    if (g_mwHIO.getArrowFlag() || dMeter2Info_getMiniGameItemSetFlag() == 1) {
        if (dComIfGs_getArrowMax() != dComIfGs_getArrowNum()) {
            dComIfGp_setItemArrowNumCount(dComIfGs_getArrowMax());
        }

        if (dComIfGp_getItemArrowNumCount() < 0) {
            dComIfGp_clearItemArrowNumCount();
        }
    }

    if (dComIfGp_getItemArrowNumCount() != 0 || mItemMaxNum[2] != dComIfGs_getArrowMax() ||
        mArrowNum != dComIfGs_getArrowNum())
    {
        var_r6 = dComIfGs_getArrowNum() + dComIfGp_getItemArrowNumCount();
        dComIfGp_clearItemArrowNumCount();

        if (var_r6 < 0) {
            var_r6 = 0;
        }

        if (var_r6 > dComIfGs_getArrowMax()) {
            var_r6 = dComIfGs_getArrowMax();
        }

        dComIfGs_setArrowNum(var_r6);
        mItemMaxNum[2] = dComIfGs_getArrowMax();

        if (mArrowNum < dComIfGs_getArrowNum()) {
            mArrowNum++;
            onArrowSoundBit(2);

            if (isArrowSoundBit(2)) {
                if (mArrowNum != dComIfGs_getArrowNum()) {
                    if (!isArrowSoundBit(0) && isArrowEquip() && mpMeterDraw->isButtonVisible()) {
                        onArrowSoundBit(0);
                        mDoAud_seStart(Z2SE_CONSUM_INC_CNT_1, NULL, 0, 0);
                    } else {
                        offArrowSoundBit(0);
                    }
                } else {
                    if (isArrowEquip() && mpMeterDraw->isButtonVisible()) {
                        mDoAud_seStart(Z2SE_CONSUM_INC_CNT_2, NULL, 0, 0);
                    }
                    offArrowSoundBit(2);
                    offArrowSoundBit(0);
                }
            }
        } else if (mArrowNum > dComIfGs_getArrowNum()) {
            mArrowNum--;
        }

        i = 0;
        var_r28 = 0;
        for (; i < 2; i++, var_r28 += 2) {
            if (mItemStatus[var_r28] == dItemNo_BOW_e || mItemStatus[var_r28] == dItemNo_LIGHT_ARROW_e ||
                mItemStatus[var_r28] == dItemNo_ARROW_LV1_e || mItemStatus[var_r28] == dItemNo_ARROW_LV2_e ||
                mItemStatus[var_r28] == dItemNo_ARROW_LV3_e || mItemStatus[var_r28] == dItemNo_HAWK_ARROW_e)
            {
                mpMeterDraw->setItemNum(i, mArrowNum, dComIfGs_getArrowMax());
            } else if (mItemStatus[var_r28] == dItemNo_PACHINKO_e) {
                mpMeterDraw->setItemNum(i, mPachinkoNum, dComIfGs_getPachinkoMax());
            } else if (mItemStatus[var_r28] == dItemNo_BOMB_ARROW_e) {
                var_r27 = dComIfGp_getSelectItemNum(i);
                var_r6_2 = dComIfGp_getSelectItemMaxNum(i);

                if (var_r27 > mArrowNum) {
                    var_r27 = mArrowNum;
                }

                if (var_r6_2 < dComIfGs_getArrowMax()) {
                    var_r6_2 = dComIfGs_getArrowMax();
                }

                mpMeterDraw->setItemNum(i, var_r27, var_r6_2);
            }
        }
    }
}

void dMeter2_c::movePachinkoNum() {
    if (g_mwHIO.getPachinkoFlag()) {
        if (dComIfGs_getPachinkoNum() != dComIfGs_getPachinkoMax()) {
            u8 max = dComIfGs_getPachinkoMax();
            dComIfGp_setItemPachinkoNumCount(max);
        }

        if (dComIfGp_getItemPachinkoNumCount() < 0) {
            dComIfGp_clearItemPachinkoNumCount();
        }
    }

    if (dComIfGp_getItemPachinkoNumCount() == 0 && mItemMaxNum[3] == dComIfGs_getPachinkoMax() &&
        mPachinkoNum == dComIfGs_getPachinkoNum())
    {
        return;
    }

    s16 pachinko_num = dComIfGs_getPachinkoNum() + dComIfGp_getItemPachinkoNumCount();
    dComIfGp_clearItemPachinkoNumCount();
    if (pachinko_num < 0) {
        pachinko_num = 0;
    }

    if (dComIfGs_getPachinkoMax() < pachinko_num) {
        pachinko_num = dComIfGs_getPachinkoMax();
    }

    dComIfGs_setPachinkoNum(pachinko_num);
    mItemMaxNum[3] = dComIfGs_getPachinkoMax();

    if (mPachinkoNum < dComIfGs_getPachinkoNum()) {
        mPachinkoNum++;
        onArrowSoundBit(2);

        if (isArrowSoundBit(2)) {
            if (mPachinkoNum != dComIfGs_getPachinkoNum()) {
                if (!isArrowSoundBit(0) && isPachinkoEquip() && mpMeterDraw->isButtonVisible()) {
                    onArrowSoundBit(0);
                    mDoAud_seStart(Z2SE_CONSUM_INC_CNT_1, 0, 0, 0);
                } else {
                    offArrowSoundBit(0);
                }
            } else {
                if (isPachinkoEquip() && mpMeterDraw->isButtonVisible()) {
                    mDoAud_seStart(Z2SE_CONSUM_INC_CNT_2, 0, 0, 0);
                }
                offArrowSoundBit(2);
                offArrowSoundBit(0);
            }
        }
    } else if (mPachinkoNum > dComIfGs_getPachinkoNum()) {
        mPachinkoNum--;
    }

    for (int i = 0; i < 2; i++) {
        if (mItemStatus[i * 2] == dItemNo_PACHINKO_e) {
            mpMeterDraw->setItemNum(i, mPachinkoNum, dComIfGs_getPachinkoMax());
        }
    }
}

void dMeter2_c::alphaAnimeLife() {
#if TARGET_PC
    const bool hideHudForALBWShop = dALBWRental_isOpen();
#else
    const bool hideHudForALBWShop = false;
#endif
    if ((mStatus & 0x4000) ||
        ((mStatus & 0x40) && dComIfGp_event_checkHind(0x10) &&
         !dComIfGp_checkPlayerStatus1(0, 0x2000)) ||
        ((daPy_getPlayerActorClass()->getSumouMode() != 0) || (mStatus & 0x100000) ||
         (mStatus & 0x80000000) || (mStatus & 8) || (mStatus & 0x10) || (mStatus & 0x01000000) ||
         (mStatus & 0x20) || (mStatus & 0x04000000) || (mStatus & 0x08000000) ||
         (mStatus & 0x10000000)) || hideHudForALBWShop)
    {
        mpMeterDraw->setAlphaLifeAnimeMin();
    } else {
        mpMeterDraw->setAlphaLifeAnimeMax();
        mDoAud_heartGaugeOn();
    }

    mpMeterDraw->setAlphaLifeChange(false);
}

void dMeter2_c::alphaAnimeKantera() {
#if TARGET_PC
    // ============================================
    // MODIFIED CODE — ALBW Port
    // Original: meter showed only when lantern occupied SLOT_1 with oil > 0.
    // On PC: meter shows whenever any ALBW meter-using item is equipped on
    // X or Y, completely independent of lantern state or whether the lantern
    // has been obtained. All other hide conditions (cutscenes, menus, etc.)
    // are preserved unchanged below.
    // ============================================
    auto isALBWItem = [](u8 item) -> bool {
        switch (item) {
        case dItemNo_PACHINKO_e:     // Slingshot
        case dItemNo_BOOMERANG_e:    // Boomerang
        case dItemNo_BOW_e:          // Bow
        case dItemNo_LIGHT_ARROW_e:  // Light Arrows
        case dItemNo_ARROW_LV1_e:    // Fire Arrows
        case dItemNo_ARROW_LV2_e:    // Ice Arrows
        case dItemNo_ARROW_LV3_e:    // Shadow Arrows
        case dItemNo_HAWK_ARROW_e:   // Hawkeye + Bow
        case dItemNo_BOMB_ARROW_e:   // Bomb Arrows
        case dItemNo_BOMB_BAG_LV1_e: // Bomb Bag Lv1
        case dItemNo_BOMB_BAG_LV2_e: // Bomb Bag Lv2
        case dItemNo_BOMB_IN_BAG_e:  // Bombs in Bag
        case dItemNo_NORMAL_BOMB_e:  // Normal Bombs
        case dItemNo_WATER_BOMB_e:   // Water Bombs
        case dItemNo_POKE_BOMB_e:    // Bomblings
        case dItemNo_HOOKSHOT_e:     // Clawshot
        case dItemNo_W_HOOKSHOT_e:   // Double Clawshot
        case dItemNo_SPINNER_e:      // Spinner
        case dItemNo_IRONBALL_e:     // Ball and Chain
        case dItemNo_COPY_ROD_e:     // Dominion Rod
        case dItemNo_COPY_ROD_2_e:   // Dominion Rod (powered)
        case dItemNo_ARMOR_e:        // Magic Armor
            return true;
        default:
            return false;
        }
    };
    bool noALBWItem = !isALBWItem(mItemStatus[X_ITEM]) && !isALBWItem(mItemStatus[Y_ITEM]);
    if (noALBWItem ||
#else
    if (dComIfGs_getMaxOil() == 0 || dComIfGs_getItem(SLOT_1, true) != dItemNo_KANTERA_e ||
        !daPy_getPlayerActorClass()->checkUseKandelaar(0) ||
#endif
        (mStatus & 0x4000) ||
        ((mStatus & 0x40) && dComIfGp_event_checkHind(0x400)) || dComIfGp_getOxygenShowFlag() ||
        ((daPy_getPlayerActorClass()->getSumouMode() != 0) ||
         (daPy_getPlayerActorClass()->checkCanoeSlider() &&
          (dComIfG_getTimerMode() == 3 || dComIfG_getTimerMode() == 4)) ||
         (mStatus & 0x100000) || (mStatus & 0x80000000) || (mStatus & 8) || (mStatus & 0x10) ||
         (mStatus & 0x01000000) || (mStatus & 0x20) || (mStatus & 0x04000000) ||
         (mStatus & 0x08000000) || (mStatus & 0x10000000)))
    {
        mpMeterDraw->setAlphaKanteraAnimeMin();
    } else {
        mpMeterDraw->setAlphaKanteraAnimeMax();
    }
#if TARGET_PC
    // ============================================
    // MODIFIED CODE ENDS HERE
    // ============================================
#endif

    mpMeterDraw->setAlphaKanteraChange(true);
#if TARGET_PC
    // ============================================
    // NEW CODE — ALBW Port
    // Drive the ALBW stamina meter (magic screen, mMeterAlphaRate[0])
    // with the same HUD-hide conditions as all other meters so it fades
    // in/out with the HUD during cutscenes, menus, and other states where
    // game elements should not be on screen.
    //
    // Unlike the kantera meter this does NOT check noALBWItem — the ALBW
    // meter is relevant regardless of what items are in the X/Y slots
    // because sword attacks and agility also drain it.  It therefore shows
    // whenever the player is in normal gameplay and hides under exactly the
    // same conditions that suppress the life gauge and rupee counter.
    // ============================================
    if ((mStatus & 0x4000) ||
        ((mStatus & 0x40) && dComIfGp_event_checkHind(0x400)) ||
        dComIfGp_getOxygenShowFlag() ||
        (daPy_getPlayerActorClass()->getSumouMode() != 0) ||
        (daPy_getPlayerActorClass()->checkCanoeSlider() &&
         (dComIfG_getTimerMode() == 3 || dComIfG_getTimerMode() == 4)) ||
        (mStatus & 0x100000) || (mStatus & 0x80000000) || (mStatus & 8) || (mStatus & 0x10) ||
        (mStatus & 0x01000000) || (mStatus & 0x20) || (mStatus & 0x04000000) ||
        (mStatus & 0x08000000) || (mStatus & 0x10000000) ||
        // ============================================
        // NEW CODE — ALBW Port
        // Hide the ALBW stamina meter while Link is in wolf form.
        // Covers both scripted Wolf Link sections and manual transformation.
        // The existing animation system fades alpha smoothly to 0 on enter
        // and back to full on return to human — no extra state needed.
        // Meter state (value, lockout, recovery) is unaffected; only alpha
        // changes. Wolf Link sections are already gameplay-unaffected per
        // the "not touching Wolf Link with ALBW meter" constraint.
        // ============================================
        dMeter2_isWolfForm() || dMeter2_hideHudForALBWShop())
        // ============================================
        // NEW CODE ENDS HERE
        // ============================================
    {
        mpMeterDraw->setAlphaMagicAnimeMin();
    } else {
        mpMeterDraw->setAlphaMagicAnimeMax();
    }
    // ============================================
    // NEW CODE ENDS HERE
    // ============================================
#endif
}

void dMeter2_c::alphaAnimeOxygen() {
    if (!dComIfGp_getOxygenShowFlag() || (mStatus & 0x4000) ||
        ((mStatus & 0x40) && dComIfGp_event_checkHind(0x800)) ||
        ((daPy_getPlayerActorClass()->getSumouMode() != 0) ||
         (daPy_getPlayerActorClass()->checkCanoeSlider() &&
          (dComIfG_getTimerMode() == 3 || dComIfG_getTimerMode() == 4)) ||
         (mStatus & 0x100000) || (mStatus & 0x80000000) || (mStatus & 8) || (mStatus & 0x10) ||
         (mStatus & 0x01000000) || (mStatus & 0x20) || (mStatus & 0x04000000) ||
         (mStatus & 0x08000000) || (mStatus & 0x10000000)))
    {
        mpMeterDraw->setAlphaOxygenAnimeMin();

        if (!dComIfGp_getOxygenShowFlag()) {
            field_0x1e1 = 0;
        }
    } else {
        mpMeterDraw->setAlphaOxygenAnimeMax();

        if (field_0x1e1 == 0) {
            field_0x1e1 = 1;

            if (mpMeterDraw->getMeterGaugeAlphaRate(2) > 0.0f) {
                Z2GetAudioMgr()->seStart(Z2SE_SWIM_TIMER_ON, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f,
                                         0);
            }
        }
    }

    mpMeterDraw->setAlphaOxygenChange(false);
}

void dMeter2_c::alphaAnimeLightDrop() {
    if (!isShowLightDrop()) {
        mpMeterDraw->setAlphaLightDropAnimeMin();
    } else {
        mpMeterDraw->setAlphaLightDropAnimeMax();
    }

    mpMeterDraw->setAlphaLightDropChange(false);
}


void dMeter2_c::alphaAnimeRupee() {
    if ((mStatus & 0x4000) || ((mStatus & 0x40) && dComIfGp_event_checkHind(0x80)) ||
        daPy_getPlayerActorClass()->getSumouMode() ||
        (daPy_getPlayerActorClass()->checkCanoeSlider() && dComIfG_getTimerMode() == 4) ||
        ((strcmp(dComIfGp_getStartStageName(), "R_SP127") != 0 || dComIfGp_event_checkHind(0x80)) &&
         (((mStatus & 0x40000000) && !(mStatus & 0x100)) || (mStatus & 0x1000) ||
          (mStatus & 0x100000))) ||
        (mStatus & 0x80000000) || (mStatus & 8) || (mStatus & 0x10) || (mStatus & 0x01000000) ||
        ((mStatus & 0x20) != 0) || (mStatus & 0x04000000) || (mStatus & 0x08000000) ||
        (mStatus & 0x10000000) || dMeter2Info_isSub2DStatus(1))
    {
        mpMeterDraw->setAlphaRupeeAnimeMin();
    } else {
        mpMeterDraw->setAlphaRupeeAnimeMax();
    }

    mpMeterDraw->setAlphaRupeeChange(false);
}

void dMeter2_c::alphaAnimeKey() {
    if ((mStatus & 0x4000) || ((mStatus & 0x40) && dComIfGp_event_checkHind(0x40)) ||
        !isKeyVisible() || (mStatus & 0x40000000) || (mStatus & 0x1000) || (mStatus & 0x100000) ||
        (mStatus & 0x80000000) || daPy_getPlayerActorClass()->getSumouMode() ||
        (daPy_getPlayerActorClass()->checkCanoeSlider() &&
         (dComIfG_getTimerMode() == 3 || dComIfG_getTimerMode() == 4)) ||
        (mStatus & 0x100) || (mStatus & 8) || (mStatus & 0x10) ||
        (mStatus & 0x1000000) || (mStatus & 0x20) || (mStatus & 0x4000000) ||
        (mStatus & 0x8000000) || (mStatus & 0x10000000) || dMeter2Info_isSub2DStatus(1))
    {
        mpMeterDraw->setAlphaKeyAnimeMin();
    } else {
        mpMeterDraw->setAlphaKeyAnimeMax();
    }

    mpMeterDraw->setAlphaKeyChange(false);
}

void dMeter2_c::alphaAnimeButton() {
    u8 var_31;
    var_31 = 0;

    if ((mStatus & 0x4000) ||
        ((mStatus & 0x100) && dMsgObject_getMsgObjectClass()->isAutoMessageFlag()) ||
        ((mStatus & 0x40000000) && !(mStatus & 0x100)) || (mStatus & 0x80000000) || (mStatus & 8) ||
        (mStatus & 0x10) || (mStatus & 0x20) || (mStatus & 0x04000000) || (mStatus & 0x10000000))
    {
        mpMeterDraw->setAlphaButtonAnimeMin();
        mpMeterDraw->setAlphaButtonAAnimeMin();
        mpMeterDraw->setAlphaButtonBAnimeMin();
    } else if (!(mStatus & 0x1000000) &&
               ((mStatus & 0x100) || daPy_getPlayerActorClass()->checkHawkWait() ||
                (mStatus & 0x40) && dComIfGp_event_checkHind(1)))
    {
        mpMeterDraw->setAlphaButtonAnimeMin();
        var_31 = 1;
    } else if (dMeter2Info_getItemExplainWindowStatus()) {
        mpMeterDraw->setAlphaButtonAnimeMin();
        var_31 = 1;
    } else {
        mpMeterDraw->setAlphaButtonAnimeMax();
    }

    field_0x1e6 = 0;

    if (var_31 == 1) {
        if (dMeter2Info_getItemExplainWindowStatus()) {
            mpMeterDraw->setAlphaButtonAAnimeMin();
        } else if ((mStatus & 0x100) || daPy_getPlayerActorClass()->checkHawkWait() ||
                   !dComIfGp_event_checkHind(2))
        {
            mpMeterDraw->setAlphaButtonAAnimeMax();
            onShowFlag(0);
        } else {
            mpMeterDraw->setAlphaButtonAAnimeMin();
        }

        if (mpMeterDraw->isBButtonShow(false)) {
            mpMeterDraw->setAlphaButtonBAnimeMax();
            onShowFlag(1);
        } else {
            mpMeterDraw->setAlphaButtonBAnimeMin();
        }
    }

    if (!isShowFlag(0)) {
        mpMeterDraw->setButtonIconAAlpha(mDoStatus, mStatus, field_0x128 == 0);
    }

    if (!isShowFlag(1)) {
        mpMeterDraw->setButtonIconBAlpha(mAStatus, mStatus, field_0x128 == 0);
    }

    for (int i = 0; i < 2; i++) {
        mpMeterDraw->setButtonIconAlpha(i, mItemStatus[i * 2], mStatus,
                                        field_0x128 == 0 ? true : false);
        // MODIFIED CODE — ALBW Port
        // Source: Mod::visibleAmmo and silentAmmo in ALBW main.cpp
        // Original code showed ammo for specific items.
        // ALBW version hides ALL ammo counts since the shared
        // meter replaces individual ammo tracking.
        // Only lantern meter display is kept via drawKanteraMeter below.
        // ============================================
#if TARGET_PC
        mpMeterDraw->drawItemNum(i, 0.0f);
#else
        if (field_0x128 == 0 && dMeter2Info_getMiniGameItemSetFlag() != 1 &&
            (mItemStatus[i * 2] == dItemNo_BOW_e || mItemStatus[i * 2] == dItemNo_LIGHT_ARROW_e ||
                mItemStatus[i * 2] == dItemNo_ARROW_LV1_e ||
                mItemStatus[i * 2] == dItemNo_ARROW_LV2_e ||
                mItemStatus[i * 2] == dItemNo_ARROW_LV3_e ||
                mItemStatus[i * 2] == dItemNo_BOMB_BAG_LV1_e ||
                mItemStatus[i * 2] == dItemNo_NORMAL_BOMB_e ||
                mItemStatus[i * 2] == dItemNo_WATER_BOMB_e ||
                mItemStatus[i * 2] == dItemNo_POKE_BOMB_e ||
                mItemStatus[i * 2] == dItemNo_HAWK_ARROW_e ||
                mItemStatus[i * 2] == dItemNo_BOMB_ARROW_e ||
                mItemStatus[i * 2] == dItemNo_PACHINKO_e ||
                mItemStatus[i * 2] == dItemNo_BEE_CHILD_e))
        {
            mpMeterDraw->drawItemNum(i, 1.0f);
        } else {
            mpMeterDraw->drawItemNum(i, 0.0f);
        }
#endif
        // ============================================
        // MODIFIED CODE ENDS HERE
        // ============================================

        if (field_0x128 == 0 && mItemStatus[i * 2] == dItemNo_KANTERA_e) {
            mpMeterDraw->drawKanteraMeter(i, 1.0f);
        } else {
            mpMeterDraw->drawKanteraMeter(i, 0.0f);
        }
    }

#if TARGET_PC
    mpMeterDraw->setButtonIconZItemAlpha(mStatus);
#endif

    mpMeterDraw->setAlphaButtonChange(false);
}

void dMeter2_c::alphaAnimeButtonCross() {
    if ((mStatus & 0x4000) || ((mStatus & 0x40) && dComIfGp_event_checkHind(0x100)) ||
        ((daPy_getPlayerActorClass()->getSumouMode() != 0) || (mStatus & 0x100) ||
         (mStatus & 0x80) || (mStatus & 0x40000000) || (mStatus & 0x1000) || (mStatus & 0x100000) ||
         (mStatus & 0x80000000) || (mStatus & 8) || (mStatus & 0x10) || (mStatus & 0x01000000) ||
         (mStatus & 0x20) || (mStatus & 0x04000000) || (mStatus & 0x08000000) ||
         (mStatus & 0x10000000) || (mStatus & 0x20000000)) ||
        daPy_getPlayerActorClass()->checkEnemyAttentionLock() || dMeter2Info_isGameStatus(1))
    {
        mpMeterDraw->setAlphaButtonCrossAnimeMin();

        if ((!dComIfGp_event_chkEventFlag(0x40) || dMeter2Info_isGameStatus(2) ||
             (mStatus & 0x100)) &&
            field_0x190 > 0)
        {
            field_0x190--;
        }
    } else if (dMeter2Info_isSub2DStatus(1) || dMeter2Info_isFloatingMessageVisible()) {
        mpMeterDraw->setAlphaButtonCrossAnimeMin();

        if (field_0x190 < 5) {
            field_0x190++;
        }
    } else {
#if TARGET_PC
        if (dusk::getSettings().game.enableTouchControls) {
            mpMeterDraw->setAlphaButtonCrossAnimeMin();
        } else {
            mpMeterDraw->setAlphaButtonCrossAnimeMax();
        }
#else
        mpMeterDraw->setAlphaButtonCrossAnimeMax();
#endif

        if (field_0x190 < 5) {
            field_0x190++;
        }
    }

    if (mpMap != NULL) {
        mpMap->setMapAlpha((field_0x190 * 255.0f) / 5.0f);
    }
}

bool dMeter2_c::isShowLightDrop() {
    if (!g_drawHIO.mLightDrop.mAnimDebug) {
        if ((mStatus & 0x4000) || !dComIfGs_isLightDropGetFlag(dComIfGp_getStartStageDarkArea()) ||
            dMeter2Info_getLightDropGetFlag(dComIfGp_getStartStageDarkArea()) <= 1 ||
            !dKy_darkworld_check() || ((mStatus & 0x40) && dComIfGp_event_checkHind(0x200)) ||
            daPy_getPlayerActorClass()->getSumouMode() ||
            (daPy_getPlayerActorClass()->checkCanoeSlider() &&
             (dComIfG_getTimerMode() == 3 || dComIfG_getTimerMode() == 4)) ||
            (mStatus & 0x40000000) || (mStatus & 0x00001000) || (mStatus & 0x00100000) ||
            (mStatus & 0x80000000) || (mStatus & 0x00000100) || (mStatus & 0x00000080) ||
            (mStatus & 0x00000008) || (mStatus & 0x00000010) || (mStatus & 0x01000000) ||
            (mStatus & 0x00000020) || (mStatus & 0x04000000) || (mStatus & 0x08000000) ||
            (mStatus & 0x10000000) || dMeter2Info_isSub2DStatus(1))
        {
            return false;
        }
    }

    return true;
}

void dMeter2_c::killSubContents(u8 param_0) {
    if (mSubContentType != param_0 && mSubContentType != 0) {
        bool free = false;

        if (mpSubContents != NULL) {
            JKR_DELETE(mpSubContents);
            mpSubContents = NULL;
            free = true;
        }

        if (mpSubSubContents != NULL) {
            JKR_DELETE(mpSubSubContents);
            mpSubSubContents = NULL;
            free = true;
        }

        if (free) {
            mpSubHeap->freeAll();
        }

        mSubContentType = 0;
        mSubContentsStringType = 0;
    }
}

u8 dMeter2_c::isKeyVisible() {
    if (dStage_stagInfo_ChkKeyDisp(dComIfGp_getStage()->getStagInfo())) {
        if (dStage_stagInfo_GetSTType(dComIfGp_getStage()->getStagInfo()) == ST_FIELD) {
            return dComIfGs_getKeyNum() != 0;
        } else {
            return true;
        }
    }

    return false;
}

int dMeter2_c::isArrowEquip() {
    for (int i = 0; i < 2; i++) {
        if (mItemStatus[i * 2] == dItemNo_BOW_e || mItemStatus[i * 2] == dItemNo_LIGHT_ARROW_e ||
            mItemStatus[i * 2] == dItemNo_ARROW_LV1_e || mItemStatus[i * 2] == dItemNo_ARROW_LV2_e ||
            mItemStatus[i * 2] == dItemNo_ARROW_LV3_e || mItemStatus[i * 2] == dItemNo_HAWK_ARROW_e ||
            mItemStatus[i * 2] == dItemNo_BOMB_ARROW_e)
        {
            return i + 1;
        }
    }
    return 0;
}

int dMeter2_c::isPachinkoEquip() {
    for (int i = 0; i < 2; i++) {
        if (mItemStatus[i * 2] == dItemNo_PACHINKO_e) {
            return i + 1;
        }
    }

    return 0;
}

static int dMeter2_Draw(dMeter2_c* i_this) {
    return i_this->_draw();
}

static int dMeter2_Execute(dMeter2_c* i_this) {
    return i_this->_execute();
}

static int dMeter2_IsDelete(dMeter2_c* i_this) {
    return 1;
}

static int dMeter2_Delete(dMeter2_c* i_this) {
    return i_this->_delete();
}

static int dMeter2_Create(msg_class* i_this) {
    dMeter2Info_setMeterClass(static_cast<dMeter2_c*>(i_this));
    dComIfGp_2dShowOn();
    fopMsgM_Create(fpcNm_MENUWINDOW_e, NULL, NULL);

    g_drawHIO.field_0x4 = -1;
    g_ringHIO.field_0x4 = -1;
    g_fmapHIO.field_0x4 = -1;
    g_cursorHIO.field_0x4 = -1;

    u32 id = fopMsgM_Create(fpcNm_MSG_OBJECT_e, NULL, NULL);
    fopMsgM_setMessageID(id);

    dTimer_createStockTimer();
    fopMsgM_setStageLayer(i_this);

    return static_cast<dMeter2_c*>(i_this)->_create();
}

static leafdraw_method_class l_dMeter2_Method = {
    (process_method_func)dMeter2_Create,  (process_method_func)dMeter2_Delete,
    (process_method_func)dMeter2_Execute, (process_method_func)dMeter2_IsDelete,
    (process_method_func)dMeter2_Draw,
};

DUSK_PROFILE msg_process_profile_definition DUSK_CONST g_profile_METER2 = {
    /* Layer ID    */ fpcLy_CURRENT_e,
    /* List ID     */ 12,
    /* List Prio   */ fpcPi_CURRENT_e,
    /* Proc Name   */ fpcNm_METER2_e,
    /* Proc SubMtd */ &g_fpcLf_Method.base,
    /* Size        */ sizeof(dMeter2_c),
    /* Size Other  */ 0,
    /* Parameters  */ 0,
    /* Leaf SubMtd */ &g_fopMsg_Method,
    /* Draw Prio   */ fpcDwPi_METER2_e,
    /* Msg SubMtd  */ &l_dMeter2_Method,
};

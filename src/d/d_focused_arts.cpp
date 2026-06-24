/**
 * d_focused_arts.cpp
 * Focused Arts — tier purchase API, meter fill/bank/spend hooks, debug query API.
 */

#if TARGET_PC

#include "d/d_focused_arts.h"
#include "d/d_albw_combat.h"
#include "d/d_albw_shield.h"
#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"
#include "d/d_meter2_info.h"
#include "d/actor/d_a_player.h"
#include "dusk/settings.h"
#include "dusk/trace_noop.h"
#include <algorithm>
#include <cstdio>

namespace {

// dSv_event_c::mEvent[] byte — index 103 (unused by vanilla; MQ uses 100–102).
static constexpr u16 kFocusedArtsShopTierReg = static_cast<u16>(103 << 8) | 0xFF;

static constexpr int kFocusedArtsShopPrices[kFocusedArtsMaxTier] = {
    500,
    1500,
    3500,
};

static constexpr int kItemFillStep = 2;  // +1/6 of 12
static constexpr int kRecentEventLines = 4;

static int s_bankCount = 0;
static int s_fillNumerator = 0;
static int s_spendColumn = 0;
static bool s_inSpendSequence = false;
static bool s_forceNewBaseDamage = false;
static bool s_jsFinisherLockoutActive = false;
static int s_backSliceAlbwSuppressFrames = 0;
static int s_maintainStackedFrames = 0;
static bool s_gsHurricaneFinisherArmed = false;
static int s_ebGreatSpinAoeFrames = 0;
static char s_recentEvents[kRecentEventLines][72] = {};
static int s_recentEventHead = 0;

static constexpr int kEbGreatSpinAoeDurationFrames = 30;

static bool isHiddenSkillCutType(int i_cutType) {
    switch (i_cutType) {
    case daPy_py_c::CUT_TYPE_TWIRL:
    case daPy_py_c::CUT_TYPE_JUMP:
    case daPy_py_c::CUT_TYPE_LARGE_JUMP:
    case daPy_py_c::CUT_TYPE_LARGE_JUMP_INIT:
    case daPy_py_c::CUT_TYPE_LARGE_JUMP_FINISH:
    case daPy_py_c::CUT_TYPE_LARGE_TURN_LEFT:
    case daPy_py_c::CUT_TYPE_LARGE_TURN_RIGHT:
    case daPy_py_c::CUT_TYPE_MORTAL_DRAW_A:
    case daPy_py_c::CUT_TYPE_MORTAL_DRAW_B:
    case daPy_py_c::CUT_TYPE_HEAD_JUMP:
        return true;
    default:
        return false;
    }
}

static bool isInstaKillFinisherCutType(int i_cutType) {
    return i_cutType == daPy_py_c::CUT_TYPE_FINISH_VERTICAL ||
           i_cutType == daPy_py_c::CUT_TYPE_FINISH_LEFT ||
           i_cutType == daPy_py_c::CUT_TYPE_FINISH_STAB;
}

static int getDamageTierSteps() {
    if (s_forceNewBaseDamage && !s_inSpendSequence && s_maintainStackedFrames <= 0) {
        return 0;
    }

    if (s_inSpendSequence || s_maintainStackedFrames > 0) {
        return dFocusedArts_getMaxBank();
    }

    return s_bankCount;
}

static bool isSpecialFinisherSpendActive() {
    return dFocusedArts_isEnabled() && dFocusedArts_hasSpecialFinishers() &&
           s_spendColumn == 1 && s_maintainStackedFrames > 0;
}

static u16 applyPercentMult(u16 i_power, int i_numerator, int i_denominator) {
    if (i_power == 0 || i_numerator <= 0) {
        return i_power;
    }
    const u32 scaled =
        (static_cast<u32>(i_power) * static_cast<u32>(i_numerator)) / static_cast<u32>(i_denominator);
    return static_cast<u16>(std::min<u32>(scaled, 0xFFFFu));
}

static u16 previewFaMeleeDamage(u16 i_vanillaPower, int i_cutType, int tierSteps, bool i_finisherSpend) {
    if (!dFocusedArts_isEnabled() || i_vanillaPower == 0) {
        return i_vanillaPower;
    }
    if (!isHiddenSkillCutType(i_cutType) || isInstaKillFinisherCutType(i_cutType)) {
        return i_vanillaPower;
    }

    const int numerator = 4 + (2 * tierSteps);
    u16 resolved = applyPercentMult(i_vanillaPower, numerator, 10);

    if (i_finisherSpend && dFocusedArts_hasSpecialFinishers()) {
        switch (i_cutType) {
        case daPy_py_c::CUT_TYPE_MORTAL_DRAW_A:
        case daPy_py_c::CUT_TYPE_MORTAL_DRAW_B:
            if (dComIfGs_getLife() <= 1) {
                resolved = applyPercentMult(resolved, 3, 1);
            } else {
                resolved = applyPercentMult(resolved, 3, 2);
            }
            break;
        case daPy_py_c::CUT_TYPE_HEAD_JUMP:
            resolved = applyPercentMult(resolved, 3, 2);
            break;
        default:
            break;
        }
    }

    return std::max<u16>(resolved, 1);
}

struct HiddenSkillAttackDebugSpec {
    const char* shortName;
    u16 eventBit;
    int cutType;
    int atp;
    bool faMelee;
    bool instakill;
    const char* note;
};

static constexpr HiddenSkillAttackDebugSpec kHiddenSkillAttackDebugSpecs[] = {
    {"Ending Blow", dSv_event_flag_c::F_0339, daPy_py_c::CUT_TYPE_FINISH_VERTICAL, 4, false, true,
     "T1 fin=GS AOE 50%"},
    {"Shield Attack", dSv_event_flag_c::F_0338, daPy_py_c::CUT_TYPE_NONE, 0, false, false,
     "bash/parry (not FA melee)"},
    {"Back Slice", dSv_event_flag_c::F_0340, daPy_py_c::CUT_TYPE_TWIRL, 3, true, false, nullptr},
    {"Helm Split", dSv_event_flag_c::F_0341, daPy_py_c::CUT_TYPE_HEAD_JUMP, 3, true, false,
     nullptr},
    {"Mortal Draw", dSv_event_flag_c::F_0342, daPy_py_c::CUT_TYPE_MORTAL_DRAW_A, 4, true, false,
     nullptr},
    {"Jump Strike", dSv_event_flag_c::F_0343, daPy_py_c::CUT_TYPE_LARGE_JUMP, 4, true, false,
     nullptr},
    {"Great Spin", dSv_event_flag_c::F_0344, daPy_py_c::CUT_TYPE_LARGE_TURN_LEFT, 4, true, false,
     "T1 fin=Hurricane"},
};

static u16 vanillaLinkSwordPowerFromAtp(int i_atp) {
    u8 power = static_cast<u8>(i_atp);
    if (power == 1) {
        return 1;
    }
    if (power == 2) {
        return 10;
    }
    if (power == 3) {
        return 30;
    }
    if (power == 6) {
        return 80;
    }
    if (power >= 4) {
        return 200;
    }
    return power;
}

static u16 applyEquippedSwordAttackMult(u16 i_power) {
    if (i_power == 0) {
        return i_power;
    }

    u16 power = i_power;
    if (daPy_py_c::checkMasterSwordEquip()) {
        power = static_cast<u16>(std::min<u32>(static_cast<u32>(power) * 2u, 0xFFFFu));
    }
    if (daPy_py_c::checkWoodSwordEquip()) {
        power = static_cast<u16>(std::max<u16>(power / 2, 1));
    }
    return power;
}

static const char* getEquippedSwordLabel() {
    const u8 sword = dComIfGs_getSelectEquipSword();
    if (sword == dItemNo_WOOD_STICK_e) {
        return "Wood (no FA fill)";
    }
    if (sword == dItemNo_SWORD_e) {
        return "Ordon";
    }
    if (sword == dItemNo_MASTER_SWORD_e) {
        return "Master";
    }
    if (sword == dItemNo_LIGHT_SWORD_e) {
        return "Light";
    }
    return "Unknown";
}

static bool isHiddenSkillEventAcquired(u16 i_eventBit) {
    return dComIfGs_isEventBit(i_eventBit) != 0;
}

static void logFaEvent(const char* i_msg) {
    std::snprintf(s_recentEvents[s_recentEventHead], sizeof(s_recentEvents[s_recentEventHead]), "%s",
                  i_msg);
    s_recentEventHead = (s_recentEventHead + 1) % kRecentEventLines;
    CONAV_LOG("fa", "%s", i_msg);
}

static u8 readShopTierReg() {
    return dComIfGs_getEventReg(kFocusedArtsShopTierReg);
}

static void writeShopTierReg(u8 tier) {
    dComIfGs_setEventReg(kFocusedArtsShopTierReg, tier);
}

static int clampPurchasedTier(int tier) {
    if (tier < 0) {
        return 0;
    }
    if (tier > kFocusedArtsMaxTier) {
        return kFocusedArtsMaxTier;
    }
    return tier;
}

static bool isCheatAllTiers() {
    return dusk::getSettings().game.focusedArtsCheat.getValue() != dusk::FocusedArtsCheatMode::Off;
}

static bool isCheatMaxBank() {
    const auto mode = dusk::getSettings().game.focusedArtsCheat.getValue();
    return mode == dusk::FocusedArtsCheatMode::OnMaxBank ||
           mode == dusk::FocusedArtsCheatMode::WithDebugMaxBank;
}

static void applyCheatMaxBankIfEnabled() {
    if (!isCheatMaxBank() || !dFocusedArts_isEnabled() || s_inSpendSequence) {
        return;
    }

    const int maxBank = dFocusedArts_getMaxBank();
    if (maxBank <= 0 || s_bankCount >= maxBank) {
        return;
    }

    s_bankCount = maxBank;
    s_fillNumerator = 0;
    logFaEvent("cheat max bank");
}

static int getSwordFillStep() {
    const u8 sword = dComIfGs_getSelectEquipSword();
    if (sword == dItemNo_WOOD_STICK_e) {
        return 0;
    }
    if (sword == dItemNo_SWORD_e) {
        return kItemFillStep;
    }
    if (sword == dItemNo_MASTER_SWORD_e || sword == dItemNo_LIGHT_SWORD_e) {
        return 1;
    }
    return 0;
}

static void clearFillProgress() {
    s_fillNumerator = 0;
}

static void addFillSteps(int i_steps) {
    if (i_steps <= 0) {
        return;
    }

    if (s_inSpendSequence) {
        return;
    }

    const int maxBank = dFocusedArts_getMaxBank();
    if (maxBank <= 0 || s_bankCount >= maxBank) {
        return;
    }

    s_fillNumerator += i_steps;
    while (s_fillNumerator >= kFocusedArtsFillDenominator && s_bankCount < maxBank) {
        s_fillNumerator -= kFocusedArtsFillDenominator;
        s_bankCount++;
        s_forceNewBaseDamage = false;
        if (s_jsFinisherLockoutActive) {
            s_jsFinisherLockoutActive = false;
            logFaEvent("JS finisher bonus cleared (new bank)");
        }
        char buf[72];
        std::snprintf(buf, sizeof(buf), "bank +1 -> %d/%d fill=%d/12", s_bankCount, maxBank,
                      s_fillNumerator);
        logFaEvent(buf);
    }
}

static void beginSpendCharge(int i_spendColumn) {
    s_spendColumn = i_spendColumn;
    s_maintainStackedFrames = 300;
    if (s_bankCount > 0) {
        s_bankCount--;
    }
    char buf[72];
    std::snprintf(buf, sizeof(buf), "spend T%d bank=%d fin=%d noALBW", i_spendColumn, s_bankCount,
                  (i_spendColumn == 1 && dFocusedArts_hasSpecialFinishers()) ? 1 : 0);
    logFaEvent(buf);
    if (s_bankCount <= 0) {
        s_inSpendSequence = false;
    }
}

}  // namespace

bool dFocusedArts_isEnabled() {
    return dusk::getSettings().game.focusedArtsTest.getValue();
}

bool dFocusedArts_isDebugOverlayEnabled() {
    if (!dFocusedArts_isEnabled()) {
        return false;
    }
    const auto mode = dusk::getSettings().game.focusedArtsCheat.getValue();
    return mode == dusk::FocusedArtsCheatMode::WithDebug ||
           mode == dusk::FocusedArtsCheatMode::WithDebugMaxBank;
}

bool dFocusedArts_shouldSuppressAlbwMeterDrain() {
    if (!dFocusedArts_isEnabled()) {
        return false;
    }

    if (s_backSliceAlbwSuppressFrames > 0) {
        return true;
    }

    if (s_inSpendSequence) {
        return true;
    }

    // Spend columns (T3/T2/T1) each grant a short no-ALBW window after the charge is spent.
    return s_maintainStackedFrames > 0;
}

int dFocusedArts_getPurchasedTier() {
    return clampPurchasedTier(readShopTierReg());
}

int dFocusedArts_getEffectiveTier() {
    if (!dFocusedArts_isEnabled()) {
        return 0;
    }
    if (isCheatAllTiers()) {
        return kFocusedArtsMaxTier;
    }
    return dFocusedArts_getPurchasedTier();
}

int dFocusedArts_getMaxBank() {
    return dFocusedArts_getEffectiveTier();
}

bool dFocusedArts_hasSpecialFinishers() {
    return dFocusedArts_getEffectiveTier() >= kFocusedArtsMaxTier;
}

int dFocusedArts_getShopTierPrice(int tier) {
    if (tier < 1 || tier > kFocusedArtsMaxTier) {
        return 0;
    }
    return kFocusedArtsShopPrices[tier - 1];
}

int dFocusedArts_getNextShopTierPrice() {
    const int nextTier = dFocusedArts_getPurchasedTier() + 1;
    if (nextTier > kFocusedArtsMaxTier) {
        return 0;
    }
    return dFocusedArts_getShopTierPrice(nextTier);
}

bool dFocusedArts_canPurchaseShopTier() {
    if (!dFocusedArts_isEnabled()) {
        return false;
    }
    if (isCheatAllTiers()) {
        return false;
    }
    return dFocusedArts_getPurchasedTier() < kFocusedArtsMaxTier;
}

bool dFocusedArts_tryPurchaseShopTier() {
    if (!dFocusedArts_canPurchaseShopTier()) {
        return false;
    }
    writeShopTierReg(static_cast<u8>(dFocusedArts_getPurchasedTier() + 1));
    return true;
}

const char* dFocusedArts_getShopTierName(int tier) {
    switch (tier) {
    case 1:
        return "Focused Arts I";
    case 2:
        return "Focused Arts II";
    case 3:
        return "Focused Arts III";
    default:
        return "Focused Arts";
    }
}

const char* dFocusedArts_getShopTierDesc(int tier) {
    switch (tier) {
    case 1:
        return "Bank one Focused Arts charge. Hidden skills use the first spend column.";
    case 2:
        return "Bank two charges. Spend from the second column, then the first.";
    case 3:
        return "Bank three charges and unlock Special Finishers on the final spend.";
    default:
        return "";
    }
}

void dFocusedArts_resetRuntimeState() {
    s_bankCount = 0;
    s_fillNumerator = 0;
    s_spendColumn = 0;
    s_inSpendSequence = false;
    s_forceNewBaseDamage = false;
    s_jsFinisherLockoutActive = false;
    s_backSliceAlbwSuppressFrames = 0;
    s_maintainStackedFrames = 0;
    s_gsHurricaneFinisherArmed = false;
    s_ebGreatSpinAoeFrames = 0;
    s_recentEventHead = 0;
    for (int i = 0; i < kRecentEventLines; i++) {
        s_recentEvents[i][0] = '\0';
    }
    logFaEvent("reset");
    applyCheatMaxBankIfEnabled();
}

void dFocusedArts_onStageLoad() {
    if (!dFocusedArts_isEnabled()) {
        return;
    }

    dFocusedArts_resetRuntimeState();
}

int dFocusedArts_getBankCount() {
    return s_bankCount;
}

int dFocusedArts_getFillNumerator() {
    return s_fillNumerator;
}

int dFocusedArts_getFillDenominator() {
    return kFocusedArtsFillDenominator;
}

bool dFocusedArts_isSpendReady() {
    if (!dFocusedArts_isEnabled()) {
        return false;
    }
    const int maxBank = dFocusedArts_getMaxBank();
    return maxBank > 0 && s_bankCount >= maxBank;
}

bool dFocusedArts_hasSpecialFinisherAvailable() {
    return dFocusedArts_isSpendReady() && dFocusedArts_hasSpecialFinishers();
}

int dFocusedArts_getSpendColumn() {
    return s_spendColumn;
}

bool dFocusedArts_isInSpendSequence() {
    return s_inSpendSequence;
}

int dFocusedArts_getChargedDamageTier() {
    return s_bankCount;
}

const char* dFocusedArts_getLastEventText() {
    const int idx = (s_recentEventHead + kRecentEventLines - 1) % kRecentEventLines;
    return s_recentEvents[idx][0] != '\0' ? s_recentEvents[idx] : "(none)";
}

const char* dFocusedArts_getRecentEventLine(int i_index) {
    if (i_index < 0 || i_index >= kRecentEventLines) {
        return "";
    }
    const int idx =
        (s_recentEventHead + kRecentEventLines - 1 - i_index + kRecentEventLines) % kRecentEventLines;
    return s_recentEvents[idx];
}

void dFocusedArts_onConnectedSwordHit() {
    if (!dFocusedArts_isEnabled()) {
        return;
    }
    if (s_inSpendSequence) {
        return;
    }
    if (dMeter2_isALBWLocked()) {
        return;
    }

    const int step = getSwordFillStep();
    if (step <= 0) {
        return;
    }

    const int fillBefore = s_fillNumerator;
    const int bankBefore = s_bankCount;
    addFillSteps(step);
    if (s_fillNumerator != fillBefore || s_bankCount != bankBefore) {
        char buf[72];
        std::snprintf(buf, sizeof(buf), "sword fill +%d -> %d/12 bank=%d/%d", step, s_fillNumerator,
                      s_bankCount, dFocusedArts_getMaxBank());
        logFaEvent(buf);
    }
}

void dFocusedArts_onConnectedItemHit() {
    if (!dFocusedArts_isEnabled()) {
        return;
    }
    if (s_inSpendSequence) {
        return;
    }
    addFillSteps(kItemFillStep);
    char buf[72];
    std::snprintf(buf, sizeof(buf), "item fill +%d -> %d/12 bank=%d/%d", kItemFillStep,
                  s_fillNumerator, s_bankCount, dFocusedArts_getMaxBank());
    logFaEvent(buf);
}

void dFocusedArts_onPlayerHiddenSkillUse() {
    if (!dFocusedArts_isEnabled()) {
        return;
    }

    const int maxBank = dFocusedArts_getMaxBank();
    if (maxBank <= 0) {
        return;
    }

    if (s_bankCount > 0 && (s_inSpendSequence || s_bankCount >= maxBank)) {
        if (!s_inSpendSequence && s_bankCount >= maxBank) {
            s_inSpendSequence = true;
        }
        beginSpendCharge(s_bankCount);
        return;
    }

    clearFillProgress();
    char buf[72];
    std::snprintf(buf, sizeof(buf), "HS fill reset bank=%d/%d", s_bankCount, maxBank);
    logFaEvent(buf);
}

void dFocusedArts_onHiddenSkillChargeStart() {
    if (!dFocusedArts_isEnabled()) {
        return;
    }
    if (s_fillNumerator <= 0) {
        return;
    }

    clearFillProgress();
    char buf[72];
    std::snprintf(buf, sizeof(buf), "HS charge start drained fill bank=%d/%d", s_bankCount,
                  dFocusedArts_getMaxBank());
    logFaEvent(buf);
}

void dFocusedArts_onDamageTaken() {
    if (!dFocusedArts_isEnabled()) {
        return;
    }
    if (s_fillNumerator <= 0) {
        return;
    }

    clearFillProgress();
    char buf[72];
    std::snprintf(buf, sizeof(buf), "damage drained fill bank=%d/%d", s_bankCount,
                  dFocusedArts_getMaxBank());
    logFaEvent(buf);
}

void dFocusedArts_update() {
    if (!dFocusedArts_isEnabled()) {
        return;
    }

    if (s_backSliceAlbwSuppressFrames > 0) {
        s_backSliceAlbwSuppressFrames--;
    }

    if (s_maintainStackedFrames > 0) {
        s_maintainStackedFrames--;
    }

    if (s_ebGreatSpinAoeFrames > 0) {
        s_ebGreatSpinAoeFrames--;
    }

    if (s_jsFinisherLockoutActive && !dMeter2_isALBWLocked()) {
        s_jsFinisherLockoutActive = false;
        logFaEvent("JS finisher bonus ended (lockout cleared)");
    }

    applyCheatMaxBankIfEnabled();
}

void dFocusedArts_onHiddenSkillProcStarted(int i_cutType) {
    if (!isSpecialFinisherSpendActive()) {
        return;
    }

    switch (i_cutType) {
    case daPy_py_c::CUT_TYPE_LARGE_JUMP_INIT:
    case daPy_py_c::CUT_TYPE_LARGE_JUMP:
    case daPy_py_c::CUT_TYPE_LARGE_JUMP_FINISH:
        dMeter2_drainALBWToLockout();
        s_jsFinisherLockoutActive = true;
        logFaEvent("JS T1 finisher -> ALBW lockout + item +300%");
        break;
    case daPy_py_c::CUT_TYPE_TWIRL:
        s_backSliceAlbwSuppressFrames = 90;
        logFaEvent("Back Slice T1 finisher -> 3s no ALBW spend");
        break;
    case daPy_py_c::CUT_TYPE_MORTAL_DRAW_A:
    case daPy_py_c::CUT_TYPE_MORTAL_DRAW_B: {
        const u16 life = dComIfGs_getLife();
        const u16 maxLife = dComIfGs_getMaxLifeGauge();
        if (life > 1 && maxLife > 0) {
            const u16 drain = std::max<u16>(1, static_cast<u16>(maxLife / 2));
            dComIfGs_setLife(life > drain ? static_cast<u8>(life - drain) : 1);
        }
        if (dComIfGs_getLife() <= 1) {
            if (daPy_getPlayerActorClass()->checkEndResetFlg0(daPy_py_c::ERFLG0_FORCE_WOLF_CHANGE)) {
                daPy_getPlayerActorClass()->onForceWolfChange();
                logFaEvent("MD T1 finisher -> forced wolf @ 1 HP");
            } else {
                logFaEvent("MD T1 finisher @ 1 HP (wolf transform unavailable)");
            }
        } else {
            logFaEvent("MD T1 finisher -> -half HP");
        }
        break;
    }
    case daPy_py_c::CUT_TYPE_HEAD_JUMP:
        dShield_fillBashChargesToMax();
        logFaEvent("Helm T1 finisher -> max bash charges");
        break;
    default:
        break;
    }
}

u16 dFocusedArts_resolveMeleeDamage(u16 i_vanillaPower, int i_cutType) {
    if (!dFocusedArts_isEnabled() || i_vanillaPower == 0) {
        return i_vanillaPower;
    }
    if (!isHiddenSkillCutType(i_cutType) || isInstaKillFinisherCutType(i_cutType)) {
        return i_vanillaPower;
    }

    const int tierSteps = getDamageTierSteps();
    int numerator = 4 + (2 * tierSteps);
    int denominator = 10;
    u16 resolved = applyPercentMult(i_vanillaPower, numerator, denominator);

    if (isSpecialFinisherSpendActive()) {
        switch (i_cutType) {
        case daPy_py_c::CUT_TYPE_MORTAL_DRAW_A:
        case daPy_py_c::CUT_TYPE_MORTAL_DRAW_B:
            if (dComIfGs_getLife() <= 1) {
                resolved = applyPercentMult(resolved, 3, 1);
            } else {
                resolved = applyPercentMult(resolved, 3, 2);
            }
            break;
        case daPy_py_c::CUT_TYPE_HEAD_JUMP:
            resolved = applyPercentMult(resolved, 3, 2);
            break;
        default:
            break;
        }
    }

    return std::max<u16>(resolved, 1);
}

void dFocusedArts_applyItemDamageBoost(u16& io_attackPower) {
    if (!dFocusedArts_isEnabled() || io_attackPower == 0 || s_jsFinisherLockoutActive) {
        return;
    }

    int tierSteps = getDamageTierSteps();
    if (tierSteps <= 0 && s_maintainStackedFrames <= 0 && !s_inSpendSequence) {
        return;
    }

    int bonusTenths = tierSteps * 5;
    if (isSpecialFinisherSpendActive()) {
        bonusTenths += 5;
    }

    io_attackPower = applyPercentMult(io_attackPower, 10 + bonusTenths, 10);
}

bool dFocusedArts_isJsFinisherLockoutActive() {
    return s_jsFinisherLockoutActive && dMeter2_isALBWLocked();
}

bool dFocusedArts_isOnFinalSpendCharge() {
    return dFocusedArts_isEnabled() && dFocusedArts_hasSpecialFinishers() &&
           s_inSpendSequence && s_bankCount == 1;
}

bool dFocusedArts_isSpecialFinisherSpendActive() {
    return isSpecialFinisherSpendActive();
}

bool dFocusedArts_shouldLaunchGsHurricaneFinisher() {
    return dFocusedArts_isOnFinalSpendCharge();
}

bool dFocusedArts_tryArmGsHurricaneFinisher() {
    if (!dFocusedArts_isOnFinalSpendCharge()) {
        return false;
    }

    s_gsHurricaneFinisherArmed = true;
    dFocusedArts_onPlayerHiddenSkillUse();
    logFaEvent("GS T1 finisher armed -> hurricane");
    return true;
}

bool dFocusedArts_consumeGsHurricaneFinisherArmed() {
    if (!s_gsHurricaneFinisherArmed) {
        return false;
    }

    s_gsHurricaneFinisherArmed = false;
    return true;
}

bool dFocusedArts_onEndingBlowContact() {
    if (!isSpecialFinisherSpendActive()) {
        return false;
    }

    s_ebGreatSpinAoeFrames = kEbGreatSpinAoeDurationFrames;
    logFaEvent("EB T1 finisher -> GS AOE 50%");
    return true;
}

bool dFocusedArts_isEndingBlowGreatSpinAoeActive() {
    return dFocusedArts_isEnabled() && s_ebGreatSpinAoeFrames > 0;
}

static constexpr u16 kGsHurricaneTestHitPower = 50;

u16 dFocusedArts_getGsHurricaneHitPower() {
    return kGsHurricaneTestHitPower;
}

u16 dFocusedArts_getEndingBlowGreatSpinAoePower(u16 i_greatSpinAttackPower) {
    return applyPercentMult(i_greatSpinAttackPower, 1, 2);
}

bool dFocusedArts_shouldShowShopTierRow() {
    if (!dFocusedArts_isEnabled() || isCheatAllTiers()) {
        return false;
    }
    return dFocusedArts_getPurchasedTier() < kFocusedArtsMaxTier;
}

int dFocusedArts_getNextShopTierIndex() {
    const int next = dFocusedArts_getPurchasedTier() + 1;
    if (next < 1 || next > kFocusedArtsMaxTier) {
        return 0;
    }
    return next;
}

void dFocusedArts_onBackSliceFinisherEnded() {
    if (!isSpecialFinisherSpendActive()) {
        return;
    }

    s_forceNewBaseDamage = true;
    logFaEvent("Back Slice finisher ended -> new base damage");
}

int dFocusedArts_getHiddenSkillAttackDebugLineCount() {
    return static_cast<int>(sizeof(kHiddenSkillAttackDebugSpecs) / sizeof(kHiddenSkillAttackDebugSpecs[0]));
}

const char* dFocusedArts_getHiddenSkillAttackDebugSwordLabel() {
    return getEquippedSwordLabel();
}

void dFocusedArts_formatHiddenSkillAttackDebugLine(int i_index, char* o_buf, size_t i_bufSize) {
    if (o_buf == NULL || i_bufSize == 0) {
        return;
    }
    o_buf[0] = '\0';

    if (i_index < 0 ||
        i_index >= static_cast<int>(sizeof(kHiddenSkillAttackDebugSpecs) / sizeof(kHiddenSkillAttackDebugSpecs[0])))
    {
        return;
    }

    const HiddenSkillAttackDebugSpec& spec = kHiddenSkillAttackDebugSpecs[i_index];
    const bool acquired = isHiddenSkillEventAcquired(spec.eventBit);

    if (!acquired) {
        std::snprintf(o_buf, i_bufSize, "[ ] %-11s --", spec.shortName);
        return;
    }

    if (spec.instakill) {
        std::snprintf(o_buf, i_bufSize, "[x] %-11s instakill", spec.shortName);
        return;
    }

    if (!spec.faMelee) {
        std::snprintf(o_buf, i_bufSize, "[x] %-11s %s", spec.shortName,
                      spec.note != nullptr ? spec.note : "n/a");
        return;
    }

    const u16 vanilla = applyEquippedSwordAttackMult(vanillaLinkSwordPowerFromAtp(spec.atp));
    const int tierSteps = getDamageTierSteps();
    const bool finisher = isSpecialFinisherSpendActive();
    const u16 now = previewFaMeleeDamage(vanilla, spec.cutType, tierSteps, finisher);
    const u16 t1 = previewFaMeleeDamage(vanilla, spec.cutType, 1, false);
    const u16 t2 = previewFaMeleeDamage(vanilla, spec.cutType, 2, false);
    const u16 t3 = previewFaMeleeDamage(vanilla, spec.cutType, 3, false);
    const u16 t1Fin = previewFaMeleeDamage(vanilla, spec.cutType, 1, true);

    if (finisher && t1Fin != now) {
        std::snprintf(o_buf, i_bufSize, "[x] %-11s V=%-3u now=%u T1/2/3=%u/%u/%u fin=%u", spec.shortName,
                      vanilla, now, t1, t2, t3, t1Fin);
        return;
    }

    if (spec.note != nullptr && dFocusedArts_hasSpecialFinishers()) {
        std::snprintf(o_buf, i_bufSize, "[x] %-11s V=%-3u now=%u T1/2/3=%u/%u/%u (%s)", spec.shortName,
                      vanilla, now, t1, t2, t3, spec.note);
        return;
    }

    std::snprintf(o_buf, i_bufSize, "[x] %-11s V=%-3u now=%u T1/2/3=%u/%u/%u", spec.shortName, vanilla,
                  now, t1, t2, t3);
}

#endif

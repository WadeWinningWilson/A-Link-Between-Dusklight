/**
 * d_albw_shield.cpp — ALBW Port Phase 4
 * LoP-style perfect guard + bash charge economy + spur-style bash HUD.
 */

#if TARGET_PC

#include "d/dolzel.h" // IWYU pragma: keep
#include "d/d_albw_shield.h"
#include "d/d_albw_combat.h"
#include "d/d_albw_hp_mult.h"
#include "d/d_albw_wolf_combat.h"
#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"
#include "d/d_cc_d.h"
#include "d/d_item_data.h"
#include "d/d_meter2.h"
#include "d/d_meter2_info.h"
#include "d/d_meter2_draw.h"
#include "d/d_meter_HIO.h"
#include "d/d_pane_class.h"
#include "dusk/settings.h"
#include "f_op/f_op_actor.h"
#include "f_pc/f_pc_manager.h"
#include "SSystem/SComponent/c_counter.h"
#include "Z2AudioLib/Z2Instances.h"
#include "Z2AudioLib/Z2SeMgr.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <climits>
#include "JSystem/J2DGraph/J2DGrafContext.h"
#include "JSystem/J2DGraph/J2DScreen.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "f_op/f_op_actor.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_set>

namespace {

constexpr u32 PARRY_WINDOW_FRAMES = 4;
constexpr u32 PARRY_BUFFER_FRAMES = 1;
constexpr u32 PARRY_WINDOW_TOTAL = PARRY_WINDOW_FRAMES + PARRY_BUFFER_FRAMES;

constexpr int PARRY_ALBW_GAIN_NUM = 1;
constexpr int PARRY_ALBW_GAIN_DEN = 6;
constexpr int FAIL_ALBW_PENALTY_NUM     = 1;
constexpr int FAIL_ALBW_PENALTY_DEN     = 10;
constexpr int HYLIAN_FAIL_ALBW_PENALTY_DEN = 5;  // 20% of max — Hylian only
constexpr int BASH_ALBW_HIT_GAIN_NUM = 5;
constexpr int BASH_ALBW_HIT_GAIN_DEN = 100;

// Hylian: heavier bash economy — +1 parry unchanged; failed/normal block costs 2 charges.
constexpr u8 HYLIAN_FAIL_CHARGE_LOSS = 2;
constexpr u8 DEFAULT_FAIL_CHARGE_LOSS = 1;

// Shield durability (Phase 2) — Hylian: +12% max repair on parry, +30% damage taken on hits.
constexpr u8 HYLIAN_DURABILITY_DAMAGE_MUL = 130;
constexpr u8 HYLIAN_PARRY_REPAIR_PCT = 12;

// Quest shield: dItemNo_WOOD_SHIELD (0x2A) -> CWShd (d_a_obj_shield.cpp, M_072). See docs/shield-ordon-quest-assets.md.
// dItemNo_SHIELD (0x2B) -> SWShd (checkShopWoodShieldEquip). HyShd = Hylian.
static const char SHIELD_ARC_ORDON[] = "CWShd";
static const char SHIELD_ARC_WOODEN[] = "SWShd";
static const char SHIELD_ARC_HYLIAN[] = "HyShd";

// Scale down vs full rupi_n bounds so icons match the green rupee gem, not the whole row.
constexpr f32 BASH_ICON_RUPEE_SIZE_MUL = 0.78f;

// Fallback when rupee / hakusha panes are not ready (matches ~mRupeeScale spur sizing).
constexpr f32 BASH_ICON_SCALE_FALLBACK = 0.8f;
constexpr f32 BASH_ICON_SPACING_FALLBACK = 28.0f;
constexpr f32 BASH_ROW_OFFSET_FALLBACK = 48.0f;

struct ShieldTierConfig {
    u8 maxCharges;
    u8 bashThreshold;
};

struct DurabilityTierConfig {
    u16 maxHp;
    u16 drainBlock;
    u16 drainSmall;
};

enum class ShieldTier {
    None,
    Ordon,
    Wooden,
    Hylian,
};

ShieldTierConfig tierConfig(ShieldTier tier) {
    switch (tier) {
    case ShieldTier::Ordon:
        return {2, 2};
    case ShieldTier::Wooden:
        return {4, 4};
    case ShieldTier::Hylian:
        return {6, 4};
    default:
        return {0, 0};
    }
}

DurabilityTierConfig durabilityTierConfig(ShieldTier tier) {
    switch (tier) {
    case ShieldTier::Ordon:
        return {134, 18, 10};
    case ShieldTier::Wooden:
        return {120, 15, 8};
    case ShieldTier::Hylian:
        return {160, 22, 12};
    default:
        return {0, 0, 0};
    }
}

ShieldTier shieldTierFromShieldArc(const char* i_arcName) {
    if (i_arcName == NULL) {
        return ShieldTier::None;
    }
    if (strcmp(i_arcName, SHIELD_ARC_HYLIAN) == 0) {
        return ShieldTier::Hylian;
    }
    if (strcmp(i_arcName, SHIELD_ARC_WOODEN) == 0) {
        return ShieldTier::Wooden;
    }
    if (strcmp(i_arcName, SHIELD_ARC_ORDON) == 0) {
        return ShieldTier::Ordon;
    }
    return ShieldTier::None;
}

ShieldTier shieldTierFromLink(const daAlink_c* i_link) {
    if (i_link == NULL) {
        return ShieldTier::None;
    }
    // WOOD_SHIELD (0x2A) = Ordon house quest shield (Obj_Shield / wshield). Misnamed "carving" helper.
    if (i_link->checkCarvingWoodShieldEquip()) {
        return ShieldTier::Ordon;
    }
    // SHIELD (0x2B) = shop/replacement slot (checkShopWoodShieldEquip). Higher bash cap in mod.
    if (i_link->checkShopWoodShieldEquip()) {
        return ShieldTier::Wooden;
    }
    if (dComIfGs_getSelectEquipShield() == dItemNo_HYLIA_SHIELD_e) {
        return ShieldTier::Hylian;
    }
    ShieldTier tier = shieldTierFromShieldArc(i_link->mShieldArcName);
    if (tier != ShieldTier::None) {
        return tier;
    }
    if (dComIfGs_isItemFirstBit(dItemNo_WOOD_SHIELD_e)) {
        return ShieldTier::Ordon;
    }
    if (dComIfGs_isItemFirstBit(dItemNo_SHIELD_e)) {
        return ShieldTier::Wooden;
    }
    return ShieldTier::None;
}

ShieldTierConfig currentTierCfg(const daAlink_c* i_link) {
    return tierConfig(shieldTierFromLink(i_link));
}

std::unordered_set<fpc_ProcID> sCombatEnemyIDs;
s16 sCombatLastRoomNo = -1;

void registerCombatEnemy(fopAc_ac_c* i_attacker) {
    if (i_attacker != NULL && fopAcM_GetGroup(i_attacker) == fopAc_ENEMY_e) {
        sCombatEnemyIDs.insert(fopAcM_GetID(i_attacker));
    }
}

void syncCombatRoom() {
    daAlink_c* link = daAlink_getAlinkActorClass();
    if (link == NULL) {
        return;
    }

    const s16 roomNo = fopAcM_GetRoomNo(link);
    if (sCombatLastRoomNo >= 0 && roomNo != sCombatLastRoomNo) {
        sCombatEnemyIDs.clear();
    }
    sCombatLastRoomNo = roomNo;
}

// Shield aux HUD (durability row + bash spurs): visible while guarding / guard slip,
// then linger 2s after upper guard anim ends. Not tied to Z-target or combat IDs.
static constexpr u16 SHIELD_HUD_LINGER_FRAMES = 120;
u16 sShieldHudLingerFrames = 0;

u32 sSimFrame = 0;
u32 sGuardOnsetFrame = 0;
bool sWasGuardActive = false;

u8 sBashCharges = 0;
u8 sBashDenyFlashFrames = 0;
bool sBashSpendChainActive = false;
bool sLastBashSpendFromFullBar = false;
bool sLastBashSpendOpenedAtThreshold = false;
bool sFullBarPunishPending = false;
bool sThresholdPunishPending = false;
bool sFullBarBashInFlight = false;
bool sHelmAlbwCharged = false;
fpc_ProcID sHelmPunishTargetId = fpcM_ERROR_PROCESS_ID_e;
dAlbwHelmBashTier sHelmPunishTier = dAlbwHelmBash_THRESHOLD;
u16 sPunishWindowFrames = 0;
static constexpr u16 HELM_PUNISH_WINDOW_FRAMES = 75;
static constexpr f32 HELM_HEAD_LOCK_Y_OFFSET = 130.0f;

void setEnemyHeadLockForHelm(fopEn_enemy_c* i_enemy) {
    if (i_enemy == NULL) {
        return;
    }

    cXyz lockPos(i_enemy->eyePos);
    lockPos.y += HELM_HEAD_LOCK_Y_OFFSET;
    i_enemy->setHeadLockPos(&lockPos);
    i_enemy->onHeadLockFlg();
}
u8 sLastLoggedEquipShield = 0xFF;

u16 sShieldDurability = 0;
u16 sShieldDurabilityMax = 0;
bool sShieldBroken = false;
u8 sLastDurabilityEquipShield = 0xFF;
bool sBashAlbwGranted = false;

f32 sDenyPikariFrame = 0.0f;

struct BashHudResources {
    J2DScreen* mpIconScreen;
    CPaneMgr* mpIconOn;
    CPaneMgr* mpIconOff;
    bool ready;
};

struct BashHudLayout {
    f32 iconScale;
    f32 spacing;
    f32 rowOffsetY;
};

BashHudResources sHud;

BashHudLayout computeBashHudLayout() {
    BashHudLayout layout = {BASH_ICON_SCALE_FALLBACK * g_drawHIO.mSpurIconScale,
                            BASH_ICON_SPACING_FALLBACK, BASH_ROW_OFFSET_FALLBACK};

    if (!sHud.ready || sHud.mpIconOn == NULL) {
        return layout;
    }

    f32 rupeeSize = g_drawHIO.mRupeeScale * 24.0f;
    dMeter2_c* meter = dMeter2Info_getMeterClass();
    if (meter != NULL) {
        dMeter2Draw_c* draw = meter->getMeterDrawPtr();
        if (draw != NULL) {
            const f32 refSize = draw->getRupeeHudReferenceSize();
            if (refSize >= 1.0f) {
                rupeeSize = refSize;
            }
        }
    }

    const f32 hakushaBase = sHud.mpIconOn->getInitSizeX() * sHud.mpIconOn->getInitScaleX();
    if (rupeeSize < 1.0f || hakushaBase < 1.0f) {
        return layout;
    }

    layout.iconScale = g_drawHIO.mSpurIconScale * (rupeeSize / hakushaBase);
    layout.spacing = rupeeSize * 1.08f;
    layout.rowOffsetY = rupeeSize * 1.12f;
    return layout;
}

void applyBashIconScales(const BashHudLayout& i_layout) {
    if (!sHud.ready || sHud.mpIconOn == NULL || sHud.mpIconOff == NULL) {
        return;
    }

    const f32 offMul =
        g_drawHIO.mSpurIconScale > 0.0f ? (g_drawHIO.mUsedSpurIconScale / g_drawHIO.mSpurIconScale) : 1.0f;
    sHud.mpIconOn->scale(i_layout.iconScale, i_layout.iconScale);
    sHud.mpIconOff->scale(i_layout.iconScale * offMul, i_layout.iconScale * offMul);
}

void logChargeEvent(const char* i_event) {
    daAlink_c* link = daAlink_getAlinkActorClass();
    const ShieldTierConfig cfg = currentTierCfg(link);
    OS_REPORT("[dShield] %s: arc=%s equip=%u(0x%x) tier=%u max=%u charges=%u thresh=%u "
              "chain=%u albw=%d/%d\n",
              i_event, link != NULL && link->mShieldArcName != NULL ? link->mShieldArcName : "?",
              dComIfGs_getSelectEquipShield(), dComIfGs_getSelectEquipShield(),
              (unsigned)shieldTierFromLink(link), cfg.maxCharges, sBashCharges, cfg.bashThreshold,
              sBashSpendChainActive ? 1 : 0, dMeter2_getALBWMeterValue(), dMeter2_getALBWMaxValue());
}

static void dShield_debugLogToFile(const char* fmt, ...) {
    if (!dusk::getSettings().game.showDarknutBashDebug.getValue()) {
        return;
    }

    static bool sResetDone = false;

    char path[512];
    path[0] = '\0';
    const char* user = getenv("USERPROFILE");
    if (user && user[0] != '\0') {
        snprintf(path, sizeof(path), "%s/Documents/dusklight/albw_darknut_debug.txt", user);
    } else {
        strncpy(path, "albw_darknut_debug.txt", sizeof(path) - 1);
    }

    FILE* fp = fopen(path, sResetDone ? "a" : "w");
    if (!fp) {
        fp = fopen("albw_darknut_debug.txt", sResetDone ? "a" : "w");
    }
    if (!fp) {
        return;
    }
    if (!sResetDone) {
        sResetDone = true;
        fprintf(fp, "--- ALBW Darknut + shield bash debug ---\n");
    }

    va_list args;
    va_start(args, fmt);
    vfprintf(fp, fmt, args);
    va_end(args);
    fclose(fp);
}

void logEquipTierIfChanged(const daAlink_c* i_link) {
    const u8 equip = dComIfGs_getSelectEquipShield();
    if (equip == sLastLoggedEquipShield) {
        return;
    }
    sLastLoggedEquipShield = equip;
    logChargeEvent("equip-change");
}

void syncDurabilityToEquip(const daAlink_c* i_link) {
    if (!dShield_isDurabilityEnabled() || i_link == NULL || !i_link->checkShieldGet()) {
        return;
    }

    const u8 equip = dComIfGs_getSelectEquipShield();
    if (equip == sLastDurabilityEquipShield && sShieldDurabilityMax != 0) {
        return;
    }

    sLastDurabilityEquipShield = equip;
    const DurabilityTierConfig cfg = durabilityTierConfig(shieldTierFromLink(i_link));
    sShieldDurabilityMax = cfg.maxHp;
    sShieldDurability = cfg.maxHp;
    sShieldBroken = false;
    OS_REPORT("[dShield] durability refill max=%u tier=%u\n", sShieldDurabilityMax,
              (unsigned)shieldTierFromLink(i_link));
}

u16 scaleDurabilityDamage(u16 i_amount, ShieldTier tier, fopAc_ac_c* i_attacker) {
    if (i_amount == 0) {
        return 0;
    }

    u16 amount = i_amount;
    if (i_attacker != NULL && fopAcM_GetGroup(i_attacker) == fopAc_ENEMY_e) {
        amount = dAlbwHP_applyDurabilityMult(fopAcM_GetName(i_attacker), amount);
    }

    if (tier != ShieldTier::Hylian) {
        return amount;
    }
    return (u16)((amount * HYLIAN_DURABILITY_DAMAGE_MUL) / 100);
}

bool applyDurabilityDamage(const daAlink_c* i_link, u16 i_amount, fopAc_ac_c* i_attacker) {
    if (!dShield_isDurabilityEnabled() || i_link == NULL || i_amount == 0 || sShieldBroken) {
        return false;
    }

    const ShieldTier tier = shieldTierFromLink(i_link);
    const u16 scaled = scaleDurabilityDamage(i_amount, tier, i_attacker);
    if (sShieldDurability > scaled) {
        sShieldDurability -= scaled;
    } else {
        sShieldDurability = 0;
    }

    OS_REPORT("[dShield] durability -%u -> %u/%u\n", scaled, sShieldDurability, sShieldDurabilityMax);

    if (sShieldDurability == 0) {
        sShieldBroken = true;
        return true;
    }
    return false;
}

u8 durabilityTierStyle(const daAlink_c* i_link) {
    switch (shieldTierFromLink(i_link)) {
    case ShieldTier::Ordon:
        return 0;
    case ShieldTier::Wooden:
        return 1;
    case ShieldTier::Hylian:
        return 2;
    default:
        return 0;
    }
}

void repairDurabilityOnParry(const daAlink_c* i_link) {
    if (!dShield_isDurabilityEnabled() || i_link == NULL || shieldTierFromLink(i_link) != ShieldTier::Hylian) {
        return;
    }

    const u16 repair = (sShieldDurabilityMax * HYLIAN_PARRY_REPAIR_PCT) / 100;
    sShieldDurability = (u16)std::min<u32>(sShieldDurabilityMax, sShieldDurability + repair);
    sShieldBroken = false;
    OS_REPORT("[dShield] durability parry +%u -> %u/%u\n", repair, sShieldDurability, sShieldDurabilityMax);
}

void ensureDurabilityInitialized(const daAlink_c* i_link) {
    if (sShieldDurabilityMax != 0 || i_link == NULL || !i_link->checkShieldGet()) {
        return;
    }

    sLastDurabilityEquipShield = dComIfGs_getSelectEquipShield();
    const DurabilityTierConfig cfg = durabilityTierConfig(shieldTierFromLink(i_link));
    sShieldDurabilityMax = cfg.maxHp;
    sShieldDurability = cfg.maxHp;
    sShieldBroken = false;
}

void clampChargesToTier(const daAlink_c* i_link) {
    const u8 maxCharges = currentTierCfg(i_link).maxCharges;
    if (sBashCharges > maxCharges) {
        sBashCharges = maxCharges;
    }
    if (sBashCharges == 0) {
        sBashSpendChainActive = false;
    }
}

void loseBashCharges(u8 i_amount) {
    if (i_amount == 0) {
        return;
    }
    if (sBashCharges > i_amount) {
        sBashCharges -= i_amount;
    } else {
        sBashCharges = 0;
    }
    if (sBashCharges == 0) {
        sBashSpendChainActive = false;
    }
}

u8 failBlockChargeLoss(const daAlink_c* i_link) {
    return shieldTierFromLink(i_link) == ShieldTier::Hylian ? HYLIAN_FAIL_CHARGE_LOSS
                                                            : DEFAULT_FAIL_CHARGE_LOSS;
}

void playParryEarnSe() {
    Z2GetAudioMgr()->seStart(Z2SE_ITEM_RING_IN, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
}

void playBashDenySe() {
    Z2GetAudioMgr()->seStart(Z2SE_SY_ITEM_USE_CANCEL, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
}

bool isGuardInputHeld(const daAlink_c* i_link) {
    if (i_link->manualShieldButton()) {
        return true;
    }
    return i_link->checkUpperGuardAnime();
}

bool isInParryWindow() {
    if (sGuardOnsetFrame == 0) {
        return false;
    }
    const u32 age = sSimFrame - sGuardOnsetFrame;
    return age <= PARRY_WINDOW_TOTAL;
}

bool isGuardBreakAttack(int i_atSpl) {
    return i_atSpl == 9 || i_atSpl == 10 || i_atSpl == 11;
}

bool isMountSpearGuardBreakExempt(fopAc_ac_c* i_attacker) {
    if (i_attacker == NULL) {
        return false;
    }

    const s16 name = fopAcM_GetName(i_attacker);
    return name == fpcNm_E_RDB_e || name == fpcNm_B_GND_e;
}

bool isMeterGameplayHudVisible() {
    return !dComIfGp_isPauseFlag() && dComIfGp_isHeapLockFlag() != 6;
}

void grantBashAlbwOnce() {
    if (sBashAlbwGranted) {
        return;
    }

    sBashAlbwGranted = true;
    const int albwBefore = dMeter2_getALBWMeterValue();
    dMeter2_addALBWFraction(BASH_ALBW_HIT_GAIN_NUM, BASH_ALBW_HIT_GAIN_DEN);
    logChargeEvent("bash-hit");
    OS_REPORT("[dShield] bash-hit ALBW %d -> %d (+%d max %d)\n", albwBefore, dMeter2_getALBWMeterValue(),
              (dMeter2_getALBWMaxValue() * BASH_ALBW_HIT_GAIN_NUM) / BASH_ALBW_HIT_GAIN_DEN,
              dMeter2_getALBWMaxValue());
}

void updateShieldHudLinger(daAlink_c* i_link) {
    if (i_link == NULL || !i_link->checkShieldGet() || i_link->checkHorseRide()) {
        sShieldHudLingerFrames = 0;
        return;
    }

    // Any active guard (ZR, Z-target, slip recovery) — no combat / lock-on gate.
    const bool guardHeld =
        i_link->checkPlayerGuard() || i_link->manualShieldButton();
    if (guardHeld) {
        sShieldHudLingerFrames = SHIELD_HUD_LINGER_FRAMES;
    } else if (!i_link->checkGuardSlipMode() && !i_link->checkGuardBreakMode() &&
               sShieldHudLingerFrames > 0) {
        sShieldHudLingerFrames--;
    }
}

bool isShieldGuardRecoveryVisible(const daAlink_c* i_link) {
    if (i_link == NULL) {
        return false;
    }

    return i_link->checkGuardSlipMode() || i_link->checkGuardBreakMode();
}

bool isShieldAuxHudVisible(const daAlink_c* i_link) {
    if (i_link == NULL) {
        return false;
    }

    if (sShieldHudLingerFrames > 0) {
        return true;
    }

    if (isShieldGuardRecoveryVisible(i_link)) {
        return true;
    }

    if (i_link->manualShieldButton()) {
        return true;
    }

    return i_link->checkPlayerGuard();
}

bool shouldShowDurabilityHud() {
    if (!dShield_isDurabilityEnabled()) {
        return false;
    }

    daAlink_c* link = daAlink_getAlinkActorClass();
    if (link == NULL || !link->checkShieldGet() || link->checkHorseRide() || sShieldDurabilityMax == 0) {
        return false;
    }

    return isShieldAuxHudVisible(link);
}

bool shouldShowBashHud() {
    if (!dShield_isParryCombatEnabled()) {
        return false;
    }

    daAlink_c* link = daAlink_getAlinkActorClass();
    if (link == NULL || !link->checkShieldGet() || link->checkHorseRide()) {
        return false;
    }

    if (currentTierCfg(link).maxCharges == 0) {
        return false;
    }

    return isShieldAuxHudVisible(link);
}

void deleteBashHud() {
    if (sHud.mpIconOn != NULL) {
        JKR_DELETE(sHud.mpIconOn);
        sHud.mpIconOn = NULL;
    }
    if (sHud.mpIconOff != NULL) {
        JKR_DELETE(sHud.mpIconOff);
        sHud.mpIconOff = NULL;
    }
    if (sHud.mpIconScreen != NULL) {
        JKR_DELETE(sHud.mpIconScreen);
        sHud.mpIconScreen = NULL;
    }
    sHud.ready = false;
}

bool ensureBashHud() {
    if (sHud.ready) {
        return true;
    }

    dMeter2_c* meter = dMeter2Info_getMeterClass();
    if (meter == NULL) {
        return false;
    }

    dMeter2Draw_c* draw = meter->getMeterDrawPtr();
    if (draw == NULL) {
        return false;
    }

    J2DScreen* mainScreen = draw->getMainScreenPtr();
    if (mainScreen == NULL) {
        return false;
    }

    sHud.mpIconScreen = JKR_NEW J2DScreen();
    if (sHud.mpIconScreen == NULL) {
        deleteBashHud();
        return false;
    }

    if (!sHud.mpIconScreen->setPriority("zelda_game_image_hakusha_parts.blo", 0x20000,
                                        dComIfGp_getMain2DArchive())) {
        deleteBashHud();
        static bool sLoggedBloFailure = false;
        if (!sLoggedBloFailure) {
            sLoggedBloFailure = true;
            OS_REPORT("[dShield] bash HUD: failed to load hakusha_parts.blo\n");
        }
        return false;
    }

    dPaneClass_showNullPane(sHud.mpIconScreen);

    sHud.mpIconOn = JKR_NEW CPaneMgr(sHud.mpIconScreen, MULTI_CHAR('haku_n'), 2, NULL);
    sHud.mpIconOff = JKR_NEW CPaneMgr(sHud.mpIconScreen, MULTI_CHAR('haku_b_n'), 2, NULL);
    if (sHud.mpIconOn == NULL || sHud.mpIconOff == NULL) {
        deleteBashHud();
        return false;
    }

    sHud.mpIconOn->setAlphaRate(1.0f);
    sHud.mpIconOff->setAlphaRate(1.0f);
    applyBashIconScales(computeBashHudLayout());
    sHud.mpIconOn->show();
    sHud.mpIconOff->show();

    sHud.ready = true;
    return true;
}

void tickDenyPikari() {
    if (sBashDenyFlashFrames == 0) {
        sDenyPikariFrame = 0.0f;
        return;
    }

    if (sDenyPikariFrame <= 0.0f) {
        sDenyPikariFrame = 18.0f - g_drawHIO.mSpurIconPikariAnimSpeed;
    } else {
        sDenyPikariFrame += g_drawHIO.mSpurIconPikariAnimSpeed;
        if (sDenyPikariFrame > 28.0f) {
            sDenyPikariFrame = 0.0f;
        }
    }
}

} // namespace

bool dShield_isParryCombatEnabled() {
    return dusk::getSettings().game.shieldParryCombat;
}

bool dShield_isDurabilityEnabled() {
    return dusk::getSettings().game.shieldDurability;
}

void dShield_resetSession() {
    deleteBashHud();
    sSimFrame = 0;
    sGuardOnsetFrame = 0;
    sWasGuardActive = false;
    sBashCharges = 0;
    sBashDenyFlashFrames = 0;
    sBashSpendChainActive = false;
    sLastBashSpendFromFullBar = false;
    sLastBashSpendOpenedAtThreshold = false;
    sFullBarPunishPending = false;
    sThresholdPunishPending = false;
    sFullBarBashInFlight = false;
    sHelmAlbwCharged = false;
    sHelmPunishTargetId = fpcM_ERROR_PROCESS_ID_e;
    sHelmPunishTier = dAlbwHelmBash_THRESHOLD;
    sPunishWindowFrames = 0;
    sDenyPikariFrame = 0.0f;
    sCombatEnemyIDs.clear();
    sCombatLastRoomNo = -1;
    sLastLoggedEquipShield = 0xFF;
    sShieldDurability = 0;
    sShieldDurabilityMax = 0;
    sShieldBroken = false;
    sLastDurabilityEquipShield = 0xFF;
    sBashAlbwGranted = false;
    sShieldHudLingerFrames = 0;
}

void dShield_updateGuardTracking(daAlink_c* i_link) {
    if (i_link == NULL) {
        return;
    }

    if (dShield_isDurabilityEnabled()) {
        syncDurabilityToEquip(i_link);
    }

    updateShieldHudLinger(i_link);

    if (!dShield_isParryCombatEnabled()) {
        return;
    }

    logEquipTierIfChanged(i_link);
    sSimFrame++;

    const bool guardActive = i_link->checkUpperGuardAnime() && isGuardInputHeld(i_link);

    if (guardActive && !sWasGuardActive) {
        sGuardOnsetFrame = sSimFrame;
    } else if (!guardActive) {
        sGuardOnsetFrame = 0;
    }

    sWasGuardActive = guardActive;
    clampChargesToTier(i_link);
    syncCombatRoom();

    if (i_link->checkAttentionLock()) {
        registerCombatEnemy(i_link->getAtnActor());
    }

    if (dShield_hasHelmPunishPending()) {
        fopAc_ac_c* target = i_link->mTargetedActor;
        if (target == NULL || fopAcM_GetID(target) != sHelmPunishTargetId) {
            dShield_clearFullBarPunishPending();
        } else if (fopAcM_GetGroup(target) == fopAc_ENEMY_e) {
            setEnemyHeadLockForHelm((fopEn_enemy_c*)target);
        }
    }

    if (sBashDenyFlashFrames > 0) {
        sBashDenyFlashFrames--;
    }

    if (sPunishWindowFrames > 0) {
        sPunishWindowFrames--;
        if (sPunishWindowFrames == 0) {
            dShield_clearFullBarPunishPending();
            sHelmPunishTargetId = fpcM_ERROR_PROCESS_ID_e;
        }
    }
}

bool dShield_onShieldHit(daAlink_c* i_link, int i_atSpl, fopAc_ac_c* i_attacker) {
    if (!dShield_isParryCombatEnabled()) {
        return false;
    }

    registerCombatEnemy(i_attacker);

    if (!i_link->checkUpperGuardAnime() ||
        (isGuardBreakAttack(i_atSpl) && !dShield_shouldDeferGuardBreak(i_atSpl, i_attacker))) {
        return false;
    }

    if (!isGuardInputHeld(i_link) || !isInParryWindow()) {
        return false;
    }

    const ShieldTierConfig cfg = currentTierCfg(i_link);
    if (cfg.maxCharges == 0) {
        return false;
    }

    dMeter2_addALBWFraction(PARRY_ALBW_GAIN_NUM, PARRY_ALBW_GAIN_DEN);

    if (sBashCharges < cfg.maxCharges) {
        sBashCharges++;
        playParryEarnSe();
        logChargeEvent("parry+");
    }

    repairDurabilityOnParry(i_link);

    return true;
}

void dShield_pollGuardAttackHit(daAlink_c* i_link) {
    if (!dShield_isParryCombatEnabled() || i_link == NULL) {
        return;
    }

    if (i_link->mGuardAtCps.ChkAtHit()) {
        grantBashAlbwOnce();
    }
}

bool dShield_onBlockHit(daAlink_c* i_link, int i_atSpl, bool i_perfect, fopAc_ac_c* i_attacker) {
    if (!dShield_isDurabilityEnabled() || i_link == NULL || i_perfect) {
        return false;
    }

    if (i_link->mEquipItem == dItemNo_IRONBALL_e || !i_link->checkShieldGet() || isGuardBreakAttack(i_atSpl)) {
        return false;
    }

    syncDurabilityToEquip(i_link);
    if (sShieldBroken) {
        return true;
    }

    const DurabilityTierConfig cfg = durabilityTierConfig(shieldTierFromLink(i_link));
    if (cfg.maxHp == 0) {
        return false;
    }

    return applyDurabilityDamage(i_link, cfg.drainBlock, i_attacker);
}

bool dShield_onGuardSlip(daAlink_c* i_link, int i_atSpl) {
    (void)i_atSpl;
    if (!dShield_isDurabilityEnabled() || i_link == NULL) {
        return false;
    }

    syncDurabilityToEquip(i_link);
    return sShieldBroken;
}

bool dShield_onSmallGuardHit(daAlink_c* i_link, fopAc_ac_c* i_attacker) {
    if (!dShield_isDurabilityEnabled() || i_link == NULL) {
        return false;
    }

    if (i_link->mEquipItem == dItemNo_IRONBALL_e || !i_link->checkShieldGet()) {
        return false;
    }

    syncDurabilityToEquip(i_link);
    if (sShieldBroken) {
        return true;
    }

    const DurabilityTierConfig cfg = durabilityTierConfig(shieldTierFromLink(i_link));
    if (cfg.maxHp == 0) {
        return false;
    }

    return applyDurabilityDamage(i_link, cfg.drainSmall, i_attacker);
}

bool dShield_isBroken() {
    return dShield_isDurabilityEnabled() && sShieldBroken;
}

void dShield_onFailedGuardBlock(fopAc_ac_c* i_attacker) {
    if (!dShield_isParryCombatEnabled()) {
        return;
    }

    registerCombatEnemy(i_attacker);

    daAlink_c* link = daAlink_getAlinkActorClass();
    const ShieldTierConfig cfg = currentTierCfg(link);
    if (cfg.maxCharges == 0) {
        return;
    }

    if (shieldTierFromLink(link) == ShieldTier::Hylian) {
        dMeter2_subALBWFraction(FAIL_ALBW_PENALTY_NUM, HYLIAN_FAIL_ALBW_PENALTY_DEN);
    } else {
        dMeter2_subALBWFraction(FAIL_ALBW_PENALTY_NUM, FAIL_ALBW_PENALTY_DEN);
    }

    const u8 loss = failBlockChargeLoss(link);
    if (sBashCharges > 0) {
        loseBashCharges(loss);
    }

    logChargeEvent(loss > 1 ? "fail--" : "fail-");

    if (link != NULL && dMeter2_getALBWMeterValue() <= 0) {
        link->procGuardBreakInit();
    }
}

bool dShield_shouldDeferGuardBreak(int i_atSpl, fopAc_ac_c* i_attacker) {
    return dShield_isParryCombatEnabled() && isGuardBreakAttack(i_atSpl) &&
           !isMountSpearGuardBreakExempt(i_attacker);
}

bool dShield_onFailedGuardBreakBlock(daAlink_c* i_link, int i_atSpl, fopAc_ac_c* i_attacker) {
    if (!dShield_shouldDeferGuardBreak(i_atSpl, i_attacker) || i_link == NULL) {
        return false;
    }

    registerCombatEnemy(i_attacker);
    dShield_onFailedGuardBlock(i_attacker);

    if (dShield_isDurabilityEnabled() && i_link->checkShieldGet() &&
        i_link->mEquipItem != dItemNo_IRONBALL_e) {
        syncDurabilityToEquip(i_link);
        if (!sShieldBroken) {
            const DurabilityTierConfig cfg = durabilityTierConfig(shieldTierFromLink(i_link));
            if (cfg.maxHp != 0) {
                const u16 drain = (u16)((cfg.drainBlock * 3) / 2);
                if (applyDurabilityDamage(i_link, drain, i_attacker)) {
                    dShield_destroyFromDurability(i_link);
                }
            }
        }
    }

    if (!i_link->checkGuardBreakMode()) {
        i_link->procGuardBreakInit();
    }

    return true;
}

void dShield_repairDurabilityFraction(int numerator, int denominator) {
    if (!dShield_isDurabilityEnabled() || numerator <= 0 || denominator <= 0) {
        return;
    }

    daAlink_c* link = daAlink_getAlinkActorClass();
    if (link == NULL || !link->checkShieldGet()) {
        return;
    }

    ensureDurabilityInitialized(link);

    const u16 repair = (u16)((sShieldDurabilityMax * (u32)numerator) / (u32)denominator);
    if (repair == 0) {
        return;
    }

    sShieldDurability = (u16)std::min<u32>(sShieldDurabilityMax, sShieldDurability + repair);
    if (sShieldDurability > 0) {
        sShieldBroken = false;
    }

    OS_REPORT("[dShield] durability oil +%u -> %u/%u\n", repair, sShieldDurability, sShieldDurabilityMax);
}

void dShield_destroyFromDurability(daAlink_c* i_link) {
    if (!dShield_isDurabilityEnabled() || i_link == NULL) {
        return;
    }

    const u8 destroyedShield = dComIfGs_getSelectEquipShield();
    if (destroyedShield == dItemNo_NONE_e) {
        return;
    }

    if (dComIfGs_isItemFirstBit(destroyedShield)) {
        dMeter2_onShieldDestroyedForRental(destroyedShield);
    }

    dMeter2Info_setShield(dItemNo_NONE_e, true);
    i_link->stickArrowIncrement(1);

    if (destroyedShield == dItemNo_WOOD_SHIELD_e || destroyedShield == dItemNo_SHIELD_e) {
        i_link->setWoodShieldBurnOutEffect();
        i_link->seStartOnlyReverb(Z2SE_AL_WOOD_SHIELD_BURN);
    }

    dMeter2Info_setFloatingMessage(2047, 90, false);
    OS_REPORT("[dShield] durability destroy item=%u (msg 2047)\n", destroyedShield);
}

bool dShield_shouldDrawDurabilityHud() {
    return shouldShowDurabilityHud();
}

u16 dShield_getDurability() {
    return sShieldDurability;
}

u16 dShield_getDurabilityMax() {
    return sShieldDurabilityMax;
}

u8 dShield_getDurabilityTierStyle() {
    return durabilityTierStyle(daAlink_getAlinkActorClass());
}

f32 dShield_getDurabilityMeterWidthScale() {
    switch (dShield_getDurabilityTierStyle()) {
    case 0:
        return 0.5f;
    default:
        return 1.0f;
    }
}

bool dShield_tryBeginGuardAttack() {
    sLastBashSpendFromFullBar = false;
    sLastBashSpendOpenedAtThreshold = false;
    sHelmPunishTargetId = fpcM_ERROR_PROCESS_ID_e;

    if (!dShield_isParryCombatEnabled()) {
        return true;
    }

    daAlink_c* link = daAlink_getAlinkActorClass();
    const ShieldTierConfig cfg = currentTierCfg(link);

    if (cfg.maxCharges == 0 || sBashCharges == 0) {
        sBashDenyFlashFrames = 12;
        sDenyPikariFrame = 0.0f;
        playBashDenySe();
        logChargeEvent("bash-deny");
        return false;
    }

    const u8 chargesBeforeSpend = sBashCharges;
    const bool openingChain = !sBashSpendChainActive;
    const bool atFullBar = chargesBeforeSpend >= cfg.maxCharges;
    const bool atThreshold = chargesBeforeSpend >= cfg.bashThreshold;
    const bool canOpenChain = atThreshold;
    const bool canContinueChain = sBashSpendChainActive;

    if (!canOpenChain && !canContinueChain) {
        sBashDenyFlashFrames = 12;
        sDenyPikariFrame = 0.0f;
        playBashDenySe();
        logChargeEvent("bash-deny");
        dShield_debugLogToFile("f=%06d evt=bash-deny at=%u/%u chain=%d\n", g_Counter.mCounter0,
                               chargesBeforeSpend, cfg.maxCharges, sBashSpendChainActive ? 1 : 0);
        return false;
    }

    if (openingChain) {
        sLastBashSpendFromFullBar = atFullBar;
        sLastBashSpendOpenedAtThreshold = atThreshold;
        sFullBarBashInFlight = atFullBar;
    } else {
        if (atFullBar) {
            sLastBashSpendFromFullBar = true;
            sFullBarBashInFlight = true;
        } else if (sFullBarBashInFlight) {
            sLastBashSpendFromFullBar = true;
        }

        if (atThreshold) {
            sLastBashSpendOpenedAtThreshold = true;
        }
    }

    if (!sBashSpendChainActive) {
        sBashSpendChainActive = true;
    }

    sBashCharges--;
    if (sBashCharges == 0) {
        sBashSpendChainActive = false;
    }

    logChargeEvent("bash-start");
    dShield_debugLogToFile(
        "f=%06d evt=bash-start at=%u/%u after=%u open=%d chain=%d full=%d pending=%d inFlight=%d "
        "lastFull=%d\n",
        g_Counter.mCounter0, chargesBeforeSpend, cfg.maxCharges, sBashCharges, openingChain ? 1 : 0,
        sBashSpendChainActive ? 1 : 0, atFullBar ? 1 : 0, sFullBarPunishPending ? 1 : 0,
        sFullBarBashInFlight ? 1 : 0, sLastBashSpendFromFullBar ? 1 : 0);
    return true;
}

bool dShield_consumeLastBashSpendFromFullBar() {
    if (!sLastBashSpendFromFullBar) {
        dShield_debugLogToFile("f=%06d evt=gb-consume fail pending=%d inFlight=%d bash=%u/%u\n",
                               g_Counter.mCounter0, sFullBarPunishPending ? 1 : 0,
                               sFullBarBashInFlight ? 1 : 0, sBashCharges,
                               dShield_getMaxBashCharges());
        return false;
    }

    sLastBashSpendFromFullBar = false;
    sFullBarBashInFlight = false;
    sFullBarPunishPending = true;
    dShield_debugLogToFile("f=%06d evt=gb-consume ok pending=1 bash=%u/%u\n", g_Counter.mCounter0,
                           sBashCharges, dShield_getMaxBashCharges());
    return true;
}

void dShield_clearFullBarPunishPending() {
    sFullBarPunishPending = false;
    sThresholdPunishPending = false;
    sFullBarBashInFlight = false;
    sPunishWindowFrames = 0;
    sHelmPunishTargetId = fpcM_ERROR_PROCESS_ID_e;
}

void dShield_clearHelmSplitterMeterCharge() {
    sHelmAlbwCharged = false;
}

bool dShield_chargeHelmSplitterMeterOnce() {
    if (sHelmAlbwCharged) {
        return true;
    }

    const int albwBefore = dMeter2_getALBWMeterValue();
    dMeter2_onALBWHiddenSkill();
    dMeter2_commitALBWHiddenSkillIfPending();
    sHelmAlbwCharged = true;
    dShield_debugLogToFile("f=%06d evt=helm-drain bash=%u/%u albw=%d->%d\n", g_Counter.mCounter0,
                           sBashCharges, dShield_getMaxBashCharges(), albwBefore,
                           dMeter2_getALBWMeterValue());
    return true;
}

bool dShield_hasFullBarPunishPending() {
    return sFullBarPunishPending;
}

bool dShield_hasThresholdPunishPending() {
    return sThresholdPunishPending;
}

bool dShield_hasHelmPunishPending() {
    return sFullBarPunishPending || sThresholdPunishPending;
}

bool dShield_hasHelmPunishOnTarget(fopEn_enemy_c* i_enemy) {
    if (i_enemy == NULL || !dShield_hasHelmPunishPending()) {
        return false;
    }

    return sHelmPunishTargetId == fopAcM_GetID(i_enemy);
}

bool dShield_canUseFullBarPunish() {
    if (!dShield_isParryCombatEnabled()) {
        return true;
    }

    return sFullBarPunishPending;
}

bool dShield_tryConsumeFullBarPunish() {
    if (!dShield_isParryCombatEnabled()) {
        return true;
    }

    if (!sFullBarPunishPending) {
        return false;
    }

    sFullBarPunishPending = false;
    sFullBarBashInFlight = false;
    logChargeEvent("full-punish");
    return true;
}

bool dShield_tryConsumeThresholdPunish() {
    if (!dShield_isParryCombatEnabled()) {
        return true;
    }

    if (!sThresholdPunishPending) {
        return false;
    }

    sThresholdPunishPending = false;
    logChargeEvent("threshold-punish");
    return true;
}

bool dShield_consumeLastBashSpendOpenedAtThreshold() {
    if (!sLastBashSpendOpenedAtThreshold) {
        dShield_debugLogToFile("f=%06d evt=p2-consume fail bash=%u/%u threshold=%d\n",
                               g_Counter.mCounter0, sBashCharges, dShield_getMaxBashCharges(),
                               sLastBashSpendOpenedAtThreshold ? 1 : 0);
        return false;
    }

    sLastBashSpendOpenedAtThreshold = false;
    sThresholdPunishPending = true;
    dShield_debugLogToFile("f=%06d evt=p2-consume ok pending=1 bash=%u/%u\n", g_Counter.mCounter0,
                           sBashCharges, dShield_getMaxBashCharges());
    return true;
}

bool dShield_hasFullBarBashInFlight() {
    return sFullBarBashInFlight;
}

bool dShield_tryGrantHelmPunishCredit(fopEn_enemy_c* i_enemy) {
    if (!dShield_isParryCombatEnabled() || !dAlbw_isHiddenSkillReworkEnabled() || i_enemy == NULL) {
        return false;
    }

    const fpc_ProcID enemyId = fopAcM_GetID(i_enemy);
    if (sHelmPunishTargetId == enemyId && dShield_hasHelmPunishPending()) {
        return true;
    }

    const dAlbwHelmBashTier tier = dAlbwCombat_getHelmBashTier(i_enemy);
    const bool granted = tier == dAlbwHelmBash_MAX ? dShield_consumeLastBashSpendFromFullBar()
                                                   : dShield_consumeLastBashSpendOpenedAtThreshold();
    if (!granted) {
        dShield_debugLogToFile("f=%06d evt=helm-credit-deny tier=%d bash=%u/%u thr=%u\n",
                               g_Counter.mCounter0, (int)tier, sBashCharges,
                               dShield_getMaxBashCharges(), dShield_getBashThreshold());
        return false;
    }

    sHelmPunishTargetId = enemyId;
    sHelmPunishTier = tier;
    sPunishWindowFrames = HELM_PUNISH_WINDOW_FRAMES;
    setEnemyHeadLockForHelm(i_enemy);
    dShield_debugLogToFile("f=%06d evt=helm-credit-ok tier=%d window=%u bash=%u/%u\n",
                           g_Counter.mCounter0, (int)tier, sPunishWindowFrames, sBashCharges,
                           dShield_getMaxBashCharges());
    return true;
}

bool dShield_tryBeginHelmSplitter() {
    if (!dShield_isParryCombatEnabled() || !dAlbw_isHiddenSkillReworkEnabled()) {
        return true;
    }

    daAlink_c* link = daAlink_getAlinkActorClass();
    fopEn_enemy_c* enemy =
        link != NULL ? (fopEn_enemy_c*)link->mTargetedActor : NULL;

    if (!dShield_hasHelmPunishOnTarget(enemy)) {
        sBashDenyFlashFrames = 12;
        sDenyPikariFrame = 0.0f;
        playBashDenySe();
        logChargeEvent("helm-deny");
        dShield_debugLogToFile("f=%06d evt=helm-deny-wrong-target pending=%d target=%d credit=%d\n",
                               g_Counter.mCounter0, dShield_hasHelmPunishPending() ? 1 : 0,
                               enemy != NULL ? (int)fopAcM_GetID(enemy) : -1,
                               (int)sHelmPunishTargetId);
        return false;
    }

    const dAlbwHelmBashTier tier = sHelmPunishTier;
    const bool consumed = tier == dAlbwHelmBash_MAX ? dShield_tryConsumeFullBarPunish()
                                                    : dShield_tryConsumeThresholdPunish();
    if (consumed) {
        sHelmPunishTargetId = fpcM_ERROR_PROCESS_ID_e;
        sPunishWindowFrames = 0;
        dShield_clearHelmSplitterMeterCharge();
        dShield_debugLogToFile("f=%06d evt=helm-ok tier=%d bash=%u/%u\n", g_Counter.mCounter0,
                               (int)tier, sBashCharges, dShield_getMaxBashCharges());
        return true;
    }

    sBashDenyFlashFrames = 12;
    sDenyPikariFrame = 0.0f;
    playBashDenySe();
    logChargeEvent("helm-deny");
    dShield_debugLogToFile("f=%06d evt=helm-deny pending=%d inFlight=%d bash=%u/%u\n",
                           g_Counter.mCounter0, sFullBarPunishPending ? 1 : 0,
                           sFullBarBashInFlight ? 1 : 0, sBashCharges,
                           dShield_getMaxBashCharges());
    return false;
}

void dShield_onGuardAttackConnect() {
    if (!dShield_isParryCombatEnabled()) {
        return;
    }

    grantBashAlbwOnce();

    daAlink_c* link = daAlink_getAlinkActorClass();
    if (link == NULL) {
        return;
    }

    cCcD_Obj* hitObj = link->mGuardAtCps.GetAtHitObj();
    if (hitObj == NULL) {
        return;
    }

    fopAc_ac_c* hitActor = hitObj->GetAc();
    if (hitActor != NULL && fopAcM_GetGroup(hitActor) == fopAc_ENEMY_e) {
        dShield_tryGrantHelmPunishCredit((fopEn_enemy_c*)hitActor);
    }
}

u8 dShield_getBashCharges() {
    return sBashCharges;
}

u8 dShield_getMaxBashCharges() {
    return currentTierCfg(daAlink_getAlinkActorClass()).maxCharges;
}

u8 dShield_getBashThreshold() {
    return currentTierCfg(daAlink_getAlinkActorClass()).bashThreshold;
}

u8 dShield_getDenyFlashFrames() {
    return sBashDenyFlashFrames;
}

bool dShield_canSpendBash() {
    if (!dShield_isParryCombatEnabled()) {
        return true;
    }

    daAlink_c* link = daAlink_getAlinkActorClass();
    const ShieldTierConfig cfg = currentTierCfg(link);
    if (cfg.maxCharges == 0 || sBashCharges == 0) {
        return false;
    }
    return sBashCharges >= cfg.bashThreshold || sBashSpendChainActive;
}

bool dShield_isBashSpendChainActive() {
    return sBashSpendChainActive;
}

void dShield_drawBashCharges() {
    daAlink_c* link = daAlink_getAlinkActorClass();
    updateShieldHudLinger(link);

    // ============================================
    // NEW CODE — ALBW Port
    // Wolf charge icons occupy this row during wolf form.
    // Suppress spur rendering for that window; they return on transform back.
    // ============================================
    if (daPy_py_c::checkNowWolf() && dAlbwWolfCombat_isEnabled()) {
        return;
    }
    // ============================================
    // NEW CODE ENDS HERE
    // ============================================

    if (!shouldShowBashHud() || !ensureBashHud()) {
        return;
    }

    const u8 maxCharges = dShield_getMaxBashCharges();
    const u8 charges = dShield_getBashCharges();
    const bool denyFlash = sBashDenyFlashFrames > 0;

    tickDenyPikari();

    const BashHudLayout layout = computeBashHudLayout();
    applyBashIconScales(layout);

    if (sHud.mpIconScreen == NULL || sHud.mpIconOn == NULL || sHud.mpIconOff == NULL) {
        return;
    }

    Vec rupeeCenter;
    rupeeCenter.x = g_drawHIO.mRupeePosX;
    rupeeCenter.y = g_drawHIO.mRupeePosY;
    rupeeCenter.z = 0.0f;

    dMeter2_c* meter = dMeter2Info_getMeterClass();
    if (meter != NULL) {
        dMeter2Draw_c* draw = meter->getMeterDrawPtr();
        if (draw != NULL && !draw->getShieldHudAnchorCenter(&rupeeCenter)) {
            draw->getRupeeAnchorCenter(&rupeeCenter);
        }
    }
    const f32 totalWidth = layout.spacing * (f32)(maxCharges - 1);
    f32 posX = rupeeCenter.x - totalWidth * 0.5f;
    const f32 posY = rupeeCenter.y - layout.rowOffsetY;

    J2DGrafContext* grafCtx = dComIfGp_getCurrentGrafPort();
    if (grafCtx == NULL) {
        return;
    }
    grafCtx->setup2D();

    dMeter2Draw_c* meterDraw = meter != NULL ? meter->getMeterDrawPtr() : NULL;

    for (int i = 0; i < maxCharges; i++) {
        const bool filled = (u8)i < charges;

        if (filled) {
            sHud.mpIconOn->show();
            sHud.mpIconOff->hide();
            sHud.mpIconOn->setAlphaRate(1.0f);
        } else {
            sHud.mpIconOn->hide();
            sHud.mpIconOff->show();
            sHud.mpIconOff->setAlphaRate(1.0f);
        }

        sHud.mpIconOn->translate(posX, posY);
        sHud.mpIconOff->translate(posX, posY);
        sHud.mpIconScreen->draw(0.0f, 0.0f, grafCtx);

        if (denyFlash && sDenyPikariFrame > 0.0f && meterDraw != NULL) {
            const Vec center = sHud.mpIconOn->getGlobalVtxCenter(false, 0);
            const f32 pikariScale =
                g_drawHIO.mSpurIconScale > 0.0f
                    ? g_drawHIO.mSpurIconPikariScale * (layout.iconScale / g_drawHIO.mSpurIconScale)
                    : g_drawHIO.mSpurIconPikariScale;
            meterDraw->drawPikariHakusha(
                center.x, center.y, sDenyPikariFrame, pikariScale * 0.85f,
                g_drawHIO.mSpurIconPikariFrontOuter, g_drawHIO.mSpurIconPikariFrontInner,
                g_drawHIO.mSpurIconPikariBackOuter, g_drawHIO.mSpurIconPikariBackInner);
        }

        posX += layout.spacing;
    }
}

#endif // TARGET_PC

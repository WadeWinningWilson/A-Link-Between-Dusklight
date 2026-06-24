#include "dusk/truetest.hpp"

#include "d/d_com_inf_game.h"
#include "d/d_save.h"
#include "d/d_stage.h"
#include "dusk/settings.h"
#include "f_op/f_op_scene_mng.h"

namespace dusk::truetest {

static dSv_reserve_c& reserve() {
    return dComIfGs_getSaveData()->getReserve();
}

bool isTrueTestSave() {
    return reserve().isTrueTest();
}

bool isFieldGameplayActive() {
    return fpcM_SearchByName(fpcNm_PLAY_SCENE_e) != NULL;
}

bool canChangeGlobalTrueAlbwMode() {
    return !isFieldGameplayActive();
}

bool isAlbwPostmanUnlocked() {
    return isTrueTestSave() || dComIfGs_isEventBit(dSv_event_flag_c::F_0625);
}

bool isTrueAlbwShopEnabled() {
    if (reserve().isTrueTest()) {
        return true;
    }

    const TrueAlbwMode mode = getSettings().game.trueAlbwMode.getValue();
    return mode == TrueAlbwMode::TrueAlbw || mode == TrueAlbwMode::TrueTest;
}

static void applyTwilightBubblePolicy() {
    dComIfGs_onDarkClearLV(0);
    for (int i = 1; i < 4; ++i) {
        dComIfGs_offDarkClearLV(i);
    }

    // Forced wolf in uncleared bubbles; MS allows voluntary form (see Wolf_Change_Check hook).
    dComIfGs_offEventBit(dSv_event_flag_c::M_077);
    for (int i = 0; i < 3; ++i) {
        dComIfGs_onTransformLV(i);
    }
}

static void applyBridgeState() {
    dComIfGs_onEventBit(dSv_event_flag_c::M_018);
    dComIfGs_onEventBit(dSv_event_flag_c::M_092);
}

bool isGanonBarrierCleared() {
    for (u8 i = 0; i < 4; ++i) {
        if (!dComIfGs_isCollectMirror(i)) {
            return false;
        }
    }
    for (u8 i = 0; i < 3; ++i) {
        if (!dComIfGs_isCollectCrystal(i)) {
            return false;
        }
    }
    return true;
}

static void syncGanonBarrierEventFlags() {
    if (isGanonBarrierCleared()) {
        dComIfGs_onEventBit(dSv_event_flag_c::F_0542);
    } else {
        dComIfGs_offEventBit(dSv_event_flag_c::F_0542);
    }
}

static void applyGanonGatePolicyMigration() {
    if (reserve().hasGanonGatePolicyV1()) {
        return;
    }

    for (u8 i = 0; i < 4; ++i) {
        dComIfGs_offCollectMirror(i);
    }
    for (u8 i = 0; i < 3; ++i) {
        dComIfGs_offCollectCrystal(i);
    }
    syncGanonBarrierEventFlags();
    reserve().setGanonGatePolicyV1(true);
}

static void applyWorldBootstrap() {
    applyTwilightBubblePolicy();
    applyBridgeState();

    dComIfGs_onEventBit(dSv_event_flag_c::F_0238);
    // Do not set F_0625 (Saved Talo): Postman uses isTrueTestSave() instead.
    // F_0625 without the Ordon day-3 chain triggers the Rusl escort near Coro.
    // Ganon barrier: cleared via isGanonBarrierCleared() (mirrors + fused shadows), not F_0542 bootstrap.

    dComIfGs_onEventBit(dSv_event_flag_c::F_0354);
    syncGanonBarrierEventFlags();
}

static void applyTwilightPolicyMigration() {
    if (reserve().hasTwilightPolicyV1()) {
        return;
    }

    applyTwilightBubblePolicy();
    applyBridgeState();
    reserve().setTwilightPolicyV1(true);
}

static void sanitizeOrdonStoryFlags() {
    if (dComIfGs_isEventBit(dSv_event_flag_c::F_0625)) {
        dComIfGs_offEventBit(dSv_event_flag_c::F_0625);
    }
}

static void applyFaronWorldState() {
    // Layer drivers (see getLayerNo_common_common F_SP108): past Ordon day 2 + Forest Temple.
    dComIfGs_onEventBit(dSv_event_flag_c::F_0565);
    dComIfGs_onEventBit(dSv_event_flag_c::M_022);

    // Coro / lantern arc — avoid post-twilight first-meeting giveaway on TRUETEST.
    dComIfGs_onEventBit(dSv_event_flag_c::M_095);
    dComIfGs_onEventBit(dSv_event_flag_c::F_0217);
    dComIfGs_onEventBit(dSv_event_flag_c::F_0218);
    dComIfGs_onEventBit(dSv_event_flag_c::F_0222);
    dComIfGs_onEventBit(dSv_event_flag_c::F_0223);
    dComIfGs_onEventBit(dSv_event_flag_c::F_0224);
    dComIfGs_onEventBit(dSv_event_flag_c::F_0226);
    dComIfGs_onEventBit(dSv_event_flag_c::F_0248);
    dComIfGs_onEventBit(dSv_event_flag_c::F_0522);

    // Coro gate → Hyrule Field (memory switches set by monkey / gate events in vanilla).
    dComIfGs_onSaveSwitch(81);
    dComIfGs_onSaveSwitch(82);
    dComIfGs_onSaveSwitch(83);
}

namespace {

enum class AreaFlagType : u8 { Tbox, Switch, Item };

struct AreaFlagInd {
    AreaFlagType type;
    int flagID;
};

static AreaFlagType byteIndexToAreaFlagType(u8 byteIndex) {
    if (byteIndex < 2 * sizeof(u32)) {
        return AreaFlagType::Tbox;
    }
    if (byteIndex < 6 * sizeof(u32)) {
        return AreaFlagType::Switch;
    }
    return AreaFlagType::Item;
}

static u8 byteIndexToAreaByteIndex(u8 byteIndex) {
    if (byteIndex < 2 * sizeof(u32)) {
        return byteIndex;
    }
    if (byteIndex < 6 * sizeof(u32)) {
        return byteIndex - 2 * sizeof(u32);
    }
    return byteIndex - 6 * sizeof(u32);
}

static u8 byteIndexToAreaU32Index(u8 byteIndex) {
    return byteIndexToAreaByteIndex(byteIndex) / sizeof(u32);
}

static AreaFlagInd areaFlagFromEventMask(u16 eventFlag) {
    constexpr int areaIndexSize = 5;
    const u8 byteInd = eventFlag >> 8;
    const u8 eventU32Ind = byteIndexToAreaU32Index(byteInd);
    const u8 relativeByteInd = byteIndexToAreaByteIndex(byteInd);
    const u8 bitsToSkip = 8 * ((sizeof(u32) - 1) - (relativeByteInd % sizeof(u32)));
    const int areaFlag = (eventU32Ind << areaIndexSize) |
                         ((std::countr_zero(eventFlag) + bitsToSkip) & ((1 << areaIndexSize) - 1));
    return AreaFlagInd{byteIndexToAreaFlagType(byteInd), areaFlag};
}

static void onStageAreaSwitch(int stageNo, u16 eventAreaMask) {
    const AreaFlagInd flag = areaFlagFromEventMask(eventAreaMask);
    if (flag.type != AreaFlagType::Switch) {
        return;
    }
    dComIfGs_getSaveData()->getSave(stageNo).getBit().onSwitch(flag.flagID);
}

static void applyHyruleFieldAccess() {
    // Hyrule Field (dStage_SaveTbl_FIELD): cleared boulders / open paths into Castle Town.
    // Matches eventAreaFlagsHyruleField in ImGuiEventFlags.hpp.
    static constexpr u16 kFieldRockAndPathFlags[] = {
        0x0808,  // East Castle Town bridge
        0x0B08,  // Castle Town portal
        0x0D02,  // enter field west of castle town from the east
        0x0E01,  // enter bridge east of castle town
        0x0F10,  // open path from faron field to south of castle town
        0x0F40,  // enter field west of castle town from the north
        0x0F80,  // enter field south of castle town
        0x1204,  // blown up rock south of castle town (Coro / faron approach)
        0x1208,  // blown up rock west of castle town
        0x1210,  // blown up northern rock west of castle town
    };

    for (u16 flag : kFieldRockAndPathFlags) {
        onStageAreaSwitch(dStage_SaveTbl_FIELD, flag);
    }
}

}  // namespace

void applyNewSavePreset(bool i_trueTestSelected) {
    reserve().setTrueTest(i_trueTestSelected);
    reserve().setBootstrapApplied(false);
    reserve().setTwilightPolicyV1(false);
    reserve().setGanonGatePolicyV1(false);
}

int getFaronLayerFloor() {
    return isTrueTestSave() ? 2 : -1;
}

void onStageLoad() {
    if (!reserve().isTrueTest()) {
        return;
    }

    // Clear F_0625 from first-bootstrap saves and any accidental re-sets.
    sanitizeOrdonStoryFlags();
    applyFaronWorldState();
    applyHyruleFieldAccess();
    applyTwilightPolicyMigration();
    applyGanonGatePolicyMigration();
    syncGanonBarrierEventFlags();

    if (reserve().isBootstrapApplied()) {
        return;
    }

    applyWorldBootstrap();
    reserve().setBootstrapApplied(true);
    reserve().setTwilightPolicyV1(true);
    reserve().setGanonGatePolicyV1(true);
}

}  // namespace dusk::truetest

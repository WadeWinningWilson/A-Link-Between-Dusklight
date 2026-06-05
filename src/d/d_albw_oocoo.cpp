// ============================================
// NEW CODE — ALBW Port
// Postman Cuckoo's Return service (dungeon entrance warp).
// ============================================
#include "d/d_albw_oocoo.h"

#if TARGET_PC

#include "d/actor/d_a_alink.h"
#include "d/d_item_data.h"
#include "d/d_com_inf_game.h"
#include "d/d_save.h"

namespace {

constexpr int kPrice = 300;

char sDeathDungeonStage[8] = {};
bool sDiedInDungeon        = false;
bool sChoseOrdonAfterDeath = false;
bool sUsedOocooThisDeath   = false;
bool sPendingEntranceWarp  = false;

bool hasMetOocooSrOnce() {
    // Sr. in inventory, Oocoo's note (TKS letter), Jr. after first warp, or dungeon warp flag.
    return dComIfGp_checkItemGet(dItemNo_DUNGEON_EXIT_e, 1)
        || dComIfGp_checkItemGet(dItemNo_DUNGEON_EXIT_2_e, 1)
        || dComIfGp_checkItemGet(dItemNo_LV7_DUNGEON_EXIT_e, 1)
        || dComIfGp_checkItemGet(dItemNo_DUNGEON_BACK_e, 1)
        || dComIfGp_checkItemGet(dItemNo_TKS_LETTER_e, 1)
        || dComIfGs_isDungeonItemWarp();
}

bool postmanRentalActive() {
    return dComIfGs_isEventBit(dSv_event_flag_c::saveBitLabels[625]);
}

}  // namespace

void dALBWOocoo_onDeathWarpContext(const char* i_lastStageName, bool i_diedInDungeon) {
    sDiedInDungeon        = i_diedInDungeon;
    sChoseOrdonAfterDeath = false;
    sUsedOocooThisDeath   = false;
    sPendingEntranceWarp  = false;
    sDeathDungeonStage[0] = '\0';

    if (i_diedInDungeon && i_lastStageName != NULL) {
        int i = 0;
        for (; i < 7 && i_lastStageName[i] != '\0'; i++) {
            sDeathDungeonStage[i] = i_lastStageName[i];
        }
        sDeathDungeonStage[i] = '\0';
    }
}

void dALBWOocoo_onWarpChoice(int i_choice) {
    if (i_choice == 1 && sDiedInDungeon) {
        sChoseOrdonAfterDeath = true;
    }
}

bool dALBWOocoo_canShowInShop() {
    if (!postmanRentalActive()) {
        return false;
    }
    if (!hasMetOocooSrOnce()) {
        return false;
    }
    if (!sDiedInDungeon || !sChoseOrdonAfterDeath) {
        return false;
    }
    return !sUsedOocooThisDeath;
}

bool dALBWOocoo_tryPurchase() {
    if (!dALBWOocoo_canShowInShop()) {
        return false;
    }
    if (sDeathDungeonStage[0] == '\0') {
        return false;
    }

    const u16 rupees = dComIfGs_getRupee();
    if (rupees < (u16)kPrice) {
        return false;
    }

    dComIfGs_setRupee(rupees - (u16)kPrice);
    sUsedOocooThisDeath  = true;
    sPendingEntranceWarp = true;
    return true;
}

void dALBWOocoo_executePendingWarp() {
    if (!sPendingEntranceWarp) {
        return;
    }
    sPendingEntranceWarp = false;

    if (sDeathDungeonStage[0] == '\0') {
        return;
    }

    // Entrance spawn: room 0, point 0 (vanilla escape-warp landing pattern).
    dComIfGp_setNextStage(sDeathDungeonStage, 0, 0, -1);

    daAlink_c* link = daAlink_getAlinkActorClass();
    if (link != NULL) {
        link->procDungeonWarpSceneStartInit();
    }
}

const char* dALBWOocoo_getServiceName() {
    return "Oocoo's Return";
}

const char* dALBWOocoo_getServiceDesc() {
    return "A small friend flew all this way here to help! Let me feed him "
           "to restore his strength.";
}

int dALBWOocoo_getServicePrice() {
    return kPrice;
}

#endif  // TARGET_PC

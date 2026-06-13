/**
 * d_albw_master_quest.cpp
 * ALBW Master Quest — shop tiers, save-backed purchase counts, heart grant scaling.
 */

#if TARGET_PC

#include "d/d_albw_master_quest.h"
#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"
#include "d/d_meter2_info.h"
#include "dusk/settings.h"

namespace {

// Full byte of dSv_event_c::mEvent[] — indices 100–102 (confirmed unused by vanilla).
static constexpr u16 kHeartShopTierReg      = static_cast<u16>(100 << 8) | 0xFF;
static constexpr u16 kMeterShopTierReg      = static_cast<u16>(101 << 8) | 0xFF;
static constexpr u16 kBonusHalfHeartsReg    = static_cast<u16>(102 << 8) | 0xFF;

static constexpr int kAlbwMQMaxLifeQuarters = 80;  // 20 hearts

static constexpr int kHeartShopPrices[kAlbwMQHeartShopTiers] = {
    44, 56, 72, 92, 117, 149, 191, 244, 311, 500, 1000, 2100, 3200, 4000, 5000, 8000, 9999,
};

static constexpr int kMeterShopPrices[kAlbwMQMeterShopTiers] = {
    100, 110, 120, 130, 140, 150, 200, 210, 220, 230, 240, 250,
    300, 400, 500, 800, 1000, 2000, 2500, 2500, 3333, 3333, 3333,
};

static u8 readTierReg(u16 reg) {
    return dComIfGs_getEventReg(reg);
}

static void writeTierReg(u16 reg, u8 tier) {
    dComIfGs_setEventReg(reg, tier);
}

static u8 readBonusHalfHearts() {
    return readTierReg(kBonusHalfHeartsReg);
}

static void writeBonusHalfHearts(u8 count) {
    writeTierReg(kBonusHalfHeartsReg, count);
}

// Half a heart = +2 quarter-pips. Bonus is stored separately because (maxLife/5)*4 jumps in steps of 4.
// Do not bump save maxLife here — that adds a phantom full container on top of the bonus.
static void grantHalfHeartMaxCapacity() {
    if (dComIfGs_getMaxLifeGauge() + 2 > kAlbwMQMaxLifeQuarters) {
        return;
    }

    writeBonusHalfHearts(static_cast<u8>(readBonusHalfHearts() + 1));
    dComIfGp_setItemLifeCount(2.0f, 0);
}

}  // namespace

int dAlbwMQ_getBonusMaxLifeQuarters() {
    if (!dAlbwMQ_isEnabled()) {
        return 0;
    }
    return readBonusHalfHearts() * 2;
}

u16 dAlbwMQ_getDisplayMaxLifeInternal() {
    const u16 gauge = dComIfGs_getMaxLifeGauge();
    return static_cast<u16>(((gauge + 3) / 4) * 5);
}

bool dAlbwMQ_isEnabled() {
    return dusk::getSettings().game.masterQuest.getValue();
}

int dAlbwMQ_getHeartShopTier() {
    const int tier = readTierReg(kHeartShopTierReg);
    if (tier < 0) {
        return 0;
    }
    if (tier > kAlbwMQHeartShopTiers) {
        return kAlbwMQHeartShopTiers;
    }
    return tier;
}

int dAlbwMQ_getMeterShopTier() {
    const int tier = readTierReg(kMeterShopTierReg);
    if (tier < 0) {
        return 0;
    }
    if (tier > kAlbwMQMeterShopTiers) {
        return kAlbwMQMeterShopTiers;
    }
    return tier;
}

int dAlbwMQ_getHeartShopPrice(int tier) {
    if (tier < 0 || tier >= kAlbwMQHeartShopTiers) {
        return 0;
    }
    return kHeartShopPrices[tier];
}

int dAlbwMQ_getMeterShopPrice(int tier) {
    if (tier < 0 || tier >= kAlbwMQMeterShopTiers) {
        return 0;
    }
    return kMeterShopPrices[tier];
}

bool dAlbwMQ_canPurchaseHeartShop() {
    if (!dAlbwMQ_isEnabled()) {
        return false;
    }
    if (dComIfGs_getMaxLifeGauge() + 2 > kAlbwMQMaxLifeQuarters) {
        return false;
    }
    return dAlbwMQ_getHeartShopTier() < kAlbwMQHeartShopTiers;
}

bool dAlbwMQ_canPurchaseMeterShop() {
    if (!dAlbwMQ_isEnabled()) {
        return false;
    }
    return dAlbwMQ_getMeterShopTier() < kAlbwMQMeterShopTiers;
}

bool dAlbwMQ_tryPurchaseHeartShop() {
    if (!dAlbwMQ_canPurchaseHeartShop()) {
        return false;
    }
    grantHalfHeartMaxCapacity();
    writeTierReg(kHeartShopTierReg, static_cast<u8>(dAlbwMQ_getHeartShopTier() + 1));
    return true;
}

bool dAlbwMQ_tryPurchaseMeterShop() {
    if (!dAlbwMQ_canPurchaseMeterShop()) {
        return false;
    }
    writeTierReg(kMeterShopTierReg, static_cast<u8>(dAlbwMQ_getMeterShopTier() + 1));
    dMeter2_onALBWMeterShopPurchase();
    return true;
}

void dAlbwMQ_applyPieceHeartGrant() {
    if (!dAlbwMQ_isEnabled() || dComIfGs_getMaxLifeGauge() + 2 > kAlbwMQMaxLifeQuarters) {
        return;
    }
    // Fifth piece in a cycle (maxLife % 5 == 4 before grant): half a heart for the set.
    if ((dComIfGs_getMaxLife() % 5) == 4) {
        grantHalfHeartMaxCapacity();
    }
}

void dAlbwMQ_applyContainerHeartGrant() {
    if (!dAlbwMQ_isEnabled() || dComIfGs_getMaxLifeGauge() + 2 > kAlbwMQMaxLifeQuarters) {
        return;
    }
    grantHalfHeartMaxCapacity();
}

const char* dAlbwMQ_getHeartShopName() {
    return "Heart Upgrade";
}

const char* dAlbwMQ_getMeterShopName() {
    return "Stamina Upgrade";
}

const char* dAlbwMQ_getHeartShopDesc() {
    if (dComIfGs_getMaxLifeGauge() + 2 > kAlbwMQMaxLifeQuarters) {
        return "Sold out.";
    }
    if (!dAlbwMQ_canPurchaseHeartShop()) {
        return "Sold out.";
    }
    return "Permanently increases your maximum health by half a heart.";
}

const char* dAlbwMQ_getMeterShopDesc() {
    if (!dAlbwMQ_canPurchaseMeterShop()) {
        return "Sold out.";
    }
    return "Permanently expands your ALBW stamina meter.";
}

#endif

/**
 * d_albw_master_quest.h
 * ALBW Master Quest — optional Postman shop upgrades and reduced heart/meter grants.
 */

#pragma once

#if TARGET_PC

// ============================================
// NEW CODE — ALBW Port
// ============================================

static constexpr int kAlbwMQHeartShopTiers     = 17;
static constexpr int kAlbwMQMeterShopTiers     = 23;
static constexpr int kAlbwMQMeterUnitsPerBuy   = 632;
static constexpr int kAlbwMQMaxInternalLife    = 100;

bool dAlbwMQ_isEnabled();

// Extra max-HP quarter-pips from MQ half-heart grants (shop, 5th piece, containers).
int  dAlbwMQ_getBonusMaxLifeQuarters();
// Internal maxLife value for HUD heart-container slot count (display only; save maxLife unchanged).
u16  dAlbwMQ_getDisplayMaxLifeInternal();

int  dAlbwMQ_getHeartShopTier();
int  dAlbwMQ_getMeterShopTier();

int  dAlbwMQ_getHeartShopPrice(int tier);
int  dAlbwMQ_getMeterShopPrice(int tier);

bool dAlbwMQ_canPurchaseHeartShop();
bool dAlbwMQ_canPurchaseMeterShop();

// Shop purchase (rupee check/deduct done by caller). Returns false if sold out or at cap.
bool dAlbwMQ_tryPurchaseHeartShop();
bool dAlbwMQ_tryPurchaseMeterShop();

// Heart grants when Master Quest is on (no-op when off).
void dAlbwMQ_applyPieceHeartGrant();
void dAlbwMQ_applyContainerHeartGrant();

const char* dAlbwMQ_getHeartShopName();
const char* dAlbwMQ_getHeartShopDesc();
const char* dAlbwMQ_getMeterShopName();
const char* dAlbwMQ_getMeterShopDesc();

// ============================================
// NEW CODE ENDS HERE
// ============================================

#endif

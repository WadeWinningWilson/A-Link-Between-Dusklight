// ============================================
// NEW CODE — ALBW Port
// d_albw_rental.cpp
//
// Postman rental shop state machine. Opens when the player talks to the
// rental Postman (getBitSW() == 0x42) in F_SP103 room 1 as human Link.
//
// Flow:
//   dALBWRental_open()        — called from evtTalk() intercept
//   dALBWRental_tickAndDraw() — called every frame from Draw()
//   dALBWRental_close()       — triggered by B button or "Leave"
//
// State machine:
//   STATE_GREETING  — native TP dialogue; advance → shop
//   STATE_SHOP      — native letter-select window (dALBWShop_c)
//   STATE_FAREWELL  — native TP dialogue; advance → closed
//
// Greeting and farewell use dALBWDialogue_c (native zelda message window).
// The shop list uses dALBWShop_c (letter-select BLO from letres.arc).
//
// All timing uses std::chrono so behaviour is framerate-independent.
// The grace period (300 ms) prevents the talk-event A-press from
// immediately skipping the greeting, and the shop B-press from
// immediately skipping the farewell.
//
// Availability list is filtered to items that:
//   1. Were previously stripped (dMeter2_isALBWRentalEligible() == true)
//   2. Are not currently owned (dComIfGs_isItemFirstBit() == false)
// ============================================

#include "d/d_albw_rental.h"

#if TARGET_PC

#include <chrono>
#include <cstdio>
#include <cstring>

#include "d/d_albw_oocoo.h"
#include "d/d_albw_shade_refuge.h"
#include "d/d_albw_master_quest.h"
#include "d/d_focused_arts.h"
#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"
#include "d/d_meter2_info.h"
#include "dusk/settings.h"
#include "dusk/truetest.hpp"
#include "dusk/ui/ui.hpp"
#include "m_Do/m_Do_controller_pad.h"
// ============================================
// Internal types and item table
// ============================================

namespace {

// ============================================
// NEW CODE — ALBW Port (Shop Category Pages)
// The shop is split into category pages, navigated left/right.  Pages are
// data-driven: add a category here + a kPages row + tag entries, and a new
// page appears automatically (e.g. the future Swords page).  A page is only
// shown when it has at least one visible row (see categoryHasVisibleRows).
//   CAT_UPGRADES — non-item services: Master-Quest heart/stamina upgrades,
//                  Oocoo's Return, and (future) the warp/location service.
// ============================================
enum ALBWShopCategory {
    CAT_SHIELDS = 0,
    CAT_ITEMS,
    CAT_ARMOR,
    CAT_UPGRADES,
    CAT_SWORDS,   // future page; no entries tagged yet
    CAT_COUNT,
};

struct ALBWShopPage {
    ALBWShopCategory category;
    const char*      title;   // page heading + counter shown via dALBWRental_getPageTitle()
};

// Page order = left-to-right tab order.  Uncomment CAT_SWORDS (and tag entries)
// to add the swords page later.
static const ALBWShopPage kPages[] = {
    { CAT_SHIELDS,  "Shields"             },
    { CAT_ITEMS,    "Items"               },
    { CAT_ARMOR,    "Armor"               },
    { CAT_UPGRADES, "Upgrades & Services" },
    // { CAT_SWORDS, "Swords" },
};
static constexpr int kPageCount = sizeof(kPages) / sizeof(kPages[0]);
// ============================================
// NEW CODE ENDS HERE
// ============================================

struct ALBWRentalEntry {
    const char* name;
    u8          itemNo;
    int         slotNo;   // inventory slot to restore; -1 = no slot (Magic Armor)
    int         price;    // cost in rupees
    const char* desc;     // shown in description area when item is purchasable
    // ============================================
    // NEW CODE — ALBW Port
    // Optional per-entry eligibility override.  nullptr = use the default
    // dMeter2_isALBWRentalEligible(itemNo) check.  Set for entries whose
    // ??? → real-name condition depends on external state beyond the item's
    // own rental-eligibility save bit (e.g. Deity Armor requires both Magic
    // Armor eligibility AND the Colossal Wallet).
    // ============================================
    bool (*eligibilityCheck)() = nullptr;
    // ============================================
    // NEW CODE ENDS HERE
    // ============================================
    // ============================================
    // NEW CODE — ALBW Port (Shop Pages + True ALBW)
    // category    — which shop page this entry lives on.  Defaults to CAT_ITEMS
    //               so only shields/armor entries need to tag a category below.
    // alwaysGated — when true, the True ALBW setting does NOT bypass this entry's
    //               eligibility check (Deity Armor keeps its unlock conditions).
    // ============================================
    ALBWShopCategory category    = CAT_ITEMS;
    bool             alwaysGated = false;
    // ============================================
    // NEW CODE ENDS HERE
    // ============================================
};

// All rentable ALBW items in ascending price order.
// slotNo values match the strip logic in d_gameover.cpp _create().
// desc — shown in the right parchment panel when the entry is purchasable.
static const ALBWRentalEntry kItems[] = {
    { "Ordon Shield",
      (u8)dItemNo_WOOD_SHIELD_e,  -1,      50,
      "A humble offering for the Princess' Royal Family. Do be more gentle with it now!",
      []() -> bool { return dMeter2_isShieldRentalEligible((u8)dItemNo_WOOD_SHIELD_e); },
      CAT_SHIELDS },
    { "Wooden Shield",
      (u8)dItemNo_SHIELD_e,       -1,      150,
      "A sturdier wooden shield with subtle metal reinforcements....sadly didn't stop me "
      "from getting this nasty splinter.",
      []() -> bool { return dMeter2_isShieldRentalEligible((u8)dItemNo_SHIELD_e); },
      CAT_SHIELDS },
    { "Hylian Shield",
      (u8)dItemNo_HYLIA_SHIELD_e, -1,      500,
      "An expert duelists' shield of choice, passed down by the Hyrulean Royal family. A "
      "small inscription along the border reads: \" May the Goddess reunite us \".",
      []() -> bool { return dMeter2_isShieldRentalEligible((u8)dItemNo_HYLIA_SHIELD_e); },
      CAT_SHIELDS },
    { "Slingshot",
      (u8)dItemNo_PACHINKO_e,     SLOT_23, 15,
      "A child's reliable pellet launcher, perfect for tiny insects." },
    { "Boomerang",
      (u8)dItemNo_BOOMERANG_e,    SLOT_0,  30,
      "A mysterious boomerang that supposedly houses the Fairy of Winds...has a pungent odor" },
    { "Bombs",
      (u8)dItemNo_BOMB_BAG_LV1_e, SLOT_15, 50,
      "Standard-issue explosives, handle with care" },
    { "Bomblings",
      (u8)dItemNo_POKE_BOMB_e,    SLOT_17, 50,
      "These little fellows roll themselves...why did my boss bring these here" },
    { "Water Bombs",
      (u8)dItemNo_BOMB_BAG_LV2_e, SLOT_16, 100,
      "Specially sealed for aquatic demolition..." },
    { "Dominion Rod",
      (u8)dItemNo_COPY_ROD_e,     SLOT_8,  100,
      "Bring to life statues of old...no refunds if broken on transit" },
    { "Clawshot",
      (u8)dItemNo_HOOKSHOT_e,     SLOT_9,  110,
      "Long gone are the days of single hook traversal" },
    { "Bow",
      (u8)dItemNo_BOW_e,           SLOT_4,  150,
      "A sacred treasure, moistuerize properly to undo wood warping" },
    { "Double Clawshot",
      (u8)dItemNo_W_HOOKSHOT_e,   SLOT_10, 200,
      "Double the traversal....I need a pair like these" },
    { "Spinner",
      (u8)dItemNo_SPINNER_e,       SLOT_2,  200,
      "Ancient item that grants you.......what is this darn thing?" },
    { "Ball and Chain",
      (u8)dItemNo_IRONBALL_e,      SLOT_6,  350,
      "Heavy...heavy...please pick it up yourself" },
    // ============================================
    // NEW CODE — ALBW Port (Outfits — Armors tab)
    // Ordon Clothes: the early-game casual outfit (dItemNo_WEAR_CASUAL_e).  Sits
    // at the TOP of the Armor page (armors listed in order of likely obtainment:
    // Ordon, Hero's Green, Sumo, Zora, Magic Armor, Deity).  Eligibility =
    // "worn once" (escaped sewers); True ALBW bypasses it (not alwaysGated).
    // Purchase grants + auto-equips via dMeter2_grantRentalClothes (see tryPurchase).
    // ============================================
    { "Ordon Clothes",
      (u8)dItemNo_WEAR_CASUAL_e,   -1,      50,
      "Humble small town wear fit for any...unassuming explorer.",
      []() -> bool { return dMeter2_isCasualWearEligible(); },
      CAT_ARMOR },
    // ============================================
    // NEW CODE ENDS HERE
    // ============================================
    { "Magic Armor",
      (u8)dItemNo_ARMOR_e,         -1,      500,
      "Legendary Golden protection, keep an eye on your wallet with this!",
      nullptr, CAT_ARMOR },
    // ============================================
    // NEW CODE — ALBW Port
    // Deity Armor: 13th rental entry.  Not a physical inventory item; it is
    // an ability flag (dItemNo_DEITY_ARMOR_e) that enhances Magic Armor once
    // purchased.  Requires the player to have held Magic Armor at least once
    // (ARMOR_e rental eligibility) AND to own the Colossal Wallet (9999 cap,
    // granted on Cave of Ordeals completion).  Full behavior implementation
    // is deferred; purchasing the entry sets the flag, unlocking the armor's
    // upgraded mechanics when they are wired up.
    // ============================================
    { "Deity Armor",
      (u8)dItemNo_DEITY_ARMOR_e,   -1,   5000,
      "Oooh a priceless treasure. but perhaps far too expensive for you!",
      []() -> bool {
          // Show as real name only when both conditions are satisfied:
          //   1. Magic Armor has been stripped at least once (rental-eligible).
          //   2. Colossal Wallet is held (rewarded from Cave of Ordeals).
          return dMeter2_isALBWRentalEligible((u8)dItemNo_ARMOR_e)
              && dComIfGs_getWalletSize() == COLOSSAL_WALLET;
      },
      CAT_ARMOR, true
    },
    // ============================================
    // NEW CODE ENDS HERE
    // ============================================
};
static constexpr int kItemCount = sizeof(kItems) / sizeof(kItems[0]);

// ============================================
// Timing constants (framerate-independent)
// ============================================

using clock = std::chrono::steady_clock;

// ============================================
// Shop state
// ============================================

enum ALBWRentalState {
    STATE_CLOSED   = 0,
    STATE_GREETING,   // native dialogue; advance → shop
    STATE_SHOP,       // native letter-select window
    STATE_FAREWELL,   // native dialogue; advance → closed
};

static ALBWRentalState sState = STATE_CLOSED;

// Every entry the player can see in the table (eligible+owned items hidden).
// purchasable == true  → eligible AND not owned → real name, can press A
// purchasable == false → not eligible yet       → shown as "?????"
enum VisibleKind {
    VISIBLE_MQ_HEART = 0,
    VISIBLE_MQ_METER,
    VISIBLE_FA_TIER,
    VISIBLE_ITEM,
    VISIBLE_SHADE_REFUGE,  // "Return to Last Shade Watcher" (above Oocoo)
    VISIBLE_OOCOO,
};

struct VisibleEntry {
    VisibleKind kind;
    int         kItemsIdx;   // index into kItems[] when kind == VISIBLE_ITEM
    bool        purchasable;
};

static constexpr int kVisibleListMax = 3 + kItemCount + 2;  // MQ rows + FA tier + services
static VisibleEntry sVisibleList[kVisibleListMax];
static int          sVisibleCount     = 0;  // total rows shown (inc. ?????)
static int          sAvailCount       = 0;  // purchasable rows only (for button state)
static int          sSelectedIdx      = 0;
static bool         sPurchasedThisSession = false;

// ============================================
// NEW CODE — ALBW Port (Shop Category Pages)
// Page navigation state.  sCurrentPageIdx indexes sActivePages[], which holds
// only the categories that currently have >=1 visible row (built in
// rebuildActivePages()).  Left/right in tick() steps sCurrentPageIdx; the
// renderer is unchanged — getVisibleList() just returns the current page.
// ============================================
static int              sCurrentPageIdx          = 0;   // index into sActivePages
static ALBWShopCategory sActivePages[kPageCount] = {};  // categories with content
static int              sActivePageCount         = 0;
// ============================================
// NEW CODE ENDS HERE
// ============================================

// Post-purchase cooldown: blocks A only (navigation stays live). Short window
// for fanfare/feedback before the next buy.
static constexpr auto kPurchaseCooldownSuccess = std::chrono::seconds(2);
static constexpr auto kPurchaseCooldownFailure = std::chrono::seconds(1);

static const char*       sStatusMsg    = nullptr;
static clock::time_point sStatusExpiry;

// Consumed by dALBWShop_c to scroll its 6-row viewport to keep selection visible.
static bool sScrollToSelected = false;

// Set to true when the shop transitions to STATE_CLOSED.  evtTalk() reads
// and clears this flag to know when to call evtChange() and release Link.
static bool sJustClosed = false;

// ============================================
// NEW CODE — ALBW Port
// One-shot state-transition signals consumed by daNpc_Post_c::Execute().
// Each flag is set when the shop enters the corresponding state and cleared
// by the matching dALBWRental_justEntered*() call so it fires only once.
// This lets the Postman actor play the correct body/face animation and voice
// squeak in response to each phase transition without polling the full state.
// ============================================
static bool sJustEnteredGreeting  = false;  // → MOT_HELLO + Z2SE_POST_V_APPEAR
static bool sJustEnteredShop      = false;  // → MOT_WAIT_A (explicit idle lock-in)
static bool sJustEnteredFarewell  = false;  // → MOT_BYE   + Z2SE_POST_V_FANFARE
// ============================================
// NEW CODE — ALBW Port (Native Dialogue)
// sNativeDialogueJustDismissed: set by advanceToShop() so the first tick of
// STATE_SHOP skips A-press checks (absorbs the button that dismissed greeting).
// sGreetingText / sFarewellText: pointers to the string literals chosen in
// dALBWRental_open() / dALBWRental_close(); consumed by dALBWDialogue_c.
// ============================================
#if TARGET_PC_NATIVE_UI
static bool        sNativeDialogueJustDismissed = false;
static const char* sGreetingText                = "";
static const char* sGreetingPage2               = nullptr;  // null for one-page greetings
static const char* sGreetingPage3               = nullptr;
static const char* sFarewellText                = "";
// Analog-stick repeat cooldown — counts down in tick(); reset in open().
static int         sStickNavCooldown            = 0;
#endif
// ============================================
// NEW CODE ENDS HERE
// ============================================
// ============================================
// NEW CODE — ALBW Port (Purchase Sound)
// Set true by tryPurchase() on a successful rental; consumed once by
// dALBWRental_justPurchased() so the postman can play a congratulatory
// voice squeak without the caller needing to know what sound to play.
// To remove purchase sounds entirely: delete this flag, the sJustPurchased
// assignment in tryPurchase(), and dALBWRental_justPurchased() below.
// ============================================
static bool sJustPurchased        = false;  // → Z2SE_POST_V_FANFARE congratulatory fanfare
static bool sJustFailedPurchase   = false;  // → Z2SE_POST_V_RUN_HIGH insufficient rupees
#if TARGET_PC
static bool sHideVanillaTalkMsg = false;
#endif
// ============================================
// NEW CODE ENDS HERE
// ============================================

// ============================================
// Helpers
// ============================================

// Returns true if any ALBW item has ever been stripped from this save.
static bool hasAnyEligible() {
    for (int i = 0; i < kItemCount; ++i) {
        if (dMeter2_isALBWRentalEligible(kItems[i].itemNo)) {
            return true;
        }
    }
    return false;
}

// ============================================
// NEW CODE — ALBW Port (Shop Pages + True ALBW)
// ============================================

// Eligibility including the True ALBW setting.  When the setting is on, every
// non-exempt item is available from the start (Ravio's-shop style).  Exempt
// entries (alwaysGated, e.g. Deity Armor) keep their own check even when on.
static bool entryEligible(const ALBWRentalEntry& entry) {
    if (dusk::truetest::isTrueAlbwShopEnabled() && !entry.alwaysGated) {
        return true;
    }
    return entry.eligibilityCheck
        ? entry.eligibilityCheck()
        : dMeter2_isALBWRentalEligible(entry.itemNo);
}

// A row is shown (as ????? or rentable) unless the player already owns it.
static bool itemRowVisible(const ALBWRentalEntry& entry) {
    return !(entryEligible(entry) && dMeter2_playerOwnsRentalItem(entry.itemNo));
}

// True if the given category page currently has at least one visible row.
static bool categoryHasVisibleRows(ALBWShopCategory cat) {
    for (int i = 0; i < kItemCount; ++i) {
        if (kItems[i].category == cat && itemRowVisible(kItems[i])) {
            return true;
        }
    }
    if (cat == CAT_UPGRADES) {
        // MQ heart/meter rows always show when Master Quest is on; the Shade
        // Watcher return and Oocoo's Return are contextual.  Any one of them
        // makes the Upgrades & Services page non-empty.
        if (dAlbwMQ_isEnabled() || dShadeRefuge_canShowInShop() ||
            dALBWOocoo_canShowInShop() || dFocusedArts_shouldShowShopTierRow()) {
            return true;
        }
    }
    return false;
}

// Rebuild the list of category pages that currently have content.  Called on
// open() and after each purchase (a page can empty out).  Keeps sCurrentPageIdx
// in range.
static void rebuildActivePages() {
    sActivePageCount = 0;
    for (int p = 0; p < kPageCount; ++p) {
        if (categoryHasVisibleRows(kPages[p].category)) {
            sActivePages[sActivePageCount++] = kPages[p].category;
        }
    }
    if (sCurrentPageIdx >= sActivePageCount) {
        sCurrentPageIdx = (sActivePageCount > 0) ? sActivePageCount - 1 : 0;
    }
    if (sCurrentPageIdx < 0) {
        sCurrentPageIdx = 0;
    }
}

// Category of the page the player is currently viewing.
static ALBWShopCategory currentCategory() {
    if (sActivePageCount <= 0) {
        return CAT_SHIELDS;  // no active pages → empty list → "Not in stock yet"
    }
    if (sCurrentPageIdx < 0)                 sCurrentPageIdx = 0;
    if (sCurrentPageIdx >= sActivePageCount) sCurrentPageIdx = sActivePageCount - 1;
    return sActivePages[sCurrentPageIdx];
}
// ============================================
// NEW CODE ENDS HERE
// ============================================

// Rebuild the visible list.
//   Eligible + owned   → hidden entirely (player already has the item).
//   Not eligible       → shown as "?????" with real price; purchasable = false.
//   Eligible + !owned  → shown with real name and price;  purchasable = true.
static void rebuildVisibleList() {
    sVisibleCount = 0;
    sAvailCount   = 0;

    // ============================================
    // MODIFIED CODE — ALBW Port (Shop Category Pages)
    // Build only the rows belonging to the current category page.  The MQ
    // upgrade rows and Oocoo's Return live on the Upgrades & Services page.
    // ============================================
    const ALBWShopCategory cat = currentCategory();

    if (cat == CAT_UPGRADES && dAlbwMQ_isEnabled()) {
        sVisibleList[sVisibleCount].kind        = VISIBLE_MQ_HEART;
        sVisibleList[sVisibleCount].kItemsIdx   = -1;
        sVisibleList[sVisibleCount].purchasable = dAlbwMQ_canPurchaseHeartShop();
        if (sVisibleList[sVisibleCount].purchasable) {
            sAvailCount++;
        }
        sVisibleCount++;

        sVisibleList[sVisibleCount].kind        = VISIBLE_MQ_METER;
        sVisibleList[sVisibleCount].kItemsIdx   = -1;
        sVisibleList[sVisibleCount].purchasable = dAlbwMQ_canPurchaseMeterShop();
        if (sVisibleList[sVisibleCount].purchasable) {
            sAvailCount++;
        }
        sVisibleCount++;
    }

    if (cat == CAT_UPGRADES && dFocusedArts_shouldShowShopTierRow()) {
        sVisibleList[sVisibleCount].kind        = VISIBLE_FA_TIER;
        sVisibleList[sVisibleCount].kItemsIdx   = -1;
        sVisibleList[sVisibleCount].purchasable = dFocusedArts_canPurchaseShopTier();
        if (sVisibleList[sVisibleCount].purchasable) {
            sAvailCount++;
        }
        sVisibleCount++;
    }

    for (int i = 0; i < kItemCount; ++i) {
        const ALBWRentalEntry& entry = kItems[i];
        if (entry.category != cat) {
            continue;  // not on this page
        }
        const bool eligible = entryEligible(entry);
        const bool owned    = dMeter2_playerOwnsRentalItem(entry.itemNo);
        if (eligible && owned) {
            continue;  // hidden — player already owns this item
        }
        sVisibleList[sVisibleCount].kind        = VISIBLE_ITEM;
        sVisibleList[sVisibleCount].kItemsIdx    = i;
        sVisibleList[sVisibleCount].purchasable = eligible && !owned;
        if (sVisibleList[sVisibleCount].purchasable) {
            sAvailCount++;
        }
        sVisibleCount++;
    }

    // "Return to Last Shade Watcher" sits directly ABOVE Oocoo on the
    // Upgrades & Services page (available whenever a respawn slot is set).
    if (cat == CAT_UPGRADES && dShadeRefuge_canShowInShop() &&
        sVisibleCount < (int)(sizeof(sVisibleList) / sizeof(sVisibleList[0]))) {
        sVisibleList[sVisibleCount].kind        = VISIBLE_SHADE_REFUGE;
        sVisibleList[sVisibleCount].kItemsIdx   = -1;
        sVisibleList[sVisibleCount].purchasable = true;
        sAvailCount++;
        sVisibleCount++;
    }

    // Oocoo's Return lives on the Upgrades & Services page (contextual).
    if (cat == CAT_UPGRADES && dALBWOocoo_canShowInShop() &&
        sVisibleCount < (int)(sizeof(sVisibleList) / sizeof(sVisibleList[0]))) {
        sVisibleList[sVisibleCount].kind        = VISIBLE_OOCOO;
        sVisibleList[sVisibleCount].kItemsIdx   = -1;
        sVisibleList[sVisibleCount].purchasable = true;
        sAvailCount++;
        sVisibleCount++;
    }
    // ============================================
    // MODIFIED CODE ENDS HERE
    // ============================================

    if (sSelectedIdx >= sVisibleCount) {
        sSelectedIdx = (sVisibleCount > 0) ? sVisibleCount - 1 : 0;
    }
}

// ============================================
// NEW CODE — ALBW Port (Shop Category Pages)
// Step the current page left (-1) or right (+1), wrapping within the active
// (non-empty) pages.  Resets selection/scroll so the renderer snaps to the top
// of the new page (mScrollTop follows sSelectedIdx via populateRows clamp).
// ============================================
static void changePage(int dir) {
    if (sActivePageCount <= 1) {
        return;
    }
    sCurrentPageIdx   = (sCurrentPageIdx + dir + sActivePageCount) % sActivePageCount;
    sSelectedIdx      = 0;
    sScrollToSelected = true;
    rebuildVisibleList();
}
// ============================================
// NEW CODE ENDS HERE
// ============================================

// Attempt to purchase the item currently selected in the visible list.
// Silent no-op if the item is a ????? (not yet eligible).
static void tryPurchase(int visIdx) {
    if (visIdx < 0 || visIdx >= sVisibleCount) {
        return;
    }
    if (!sVisibleList[visIdx].purchasable) {
        return;
    }

    if (sVisibleList[visIdx].kind == VISIBLE_MQ_HEART) {
        const int price = dAlbwMQ_getHeartShopPrice(dAlbwMQ_getHeartShopTier());
        u16 rupees      = dComIfGs_getRupee();
        if (rupees < (u16)price) {
            sStatusMsg          = "Sincerest apologies, but we can't return\nthat to you for that little..";
            sStatusExpiry       = clock::now() + kPurchaseCooldownFailure;
            sJustFailedPurchase = true;
            return;
        }
        if (!dAlbwMQ_tryPurchaseHeartShop()) {
            return;
        }
        dComIfGs_setRupee(rupees - (u16)price);
        sPurchasedThisSession = true;
        sJustPurchased        = true;
        sStatusMsg            = "Your health will thank you.\nThank you for your patronage!";
        sStatusExpiry         = clock::now() + kPurchaseCooldownSuccess;
        rebuildActivePages();  // a purchase can empty the current page
        rebuildVisibleList();
        return;
    }

    if (sVisibleList[visIdx].kind == VISIBLE_MQ_METER) {
        const int price = dAlbwMQ_getMeterShopPrice(dAlbwMQ_getMeterShopTier());
        u16 rupees      = dComIfGs_getRupee();
        if (rupees < (u16)price) {
            sStatusMsg          = "Sincerest apologies, but we can't return\nthat to you for that little..";
            sStatusExpiry       = clock::now() + kPurchaseCooldownFailure;
            sJustFailedPurchase = true;
            return;
        }
        if (!dAlbwMQ_tryPurchaseMeterShop()) {
            return;
        }
        dComIfGs_setRupee(rupees - (u16)price);
        sPurchasedThisSession = true;
        sJustPurchased        = true;
        sStatusMsg            = "May your stamina carry you far.\nThank you for your patronage!";
        sStatusExpiry         = clock::now() + kPurchaseCooldownSuccess;
        rebuildActivePages();  // a purchase can empty the current page
        rebuildVisibleList();
        return;
    }

    if (sVisibleList[visIdx].kind == VISIBLE_FA_TIER) {
        const int tier = dFocusedArts_getNextShopTierIndex();
        const int price = dFocusedArts_getNextShopTierPrice();
        u16 rupees      = dComIfGs_getRupee();
        if (tier <= 0 || price <= 0) {
            return;
        }
        if (rupees < (u16)price) {
            sStatusMsg          = "Sincerest apologies, but we can't return\nthat to you for that little..";
            sStatusExpiry       = clock::now() + kPurchaseCooldownFailure;
            sJustFailedPurchase = true;
            return;
        }
        if (!dFocusedArts_tryPurchaseShopTier()) {
            return;
        }
        dComIfGs_setRupee(rupees - (u16)price);
        sPurchasedThisSession = true;
        sJustPurchased        = true;
        sStatusMsg            = "Your hidden arts will sharpen with practice.\nThank you for your patronage!";
        sStatusExpiry         = clock::now() + kPurchaseCooldownSuccess;
        rebuildActivePages();
        rebuildVisibleList();
        return;
    }

    if (sVisibleList[visIdx].kind == VISIBLE_SHADE_REFUGE) {
        if (!dShadeRefuge_tryPurchaseReturn()) {
            sStatusMsg          = "I'm afraid that's not enough to light the way back.";
            sStatusExpiry       = clock::now() + kPurchaseCooldownFailure;
            sJustFailedPurchase = true;
            return;
        }
        sPurchasedThisSession = true;
        sJustPurchased        = true;
        sStatusMsg            = "The ember will carry you back when you leave the shop.\n"
                                "Thank you for your patronage!";
        sStatusExpiry         = clock::now() + kPurchaseCooldownSuccess;
        rebuildActivePages();
        rebuildVisibleList();
        return;
    }

    if (sVisibleList[visIdx].kind == VISIBLE_OOCOO) {
        if (!dALBWOocoo_tryPurchase()) {
            sStatusMsg          = "Sincerest apologies, but we can't send the\n"
                                  "cuckoo out for that little..";
            sStatusExpiry       = clock::now() + kPurchaseCooldownFailure;
            sJustFailedPurchase = true;
            return;
        }
        sPurchasedThisSession = true;
        sJustPurchased        = true;
        sStatusMsg            = "Oocoo will carry you back when you leave the shop.\n"
                                "Thank you for your patronage!";
        sStatusExpiry         = clock::now() + kPurchaseCooldownSuccess;
        rebuildActivePages();  // a purchase can empty the current page
        rebuildVisibleList();
        return;
    }

    const ALBWRentalEntry& e = kItems[sVisibleList[visIdx].kItemsIdx];
    u16 rupees = dComIfGs_getRupee();
    if (rupees < (u16)e.price) {
        sStatusMsg          = "Sincerest apologies, but we can't return\nthat to you for that little..";
        sStatusExpiry       = clock::now() + kPurchaseCooldownFailure;
        sJustFailedPurchase = true;  // triggers Z2SE_POST_V_RUN_HIGH in d_a_npc_post.cpp
        return;
    }
    dComIfGs_setRupee(rupees - (u16)e.price);
    if (dMeter2_isShieldItem(e.itemNo)) {
        if (!dMeter2_canAcquireShield(e.itemNo)) {
            sStatusMsg    = "You can only carry one shield at a time.";
            sStatusExpiry = clock::now() + kPurchaseCooldownFailure;
            return;
        }
        dMeter2_grantRentalShield(e.itemNo);
    // ============================================
    // NEW CODE — ALBW Port (Outfits)
    // Clothes outfits (Ordon casual wear) have no inventory slot — grant +
    // auto-equip + swap the model in one call.  The shop is the only way to don
    // the Ordon outfit; vanilla collection menus can switch it back off.
    // ============================================
    } else if (e.itemNo == (u8)dItemNo_WEAR_CASUAL_e) {
        dMeter2_grantRentalClothes(e.itemNo);
    // ============================================
    // NEW CODE ENDS HERE
    // ============================================
    } else {
        dComIfGs_onItemFirstBit(e.itemNo);
        if (e.slotNo >= 0) {
            dComIfGs_setItem(e.slotNo, e.itemNo);
        }
    }
    sPurchasedThisSession = true;
    sJustPurchased        = true;   // triggers Z2SE_POST_V_FANFARE in d_a_npc_post.cpp
    sStatusMsg    = "One step closer to becoming a Senior Postman.\nI can smell the fields now, thank you for your patronage!";
    sStatusExpiry = clock::now() + kPurchaseCooldownSuccess;
    rebuildActivePages();  // a purchase can empty the current page
    rebuildVisibleList();
}

}  // namespace

// ============================================
// Public API
// ============================================

// ============================================
// NEW CODE — ALBW Port
// dALBWRental_justClosed()
// Returns true (exactly once) when the shop has just transitioned to
// STATE_CLOSED.  Used by daNpc_Post_c::evtTalk() to know when to call
// evtChange() and release Link's movement lock.  The flag is consumed on
// read so subsequent calls return false until the next shop session closes.
// ============================================
bool dALBWRental_justClosed() {
    if (sJustClosed) {
        sJustClosed = false;
        return true;
    }
    return false;
}

// ============================================
// NEW CODE ENDS HERE
// ============================================

// ============================================
// NEW CODE — ALBW Port
// Animation transition query functions.
// Each returns true exactly once — on the Execute() tick immediately after
// the shop enters the corresponding state.  Consumed on read.  The caller
// (daNpc_Post_c::Execute) uses these to play body/face animations and voice
// squeaks without having to poll the full state every frame.
//
// justEnteredGreeting() → play MOT_HELLO (greeting bow) + FACE_MOT_HELLO
//                         + Z2SE_POST_V_APPEAR voice squeak
// justEnteredShop()     → lock in MOT_WAIT_A (standing idle) + FACE_MOT_NONE_2
//                         (called when player dismisses greeting toast)
// justEnteredFarewell() → play MOT_BYE (farewell salute) + FACE_MOT_BYE
//                         + Z2SE_POST_V_FANFARE voice squeak
// ============================================
bool dALBWRental_justEnteredGreeting() {
    if (sJustEnteredGreeting) {
        sJustEnteredGreeting = false;
        return true;
    }
    return false;
}

bool dALBWRental_justEnteredShop() {
    if (sJustEnteredShop) {
        sJustEnteredShop = false;
        return true;
    }
    return false;
}

bool dALBWRental_justEnteredFarewell() {
    if (sJustEnteredFarewell) {
        sJustEnteredFarewell = false;
        return true;
    }
    return false;
}

// ============================================
// NEW CODE — ALBW Port (Purchase Sound)
// dALBWRental_justPurchased()
// Returns true exactly once after a successful rental purchase, then resets.
// Consumed by daNpc_Post_c::Execute() to play a congratulatory voice squeak.
// Deleting this function (and sJustPurchased) removes purchase sounds with
// zero impact on shop logic, animations, or dialogue interactions.
// ============================================
bool dALBWRental_justPurchased() {
    if (sJustPurchased) {
        sJustPurchased = false;
        return true;
    }
    return false;
}
// ============================================
// NEW CODE — ALBW Port (Failed Purchase Sound)
// Returns true exactly once after a failed rental attempt (not enough rupees).
// Consumed by daNpc_Post_c::Execute() to play Z2SE_POST_V_RUN_HIGH.
// ============================================
bool dALBWRental_justFailedPurchase() {
    if (sJustFailedPurchase) {
        sJustFailedPurchase = false;
        return true;
    }
    return false;
}
// ============================================
// NEW CODE ENDS HERE
// ============================================

void dALBWRental_open() {
    if (sState != STATE_CLOSED) {
        return;
    }

    sPurchasedThisSession = false;
    sStatusMsg            = nullptr;
    sStatusExpiry         = clock::time_point{};  // epoch = already expired
    sSelectedIdx          = 0;
    sVisibleCount         = 0;
    sAvailCount           = 0;
    sJustClosed           = false;
    sJustEnteredGreeting  = false;
    sJustEnteredShop      = false;
    sJustEnteredFarewell  = false;
    sJustFailedPurchase   = false;
    sScrollToSelected     = false;
#if TARGET_PC_NATIVE_UI
    sStickNavCooldown     = 0;
#endif
    // ============================================
    // NEW CODE — ALBW Port (Shop Category Pages)
    // Build the active (non-empty) page set, then default to the Upgrades &
    // Services page whenever Oocoo's Return is available so it is immediately
    // visible (it was historically pinned near the top of the flat list).
    // ============================================
    sCurrentPageIdx = 0;
    rebuildActivePages();
    if (dALBWOocoo_canShowInShop()) {
        for (int p = 0; p < sActivePageCount; ++p) {
            if (sActivePages[p] == CAT_UPGRADES) {
                sCurrentPageIdx = p;
                break;
            }
        }
    }
    // ============================================
    // NEW CODE ENDS HERE
    // ============================================
    rebuildVisibleList();

    // Greeting A — returning customer (items stripped at least once).  One page.
    // Greeting B — first visit.  Three native boxes: intro, then one sentence each.
    // True ALBW always greets as a returning customer (every item is in stock).
    const bool returning =
        dusk::truetest::isTrueAlbwShopEnabled() || hasAnyEligible();
    const char* greeting = returning
        ? "Greetings! Lost your treasured possessions? Never fear, I have a new shipment for you! All for a.....small fee!"
        : "Greetings! As an ever dutiful Junior mail carrier I return all that is lost or misplaced!";

    if (returning) {
        sGreetingText  = greeting;
        sGreetingPage2 = nullptr;
    } else {
        sGreetingText  = greeting;
        sGreetingPage2 =
            "It seems to me that this is your first time using our services, browse away and keep us in mind. "
            "You may never know when something slips off your person!";
    }
    (void)greeting;

    sState               = STATE_GREETING;
    sJustEnteredGreeting = true;  // signals Execute() to play MOT_HELLO
#if TARGET_PC
    sHideVanillaTalkMsg  = true;
#endif
// ============================================
// NEW CODE — ALBW Port
// Note: we intentionally do NOT call dComIfGp_onPauseFlag() here.
// In Dusklight's PC loop fapGm_Execute() calls mDoGph_Painter, so the
// pause flag kills 3D rendering and produces a black screen.  Instead,
// we leave the NPC talk event alive (evtTalk() does NOT call evtChange()
// for this actor).  Link's movement code checks checkEventRun() before
// processing stick input, so an active event is sufficient to lock Link
// and block D-pad sub-screens — exactly what normal NPC dialogue does.
// evtTalk() calls evtChange() once sJustClosed is set on shop close.
// ============================================
}

void dALBWRental_close() {
    if (sState != STATE_SHOP) {
        return;
    }

    const char* farewell = sPurchasedThisSession
        ? "Ahhh isn't it a joy to be reunited with one's cherished belongings? "
          "Well our business is concluded, onward to packaging!"
        : "My friend do not shed a tear, save up and return. "
          "Even I can't leave this town yet without a rupee or two more in the bank!";

    sFarewellText = farewell;
    (void)farewell;

    sState               = STATE_FAREWELL;
    sJustEnteredFarewell = true;  // signals Execute() to play MOT_BYE
}

bool dALBWRental_isOpen() {
    return sState != STATE_CLOSED;
}

#if TARGET_PC
void dALBWRental_armVanillaTalkSuppress() {
    sHideVanillaTalkMsg = true;
}

void dALBWRental_clearVanillaTalkSuppress() {
    sHideVanillaTalkMsg = false;
}

bool dALBWRental_shouldSuppressVanillaTalkMsg() {
    // Rental closed but suppress was never cleared — broke all NPC dialogue globally.
    if (sState == STATE_CLOSED) {
        sHideVanillaTalkMsg = false;
    }
    return sHideVanillaTalkMsg || dALBWRental_isOpen();
}
#endif

// ============================================
// NEW CODE — ALBW Port
// dALBWRental_tick()
// Input polling and state-machine transitions only. Call from actor Execute().
// ============================================
void dALBWRental_tick() {
    if (sState == STATE_CLOSED) {
        return;
    }

    // ---- STATE_GREETING ----
    // Native dialogue drives the transition via dALBWRental_advanceToShop().
    if (sState == STATE_GREETING) {
        return;
    }

    // ---- STATE_FAREWELL ----
    // Native dialogue drives the transition via dALBWRental_advanceToClosed().
    if (sState == STATE_FAREWELL) {
        return;
    }

    // ---- STATE_SHOP ----

// ============================================
// NEW CODE — ALBW Port (Native Dialogue)
// Absorb the A-press that dismissed the native greeting dialogue so it does not
// immediately trigger a purchase on the first frame in STATE_SHOP.
// ============================================
#if TARGET_PC_NATIVE_UI
    if (sNativeDialogueJustDismissed) {
        sNativeDialogueJustDismissed = false;
        return;
    }
#endif
// ============================================
// NEW CODE ENDS HERE
// ============================================

    // Clear status message once its display window has expired.
    if (sStatusMsg != nullptr && clock::now() >= sStatusExpiry) {
        sStatusMsg = nullptr;
    }

    // B always closes (→ farewell).
    if (mDoCPd_c::getTrigB(PAD_1)) {
        dALBWRental_close();
        return;
    }

    // ============================================
    // NEW CODE — ALBW Port (Shop Category Pages)
    // D-pad left/right tabs between category pages (Shields / Items / Armor /
    // Upgrades & Services / ...).  Only active when more than one page has
    // content.  changePage() resets selection + scroll for the new page.
    // ============================================
    if (sActivePageCount > 1) {
        if (mDoCPd_c::getTrigRight(PAD_1)) {
            changePage(+1);
            return;
        }
        if (mDoCPd_c::getTrigLeft(PAD_1)) {
            changePage(-1);
            return;
        }
    }
    // ============================================
    // NEW CODE ENDS HERE
    // ============================================

    // Navigation traverses all visible rows (real items AND ?????).
    // tryPurchase() silently ignores ????? rows — A only works on real items.
    // sScrollToSelected tells dALBWShop_c to scroll its viewport to the
    // newly focused row so gamepad users never navigate off-screen.
    if (sVisibleCount > 0) {
        if (mDoCPd_c::getTrigDown(PAD_1) && sSelectedIdx < sVisibleCount - 1) {
            sSelectedIdx++;
            sScrollToSelected = true;
        } else if (mDoCPd_c::getTrigUp(PAD_1) && sSelectedIdx > 0) {
            sSelectedIdx--;
            sScrollToSelected = true;
        } else if (sStatusMsg == nullptr && mDoCPd_c::getTrigA(PAD_1)) {
            tryPurchase(sSelectedIdx);
        }
    }

// ============================================
// NEW CODE — ALBW Port (Analog Stick Navigation)
// Left-stick Y navigation for the native J2D shop window.
// getStickY > 0 = stick pushed up   → navigate up   (sSelectedIdx--)
// getStickY < 0 = stick pushed down → navigate down (sSelectedIdx++)
// sStickNavCooldown prevents continuous scrolling while the stick is held;
// it is also reset to 0 when the shop opens so there is no startup lag.
// 12 frames ≈ 0.2 s at 60 Hz — snappy but not runaway.
// ============================================
#if TARGET_PC_NATIVE_UI
    if (sStickNavCooldown > 0) --sStickNavCooldown;
    if (sVisibleCount > 0 && sStickNavCooldown == 0) {
        const f32 stickY = mDoCPd_c::getStickY(PAD_1);
        if (stickY < -0.5f && sSelectedIdx < sVisibleCount - 1) {
            sSelectedIdx++;
            sScrollToSelected  = true;
            sStickNavCooldown  = 12;
        } else if (stickY > 0.5f && sSelectedIdx > 0) {
            sSelectedIdx--;
            sScrollToSelected  = true;
            sStickNavCooldown  = 12;
        }
    }
#endif
// ============================================
// NEW CODE ENDS HERE
// ============================================
}
// ============================================
// NEW CODE ENDS HERE
// ============================================

// ============================================
// NEW CODE — ALBW Port (Native UI)
// dALBWRental_getVisibleList()
// Returns a snapshot of the current visible list for dALBWShop_c to render.
// The returned pointer is valid until the next call to this function.
// dALBWRental_getSelectedIdx()
// Returns the currently highlighted row index so dALBWShop_c can draw a
// ">" cursor prefix and scroll its 6-row viewport to keep it visible.
// ============================================
const dALBWVisibleEntry* dALBWRental_getVisibleList(int* outCount) {
    static dALBWVisibleEntry sPubList[kVisibleListMax];
    for (int i = 0; i < sVisibleCount; ++i) {
        if (sVisibleList[i].kind == VISIBLE_MQ_HEART) {
            sPubList[i].name           = dAlbwMQ_getHeartShopName();
            sPubList[i].price          = sVisibleList[i].purchasable
                ? dAlbwMQ_getHeartShopPrice(dAlbwMQ_getHeartShopTier()) : 0;
            sPubList[i].purchasable    = sVisibleList[i].purchasable;
            sPubList[i].desc           = dAlbwMQ_getHeartShopDesc();
            sPubList[i].itemNo         = (u8)dItemNo_KAKERA_HEART_e;
            sPubList[i].isOocooService = false;
            sPubList[i].showNameWhenSoldOut = true;
        } else if (sVisibleList[i].kind == VISIBLE_MQ_METER) {
            sPubList[i].name           = dAlbwMQ_getMeterShopName();
            sPubList[i].price          = sVisibleList[i].purchasable
                ? dAlbwMQ_getMeterShopPrice(dAlbwMQ_getMeterShopTier()) : 0;
            sPubList[i].purchasable    = sVisibleList[i].purchasable;
            sPubList[i].desc           = dAlbwMQ_getMeterShopDesc();
            sPubList[i].itemNo         = (u8)dItemNo_MAGIC_LV1_e;
            sPubList[i].isOocooService = false;
            sPubList[i].showNameWhenSoldOut = true;
        } else if (sVisibleList[i].kind == VISIBLE_FA_TIER) {
            const int tier = dFocusedArts_getNextShopTierIndex();
            sPubList[i].name           = dFocusedArts_getShopTierName(tier);
            sPubList[i].price          = sVisibleList[i].purchasable
                ? dFocusedArts_getNextShopTierPrice() : 0;
            sPubList[i].purchasable    = sVisibleList[i].purchasable;
            sPubList[i].desc           = dFocusedArts_getShopTierDesc(tier);
            sPubList[i].itemNo         = (u8)dItemNo_SWORD_e;
            sPubList[i].isOocooService = false;
            sPubList[i].showNameWhenSoldOut = true;
        } else if (sVisibleList[i].kind == VISIBLE_SHADE_REFUGE) {
            sPubList[i].name           = dShadeRefuge_getServiceName();
            sPubList[i].price          = dShadeRefuge_getServicePrice();
            sPubList[i].purchasable    = true;
            sPubList[i].desc           = dShadeRefuge_getServiceDesc();
            sPubList[i].itemNo         = 0xff;  // no wheel icon → letter/envelope fallback
            sPubList[i].isOocooService = false;
            sPubList[i].showNameWhenSoldOut = false;
        } else if (sVisibleList[i].kind == VISIBLE_OOCOO) {
            sPubList[i].name           = dALBWOocoo_getServiceName();
            sPubList[i].price          = dALBWOocoo_getServicePrice();
            sPubList[i].purchasable    = true;
            sPubList[i].desc           = dALBWOocoo_getServiceDesc();
            sPubList[i].itemNo         = (u8)dItemNo_DUNGEON_BACK_e;
            sPubList[i].isOocooService = true;
            sPubList[i].showNameWhenSoldOut = false;
        } else {
            const ALBWRentalEntry& e = kItems[sVisibleList[i].kItemsIdx];
            sPubList[i].name           = e.name;
            sPubList[i].price          = e.price;
            sPubList[i].purchasable    = sVisibleList[i].purchasable;
            sPubList[i].desc           = sVisibleList[i].purchasable ? e.desc : nullptr;
            sPubList[i].itemNo         = e.itemNo;
            sPubList[i].isOocooService = false;
            sPubList[i].showNameWhenSoldOut = false;
        }
    }
    *outCount = sVisibleCount;
    return sPubList;
}

int dALBWRental_getSelectedIdx() {
    return sSelectedIdx;
}

// ============================================
// NEW CODE — ALBW Port (Shop Category Pages)
// Page heading + counter for dALBWShop_c to render in the title pane.
// getPageNumber() is 1-based; both fall back to 1 when no page is active.
// ============================================
const char* dALBWRental_getPageTitle() {
    const ALBWShopCategory cat = currentCategory();
    for (int p = 0; p < kPageCount; ++p) {
        if (kPages[p].category == cat) {
            return kPages[p].title;
        }
    }
    return "";
}

int dALBWRental_getPageNumber() {
    return (sActivePageCount > 0) ? sCurrentPageIdx + 1 : 1;
}

int dALBWRental_getPageCount() {
    return (sActivePageCount > 0) ? sActivePageCount : 1;
}
// ============================================
// NEW CODE ENDS HERE
// ============================================
// ============================================
// NEW CODE ENDS HERE
// ============================================

// ============================================
// NEW CODE — ALBW Port (Native Dialogue)
// dALBWRental_advanceToShop() / dALBWRental_advanceToClosed()
// Called from daNpc_Post_c::evtTalk() once native talkProc completes to drive
// state transitions that are normally driven by A/B button polling.
// ============================================
#if TARGET_PC_NATIVE_UI
void dALBWRental_advanceToShop() {
    if (sState == STATE_GREETING) {
        sState                       = STATE_SHOP;
        sJustEnteredShop             = true;
        sNativeDialogueJustDismissed = true;
    }
}

void dALBWRental_advanceToClosed() {
    if (sState == STATE_FAREWELL) {
        sJustClosed = true;
        sState      = STATE_CLOSED;
    }
}

bool dALBWRental_isShopState() {
    return sState == STATE_SHOP;
}

bool dALBWRental_isGreetingState() {
    return sState == STATE_GREETING;
}

bool dALBWRental_isFarewellState() {
    return sState == STATE_FAREWELL;
}

const char* dALBWRental_getGreetingText()  { return sGreetingText; }
const char* dALBWRental_getGreetingPage2() { return sGreetingPage2; }
const char* dALBWRental_getGreetingPage3() { return sGreetingPage3; }
const char* dALBWRental_getFarewellText()  { return sFarewellText; }
#endif  // TARGET_PC_NATIVE_UI
// ============================================
// NEW CODE ENDS HERE
// ============================================

#endif  // TARGET_PC

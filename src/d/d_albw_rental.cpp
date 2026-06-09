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
#include "d/d_com_inf_game.h"
#include "d/d_meter2_info.h"
#include "d/d_item_data.h"
#include "dusk/ui/ui.hpp"
#include "m_Do/m_Do_controller_pad.h"
// ============================================
// Internal types and item table
// ============================================

namespace {

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
};

// All rentable ALBW items in ascending price order.
// slotNo values match the strip logic in d_gameover.cpp _create().
// desc — shown in the right parchment panel when the entry is purchasable.
static const ALBWRentalEntry kItems[] = {
    { "Ordon Shield",
      (u8)dItemNo_WOOD_SHIELD_e,  -1,      50,
      "A humble offering for the Princess' Royal Family. Do be more gentle with it now!",
      []() -> bool { return dMeter2_isShieldRentalEligible((u8)dItemNo_WOOD_SHIELD_e); } },
    { "Wooden Shield",
      (u8)dItemNo_SHIELD_e,       -1,      150,
      "A sturdier wooden shield with subtle metal reinforcements....sadly didn't stop me "
      "from getting this nasty splinter.",
      []() -> bool { return dMeter2_isShieldRentalEligible((u8)dItemNo_SHIELD_e); } },
    { "Hylian Shield",
      (u8)dItemNo_HYLIA_SHIELD_e, -1,      500,
      "An expert duelists' shield of choice, passed down by the Hyrulean Royal family. A "
      "small inscription along the border reads: \" May the Goddess reunite us \".",
      []() -> bool { return dMeter2_isShieldRentalEligible((u8)dItemNo_HYLIA_SHIELD_e); } },
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
    { "Magic Armor",
      (u8)dItemNo_ARMOR_e,         -1,      500,
      "Legendary Golden protection, keep an eye on your wallet with this!" },
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
      }
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
    VISIBLE_ITEM = 0,
    VISIBLE_OOCOO,
};

struct VisibleEntry {
    VisibleKind kind;
    int         kItemsIdx;   // index into kItems[] when kind == VISIBLE_ITEM
    bool        purchasable;
};

static VisibleEntry sVisibleList[kItemCount + 1];
static int          sVisibleCount     = 0;  // total rows shown (inc. ?????)
static int          sAvailCount       = 0;  // purchasable rows only (for button state)
static int          sSelectedIdx      = 0;
static bool         sPurchasedThisSession = false;

// Status message shown inside the shop window (purchase result etc.).
// Uses a chrono expiry so duration is framerate-independent.
// sStatusTimer kept only as a bool-style "is message active" flag for
// the navigation gate in tick(); the actual display check uses the clock.
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

// Rebuild the visible list.
//   Eligible + owned   → hidden entirely (player already has the item).
//   Not eligible       → shown as "?????" with real price; purchasable = false.
//   Eligible + !owned  → shown with real name and price;  purchasable = true.
static void rebuildVisibleList() {
    sVisibleCount = 0;
    sAvailCount   = 0;
    for (int i = 0; i < kItemCount; ++i) {
        const ALBWRentalEntry& entry = kItems[i];
        // ============================================
        // MODIFIED CODE — ALBW Port
        // Use per-entry eligibility override when present; otherwise fall
        // back to the standard isALBWRentalEligible(itemNo) check.
        // ============================================
        const bool eligible = entry.eligibilityCheck
            ? entry.eligibilityCheck()
            : dMeter2_isALBWRentalEligible(entry.itemNo);
        // ============================================
        // MODIFIED CODE ENDS HERE
        // ============================================
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

        // Oocoo's Return sits directly under the three shield rows (kItems[0..2]).
        if (i == 2 && dALBWOocoo_canShowInShop() &&
            sVisibleCount < (int)(sizeof(sVisibleList) / sizeof(sVisibleList[0]))) {
            sVisibleList[sVisibleCount].kind        = VISIBLE_OOCOO;
            sVisibleList[sVisibleCount].kItemsIdx   = -1;
            sVisibleList[sVisibleCount].purchasable = true;
            sAvailCount++;
            sVisibleCount++;
        }
    }

    if (sSelectedIdx >= sVisibleCount) {
        sSelectedIdx = (sVisibleCount > 0) ? sVisibleCount - 1 : 0;
    }
}

// Attempt to purchase the item currently selected in the visible list.
// Silent no-op if the item is a ????? (not yet eligible).
static void tryPurchase(int visIdx) {
    if (visIdx < 0 || visIdx >= sVisibleCount) {
        return;
    }
    if (!sVisibleList[visIdx].purchasable) {
        return;  // ????? row — not purchasable yet
    }

    if (sVisibleList[visIdx].kind == VISIBLE_OOCOO) {
        if (!dALBWOocoo_tryPurchase()) {
            sStatusMsg          = "Sincerest apologies, but we can't send the\n"
                                  "cuckoo out for that little..";
            sStatusExpiry       = clock::now() + std::chrono::seconds(5);
            sJustFailedPurchase = true;
            return;
        }
        sPurchasedThisSession = true;
        sJustPurchased        = true;
        sStatusMsg            = "Oocoo will carry you back when you leave the shop.\n"
                                "Thank you for your patronage!";
        sStatusExpiry         = clock::now() + std::chrono::seconds(6);
        rebuildVisibleList();
        return;
    }

    const ALBWRentalEntry& e = kItems[sVisibleList[visIdx].kItemsIdx];
    u16 rupees = dComIfGs_getRupee();
    if (rupees < (u16)e.price) {
        sStatusMsg          = "Sincerest apologies, but we can't return\nthat to you for that little..";
        sStatusExpiry       = clock::now() + std::chrono::seconds(5);
        sJustFailedPurchase = true;  // triggers Z2SE_POST_V_RUN_HIGH in d_a_npc_post.cpp
        return;
    }
    dComIfGs_setRupee(rupees - (u16)e.price);
    if (dMeter2_isShieldItem(e.itemNo)) {
        if (!dMeter2_canAcquireShield(e.itemNo)) {
            sStatusMsg    = "You can only carry one shield at a time.";
            sStatusExpiry = clock::now() + std::chrono::seconds(5);
            return;
        }
        dMeter2_grantRentalShield(e.itemNo);
    } else {
        dComIfGs_onItemFirstBit(e.itemNo);
        if (e.slotNo >= 0) {
            dComIfGs_setItem(e.slotNo, e.itemNo);
        }
    }
    sPurchasedThisSession = true;
    sJustPurchased        = true;   // triggers Z2SE_POST_V_FANFARE in d_a_npc_post.cpp
    sStatusMsg    = "One step closer to becoming a Senior Postman.\nI can smell the fields now, thank you for your patronage!";
    sStatusExpiry = clock::now() + std::chrono::seconds(6);
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
    rebuildVisibleList();

    // Greeting A — returning customer (items stripped at least once).  One page.
    // Greeting B — first visit.  Three native boxes: intro, then one sentence each.
    const bool returning = hasAnyEligible();
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

    // Navigation traverses all visible rows (real items AND ?????).
    // tryPurchase() silently ignores ????? rows — A only works on real items.
    // sScrollToSelected tells dALBWShop_c to scroll its viewport to the
    // newly focused row so gamepad users never navigate off-screen.
    if (sStatusMsg == nullptr && sVisibleCount > 0) {
        if (mDoCPd_c::getTrigDown(PAD_1) && sSelectedIdx < sVisibleCount - 1) {
            sSelectedIdx++;
            sScrollToSelected = true;
        } else if (mDoCPd_c::getTrigUp(PAD_1) && sSelectedIdx > 0) {
            sSelectedIdx--;
            sScrollToSelected = true;
        } else if (mDoCPd_c::getTrigA(PAD_1)) {
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
    if (sStatusMsg == nullptr && sVisibleCount > 0 && sStickNavCooldown == 0) {
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
    static dALBWVisibleEntry sPubList[sizeof(kItems)/sizeof(kItems[0]) + 1];
    for (int i = 0; i < sVisibleCount; ++i) {
        if (sVisibleList[i].kind == VISIBLE_OOCOO) {
            sPubList[i].name           = dALBWOocoo_getServiceName();
            sPubList[i].price          = dALBWOocoo_getServicePrice();
            sPubList[i].purchasable    = true;
            sPubList[i].desc           = dALBWOocoo_getServiceDesc();
            sPubList[i].itemNo         = (u8)dItemNo_DUNGEON_BACK_e;
            sPubList[i].isOocooService = true;
        } else {
            const ALBWRentalEntry& e = kItems[sVisibleList[i].kItemsIdx];
            sPubList[i].name           = e.name;
            sPubList[i].price          = e.price;
            sPubList[i].purchasable    = sVisibleList[i].purchasable;
            sPubList[i].desc           = sVisibleList[i].purchasable ? e.desc : nullptr;
            sPubList[i].itemNo         = e.itemNo;
            sPubList[i].isOocooService = false;
        }
    }
    *outCount = sVisibleCount;
    return sPubList;
}

int dALBWRental_getSelectedIdx() {
    return sSelectedIdx;
}
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

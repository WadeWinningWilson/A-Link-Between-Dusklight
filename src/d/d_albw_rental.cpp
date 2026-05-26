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
//   STATE_GREETING  — Dusk toast shown; A/B or timeout → shop
//   STATE_SHOP      — centred ImGui shop window
//   STATE_FAREWELL  — Dusk toast shown; A/B or timeout → closed
//
// Greeting and farewell use push_toast() (RmlUI, type "npc-dialogue")
// because that system is stable at any framerate. ImGui dialogue boxes
// flickered at uncapped framerates due to per-frame repositioning.
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

#include <aurora/imgui.h>
#include <chrono>
#include <cstdio>
#include <cstring>

#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"
#include "d/d_meter2_info.h"
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
// desc is placeholder text — to be finalised during deliberation.
static const ALBWRentalEntry kItems[] = {
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
      "These little fellows roll themselves…why did my boss bring these here" },
    { "Water Bombs",
      (u8)dItemNo_BOMB_BAG_LV2_e, SLOT_16, 100,
      "Specially sealed for aquatic demolition…" },
    { "Dominion Rod",
      (u8)dItemNo_COPY_ROD_e,     SLOT_8,  100,
      "Bring to life statues of old...no refunds if broken on transit" },
    { "Clawshot",
      (u8)dItemNo_HOOKSHOT_e,     SLOT_9,  110,
      "Long gone are the days of single hook traversal" },
    { "Bow",
      (u8)dItemNo_BOW_e,           SLOT_4,  150,
      "A sacred treasure, moisturize properly to undo wood warping" },
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
static constexpr int kItemCount = 13;

// ============================================
// Timing constants (framerate-independent)
// ============================================

using clock = std::chrono::steady_clock;

// How long after entering a dialogue state before A/B is accepted.
// Absorbs button presses from the interaction that opened/closed the shop.
static constexpr auto kDialogueGrace    = std::chrono::milliseconds(300);

// Toast display durations. Long enough that the player will always
// dismiss via A/B before they expire. State does NOT auto-advance on
// timeout — only A/B progresses the dialogue.
static constexpr auto kGreetingDuration = std::chrono::seconds(30);
static constexpr auto kFarewellDuration = std::chrono::seconds(20);

// ============================================
// Shop state
// ============================================

enum ALBWRentalState {
    STATE_CLOSED   = 0,
    STATE_GREETING,   // toast visible; A/B or kGreetingDuration → shop
    STATE_SHOP,       // main ImGui shop window
    STATE_FAREWELL,   // toast visible; A/B or kFarewellDuration → closed
};

static ALBWRentalState   sState              = STATE_CLOSED;
static clock::time_point sDialogueOpenTime;   // when current dialogue state started

// Every entry the player can see in the table (eligible+owned items hidden).
// purchasable == true  → eligible AND not owned → real name, can press A
// purchasable == false → not eligible yet       → shown as "?????"
struct VisibleEntry {
    int  kItemsIdx;   // index into kItems[]
    bool purchasable;
};

static VisibleEntry sVisibleList[kItemCount];
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

// Set to true by tick() whenever D-pad navigation changes sSelectedIdx.
// Consumed by imguiDraw() via ImGui::SetScrollHereY() to keep the selected
// row visible inside the scrollable table — necessary because gamepads have
// no way to interact with the scroll bar directly.
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
        const bool owned    = dComIfGs_isItemFirstBit(entry.itemNo);
        if (eligible && owned) {
            continue;  // hidden — player already owns this item
        }
        sVisibleList[sVisibleCount].kItemsIdx   = i;
        sVisibleList[sVisibleCount].purchasable = eligible && !owned;
        if (sVisibleList[sVisibleCount].purchasable) {
            sAvailCount++;
        }
        sVisibleCount++;
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
    const ALBWRentalEntry& e = kItems[sVisibleList[visIdx].kItemsIdx];
    u16 rupees = dComIfGs_getRupee();
    if (rupees < (u16)e.price) {
        sStatusMsg    = "Sincerest apologies, but we can't return\nthat to you for that little..";
        sStatusExpiry = clock::now() + std::chrono::seconds(5);
        return;
    }
    dComIfGs_setRupee(rupees - (u16)e.price);
    dComIfGs_onItemFirstBit(e.itemNo);
    if (e.slotNo >= 0) {
        dComIfGs_setItem(e.slotNo, e.itemNo);
    }
    sPurchasedThisSession = true;
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
    sScrollToSelected     = false;
    rebuildVisibleList();

    // Greeting A — returning customer (items stripped at least once).
    // Greeting B — first visit (nothing has ever been stripped yet).
    const char* greeting = hasAnyEligible()
        ? "Greetings! Lost your treasured possessions? Never fear, I have a new shipment for you! All for a.....small fee!"
        : "Greetings! As an ever dutiful Junior mail carrier I return all that is lost or misplaced! "
          "It seems to me that this is your first time using our services, browse away and keep us in mind. "
          "You may never know when something slips off your person!";

    dusk::ui::push_toast({
        .type     = "npc-dialogue",
        .title    = "Postman's Lending Service",
        .content  = greeting,
        .duration = kGreetingDuration,
    });

    sDialogueOpenTime    = clock::now();
    sState               = STATE_GREETING;
    sJustEnteredGreeting = true;  // signals Execute() to play MOT_HELLO
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

    dusk::ui::push_toast({
        .type     = "npc-dialogue",
        .title    = "Postman",
        .content  = farewell,
        .duration = kFarewellDuration,
    });

    sDialogueOpenTime    = clock::now();
    sState               = STATE_FAREWELL;
    sJustEnteredFarewell = true;  // signals Execute() to play MOT_BYE
}

bool dALBWRental_isOpen() {
    return sState != STATE_CLOSED;
}

// ============================================
// NEW CODE — ALBW Port
// dALBWRental_tick()
// Input polling and state-machine transitions only.  Call from actor
// Execute() (which runs inside the sim-tick loop and may be skipped on
// frames where sim_ticks_to_run == 0 — that's fine for input; it's not
// fine for ImGui rendering, which is why ImGui lives in imguiDraw()).
// ============================================
void dALBWRental_tick() {
    if (sState == STATE_CLOSED) {
        return;
    }

    const auto elapsed = clock::now() - sDialogueOpenTime;

    // ---- STATE_GREETING ----
    // RmlUI toast is visible. Only A/B advances to the shop.
    if (sState == STATE_GREETING) {
        if (elapsed >= kDialogueGrace) {
            if (mDoCPd_c::getTrigA(PAD_1) || mDoCPd_c::getTrigB(PAD_1)) {
                // Expire the toast immediately so it fades out instead of
                // lingering for 30 s behind the shop window.
                auto& toasts = dusk::ui::get_toasts();
                if (!toasts.empty()) {
                    toasts.front().duration = std::chrono::milliseconds(1);
                }
                sState          = STATE_SHOP;
                sJustEnteredShop = true;  // signals Execute() to lock in MOT_WAIT_A
            }
        }
        return;
    }

    // ---- STATE_FAREWELL ----
    // RmlUI toast is visible. A/B dismisses it and releases the event lock.
    if (sState == STATE_FAREWELL) {
        // Safety auto-close: once the 20 s toast expires the player can
        // no longer read it.  Signal evtTalk() to end the event so Link
        // doesn't stay locked with no visible UI.
        if (elapsed >= kFarewellDuration) {
            sJustClosed = true;
            sState      = STATE_CLOSED;
            return;
        }
        if (elapsed >= kDialogueGrace) {
            if (mDoCPd_c::getTrigA(PAD_1) || mDoCPd_c::getTrigB(PAD_1)) {
                // Expire the farewell toast immediately — same pattern as
                // the greeting dismissal above.  Without this the toast
                // stays visible for its full 20 s even though the state
                // is now CLOSED, making the player think nothing happened.
                auto& toasts = dusk::ui::get_toasts();
                if (!toasts.empty()) {
                    toasts.front().duration = std::chrono::milliseconds(1);
                }
                // Signal evtTalk() that it should now call evtChange() to
                // cleanly end the event and release Link's movement lock.
                sJustClosed = true;
                sState      = STATE_CLOSED;
            }
        }
        return;
    }

    // ---- STATE_SHOP ----

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
    // sScrollToSelected tells imguiDraw() to call SetScrollHereY() on the
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
}
// ============================================
// NEW CODE ENDS HERE
// ============================================

// ============================================
// NEW CODE — ALBW Port
// dALBWRental_imguiDraw()
// ImGui shop window — called directly from the main game loop once per
// presented frame (after aurora_begin_frame, before aurora_end_frame).
// Keeping this separate from tick() means the window is submitted exactly
// once per rendered frame regardless of how many sim-ticks Execute() ran
// (the sim-tick loop can run 0 times on interpolated frames, which would
// make the window flicker if Begin/End lived inside actor Execute()).
// ============================================
void dALBWRental_imguiDraw() {
    if (sState != STATE_SHOP) {
        return;
    }

    const ImGuiIO& io = ImGui::GetIO();
    const float winW  = 520.0f;
    const float winH  = 390.0f;
    ImGui::SetNextWindowPos(
        ImVec2((io.DisplaySize.x - winW) * 0.5f,
               (io.DisplaySize.y - winH) * 0.5f),
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.92f);

    const ImGuiWindowFlags kWinFlags =
        ImGuiWindowFlags_NoTitleBar            |
        ImGuiWindowFlags_NoMove                |
        ImGuiWindowFlags_NoResize              |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings;

    if (!ImGui::Begin("##ALBWRental", nullptr, kWinFlags)) {
        ImGui::End();
        return;
    }

    // Title
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "Postman's Lending Service");
    ImGui::Separator();
    ImGui::Spacing();

    if (sVisibleCount == 0) {
        // All 12 items are owned — extremely rare edge case.
        ImGui::TextWrapped(
            "You have everything in our catalogue! Truly remarkable, "
            "a legend in the making.");
        ImGui::Spacing();
        ImGui::Spacing();
        if (ImGui::Button("Leave  (B)", ImVec2(120.0f, 0.0f))) {
            dALBWRental_close();
        }
    } else {
        // ---- Status / purchase message ----
        if (sStatusMsg != nullptr && clock::now() < sStatusExpiry) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.95f, 0.5f, 1.0f));
            ImGui::TextWrapped("%s", sStatusMsg);
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }

        // ---- Item table ----
        // Dim colour for ????? rows (not yet eligible).
        static const ImVec4 kDimCol = ImVec4(0.45f, 0.45f, 0.45f, 1.0f);

        const ImGuiTableFlags kTableFlags =
            ImGuiTableFlags_BordersOuter  |
            ImGuiTableFlags_BordersInnerH |
            ImGuiTableFlags_ScrollY       |
            ImGuiTableFlags_RowBg;

        if (ImGui::BeginTable("##rentitems", 2, kTableFlags, ImVec2(0.0f, 200.0f))) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Item",           ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Price (Rupees)", ImGuiTableColumnFlags_WidthFixed, 130.0f);
            ImGui::TableHeadersRow();

            for (int row = 0; row < sVisibleCount; ++row) {
                const VisibleEntry&    ve    = sVisibleList[row];
                const ALBWRentalEntry& e     = kItems[ve.kItemsIdx];
                const bool             isSel = (sSelectedIdx == row);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                char selId[64];
                if (ve.purchasable) {
                    snprintf(selId, sizeof(selId), "%s##r%d", e.name, row);
                } else {
                    snprintf(selId, sizeof(selId), "?????##r%d", row);
                    ImGui::PushStyleColor(ImGuiCol_Text, kDimCol);
                }

                if (ImGui::Selectable(selId, isSel,
                                      ImGuiSelectableFlags_SpanAllColumns |
                                      ImGuiSelectableFlags_AllowOverlap,
                                      ImVec2(0.0f, 0.0f))) {
                    sSelectedIdx      = row;
                    sScrollToSelected = true;  // mouse click also scrolls into view
                }

                // Scroll the table so this row stays visible after D-pad navigation.
                // Must be called immediately after Selectable while still inside
                // the scrolling region — imguiDraw() consumes the flag here.
                if (isSel && sScrollToSelected) {
                    ImGui::SetScrollHereY(0.5f);
                    sScrollToSelected = false;
                }

                if (!ve.purchasable) {
                    ImGui::PopStyleColor();
                }

                ImGui::TableSetColumnIndex(1);
                if (!ve.purchasable) {
                    ImGui::PushStyleColor(ImGuiCol_Text, kDimCol);
                }
                ImGui::Text("%d", e.price);   // real price always shown
                if (!ve.purchasable) {
                    ImGui::PopStyleColor();
                }
            }
            ImGui::EndTable();
        }

        // ---- Description for the selected row ----
        // Purchasable  → item's own desc field (bright text).
        // ????? row    → "Not in stock" / Magic Armor special line (dim text).
        // Nothing      → invisible dummy to keep layout height stable.
        ImGui::Spacing();
        if (sSelectedIdx >= 0 && sSelectedIdx < sVisibleCount) {
            const VisibleEntry&    selVe = sVisibleList[sSelectedIdx];
            const ALBWRentalEntry& selE  = kItems[selVe.kItemsIdx];
            if (selVe.purchasable) {
                // Real item available for purchase — show its description.
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
                ImGui::TextWrapped("%s", selE.desc);
                ImGui::PopStyleColor();
            } else {
                // ????? item — not yet eligible for purchase.
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.55f, 1.0f));
                ImGui::TextWrapped("Not in stock yet - Come back soon!");
                ImGui::PopStyleColor();
            }
        } else {
            ImGui::Dummy(ImVec2(0.0f, ImGui::GetTextLineHeight()));
        }

        // ---- Footer ----
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Rupees: %d", (int)dComIfGs_getRupee());
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetContentRegionAvail().x - 220.0f + ImGui::GetCursorPosX());

        // "Rent It" is disabled when a status message is showing OR the
        // selected row is a ????? (not yet eligible) item.
        const bool statusClear   = (sStatusMsg == nullptr || clock::now() >= sStatusExpiry);
        const bool selPurchasable = (sSelectedIdx >= 0 &&
                                     sSelectedIdx < sVisibleCount &&
                                     sVisibleList[sSelectedIdx].purchasable);
        const bool canBuy = statusClear && selPurchasable;
        if (!canBuy) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Rent It  (A)", ImVec2(120.0f, 0.0f))) {
            tryPurchase(sSelectedIdx);
        }
        if (!canBuy) {
            ImGui::EndDisabled();
        }
        ImGui::SameLine();
        if (ImGui::Button("Leave  (B)", ImVec2(95.0f, 0.0f))) {
            dALBWRental_close();
        }

        ImGui::Spacing();
        ImGui::TextDisabled("[D-Pad Up/Down] Navigate    [A] Rent    [B] Leave");
    }

    ImGui::End();
}
// ============================================
// NEW CODE ENDS HERE
// ============================================

// ============================================
// NEW CODE ENDS HERE
// ============================================

#endif  // TARGET_PC

#ifndef D_ALBW_RENTAL_H
#define D_ALBW_RENTAL_H

// ============================================
// NEW CODE — ALBW Port
// Postman rental shop — lets the player reclaim ALBW-mechanic items
// that were stripped on death, for a small rupee fee.
//
// Spawned as a standing Postman actor in F_SP103 (Ordon Village) room 1
// after the player completes the Talo rescue (event bit 625).
// Intercept happens in daNpc_Post_c::evtTalk() when getBitSW() == 0x42.
//
// Entry points:
//   dALBWRental_tick() — input + state machine; call from actor Execute()
//   dALBWShop_c        — native TP letter-select shop; drawn from postman Draw()
// ============================================
#if TARGET_PC
void dALBWRental_open();
void dALBWRental_close();
bool dALBWRental_isOpen();
// True while the rental postman talk event is active (includes the frame before
// STATE_GREETING is entered) so vanilla "No response..." never draws underneath.
void dALBWRental_armVanillaTalkSuppress();
void dALBWRental_clearVanillaTalkSuppress();
bool dALBWRental_shouldSuppressVanillaTalkMsg();
bool dALBWRental_justClosed();          // evtTalk()  — true once after shop closes; triggers evtChange()
void dALBWRental_tick();                // actor Execute() — input & state only

// ============================================
// NEW CODE — ALBW Port
// One-shot animation transition signals for daNpc_Post_c::Execute().
// Each returns true exactly once per state transition, then resets to false.
// Call after dALBWRental_tick() every Execute() frame.
// ============================================
bool dALBWRental_justEnteredGreeting(); // Execute() — play MOT_HELLO + FACE_MOT_HELLO + Z2SE_POST_V_APPEAR
bool dALBWRental_justEnteredShop();     // Execute() — play MOT_WAIT_A + FACE_MOT_NONE_2
bool dALBWRental_justEnteredFarewell(); // Execute() — play MOT_BYE   + FACE_MOT_BYE  + Z2SE_POST_V_THEME
// ============================================
// NEW CODE — ALBW Port (Purchase / Failed-Purchase Sounds)
// One-shot flags consumed by Execute() to play voice clips.
// Delete justPurchased + sJustPurchased in d_albw_rental.cpp to remove
// purchase sounds with zero impact on shop logic, animations, or dialogue.
// ============================================
bool dALBWRental_justPurchased();       // Execute() — play Z2SE_POST_V_FANFARE congratulatory fanfare
bool dALBWRental_justFailedPurchase();  // Execute() — play Z2SE_POST_V_RUN_HIGH insufficient rupees
// ============================================
// NEW CODE — ALBW Port (Native UI)
// Visible item list — consumed by dALBWShop_c to populate the shop window.
// ============================================
struct dALBWVisibleEntry {
    const char* name;
    int         price;
    bool        purchasable;
    const char* desc;   ///< description string (null when purchasable==false)
    u8          itemNo; ///< wheel item id for row icon (letter icon when !purchasable)
    bool        isOocooService;  ///< Cuckoo's Return (not a rental item row)
};
const dALBWVisibleEntry* dALBWRental_getVisibleList(int* outCount);
#if TARGET_PC_NATIVE_UI
void        dALBWRental_advanceToShop();
void        dALBWRental_advanceToClosed();
// True only while in STATE_SHOP (not GREETING or FAREWELL).
// Used by d_a_npc_post Draw() to guard the shop window so it never
// renders on top of the greeting or farewell dialogue.
bool        dALBWRental_isShopState();
bool        dALBWRental_isGreetingState();  // true while in STATE_GREETING
bool        dALBWRental_isFarewellState();  // true while in STATE_FAREWELL
// Text for the native dialogue windows (set during open/close, valid until next call).
const char* dALBWRental_getGreetingText();   // page 1 of greeting
const char* dALBWRental_getGreetingPage2();  // page 2 (null if one-pager)
const char* dALBWRental_getGreetingPage3();  // page 3, first-visit only (null if done)
const char* dALBWRental_getFarewellText();
// Current selection index into the visible list — for dALBWShop_c to highlight.
int         dALBWRental_getSelectedIdx();
#endif
// ============================================
// NEW CODE ENDS HERE
// ============================================
#endif
// ============================================
// NEW CODE ENDS HERE
// ============================================

#endif /* D_ALBW_RENTAL_H */

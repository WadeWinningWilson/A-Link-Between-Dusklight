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
// Two separate entry points are intentional:
//   dALBWRental_tick()      — input + state machine; call from actor Execute()
//   dALBWRental_imguiDraw() — ImGui shop window; call from the main game loop
//                             (after aurora_begin_frame, before aurora_end_frame)
//
// Keeping them separate ensures the ImGui window is submitted exactly once
// per presented frame regardless of how many sim-ticks Execute() runs.
// ============================================
#if TARGET_PC
void dALBWRental_open();
void dALBWRental_close();
bool dALBWRental_isOpen();
bool dALBWRental_justClosed();          // evtTalk()  — true once after shop closes; triggers evtChange()
void dALBWRental_tick();                // actor Execute() — input & state only
void dALBWRental_imguiDraw();           // main loop — ImGui shop window only

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
// NEW CODE ENDS HERE
// ============================================
#endif
// ============================================
// NEW CODE ENDS HERE
// ============================================

#endif /* D_ALBW_RENTAL_H */

/**
 * d_albw_rupee_popup.h
 * ALBW Port — Enemy Death Rupees "+n" HUD popup.
 *
 * Transient white "+n" text drawn just above the rupee counter with the
 * native J2D meter stack (standalone J2DTextBox, message font).  Fired
 * from grantRupees() in d_albw_enemy_rupee.cpp so the displayed amount is
 * always the granted amount — no parallel lookup tables.  Not a Dusklight
 * toast; renders inside dMeter2Draw_c::draw() like the other ALBW HUD
 * overlays and follows the rupee HUD's own fade/pause behaviour.
 */

#pragma once

#if TARGET_PC

#include "dolphin/types.h"

// ============================================
// NEW CODE — ALBW Port
// ============================================

// Arm (or re-arm) the popup with a freshly granted amount.  A new grant
// replaces the previous value and restarts the display window.
void dAlbwEnemyRupeesHud_onGrant(u16 i_amount);

// Per-frame draw tick, called from dMeter2Draw_c::draw().
void dAlbwRupeePopup_draw();

// ============================================
// NEW CODE ENDS HERE
// ============================================

#endif // TARGET_PC

/**
 * d_albw_wolf_charge_hud.h
 * ALBW Port — Wolf Link charge counter HUD.
 *
 * Draws 0-2 wolf silhouette icons in the same rupee-anchored row that the
 * shield-parry spur icons normally occupy.  Visibility mirrors the spur
 * HUD: summoned by the shield button or any charge event, lingers ~2s,
 * then fades.  Only active during wolf form with wolfLinkCombat enabled;
 * spur icons are suppressed for that same window and return when Link
 * transforms back.
 */

#pragma once

#if TARGET_PC

// ============================================
// NEW CODE — ALBW Port
// ============================================

// Called once per HUD draw tick from dMeter2Draw_c::draw().
// No-ops when not in wolf form or wolfLinkCombat is off.
void dAlbwWolfChargeHud_draw();

// Refresh the visibility linger window (charge gained / spent).
void dAlbwWolfChargeHud_notify();

// Deny event: refresh linger AND start the spur-style deny flash.
void dAlbwWolfChargeHud_notifyDeny();

// ============================================
// NEW CODE ENDS HERE
// ============================================

#endif // TARGET_PC

/**
 * d_albw_boss_hp_hud.h
 * ALBW Port — Boss health bar HUD.
 *
 * Draws a bottom-centre health bar with the boss name above it (matching the
 * approved reference), for major boss fights. Reads HP exclusively from
 * dAlbwHP_getLockonDisplayHp() so it stays in sync with every ALBW setting
 * that affects boss HP (Boss HP scaler, Boss Refinement) and with the lock-on
 * debug overlay — it never computes its own HP/scaling. Link Damage only
 * divides attack power, so it correctly does not affect the bar's max.
 *
 * Gated behind dusk::getSettings().game.bossHealthBars. Prototype scope:
 * Armogohma (fpcNm_B_GM_e) only; generalises to sBoss[] later.
 */

#pragma once

#if TARGET_PC

// ============================================
// NEW CODE — ALBW Port
// ============================================

// Per-frame draw tick, called from dMeter2Draw_c::draw().
void dAlbwBossHpHud_draw();

// ============================================
// NEW CODE ENDS HERE
// ============================================

#endif // TARGET_PC

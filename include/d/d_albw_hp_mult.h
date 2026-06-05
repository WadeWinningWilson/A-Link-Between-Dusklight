/**
 * d_albw_hp_mult.h
 * ALBW Port — Enemy HP multiplier category lookup and intercept API.
 *
 * Enemies are bucketed into four categories (Normal, MidBoss, Boss, Final)
 * each driven by an independent Dusk settings slider (1–16×, default 1×).
 * Actors in the Excluded list pass through unchanged regardless of settings.
 *
 * Called from cc_at_check() in d_cc_uty.cpp after all attack-power
 * modifiers have been applied (Master Sword ×2, sword upgrade, light-arrow
 * override) and before health is decremented.
 */

#pragma once

#if TARGET_PC

#include "f_pc/f_pc_base.h"

// ============================================
// NEW CODE — ALBW Port
// ============================================

enum dAlbwHP_Category {
    dAlbwHP_NORMAL   = 0,  // Standard enemies (Bokoblins, Stalfos, etc.)
    dAlbwHP_MID_BOSS = 1,  // Sub-bosses (Ook, Death Sword, Deku Toad, etc.)
    dAlbwHP_BOSS     = 2,  // Dungeon bosses (Diababa through Zant)
    dAlbwHP_FINAL    = 3,  // Final sequence (Ganon / Ganondorf forms)
    dAlbwHP_EXCLUDED = 4,  // Skip multiplier entirely (scripted / uncertain)
};

// Returns the category for a given actor profile name.
dAlbwHP_Category dAlbwHP_getCategory(s16 profName);

// Divides attackPower by the setting for profName's category.
// Returns max(1, attackPower / multiplier).
// Returns attackPower unchanged when multiplier == 1 or category == EXCLUDED.
int dAlbwHP_applyMult(s16 profName, int attackPower);

// Shield durability: mid-boss 1.5×, boss/final 2× on non-parry hits (final may get bespoke rules later).
u16 dAlbwHP_applyDurabilityMult(s16 profName, u16 damage);

// ============================================
// NEW CODE ENDS HERE
// ============================================

#endif // TARGET_PC

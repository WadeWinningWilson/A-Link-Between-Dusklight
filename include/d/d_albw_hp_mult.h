/**
 * d_albw_hp_mult.h
 * ALBW Port — Enemy HP scaling and Link damage decrease API.
 *
 * Two independent systems (both default 1× = vanilla):
 *   1. Per-category true max-HP multipliers — multiply health + field_0x560
 *      once per enemy instance (Common / Mid-Boss / Boss / Final Boss sliders).
 *   2. Global Link damage decrease — divides attack power before health
 *      deduction (linkDamageDecreaseMult slider).
 *
 * Actors in the Excluded category skip true-HP scaling regardless of settings.
 */

#pragma once

#if TARGET_PC

#include "f_op/f_op_actor.h"
#include "f_pc/f_pc_base.h"

// ============================================
// NEW CODE — ALBW Port
// ============================================

enum dAlbwHP_Category {
    dAlbwHP_NORMAL   = 0,  // Standard enemies (Bokoblins, Stalfos, etc.)
    dAlbwHP_MID_BOSS = 1,  // Sub-bosses (Ook, Death Sword, Deku Toad, etc.)
    dAlbwHP_BOSS     = 2,  // Dungeon bosses (Diababa through Zant)
    dAlbwHP_FINAL    = 3,  // Final sequence (Ganon / Ganondorf forms)
    dAlbwHP_EXCLUDED = 4,  // Skip true-HP scaling (scripted / uncertain)
};

// Returns the category for a given actor profile name.
dAlbwHP_Category dAlbwHP_getCategory(s16 profName);

// True max-HP multiplier for profName's category (1–16, default 1).
int dAlbwHP_getTrueHpMult(s16 profName);

// Scales a raw HP value by profName's true-HP category multiplier.
// For bosses that reset health from a constant mid-fight.
s16 dAlbwHP_scaleHpValue(s16 profName, s16 hp);

enum dAlbwHP_DarknutPhase {
    dAlbwHP_Darknut_NONE = 0,
    dAlbwHP_Darknut_ARMORED,
    dAlbwHP_Darknut_TRANSITION,
    dAlbwHP_Darknut_UNARMORED,
};

struct dAlbwHP_LockonDisplay {
    s16 current;
    s16 max;
    bool customMeter;  // true when current/max is fight progress, not raw actor HP
    dAlbwHP_DarknutPhase darknutPhase;
    s16 actorHealth;     // raw health (informational for multi-phase bosses)
    s16 actorHealthMax;  // raw field_0x560 or health
};

// Effective HP for the lock-on debug overlay (handles bosses with internal meters).
dAlbwHP_LockonDisplay dAlbwHP_getLockonDisplayHp(fopAc_ac_c* actor);

// Applies true max-HP scaling once when health is initialized (> 1).
// Called from fopAc_Execute for enemy-group actors.
void dAlbwHP_tryApplyTrueMaxHp(fopAc_ac_c* actor);

// Removes per-instance tracking when an actor is deleted.
void dAlbwHP_onActorDelete(fpc_ProcID procId);

// Divides attackPower by linkDamageDecreaseMult (global, not per category).
// Returns max(1, attackPower / multiplier). Unchanged when multiplier == 1.
int dAlbwHP_applyMult(s16 profName, int attackPower);

// Returns linkDamageDecreaseMult when > 1, else 1.
// Used by wolf charge attack scaling to undo the divide-then-multiply path.
int dAlbwHP_getRawMult(s16 profName);

// Shield durability: mid-boss 1.5×, boss/final 2× on non-parry hits (final may get bespoke rules later).
u16 dAlbwHP_applyDurabilityMult(s16 profName, u16 damage);

// ============================================
// NEW CODE ENDS HERE
// ============================================

#endif // TARGET_PC

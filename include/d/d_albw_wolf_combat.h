/**
 * d_albw_wolf_combat.h
 * ALBW Port — Wolf Link combat system feature gate.
 *
 * Convenience header so any file that needs dAlbwWolfCombat_isEnabled()
 * can include this without pulling in the full stun timer API.
 * The function is declared and implemented in d_albw_wolf_stun.h/.cpp;
 * this header simply re-exports it.
 */

#pragma once

#if TARGET_PC

// ============================================
// NEW CODE — ALBW Port
// ============================================

#include "d/d_albw_wolf_stun.h"  // provides dAlbwWolfCombat_isEnabled()

// ============================================
// NEW CODE ENDS HERE
// ============================================

#endif // TARGET_PC

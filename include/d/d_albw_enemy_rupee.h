#ifndef D_ALBW_ENEMY_RUPEE_H
#define D_ALBW_ENEMY_RUPEE_H

// ============================================
// NEW CODE — ALBW Port
// Direct wallet rupees on enemy kill and on boss fight victory.
// Additive only — vanilla enemy drop tables are unchanged.
// Gated on Dusk setting game.enemyDeathRupees (default off).
// ============================================
#if TARGET_PC

class fopAc_ac_c;

bool dAlbwEnemyRupees_isEnabled();

// Field kill: credit wallet when an fopAc_ENEMY_e actor dies (additive to table drops).
// Idempotent per actor for the rest of the session (safe if cc_at_check and actor death both fire).
void dAlbwEnemyRupees_onEnemyKill(fopAc_ac_c* enemy);

// After cc_at_check subtracts damage — grants only when HP reaches 0.
bool dAlbwEnemyRupees_tryKillAfterDamage(fopAc_ac_c* enemy, s32 attackPower);

// Boss / mid-boss / arena fight victory (one grant per profName per play session).
void dAlbwEnemyRupees_tryGrantFightVictory(s16 profName);

// Beamos and similar object traps (fopAc_ACTOR_e).
void dAlbwEnemyRupees_onBeamosDestroyed();

#endif

#endif /* D_ALBW_ENEMY_RUPEE_H */

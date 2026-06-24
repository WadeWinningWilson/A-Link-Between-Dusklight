#ifndef D_ALBW_LOCKOUT_H
#define D_ALBW_LOCKOUT_H

#if TARGET_PC

#include "f_op/f_op_actor.h"

// Session counters reset when lockout begins (meter hit 0) and when it ends (meter full).
void dAlbwLockout_onBegin();
void dAlbwLockout_onEnd();

// Per-frame tick (call from daAlink_c::execute). Decrements boomerang debuff timers.
void dAlbwLockout_update();

void dAlbwLockout_onArrowFired();
void dAlbwLockout_onBombArrowFired();
void dAlbwLockout_onSlingFired();
void dAlbwLockout_onHookshotFired();
void dAlbwLockout_onDoubleHookshotFired();

bool dAlbwLockout_canFireBow();
bool dAlbwLockout_canFireBombArrow();
bool dAlbwLockout_canFireSling();
bool dAlbwLockout_canUseDoubleHookshot();

// Lockout boomerang hit on a common enemy: ranged-open window + 4s stun (pause-based by default).
void dAlbwLockout_onBoomerangHit(fopAc_ac_c* i_enemy);

// Same debuff without fpcM_Pause (enemies that use native wobble/stun anims, e.g. Darknut, Stalfos).
void dAlbwLockout_onBoomerangHitNative(fopAc_ac_c* i_enemy);

bool dAlbwLockout_isRangedOpened(fopAc_ac_c* i_enemy);
bool dAlbwLockout_isBoomerangStunActive(fopAc_ac_c* i_enemy);

// During lockout, scale a vanilla boomerang stun timer up to 4 seconds (120 frames @ 30fps).
int dAlbwLockout_getBoomerangStunFrames(int i_baseFrames);

// Lockout-only attack-power modifiers (call from d_cc_uty after other mods, before HP apply).
void dAlbwLockout_applyAttackPowerBoost(u16& io_attackPower, u32 i_atType);

#endif // TARGET_PC

#endif // D_ALBW_LOCKOUT_H

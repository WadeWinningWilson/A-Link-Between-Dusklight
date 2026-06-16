#ifndef D_ALBW_LOCKOUT_H
#define D_ALBW_LOCKOUT_H

#if TARGET_PC

// Session counters reset when lockout begins (meter hit 0) and when it ends (meter full).
void dAlbwLockout_onBegin();
void dAlbwLockout_onEnd();

void dAlbwLockout_onArrowFired();
void dAlbwLockout_onHookshotFired();
void dAlbwLockout_onDoubleHookshotFired();

bool dAlbwLockout_canFireBow();
bool dAlbwLockout_canUseDoubleHookshot();

// Lockout-only attack-power modifiers (call from d_cc_uty after other mods, before HP apply).
void dAlbwLockout_applyAttackPowerBoost(u16& io_attackPower, u32 i_atType);

#endif // TARGET_PC

#endif // D_ALBW_LOCKOUT_H

#ifndef D_ALBW_SHIELD_H
#define D_ALBW_SHIELD_H

// ============================================
// NEW CODE — ALBW Port (Phase 4)
// Perfect parry (LoP-style guard-onset window) + bash charge economy.
// Session-only charge state; no save persistence.
// ============================================

#if TARGET_PC

class daAlink_c;

bool dShield_isParryCombatEnabled();
bool dShield_isDurabilityEnabled();

void dShield_resetSession();

void dShield_updateGuardTracking(daAlink_c* i_link);

// Called after mGuardAtCps is registered (collision runs after procGuardAttack).
void dShield_pollGuardAttackHit(daAlink_c* i_link);

// Returns true when shield should break instead of entering guard slip.
// True when durability reached 0 this hit — caller should run dShield_destroyFromDurability().
bool dShield_onBlockHit(daAlink_c* i_link, int i_atSpl, bool i_perfect, fopAc_ac_c* i_attacker);
bool dShield_onGuardSlip(daAlink_c* i_link, int i_atSpl);
bool dShield_onSmallGuardHit(daAlink_c* i_link, fopAc_ac_c* i_attacker);
bool dShield_isBroken();

void dShield_destroyFromDurability(daAlink_c* i_link);

// Partial repair as a fraction of current tier max HP (e.g. 1/5 = 20%).
void dShield_repairDurabilityFraction(int numerator, int denominator);

bool dShield_shouldDrawDurabilityHud();
u16 dShield_getDurability();
u16 dShield_getDurabilityMax();
// 0 = Ordon, 1 = Wooden, 2 = Hylian (for meter tint).
u8 dShield_getDurabilityTierStyle();
// Visual max width vs ALBW kantera row (Ordon 0.5, Wooden/Hylian 1.0).
f32 dShield_getDurabilityMeterWidthScale();

bool dShield_onShieldHit(daAlink_c* i_link, int i_atSpl, fopAc_ac_c* i_attacker);

void dShield_onFailedGuardBlock(fopAc_ac_c* i_attacker);

bool dShield_tryBeginGuardAttack();

void dShield_onGuardAttackConnect();

u8 dShield_getBashCharges();
u8 dShield_getMaxBashCharges();
u8 dShield_getBashThreshold();
u8 dShield_getDenyFlashFrames();
bool dShield_canSpendBash();
bool dShield_isBashSpendChainActive();

void dShield_drawBashCharges();

#endif // TARGET_PC

#endif // D_ALBW_SHIELD_H

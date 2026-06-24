#ifndef D_ALBW_SHIELD_H
#define D_ALBW_SHIELD_H

// ============================================
// NEW CODE — ALBW Port (Phase 4)
// Perfect parry (LoP-style guard-onset window) + bash charge economy.
// Session-only charge state; no save persistence.
// ============================================

#if TARGET_PC

class daAlink_c;
class fopEn_enemy_c;

bool dShield_isParryCombatEnabled();
bool dShield_isTestingParryReworkEnabled();
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

void dShield_playParrySuccessFeedback(daAlink_c* i_link, const cXyz* i_hitPos);

// Guard-break tier (AtSpl 9/10/11): defer vanilla instant break for parry-eligible enemies.
// Mount/spear actors (King Bulblin, horseback Ganondorf) keep vanilla behavior.
bool dShield_shouldDeferGuardBreak(int i_atSpl, fopAc_ac_c* i_attacker);

// Failed block on a deferred guard-break attack: ALBW/charge penalty, heavy durability drain.
// Returns true when caller should run procGuardBreakInit().
bool dShield_onFailedGuardBreakBlock(daAlink_c* i_link, int i_atSpl, fopAc_ac_c* i_attacker);

void dShield_onFailedGuardBlock(fopAc_ac_c* i_attacker);

bool dShield_tryBeginGuardAttack();

// Set when the current bash spend opens a chain at full charges (before decrement).
bool dShield_consumeLastBashSpendFromFullBar();
// Set when the current bash spend opens a chain at bashThreshold or higher (before decrement).
bool dShield_consumeLastBashSpendOpenedAtThreshold();

// Full-bar punish tier (P1 guard-break bash, Helm Splitter when parry combat is on).
bool dShield_canUseFullBarPunish();
bool dShield_hasFullBarPunishPending();
bool dShield_hasThresholdPunishPending();
bool dShield_hasFullBarBashInFlight();
void dShield_clearFullBarPunishPending();
bool dShield_tryConsumeThresholdPunish();
bool dShield_chargeHelmSplitterMeterOnce();
void dShield_clearHelmSplitterMeterCharge();
bool dShield_tryGrantHelmPunishCredit(fopEn_enemy_c* i_enemy);
bool dShield_hasHelmPunishPending();
bool dShield_hasHelmPunishOnTarget(fopEn_enemy_c* i_enemy);
bool dShield_tryConsumeFullBarPunish();

bool dShield_tryBeginHelmSplitter();

void dShield_onGuardAttackConnect();

u8 dShield_getBashCharges();
u8 dShield_getMaxBashCharges();
void dShield_fillBashChargesToMax();
u8 dShield_getBashThreshold();
u8 dShield_getDenyFlashFrames();
bool dShield_canSpendBash();
bool dShield_isBashSpendChainActive();

void dShield_drawBashCharges();

// ============================================
// NEW CODE — ALBW Port (Lies of Link HUD)
// Bounds of the last-drawn LoP shield-icon row in the HUD global ortho, so the
// relocated durability bar can group with the icons (left-align + width + Y).
// ============================================
struct ShieldRowLayout {
    f32 firstCenterX;  // global X of the first icon's center
    f32 centerY;       // global Y of the icon row centre
    f32 iconW;         // drawn icon width (≈ height)
    f32 spacing;       // distance between icon centres
    u8 slotCount;      // icons in the row
    bool valid;
};
bool dShield_getLastRowLayout(ShieldRowLayout* o_row);
// ============================================
// NEW CODE ENDS HERE
// ============================================

#endif // TARGET_PC

#endif // D_ALBW_SHIELD_H

/**
 * d_focused_arts.h
 * Focused Arts — shop tier purchases, charge bank query API, runtime meter state.
 *
 * Enabled when game.focusedArtsTest is on (Hidden Skill Rework is always active for ALBW).
 * Shop (or focusedArtsCheat) grants tiers 1→2→3 one at a time via tryPurchaseShopTier().
 */

#pragma once

#if TARGET_PC

static constexpr int kFocusedArtsMaxTier           = 3;
static constexpr int kFocusedArtsFillDenominator   = 12;

bool dFocusedArts_isEnabled();

// In-game FA debug overlay (Settings → With Debug); no dev console required.
bool dFocusedArts_isDebugOverlayEnabled();

// True while spending banked charges (T3→T2→T1) — ALBW HS/sword drains should not apply.
bool dFocusedArts_shouldSuppressAlbwMeterDrain();

// Save-backed shop tier (0 = none purchased). Ignores cheat — use for shop sold-out UI.
int dFocusedArts_getPurchasedTier();

// Effective tier for combat (cheat → 3 when enabled; else purchased tier).
int dFocusedArts_getEffectiveTier();
int dFocusedArts_getMaxBank();
bool dFocusedArts_hasSpecialFinishers();

int  dFocusedArts_getShopTierPrice(int tier);   // tier 1..3
int  dFocusedArts_getNextShopTierPrice();       // price for purchasedTier + 1; 0 if sold out

bool dFocusedArts_canPurchaseShopTier();
bool dFocusedArts_tryPurchaseShopTier();        // +1 save tier; rupee check by caller

const char* dFocusedArts_getShopTierName(int tier);
const char* dFocusedArts_getShopTierDesc(int tier);

// Runtime meter (field session; reset manually or on new game load).
void dFocusedArts_resetRuntimeState();
void dFocusedArts_onStageLoad();

int  dFocusedArts_getBankCount();
int  dFocusedArts_getFillNumerator();
int  dFocusedArts_getFillDenominator();
bool dFocusedArts_isSpendReady();
bool dFocusedArts_hasSpecialFinisherAvailable();

// Spend sequence (0 = not spending; 3/2/1 = active spend column for this HS).
int  dFocusedArts_getSpendColumn();
bool dFocusedArts_isInSpendSequence();

// Banked pips = charged damage tier while building (0 = new-base only, 1 = T1 chg, ...).
int dFocusedArts_getChargedDamageTier();

// Playtest diagnostics (ImGui overlay / CONAV_LOG).
const char* dFocusedArts_getLastEventText();
const char* dFocusedArts_getRecentEventLine(int i_index);

// Debug overlay: per-hidden-skill FA attack preview (save-bit acquired state).
int dFocusedArts_getHiddenSkillAttackDebugLineCount();
const char* dFocusedArts_getHiddenSkillAttackDebugSwordLabel();
void dFocusedArts_formatHiddenSkillAttackDebugLine(int i_index, char* o_buf, size_t i_bufSize);

// Combat hooks — call sites wired in cut / damage / cc collision.
void dFocusedArts_onConnectedSwordHit();
void dFocusedArts_onConnectedItemHit();
void dFocusedArts_onPlayerHiddenSkillUse();
void dFocusedArts_onHiddenSkillChargeStart();
void dFocusedArts_onDamageTaken();

// Per-frame tick (back-slice ALBW suppress timer, JS finisher lockout cleanup).
void dFocusedArts_update();

// After HS proc init + setCutType — applies T1 special finisher side effects.
void dFocusedArts_onHiddenSkillProcStarted(int i_cutType);

// Back Slice T1 finisher landed — reset FA damage to new base (0.4×V).
void dFocusedArts_onBackSliceFinisherEnded();

// FA melee damage: V × (0.4 + 0.2 × tierSteps) + finisher mults. Skips inst-kill EB cuts.
u16 dFocusedArts_resolveMeleeDamage(u16 i_vanillaPower, int i_cutType);

// FA item damage tier stacks (+50% per tier; T1 spend +50%). Skipped during JS finisher lockout.
void dFocusedArts_applyItemDamageBoost(u16& io_attackPower);

bool dFocusedArts_isJsFinisherLockoutActive();

// True while spending the last banked charge (bank==1 mid T3→T2→T1 sequence, tier 3 owned).
bool dFocusedArts_isOnFinalSpendCharge();

// True during the HS proc for that final spend (spend column 1 + maintain-stacked window).
bool dFocusedArts_isSpecialFinisherSpendActive();

// GS Hurricane: final spend charge + tier 3 special finishers (before onPlayerHiddenSkillUse).
bool dFocusedArts_shouldLaunchGsHurricaneFinisher();

// Arm + consume the GS hurricane finisher (prevents stale spend-column re-entry).
bool dFocusedArts_tryArmGsHurricaneFinisher();
bool dFocusedArts_consumeGsHurricaneFinisherArmed();

// GS Hurricane finisher hit power (playtest tuning).
u16 dFocusedArts_getGsHurricaneHitPower();

// Ending Blow T1 finisher: brief Great Spin AOE at 50% GS damage on successful stab contact.
bool dFocusedArts_onEndingBlowContact();
bool dFocusedArts_isEndingBlowGreatSpinAoeActive();
u16 dFocusedArts_getEndingBlowGreatSpinAoePower(u16 i_greatSpinAttackPower);

// Postman shop: show next tier row while save tier < 3 and cheat is not granting all tiers.
bool dFocusedArts_shouldShowShopTierRow();
int  dFocusedArts_getNextShopTierIndex();  // purchased + 1 (1..3); 0 if sold out

#endif

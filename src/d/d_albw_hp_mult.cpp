/**
 * d_albw_hp_mult.cpp
 * ALBW Port — True max-HP scaling and Link damage decrease.
 *
 * True HP: each enemy actor is scaled once on init (health + field_0x560
 * multiplied by its category slider). 1-HP actors are left unchanged.
 *
 * Link damage decrease: global attack-power divisor at the collision
 * intercept in d_cc_uty.cpp (independent of category HP sliders).
 */

#if TARGET_PC

#include "d/d_albw_hp_mult.h"
#include "d/d_albw_boss.h"
#include "dusk/settings.h"
#include "d/actor/d_a_b_tn.h"
#include "f_op/f_op_actor.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include <algorithm>
#include <cstddef>
#include <unordered_set>

// ============================================
// NEW CODE — ALBW Port
// ============================================

static std::unordered_set<fpc_ProcID> s_trueHpApplied;

// ---------------------------------------------------------------------------
// Mid-boss list — gatekeepers and story minibosses (same tier everywhere).
// ---------------------------------------------------------------------------
static const s16 sMidBoss[] = {
    fpcNm_B_GG_e,     // 0x214 Aeralfos / Gargoyle
    fpcNm_B_TN_e,     // 0x213 Darknut (Temple of Time)
    fpcNm_B_ZANTM_e,  // Phantom Zant (magic attack actor)
    fpcNm_E_DT_e,     // 0x200 Deku Toad
    fpcNm_E_GOB_e,    // 0x1B1 Dangoro
    fpcNm_E_MK_e,     // 0x1DC Ook
    fpcNm_E_PM_e,     // 0x1D9 Skull Kid (Sacred Grove)
    fpcNm_E_PZ_e,     // 0x1E4 Phantom Zant
    fpcNm_E_RDB_e,    // 0x1D5 King Bulblin
    fpcNm_E_TH_e,     // 0x1C2 Darkhammer
    fpcNm_E_VT_e,     // 0x208 Death Sword
    fpcNm_E_YC_e,     // 0x0F5 Twilit Carrier Kargarok
    fpcNm_E_YMB_e,    // 0x1F6 Twilit Bloat
};

// ---------------------------------------------------------------------------
// Dungeon boss list — main dungeon bosses + key sub-actors in those fights.
// ---------------------------------------------------------------------------
static const s16 sBoss[] = {
    fpcNm_B_BQ_e,     // 0x20C Diababa
    fpcNm_B_BH_e,     // 0x20B Diababa baba tentacles
    fpcNm_B_DS_e,     // 0x0F6 Stallord
    fpcNm_B_DR_e,     // 0x0F7 Argorok
    fpcNm_B_DRE_e,    // Argorok child actor
    fpcNm_B_GM_e,     // 0x20D Armogohma
    fpcNm_B_OB_e,     // Morpheel body
    fpcNm_B_OH_e,     // Morpheel head
    fpcNm_B_OH2_e,    // Morpheel tentacle
    fpcNm_B_YO_e,     // 0x211 Blizzeta
    fpcNm_B_YOI_e,    // Blizzeta ice (phase 2)
    fpcNm_B_ZANT_e,   // 0x0F9 Zant
    fpcNm_B_ZANTS_e,  // Zant Goron Mines phase
    fpcNm_B_ZANTZ_e,  // Zant mobile helmet
    fpcNm_E_FM_e,     // 0x1D7 Fyrus (e_fm, not b_gm)
    fpcNm_E_HZELDA_e, // 0x1CB Possessed Zelda (Hyrule Castle)
};

// ---------------------------------------------------------------------------
// Final boss list — Hyrule Castle / Hyrule Field finale.
// ---------------------------------------------------------------------------
static const s16 sFinalBoss[] = {
    fpcNm_B_GND_e, // 0x20E Ganondorf (horseback)
    fpcNm_B_MGN_e, // 0x216 Beast Ganon
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static bool inList(const s16* list, std::size_t count, s16 name) {
    for (std::size_t i = 0; i < count; ++i) {
        if (list[i] == name) {
            return true;
        }
    }
    return false;
}

#define IN(arr, name) inList(arr, std::size(arr), name)

static void markTrueHpProcessed(fpc_ProcID procId) {
    s_trueHpApplied.insert(procId);
}

static bool isTrueHpProcessed(fpc_ProcID procId) {
    return s_trueHpApplied.find(procId) != s_trueHpApplied.end();
}

static s16 scaleHpField(s16 value, int mult) {
    if (value <= 1 || mult <= 1) {
        return value;
    }
    const int scaled = static_cast<int>(value) * mult;
    return static_cast<s16>(std::min(scaled, 32767));
}

dAlbwHP_Category dAlbwHP_getCategory(s16 profName) {
    if (IN(sFinalBoss, profName)) {
        return dAlbwHP_FINAL;
    }
    if (IN(sBoss, profName)) {
        return dAlbwHP_BOSS;
    }
    if (IN(sMidBoss, profName)) {
        return dAlbwHP_MID_BOSS;
    }
    return dAlbwHP_NORMAL;
}

static int getCategoryMult(dAlbwHP_Category cat) {
    if (cat == dAlbwHP_EXCLUDED) {
        return 1;
    }

    const auto& g = dusk::getSettings().game;
    switch (cat) {
    case dAlbwHP_NORMAL:
        return g.hpMultNormal.getValue();
    case dAlbwHP_MID_BOSS:
        return g.hpMultMidBoss.getValue();
    case dAlbwHP_BOSS:
        return g.hpMultBoss.getValue();
    case dAlbwHP_FINAL:
        return g.hpMultFinalBoss.getValue();
    default:
        return 1;
    }
}

int dAlbwHP_getTrueHpMult(s16 profName) {
    const int mult = getCategoryMult(dAlbwHP_getCategory(profName));
    return mult > 1 ? mult : 1;
}

s16 dAlbwHP_scaleHpValue(s16 profName, s16 hp) {
    return scaleHpField(hp, dAlbwHP_getTrueHpMult(profName));
}

dAlbwHP_LockonDisplay dAlbwHP_getLockonDisplayHp(fopAc_ac_c* actor) {
    dAlbwHP_LockonDisplay display = {
        .current = 0,
        .max = 0,
        .customMeter = false,
        .darknutPhase = dAlbwHP_Darknut_NONE,
        .actorHealth = 0,
        .actorHealthMax = 0,
    };

    if (actor == NULL) {
        return display;
    }

    display.current = actor->health;
    display.max = actor->field_0x560 > 0 ? actor->field_0x560 : actor->health;
    display.actorHealth = display.current;
    display.actorHealthMax = display.max;

    if (fopAcM_GetName(actor) == fpcNm_B_GM_e) {
        dAlbwBoss_armogohmaFillDisplayHp(actor, &display.current, &display.max);
        display.actorHealth = display.current;
        display.actorHealthMax = display.max;
    }

    if (fopAcM_GetName(actor) == fpcNm_B_TN_e) {
        const daB_TN_c* darknut = (const daB_TN_c*)actor;
        const int armorTotal = darknut->albwArmorTotal();

        if (darknut->albwUsesInternalDamageMeter()) {
            const int maxDamage = darknut->albwInternalDamageMax();
            const int taken = darknut->albwInternalDamageTaken();
            const int remaining = maxDamage - taken;
            display.darknutPhase = dAlbwHP_Darknut_UNARMORED;
            display.max = (s16)std::min(maxDamage, 32767);
            display.current = (s16)std::max(0, std::min(remaining, 32767));
            display.customMeter = true;
        } else if (darknut->albwIsTransitionPhase()) {
            display.darknutPhase = dAlbwHP_Darknut_TRANSITION;
            display.max = (s16)armorTotal;
            display.current = 0;
            display.customMeter = true;
        } else if (darknut->albwIsArmoredPhase()) {
            const int armorRemaining = darknut->albwArmorRemaining();
            display.darknutPhase = dAlbwHP_Darknut_ARMORED;
            display.max = (s16)armorTotal;
            display.current = (s16)std::max(0, std::min(armorRemaining, 32767));
            display.customMeter = true;
        }
    }

    return display;
}

void dAlbwHP_tryApplyTrueMaxHp(fopAc_ac_c* actor) {
    if (actor == NULL || !fopAcM_IsActor(actor)) {
        return;
    }

    const fpc_ProcID procId = fpcM_GetID(actor);
    if (procId == fpcM_ERROR_PROCESS_ID_e || !fpcM_IsExecuting(procId)) {
        return;
    }
    if (isTrueHpProcessed(procId)) {
        return;
    }

    if (fopAcM_GetGroup(actor) != fopAc_ENEMY_e) {
        markTrueHpProcessed(procId);
        return;
    }

    const s16 profName = fopAcM_GetName(actor);
    if (dAlbwHP_getCategory(profName) == dAlbwHP_EXCLUDED) {
        markTrueHpProcessed(procId);
        return;
    }

    if (actor->health <= 0) {
        return;
    }

    if (actor->health <= 1) {
        markTrueHpProcessed(procId);
        return;
    }

    const int mult = dAlbwHP_getTrueHpMult(profName);
    if (mult <= 1) {
        markTrueHpProcessed(procId);
        return;
    }

    actor->health = scaleHpField(actor->health, mult);
    if (actor->field_0x560 > 1) {
        actor->field_0x560 = scaleHpField(actor->field_0x560, mult);
    } else if (actor->field_0x560 > 0) {
        actor->field_0x560 = actor->health;
    }

    markTrueHpProcessed(procId);
}

void dAlbwHP_onActorDelete(fpc_ProcID procId) {
    s_trueHpApplied.erase(procId);
}

static int getLinkDamageDecreaseMult() {
    const int mult = dusk::getSettings().game.linkDamageDecreaseMult.getValue();
    return mult > 1 ? mult : 1;
}

int dAlbwHP_applyMult(s16 profName, int attackPower) {
    (void)profName;

    if (attackPower <= 0) {
        return attackPower;
    }

    const int mult = getLinkDamageDecreaseMult();
    if (mult <= 1) {
        return attackPower;
    }
    return std::max(1, attackPower / mult);
}

int dAlbwHP_getRawMult(s16 profName) {
    (void)profName;
    return getLinkDamageDecreaseMult();
}

u16 dAlbwHP_applyDurabilityMult(s16 profName, u16 damage) {
    if (damage == 0) {
        return 0;
    }

    switch (dAlbwHP_getCategory(profName)) {
    case dAlbwHP_MID_BOSS:
        return (u16)((damage * 150U + 99U) / 100U);
    case dAlbwHP_BOSS:
    case dAlbwHP_FINAL:
        return (u16)(damage * 2U);
    default:
        return damage;
    }
}

#undef IN

// ============================================
// NEW CODE ENDS HERE
// ============================================

#endif // TARGET_PC

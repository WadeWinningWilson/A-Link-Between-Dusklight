/**
 * d_albw_hp_mult.cpp
 * ALBW Port — Enemy HP multiplier: category tables and intercept logic.
 *
 * Each enemy actor profile name is mapped to one of four categories.
 * The corresponding Dusk settings slider (1–16×) divides the incoming
 * attack power before it is subtracted from the enemy's health field,
 * making the enemy effectively harder to kill without touching HP values
 * directly. The max(1,...) floor ensures no hit ever deals 0 damage.
 *
 * Actor IDs follow the game actor list (Proc Name in d_a_*.cpp).
 * In-game names verified against actor-list spreadsheet + @brief headers.
 */

#if TARGET_PC

#include "d/d_albw_hp_mult.h"
#include "dusk/settings.h"
#include "f_pc/f_pc_name.h"
#include <algorithm>
#include <cstddef>

// ============================================
// NEW CODE — ALBW Port
// ============================================

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
// Bespoke parry/durability rules for FINAL may come later; 2× durability now.
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

int dAlbwHP_applyMult(s16 profName, int attackPower) {
    if (attackPower <= 0) {
        return attackPower;
    }

    dAlbwHP_Category cat = dAlbwHP_getCategory(profName);
    if (cat == dAlbwHP_EXCLUDED) {
        return attackPower;
    }

    int mult = 1;
    const auto& g = dusk::getSettings().game;
    switch (cat) {
    case dAlbwHP_NORMAL:
        mult = g.hpMultNormal.getValue();
        break;
    case dAlbwHP_MID_BOSS:
        mult = g.hpMultMidBoss.getValue();
        break;
    case dAlbwHP_BOSS:
        mult = g.hpMultBoss.getValue();
        break;
    case dAlbwHP_FINAL:
        mult = g.hpMultFinalBoss.getValue();
        break;
    default:
        break;
    }

    if (mult <= 1) {
        return attackPower;
    }
    return std::max(1, attackPower / mult);
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

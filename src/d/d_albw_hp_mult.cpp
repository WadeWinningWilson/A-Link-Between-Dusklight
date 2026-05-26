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
 * To exclude a misbehaving actor: add its fpcNm_*_e to sExcluded below
 * and rebuild. It will then pass through this function unchanged.
 *
 * Actor–boss mappings confirmed from "/* Proc Name *\/" entries in the
 * respective d_a_b_*.cpp / d_a_e_*.cpp actor source files.
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
// Exclude list — actors that bypass the multiplier entirely.
// Add fpcNm_*_e constants here for any actor that breaks under HP scaling.
// ---------------------------------------------------------------------------
static const s16 sExcluded[] = {
    fpcNm_B_GND_e,   // Zant's Hand — light-arrow-immune; category TBD after testing
};

// ---------------------------------------------------------------------------
// Mid-boss list — sub-bosses encountered between dungeons or as gatekeepers.
// Note: Aeralfos and Darknut also appear as regular enemies; they are NOT
// listed here so the normal-enemy slider governs all encounters of those
// actor types. Move to sMidBoss if you want the mid-boss slider to apply.
// ---------------------------------------------------------------------------
static const s16 sMidBoss[] = {
    fpcNm_B_DS_e,    // Death Sword           (Arbiter's Grounds)
    fpcNm_B_ZANTM_e, // Phantom Zant          (Palace of Twilight)
    fpcNm_E_MK_e,    // Ook                   (Forest Temple)
    fpcNm_E_RDB_e,   // King Bulblin / Bullbo  (Bridge of Eldin, Gerudo Desert)
    fpcNm_E_GOB_e,   // Dangoro               (Goron Mines)
    fpcNm_E_YC_e,    // Twilit Carrier Kargarok (Lake Hylia)
    fpcNm_E_SG_e,    // Skull Kid             (Sacred Grove)
    fpcNm_E_DT_e,    // Deku Toad             (Lakebed Temple)
    fpcNm_E_BS_e,    // Twilit Bloat          (Lake Hylia)
};

// ---------------------------------------------------------------------------
// Dungeon boss list.
// Sub-actors (tentacles, spiderlings, icicles, rider components) are included
// so the entire boss encounter scales uniformly.
// ---------------------------------------------------------------------------
static const s16 sBoss[] = {
    fpcNm_B_BQ_e,    // Twilit Parasite: Diababa     (Forest Temple)
    fpcNm_B_BH_e,    // Diababa baba tentacles
    fpcNm_B_GM_e,    // Twilit Igniter: Fyrus         (Goron Mines)
    fpcNm_B_TN_e,    // Twilit Aquatic: Morpheel      (Lakebed Temple)
    fpcNm_B_GG_e,    // Twilit Fossil: Stallord       (Arbiter's Grounds)
    fpcNm_B_YO_e,    // Twilit Ice Mass: Blizzeta     (Snowpeak Ruins)
    fpcNm_B_YOI_e,   // Blizzeta icicles
    fpcNm_B_OB_e,    // Twilit Arachnid: Armogohma   (Temple of Time)
    fpcNm_B_OH_e,    // Armogohma spiderlings
    fpcNm_B_OH2_e,   // Armogohma eye (phase 2)
    fpcNm_B_DR_e,    // Twilit Dragon: Argorok        (City in the Sky)
    fpcNm_B_DRE_e,   // Argorok rider component
    fpcNm_B_ZANT_e,  // Usurper King: Zant            (Palace of Twilight)
    fpcNm_B_ZANTZ_e, // Zant battle mobile form
};

// ---------------------------------------------------------------------------
// Final boss list — Hyrule Castle / Hyrule Field sequence.
// ---------------------------------------------------------------------------
static const s16 sFinalBoss[] = {
    fpcNm_B_GO_e,    // Dark Lord: Ganondorf          (Hyrule Castle rooftop)
    fpcNm_B_GOS_e,   // Ganondorf sub-actor           (energy sphere etc.)
    fpcNm_B_MGN_e,   // Dark Beast: Ganon             (Hyrule Field)
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static bool inList(const s16* list, std::size_t count, s16 name) {
    for (std::size_t i = 0; i < count; ++i) {
        if (list[i] == name) return true;
    }
    return false;
}

#define IN(arr, name) inList(arr, std::size(arr), name)

dAlbwHP_Category dAlbwHP_getCategory(s16 profName) {
    if (IN(sExcluded,  profName)) return dAlbwHP_EXCLUDED;
    if (IN(sFinalBoss, profName)) return dAlbwHP_FINAL;
    if (IN(sBoss,      profName)) return dAlbwHP_BOSS;
    if (IN(sMidBoss,   profName)) return dAlbwHP_MID_BOSS;
    return dAlbwHP_NORMAL;
}

int dAlbwHP_applyMult(s16 profName, int attackPower) {
    if (attackPower <= 0) return attackPower;

    dAlbwHP_Category cat = dAlbwHP_getCategory(profName);
    if (cat == dAlbwHP_EXCLUDED) return attackPower;

    int mult = 1;
    const auto& g = dusk::getSettings().game;
    switch (cat) {
    case dAlbwHP_NORMAL:   mult = g.hpMultNormal.getValue();   break;
    case dAlbwHP_MID_BOSS: mult = g.hpMultMidBoss.getValue();  break;
    case dAlbwHP_BOSS:     mult = g.hpMultBoss.getValue();     break;
    case dAlbwHP_FINAL:    mult = g.hpMultFinalBoss.getValue(); break;
    default:               break;
    }

    if (mult <= 1) return attackPower;  // 1× = off, no change
    return std::max(1, attackPower / mult);
}

#undef IN

// ============================================
// NEW CODE ENDS HERE
// ============================================

#endif // TARGET_PC

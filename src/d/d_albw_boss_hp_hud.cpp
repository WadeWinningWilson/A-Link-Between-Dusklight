/**
 * d_albw_boss_hp_hud.cpp
 * ALBW Port — Boss health bar HUD.
 *
 * Bottom-centre red health bar with the boss name above it (matching the
 * approved reference). HP comes ONLY from dAlbwHP_getLockonDisplayHp(), the
 * single source of truth shared with the lock-on debug overlay — so the bar
 * automatically reflects the Boss HP scaler and Boss Refinement, and (since
 * Link Damage only divides attack power, never the HP pool) ignores Link
 * Damage for its scale. No HP/scaling math is duplicated here.
 *
 * Bar is built from the resident solid-block texture (tt_block8x8, msgres05)
 * drawn as three tinted rects (border / empty track / red fill); the name is
 * a J2DTextBox in the message font with a black outline.
 *
 * Prototype scope: Armogohma — phase 1 body (B_GM) then phase 2 core (E_GM TYPE_GOMA).
 * Visibility/query: dAlbwBoss_armogohmaQueryHealthBar().
 */

#if TARGET_PC

#include "d/d_albw_boss_hp_hud.h"
#include "d/d_albw_boss.h"
#include "d/d_albw_hp_mult.h"
#include "d/d_com_inf_game.h"
#include "dusk/settings.h"
#include "dusk/frame_interpolation.h"
#include "f_op/f_op_actor.h"
#include <cstdlib>
#include <cstring>
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_graphic.h"
#include "JSystem/J2DGraph/J2DGrafContext.h"
#include "JSystem/J2DGraph/J2DOrthoGraph.h"
#include "JSystem/J2DGraph/J2DPicture.h"
#include "JSystem/J2DGraph/J2DTextBox.h"
#include "JSystem/JUtility/TColor.h"
#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRHeap.h"

// ============================================
// NEW CODE — ALBW Port
// ============================================

// Bar geometry as fractions of the 2D ortho extent. Tunable.
// Phase 1 values per docs/albw-boss-hp-hud-tuning-brief.md (LoP-style: thin,
// wide, low, with a small attached name).
static constexpr f32 kBarWidthFrac  = 0.58f;   // bar width / screen width
static constexpr f32 kBarHeightFrac = 0.016f;  // bar height / screen height (thinner)
static constexpr f32 kBarTopFrac    = 0.908f;  // bar top Y / screen height (sit lower)
static constexpr f32 kBarBorder     = 1.0f;    // border thickness (ortho px)
static constexpr f32 kNameFontFrac  = 0.028f;  // name font height / screen height (~30px @1080p)
static constexpr f32 kNameGap       = 3.0f;    // gap between name and bar (attached, fixed px)
static constexpr f32 kNameOutline   = 1.0f;    // name outline offset (thinner stroke)

// Solid-block texture (resident in msgres05.arc) reused for every rect.
static const char* BLOCK_BTI = "tt_block8x8.bti";

static J2DPicture* sBlock = NULL;
static J2DTextBox* sName = NULL;

// ---------------------------------------------------------------------------
// Boss name for a profile (prototype: Armogohma only).
// ---------------------------------------------------------------------------
static const char* bossDisplayName(s16 profName) {
    switch (profName) {
    case fpcNm_B_GM_e:
        return "Armogohma";
    default:
        return NULL;
    }
}

static bool ensureResources() {
    if (sBlock == NULL) {
        JKRArchive* arc = dComIfGp_getMsgArchive(5);
        if (arc == NULL) {
            return false;
        }
        ResTIMG* timg = static_cast<ResTIMG*>(arc->getResource('TIMG', BLOCK_BTI));
        if (timg == NULL) {
            return false;
        }
        sBlock = JKR_NEW J2DPicture(timg);
        if (sBlock == NULL) {
            return false;
        }
    }
    if (sName == NULL) {
        sName = JKR_NEW J2DTextBox();
        if (sName == NULL) {
            return false;
        }
        sName->setFont(mDoExt_getMesgFont());
    }
    return true;
}

// Draw a tinted rectangle using the white block texture. Alpha lets the empty
// track be translucent (LoP "see the floor through the empty bar").
static void drawRect(f32 x, f32 y, f32 w, f32 h, u8 r, u8 g, u8 b, u8 a = 255) {
    if (w <= 0.0f || h <= 0.0f) {
        return;
    }
    sBlock->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(r, g, b, a));
    sBlock->setAlpha(a);
    sBlock->draw(x, y, w, h, false, false, false);
}

// ---------------------------------------------------------------------------
// dAlbwBossHpHud_draw — public draw tick.
// ---------------------------------------------------------------------------
void dAlbwBossHpHud_draw() {
    if (!dusk::getSettings().game.bossHealthBars.getValue()) {
        return;
    }

    // Prototype: Armogohma only (phase 1 body + phase 2 core).
    dAlbwBoss_ArmogohmaBarState bar = {};
    if (!dAlbwBoss_armogohmaQueryHealthBar(&bar) || !bar.visible || bar.max <= 0 ||
        bar.current <= 0)
    {
        return;
    }

    const dAlbwHP_LockonDisplay hp = {
        .current = bar.current,
        .max = bar.max,
        .customMeter = bar.phase == 2,
        .darknutPhase = dAlbwHP_Darknut_NONE,
        .actorHealth = bar.current,
        .actorHealthMax = bar.max,
    };

    const char* name = bossDisplayName(fpcNm_B_GM_e);
    if (name == NULL || !ensureResources()) {
        return;
    }

    J2DGrafContext* gfx = dComIfGp_getCurrentGrafPort();
    if (gfx == NULL) {
        return;
    }

    // Match the footer-text pattern: explicit widescreen ortho so the bar sits
    // bottom-centre at any aspect ratio.
#ifdef TARGET_PC
    dusk::frame_interp::set_ui_tick_pending(true);
#endif
    const f32 minX = mDoGph_gInf_c::getMinXF();
    const f32 minY = mDoGph_gInf_c::getMinYF();
    const f32 scrW = mDoGph_gInf_c::getWidthF();
    const f32 scrH = mDoGph_gInf_c::getHeightF();

    J2DOrthoGraph* ortho = static_cast<J2DOrthoGraph*>(gfx);
    ortho->setOrtho(minX, minY, scrW, scrH, -1.0f, 1.0f);
    ortho->setup2D();
    ortho->setPort();

    const f32 barW = scrW * kBarWidthFrac;
    const f32 barH = scrH * kBarHeightFrac;
    const f32 barX = minX + (scrW - barW) * 0.5f;
    const f32 barY = minY + scrH * kBarTopFrac;

    f32 ratio = (f32)hp.current / (f32)hp.max;
    if (ratio < 0.0f) {
        ratio = 0.0f;
    } else if (ratio > 1.0f) {
        ratio = 1.0f;
    }

    // Border (near-black 1px frame), empty track (translucent charcoal — lost
    // health reads black/empty with the floor faintly visible), deep crimson fill.
    drawRect(barX - kBarBorder, barY - kBarBorder, barW + kBarBorder * 2.0f,
             barH + kBarBorder * 2.0f, 32, 32, 36, 255);
    drawRect(barX, barY, barW, barH, 18, 18, 22, 168);
    drawRect(barX, barY, barW * ratio, barH, 176, 8, 8, 255);

    // Boss name, centred above the bar, white with a black outline.
    const f32 fontSz = scrH * kNameFontFrac;
    const f32 nameY = barY - kNameGap - fontSz;
    sName->setFontSize(fontSz, fontSz);
    sName->setString(name);

    sName->setCharColor(0x000000FF);
    sName->setGradColor(0x000000FF);
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            if (dx == 0 && dy == 0) {
                continue;
            }
            sName->draw(barX + dx * kNameOutline, nameY + dy * kNameOutline, barW, HBIND_CENTER);
        }
    }
    sName->setCharColor(0xFFFFFFFF);
    sName->setGradColor(0xFFFFFFFF);
    sName->draw(barX, nameY, barW, HBIND_CENTER);
}

// ============================================
// NEW CODE ENDS HERE
// ============================================

#endif // TARGET_PC

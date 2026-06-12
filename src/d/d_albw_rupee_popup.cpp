/**
 * d_albw_rupee_popup.cpp
 * ALBW Port — Enemy Death Rupees "+n" HUD popup.
 *
 * Renders "+n" immediately to the right of the green rupee icon on the
 * rupee counter HUD, using the counter's own art: the metal digit
 * textures (im_font_number_32_32_ganshinkyo_N_02.bti) and the matching
 * '+' texture (dMeter2Info_getPlusTextureName()), all from the
 * boot-resident main2D archive.  Glyphs are drawn at the exact on-screen
 * size and advance of the live rupee digits (getRupeeDigitMetrics), so
 * the popup is indistinguishable from the counter's own typography.
 * Same approach as the STAR minigame "+n" score popup (d_a_balloon_2D).
 *
 * Lifecycle: a grant arms a 120-frame window; a new grant replaces the
 * displayed amount and restarts the window (the HUD mirrors exactly what
 * grantRupees() was last called with).  Fades out over the final 15
 * frames, multiplied by the rupee HUD's own alpha rate so it hides with
 * the counter in cutscenes.  Pause/heap-lock hiding comes from the
 * dMeter2Draw_c::draw() call site, shared with the other ALBW overlays.
 */

#if TARGET_PC

#include "d/d_albw_rupee_popup.h"
#include "d/d_albw_enemy_rupee.h"
#include "d/dolzel.h"
#include "d/d_com_inf_game.h"
#include "d/d_meter2.h"
#include "d/d_meter2_info.h"
#include "d/d_meter2_draw.h"
#include "d/d_meter_HIO.h"
#include "JSystem/J2DGraph/J2DGrafContext.h"
#include "JSystem/J2DGraph/J2DPicture.h"
#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "res/Layout/main2D.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ============================================
// NEW CODE — ALBW Port
// ============================================

// ---------------------------------------------------------------------------
// File-based debug log (OS_REPORT is compiled out in non-DEBUG builds).
// Written to Documents/dusklight/albw_rupee_debug.txt, truncated once per
// session.  Same pattern as the death-recovery-orb log.
// ---------------------------------------------------------------------------
static void rupeeDebugLog(const char* fmt, ...) {
    static bool sResetDone = false;

    char path[512];
    path[0] = '\0';
    const char* user = getenv("USERPROFILE");
    if (user && user[0] != '\0') {
        snprintf(path, sizeof(path), "%s/Documents/dusklight/albw_rupee_debug.txt", user);
    } else {
        strncpy(path, "albw_rupee_debug.txt", sizeof(path) - 1);
    }

    FILE* fp = fopen(path, sResetDone ? "a" : "w");
    if (!fp) {
        fp = fopen("albw_rupee_debug.txt", sResetDone ? "a" : "w");
    }
    if (!fp) {
        return;
    }
    if (!sResetDone) {
        sResetDone = true;
        fprintf(fp, "--- ALBW enemy rupee popup ---\n");
    }

    va_list args;
    va_start(args, fmt);
    vfprintf(fp, fmt, args);
    va_end(args);
    fclose(fp);
}

// Display window per grant and fade-out tail, in frames.
static constexpr s16 POPUP_FRAMES = 120;
static constexpr s16 POPUP_FADE_FRAMES = 15;
// Gap between the popup's last glyph and the counter's leftmost digit
// slot, in digit advances (popup sits to the LEFT of the counter).
static constexpr f32 POPUP_COUNTER_GAP = 1.6f;
// The '+' texture is 24x24 against 32x32 digits; draw it slightly
// smaller than a digit cell so the optical weight matches.
static constexpr f32 POPUP_PLUS_RATIO = 0.8f;
// Largest grant is 500 — three digits plus the sign.
static constexpr int POPUP_MAX_DIGITS = 4;

// Lazily-created pictures, owned for the session (balloon_2D pattern).
static J2DPicture* sPlusPic = NULL;
static J2DPicture* sDigitPic[10] = {NULL};

// Last granted amount and frames remaining in the display window.
static u16 sAmount = 0;
static s16 sFramesLeft = 0;
// One-shot log flag per arm so the log shows the first draw/skip outcome.
static bool sLoggedThisArm = false;

// ---------------------------------------------------------------------------
// dAlbwEnemyRupeesHud_onGrant — arm/replace the popup.
// ---------------------------------------------------------------------------
void dAlbwEnemyRupeesHud_onGrant(u16 i_amount) {
    if (i_amount == 0) {
        return;
    }
    sAmount = i_amount;
    sFramesLeft = POPUP_FRAMES;
    sLoggedThisArm = false;
    rupeeDebugLog("armed +%u\n", (unsigned)i_amount);
}

// ---------------------------------------------------------------------------
// ensurePics — create the '+' and digit pictures on first use.
// ---------------------------------------------------------------------------
static bool ensurePics() {
    if (sPlusPic != NULL) {
        return true;
    }

    JKRArchive* arc = dComIfGp_getMain2DArchive();
    if (arc == NULL) {
        rupeeDebugLog("ensurePics: main2D archive NULL\n");
        return false;
    }

    for (int i = 0; i < 10; i++) {
        if (sDigitPic[i] != NULL) {
            continue;
        }
        ResTIMG* timg = static_cast<ResTIMG*>(
            arc->getResource('TIMG', dMeter2Info_getNumberTextureName(i)));
        if (timg == NULL) {
            rupeeDebugLog("ensurePics: digit %d TIMG missing\n", i);
            return false;
        }
        sDigitPic[i] = JKR_NEW J2DPicture(timg);
        if (sDigitPic[i] == NULL) {
            return false;
        }
    }

    // The '+' filename contains a full-width 'x' (im_plus_metal_24x24) whose
    // byte encoding in this source tree does not match the archive's string
    // table, so the by-name lookup fails on PC.  Fall back to the archive
    // file index from the generated asset header, which is encoding-proof.
    ResTIMG* plusTimg =
        static_cast<ResTIMG*>(arc->getResource('TIMG', dMeter2Info_getPlusTextureName()));
    if (plusTimg == NULL) {
        plusTimg = static_cast<ResTIMG*>(
            arc->getIdxResource(dRes_INDEX_MAIN2D_BTI_IM_PLUS_METAL_24X24_00_e));
        rupeeDebugLog("ensurePics: plus by-name failed, by-index %s\n",
                      plusTimg != NULL ? "OK" : "FAILED");
    }
    if (plusTimg == NULL) {
        return false;
    }

    sPlusPic = JKR_NEW J2DPicture(plusTimg);
    return sPlusPic != NULL;
}

// ---------------------------------------------------------------------------
// dAlbwRupeePopup_draw — per-frame draw tick.
// ---------------------------------------------------------------------------
void dAlbwRupeePopup_draw() {
    if (sFramesLeft <= 0) {
        return;
    }

    if (!dAlbwEnemyRupees_isEnabled()) {
        sFramesLeft = 0;
        return;
    }

    // Pre-draw checks come BEFORE the timer decrement so a frame where the
    // popup cannot render (resources unavailable, HUD faded out) does not
    // silently burn the display window.
    if (!ensurePics()) {
        if (!sLoggedThisArm) {
            sLoggedThisArm = true;
            rupeeDebugLog("skip: ensurePics FAILED (main2D TIMG load)\n");
        }
        return;
    }

    dMeter2_c* meter = dMeter2Info_getMeterClass();
    dMeter2Draw_c* meterDraw = (meter != NULL) ? meter->getMeterDrawPtr() : NULL;
    if (meterDraw == NULL) {
        if (!sLoggedThisArm) {
            sLoggedThisArm = true;
            rupeeDebugLog("skip: meterDraw NULL\n");
        }
        return;
    }

    // ---- Anchor: the green rupee icon (HIO fallback like the spur row) ---
    Vec iconCenter;
    f32 iconSize;
    if (meterDraw->getRupeeAnchorCenter(&iconCenter)) {
        iconSize = meterDraw->getRupeeHudReferenceSize();
    } else {
        iconCenter.x = g_drawHIO.mRupeePosX;
        iconCenter.y = g_drawHIO.mRupeePosY;
        iconCenter.z = 0.0f;
        iconSize = 0.0f;
    }
    if (iconSize <= 0.0f) {
        iconSize = g_drawHIO.mRupeeScale * 24.0f;
    }

    // ---- Glyph metrics: exact size/advance of the live counter digits ----
    f32 digitW, digitH, advance, digitLeftX, digitRowY;
    if (!meterDraw->getRupeeDigitMetrics(&digitW, &digitH, &advance, &digitLeftX, &digitRowY)) {
        // Fallback proportions if panes are unavailable this frame.
        digitW = digitH = iconSize * 0.8f;
        advance = digitW * 0.6f;
        digitLeftX = iconCenter.x - advance * 5.0f;
        digitRowY = iconCenter.y;
    }

    // Follow the rupee counter's own fade — hold (don't burn) the window
    // while the HUD itself is hidden, e.g. during cutscenes.
    f32 hudAlphaRate = meterDraw->getRupeeHudAlphaRate();
    if (hudAlphaRate <= 0.0f) {
        if (!sLoggedThisArm) {
            sLoggedThisArm = true;
            rupeeDebugLog("skip: rupee HUD alpha %.3f (HUD hidden)\n", hudAlphaRate);
        }
        return;
    }
    if (hudAlphaRate > 1.0f) {
        hudAlphaRate = 1.0f;
    }

    // Committed to drawing this frame — consume the window.
    sFramesLeft--;

    // ---- Alpha: full, then fade over the final POPUP_FADE_FRAMES ---------
    f32 fade = 1.0f;
    if (sFramesLeft < POPUP_FADE_FRAMES) {
        fade = (f32)sFramesLeft / (f32)POPUP_FADE_FRAMES;
    }
    const u8 alpha = (u8)(255.0f * fade * hudAlphaRate);
    if (alpha == 0) {
        return;
    }

    // ---- GX state: raw J2DPicture draws need a fresh 2D setup ------------
    // The setup2D() at the top of dMeter2Draw_c::draw() is stale by this
    // point (screen/pikari draws in between), and the wolf/bash overlays
    // only run their own setup2D() conditionally.
    J2DGrafContext* grafCtx = dComIfGp_getCurrentGrafPort();
    if (grafCtx == NULL) {
        return;
    }
    grafCtx->setup2D();

    // ---- Decompose the amount into digits (most significant first) -------
    int digits[POPUP_MAX_DIGITS];
    int digitCount = 0;
    {
        u16 value = sAmount;
        do {
            digits[digitCount++] = value % 10;
            value /= 10;
        } while (value != 0 && digitCount < POPUP_MAX_DIGITS);
    }

    // ---- Layout: to the LEFT of the counter, right-aligned against the
    // leftmost digit slot so the popup never overlaps digits or icon.
    // Glyph count = '+' plus the amount's digits; the last glyph centre
    // lands POPUP_COUNTER_GAP advances left of the leftmost counter slot.
    const int glyphCount = 1 + digitCount;
    f32 posX = digitLeftX - advance * (POPUP_COUNTER_GAP + (f32)(glyphCount - 1));
    const f32 centerY = digitRowY;
    const f32 plusSize = digitH * POPUP_PLUS_RATIO;

    if (!sLoggedThisArm) {
        sLoggedThisArm = true;
        rupeeDebugLog("draw ok: +%u at (%.1f, %.1f) digitLeftX=%.1f digit=%.1fx%.1f adv=%.1f alpha=%u\n",
                      (unsigned)sAmount, posX, centerY, digitLeftX, digitW, digitH, advance,
                      (unsigned)alpha);
    }

    // ---- Draw -------------------------------------------------------------
    sPlusPic->setAlpha(alpha);
    sPlusPic->draw(posX - plusSize * 0.5f, centerY - plusSize * 0.5f,
                   plusSize, plusSize, false, false, false);
    posX += advance;

    for (int i = digitCount - 1; i >= 0; i--) {
        J2DPicture* pic = sDigitPic[digits[i]];
        pic->setAlpha(alpha);
        pic->draw(posX - digitW * 0.5f, centerY - digitH * 0.5f,
                  digitW, digitH, false, false, false);
        posX += advance;
    }
}

// ============================================
// NEW CODE ENDS HERE
// ============================================

#endif // TARGET_PC

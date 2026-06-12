/**
 * d_albw_wolf_charge_hud.cpp
 * ALBW Port — Wolf Link charge counter HUD.
 *
 * Draws up to two wolf silhouette icons (tt_wolf_icon_mini64.bti from the
 * always-resident msgres05.arc) in the same rupee-anchored screen row as
 * the shield-parry spur icons, replacing them for the duration of wolf form.
 *
 * Visibility mirrors updateShieldHudLinger() / shouldShowBashHud() in
 * d_albw_shield.cpp: holding the shield button (or any charge event —
 * gain, spend, deny) refreshes a 120-frame linger window; the icons hide
 * once it runs out.  The Midna-talk button does not summon the HUD.
 * Cutscene/pause hiding comes from the dMeter2Draw_c::draw() call site,
 * which already gates on !isPauseFlag && heapLock != 6 — the same setup
 * the spur icons use.
 *
 * Filled icons get a red halo (a slightly larger tinted under-draw of the
 * same silhouette) in the Midna charge attack energy color.  A denied
 * charge attack plays the same white pikari flash the spur icons use,
 * with identical HIO-driven colors and animation speed.
 *
 * Positioning mirrors computeBashHudLayout() / dShield_drawBashCharges():
 *   posY = rupeeCenter.y - rupeeSize * 1.12f   (same row)
 *   spacing = rupeeSize * 1.08f                (same cadence as spur row)
 */

#if TARGET_PC

#include "d/d_albw_wolf_charge_hud.h"
#include "d/d_albw_wolf_combat.h"
#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"
#include "d/d_meter2.h"
#include "d/d_meter2_info.h"
#include "d/d_meter2_draw.h"
#include "d/d_meter_HIO.h"
#include "JSystem/J2DGraph/J2DGrafContext.h"
#include "JSystem/J2DGraph/J2DPicture.h"
#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRHeap.h"

// ============================================
// NEW CODE — ALBW Port
// ============================================

// BTI texture name inside msgres05.arc (archive index 5, always resident).
static const char* WOLF_ICON_BTI = "tt_wolf_icon_mini64.bti";
// Native size of the texture; used for iconScale calculation.
static constexpr f32 WOLF_ICON_NATIVE_SIZE = 64.0f;
// Maximum charges displayed (matches mWolfChargeCount cap).
static constexpr u8 WOLF_CHARGE_MAX = 2;
// Alpha for an empty (uncharged) slot — ~30 % opacity.
static constexpr u8 WOLF_ICON_ALPHA_EMPTY = 77;
// Alpha for a filled (charged) slot — fully opaque.
static constexpr u8 WOLF_ICON_ALPHA_FULL = 255;
// Visibility window after a summon/charge event (same as shield HUD).
static constexpr u16 WOLF_HUD_LINGER_FRAMES = 120;
// Deny flash duration (same as bash deny).
static constexpr u8 WOLF_DENY_FLASH_FRAMES = 12;
// Halo under-draw: scale relative to the icon, and Midna energy red tint.
static constexpr f32 WOLF_HALO_SCALE = 1.15f;
static constexpr u8 WOLF_HALO_R = 255;
static constexpr u8 WOLF_HALO_G = 70;
static constexpr u8 WOLF_HALO_B = 55;
static constexpr u8 WOLF_HALO_ALPHA = 200;

// Lazily-initialised picture created from the BTI.  Owned for the session.
static J2DPicture* sWolfIcon = NULL;

// Visibility linger window (frames remaining).
static u16 sLingerFrames = 0;
// Deny flash window (frames remaining) and pikari animation frame.
static u8 sDenyFlashFrames = 0;
static f32 sDenyPikariFrame = 0.0f;

// ---------------------------------------------------------------------------
// ensureWolfIcon — load sWolfIcon on first use; returns false if unavailable.
// ---------------------------------------------------------------------------
static bool ensureWolfIcon() {
    if (sWolfIcon != NULL) {
        return true;
    }
    JKRArchive* arc = dComIfGp_getMsgArchive(5);
    if (arc == NULL) {
        return false;
    }
    ResTIMG* timg = static_cast<ResTIMG*>(arc->getResource('TIMG', WOLF_ICON_BTI));
    if (timg == NULL) {
        return false;
    }
    sWolfIcon = JKR_NEW J2DPicture(timg);
    return sWolfIcon != NULL;
}

// ---------------------------------------------------------------------------
// Linger / deny notifications (called from the charge economy code).
// ---------------------------------------------------------------------------
void dAlbwWolfChargeHud_notify() {
    sLingerFrames = WOLF_HUD_LINGER_FRAMES;
}

void dAlbwWolfChargeHud_notifyDeny() {
    sLingerFrames = WOLF_HUD_LINGER_FRAMES;
    sDenyFlashFrames = WOLF_DENY_FLASH_FRAMES;
    sDenyPikariFrame = 0.0f;
}

// ---------------------------------------------------------------------------
// tickDenyPikari — mirror of the spur deny flash animation driver.
// ---------------------------------------------------------------------------
static void tickWolfDenyPikari() {
    if (sDenyFlashFrames == 0) {
        sDenyPikariFrame = 0.0f;
        return;
    }
    sDenyFlashFrames--;

    if (sDenyPikariFrame <= 0.0f) {
        sDenyPikariFrame = 18.0f - g_drawHIO.mSpurIconPikariAnimSpeed;
    } else {
        sDenyPikariFrame += g_drawHIO.mSpurIconPikariAnimSpeed;
        if (sDenyPikariFrame > 28.0f) {
            sDenyPikariFrame = 0.0f;
        }
    }
}

// ---------------------------------------------------------------------------
// dAlbwWolfChargeHud_draw — public draw tick.
// ---------------------------------------------------------------------------
void dAlbwWolfChargeHud_draw() {
    // Gate: only during wolf form with the combat system enabled.
    // Reset the visibility state on the way out so a stale window does not
    // carry across a transform back to human form.
    if (!daPy_py_c::checkNowWolf() || !dAlbwWolfCombat_isEnabled()) {
        sLingerFrames = 0;
        sDenyFlashFrames = 0;
        sDenyPikariFrame = 0.0f;
        return;
    }

    daAlink_c* link = daAlink_getAlinkActorClass();

    // Shield button or a genuine enemy lock-on summons the HUD and holds
    // it open; otherwise the window decays.  LockonTruth() requires an
    // actual target actor, so a free Z-press (talk to Midna, wall-facing
    // Z-target) does not summon the icons.
    const bool enemyLockon = dComIfGp_getAttention()->LockonTruth();
    if (link != NULL && (link->manualShieldButton() || enemyLockon)) {
        sLingerFrames = WOLF_HUD_LINGER_FRAMES;
    } else if (sLingerFrames > 0) {
        sLingerFrames--;
    }

    tickWolfDenyPikari();

    if (sLingerFrames == 0) {
        return;
    }

    if (!ensureWolfIcon()) {
        return;
    }

    // ---- Rupee anchor (same source the spur row uses) --------------------
    Vec rupeeCenter;
    rupeeCenter.x = g_drawHIO.mRupeePosX;
    rupeeCenter.y = g_drawHIO.mRupeePosY;
    rupeeCenter.z = 0.0f;

    dMeter2_c* meter = dMeter2Info_getMeterClass();
    dMeter2Draw_c* meterDraw = NULL;
    if (meter != NULL) {
        meterDraw = meter->getMeterDrawPtr();
        if (meterDraw != NULL) {
            // Prefer shield anchor if present; fall back to rupee anchor.
            if (!meterDraw->getShieldHudAnchorCenter(&rupeeCenter)) {
                meterDraw->getRupeeAnchorCenter(&rupeeCenter);
            }
        }
    }

    // ---- Layout (mirrors computeBashHudLayout()) -------------------------
    f32 rupeeSize = g_drawHIO.mRupeeScale * 24.0f;  // fallback
    if (meterDraw != NULL) {
        const f32 ref = meterDraw->getRupeeHudReferenceSize();
        if (ref > 0.0f) {
            rupeeSize = ref;
        }
    }

    const f32 iconScale  = g_drawHIO.mSpurIconScale * (rupeeSize / WOLF_ICON_NATIVE_SIZE);
    const f32 iconSize   = WOLF_ICON_NATIVE_SIZE * iconScale;
    const f32 spacing    = rupeeSize * 1.08f;
    const f32 rowOffsetY = rupeeSize * 1.12f;

    const f32 totalWidth = spacing * (f32)(WOLF_CHARGE_MAX - 1);
    f32 posX             = rupeeCenter.x - totalWidth * 0.5f;
    const f32 posY       = rupeeCenter.y - rowOffsetY;
    const f32 halfIcon   = iconSize * 0.5f;
    const f32 haloSize   = iconSize * WOLF_HALO_SCALE;
    const f32 halfHalo   = haloSize * 0.5f;

    // ---- Charge state ----------------------------------------------------
    const u8 charges = (link != NULL) ? link->mWolfChargeCount : 0;

    // ---- Draw ------------------------------------------------------------
    J2DGrafContext* grafCtx = dComIfGp_getCurrentGrafPort();
    if (grafCtx == NULL) {
        return;
    }
    grafCtx->setup2D();

    const bool denyFlash = sDenyFlashFrames > 0 && sDenyPikariFrame > 0.0f;
    const f32 pikariScale = g_drawHIO.mSpurIconPikariScale *
                            (rupeeSize / WOLF_ICON_NATIVE_SIZE) * 0.85f;

    for (u8 i = 0; i < WOLF_CHARGE_MAX; i++) {
        const bool filled = i < charges;

        if (filled) {
            // Red halo under-draw: same silhouette, slightly larger, tinted
            // the Midna charge energy color.
            sWolfIcon->setAlpha(WOLF_HALO_ALPHA);
            sWolfIcon->setWhite(JUtility::TColor(WOLF_HALO_R, WOLF_HALO_G,
                                                 WOLF_HALO_B, 255));
            sWolfIcon->draw(posX - halfHalo, posY - halfHalo,
                            haloSize, haloSize, false, false, false);

            sWolfIcon->setAlpha(WOLF_ICON_ALPHA_FULL);
        } else {
            sWolfIcon->setAlpha(WOLF_ICON_ALPHA_EMPTY);
        }

        sWolfIcon->setWhite(JUtility::TColor(255, 255, 255, 255));
        sWolfIcon->draw(posX - halfIcon, posY - halfIcon,
                        iconSize, iconSize, false, false, false);

        // Deny flash: identical pikari sparkle to the spur deny, same
        // HIO colors and animation cadence.
        if (denyFlash && meterDraw != NULL) {
            meterDraw->drawPikariHakusha(
                posX, posY, sDenyPikariFrame, pikariScale,
                g_drawHIO.mSpurIconPikariFrontOuter, g_drawHIO.mSpurIconPikariFrontInner,
                g_drawHIO.mSpurIconPikariBackOuter, g_drawHIO.mSpurIconPikariBackInner);
        }

        posX += spacing;
    }
}

// ============================================
// NEW CODE ENDS HERE
// ============================================

#endif // TARGET_PC

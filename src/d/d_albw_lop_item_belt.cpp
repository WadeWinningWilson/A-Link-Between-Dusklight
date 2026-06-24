// ============================================
// NEW CODE — ALBW Port (Lies of Link HUD — item belt slot frames)
// d_albw_lop_item_belt.cpp — draws the carved "horiwaku" equip-slot frames for the
// bottom-left X/Y item belt, using textures from clctres.arc. The archive is mounted
// HUD-owned (a dedicated mDoDvdThd mount) so the pause menu's removeResourceAll() on
// its own clctres instance can't dangle our pictures. Fully self-contained for easy
// revert. See d_albw_lop_item_belt.h.
// ============================================

#include "d/d_albw_lop_item_belt.h"

#if TARGET_PC

#include "JSystem/J2DGraph/J2DGrafContext.h"
#include "JSystem/J2DGraph/J2DPicture.h"
#include "JSystem/JUtility/TColor.h"
#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRHeap.h"
#include "d/d_com_inf_game.h"

namespace {

// Layout in HUD design px. kBoxSize = outer frame size; the two slots stack above
// the Midna icon (Y slot just above Midna, X slot a gap higher).
constexpr f32 kBoxSize = 46.0f;     // square X/Y slot size
constexpr f32 kCornerDraw = 13.0f;  // corner piece DRAW size (texture stretched to fit)
constexpr f32 kEdgeThick = 5.0f;    // frame edge thickness
constexpr f32 kIconInset = kEdgeThick * 2.0f + 4.0f;  // icon margin inside the frame
constexpr f32 kBeltOffX = 0.0f;     // horizontal vs Midna centre
constexpr f32 kBeltOffY = -42.0f;   // Y slot centre above Midna
constexpr f32 kBeltGap = 50.0f;     // X slot centre above the Y slot
constexpr f32 kSwordBoxW = 92.0f;   // sword slot = wide weapon panel (landscape)
constexpr f32 kSwordBoxH = 46.0f;   // sword panel height (≈ one square)
// Sword blade lies flat: rotate the square icon about its centre so the diagonal
// item-art blade reads horizontal (hilt right, tip left). makeMatrix() applies
// -mRotateZ, so sign/amount are tuned by screenshot. Scale = blade size vs panel
// height (its rotated diagonal is the visible blade length across the wide slot).
constexpr f32 kLopSwordAngleDeg = 122.5f;  // CCW-ish; tune ±15 (135→125→124→123→122.5)
constexpr f32 kLopSwordIconScale = 1.30f;  // square icon size vs (panel height - inset)
// Focused Arts strip: extra height added BELOW the sword panel (top edge + sword stay
// put) to hold the lilac FA meter (kantera widget, drawn by the meter layer) when FA is
// active. Sized to fit the kantera bar's native height; tune via screenshot.
constexpr f32 kFaStripH = 18.0f;

J2DPicture* sCorner = NULL;  // tt_horiwaku_lu       (top-left corner; mirrored for others)
J2DPicture* sEdgeV = NULL;   // tt_horiwaku_side_l_rr (left vertical edge)
J2DPicture* sEdgeH = NULL;   // tt_horiwaku_top_rr    (top horizontal edge)
J2DPicture* sBase = NULL;    // tt_block8x8 tinted dark (slot interior fill)
ResTIMG* sCornerT = NULL;
ResTIMG* sEdgeVT = NULL;
ResTIMG* sEdgeHT = NULL;
ResTIMG* sBaseT = NULL;
// Item icons drawn fresh from their textures: [0]=X, [1]=Y, [2]=equipped sword.
J2DPicture* sIconPic[3] = {NULL, NULL, NULL};
ResTIMG* sIconT[3] = {NULL, NULL, NULL};
// Second composite layer (bottle glass etc.) for the X/Y slots: [0]=X, [1]=Y.
J2DPicture* sIconOverlay[2] = {NULL, NULL};
ResTIMG* sIconOverlayT[2] = {NULL, NULL};

// Fetch a texture from the (already-resident, boot-mounted) collect archive and keep
// the J2DPicture in sync with it. The pause menu calls removeResourceAll() on that
// archive when it closes, after which getResource re-decompresses to a NEW ResTIMG —
// so re-create the picture whenever the pointer changes (cheap when cached). No
// second mount of the resident archive (that was crashing on save load).
bool syncPic(JKRArchive* i_arc, const char* i_name, J2DPicture** io_pic, ResTIMG** io_timg, f32* o_w,
             f32* o_h) {
    ResTIMG* timg = static_cast<ResTIMG*>(i_arc->getResource('TIMG', i_name));
    if (timg == NULL) {
        return false;
    }
    if (*io_pic == NULL || *io_timg != timg) {
        if (*io_pic != NULL) {
            JKR_DELETE(*io_pic);
        }
        *io_pic = JKR_NEW J2DPicture(timg);
        *io_timg = timg;
        if (o_w != NULL) {
            *o_w = (f32)timg->width;
        }
        if (o_h != NULL) {
            *o_h = (f32)timg->height;
        }
    }
    return *io_pic != NULL;
}

bool ensureLoaded() {
    JKRArchive* arc = dComIfGp_getCollectResArchive();
    if (arc == NULL) {
        return false;
    }
    bool ok = true;
    ok = syncPic(arc, "tt_horiwaku_lu.bti", &sCorner, &sCornerT, NULL, NULL) && ok;
    ok = syncPic(arc, "tt_horiwaku_side_l_rr.bti", &sEdgeV, &sEdgeVT, NULL, NULL) && ok;
    ok = syncPic(arc, "tt_horiwaku_top_rr.bti", &sEdgeH, &sEdgeHT, NULL, NULL) && ok;
    ok = syncPic(arc, "tt_block8x8.bti", &sBase, &sBaseT, NULL, NULL) && ok;
    return ok;
}

// 9-slice the carved frame from the three pieces (corner / vertical edge / top edge),
// mirroring via draw()'s flipX/flipY flags; base fill behind.
void drawSlotFrame(f32 i_cx, f32 i_cy, f32 i_w, f32 i_h) {
    const f32 left = i_cx - i_w * 0.5f;
    const f32 top = i_cy - i_h * 0.5f;
    const f32 right = i_cx + i_w * 0.5f;
    const f32 bottom = i_cy + i_h * 0.5f;
    // Fixed DRAW sizes (textures stretch to fit) so a small box never produces
    // negative edge lengths from oversized source corners. Width/height independent
    // so the box can be a wide landscape panel (the sword slot) or a square.
    const f32 cd = kCornerDraw;
    const f32 et = kEdgeThick;
    const f32 spanW = i_w - cd * 2.0f;  // top/bottom edge run
    const f32 spanH = i_h - cd * 2.0f;  // left/right edge run

    // Dark translucent interior fill (tt_block8x8 tinted), behind the carved frame.
    sBase->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(14, 14, 20, 220));
    sBase->setAlpha(220);
    sBase->draw(left + et, top + et, i_w - et * 2.0f, i_h - et * 2.0f, false, false, false);

    sEdgeH->draw(left + cd, top, spanW, et, false, false, false);
    sEdgeH->draw(left + cd, bottom - et, spanW, et, false, true, false);
    sEdgeV->draw(left, top + cd, et, spanH, false, false, false);
    sEdgeV->draw(right - et, top + cd, et, spanH, true, false, false);

    sCorner->draw(left, top, cd, cd, false, false, false);
    sCorner->draw(right - cd, top, cd, cd, true, false, false);
    sCorner->draw(left, bottom - cd, cd, cd, false, true, false);
    sCorner->draw(right - cd, bottom - cd, cd, cd, true, true, false);
}

// Draw the assigned item icon fresh inside a slot from its own texture (recreate the
// picture when the texture pointer changes), stretched to i_w x i_h, optionally rotated
// i_angleDeg about its centre. NULL tex = nothing.
void drawIcon(f32 i_cx, f32 i_cy, f32 i_w, f32 i_h, ResTIMG* i_tex, int i_slot,
              f32 i_angleDeg = 0.0f) {
    if (i_tex == NULL) {
        return;
    }
    if (sIconPic[i_slot] == NULL || sIconT[i_slot] != i_tex) {
        if (sIconPic[i_slot] != NULL) {
            JKR_DELETE(sIconPic[i_slot]);
        }
        sIconPic[i_slot] = JKR_NEW J2DPicture(i_tex);
        sIconT[i_slot] = i_tex;
    }
    if (sIconPic[i_slot] == NULL) {
        return;
    }
    sIconPic[i_slot]->setAlpha(255);
    // Rotation rides the same draw() path we already use: makeMatrix() rotates by
    // -mRotateZ about the pivot (mRotateOffsetX/Y). Pivot at the icon centre so it
    // spins in place; angle 0 is identical to a plain draw (offsets become moot).
    sIconPic[i_slot]->mRotateOffsetX = i_w * 0.5f;
    sIconPic[i_slot]->mRotateOffsetY = i_h * 0.5f;
    sIconPic[i_slot]->mRotateZ = i_angleDeg;
    sIconPic[i_slot]->draw(i_cx - i_w * 0.5f, i_cy - i_h * 0.5f, i_w, i_h, false, false, false);
}

// Size an item icon to match vanilla proportions: native aspect (texDim / 48px ref)
// times the item's texScale, fitted into the slot's inner area i_ref. Without this,
// every icon was stretched to a fixed square — items with a smaller texScale (bottles,
// lanterns) overflowed the carved frame. Oversized results are scaled back uniformly so
// nothing ever spills past the frame.
void fitIconSize(ResTIMG* i_tex, u8 i_scalePct, f32 i_ref, f32* o_w, f32* o_h) {
    const f32 tw = (i_tex != NULL && i_tex->width > 0) ? (f32)i_tex->width : 48.0f;
    const f32 th = (i_tex != NULL && i_tex->height > 0) ? (f32)i_tex->height : 48.0f;
    const f32 s = (i_scalePct > 0 ? (f32)i_scalePct : 100.0f) / 100.0f;
    f32 w = (tw / 48.0f) * i_ref * s;
    f32 h = (th / 48.0f) * i_ref * s;
    f32 fit = 1.0f;
    if (w > i_ref) {
        fit = i_ref / w;
    }
    if (h > i_ref && (i_ref / h) < fit) {
        fit = i_ref / h;
    }
    *o_w = w * fit;
    *o_h = h * fit;
}

// Draw one composite layer fresh (recreate the picture on texture change), tinted with
// the colors vanilla applied to its source pane, at i_w x i_h centred on (i_cx,i_cy).
// NULL tex = nothing (single-layer items pass NULL for the overlay).
void drawLayer(J2DPicture** io_pic, ResTIMG** io_t, ResTIMG* i_tex, JUtility::TColor i_black,
               JUtility::TColor i_white, f32 i_cx, f32 i_cy, f32 i_w, f32 i_h) {
    if (i_tex == NULL) {
        return;
    }
    if (*io_pic == NULL || *io_t != i_tex) {
        if (*io_pic != NULL) {
            JKR_DELETE(*io_pic);
        }
        *io_pic = JKR_NEW J2DPicture(i_tex);
        *io_t = i_tex;
    }
    if (*io_pic == NULL) {
        return;
    }
    (*io_pic)->setBlackWhite(i_black, i_white);
    (*io_pic)->setAlpha(255);
    (*io_pic)->mRotateZ = 0.0f;  // X/Y icons never rotate (unlike the sword slot)
    (*io_pic)->draw(i_cx - i_w * 0.5f, i_cy - i_h * 0.5f, i_w, i_h, false, false, false);
}

}  // namespace

void dAlbwLopItemBelt_draw(J2DGrafContext* i_gctx, f32 i_midnaX, f32 i_midnaY,
                           const LopBeltIcon& i_x, const LopBeltIcon& i_y) {
    if (i_gctx == NULL || !ensureLoaded()) {
        return;
    }
    const f32 slot1X = i_midnaX + kBeltOffX;  // X slot (lower)
    const f32 slot1Y = i_midnaY + kBeltOffY;
    const f32 slot0X = slot1X;                // Y slot (upper)
    const f32 slot0Y = slot1Y - kBeltGap;

    const f32 iconRef = kBoxSize - kIconInset;
    i_gctx->setup2D();
    drawSlotFrame(slot0X, slot0Y, kBoxSize, kBoxSize);
    drawSlotFrame(slot1X, slot1Y, kBoxSize, kBoxSize);

    // Y slot (top): base content, then the overlay (e.g. bottle glass) over it.
    f32 yW, yH;
    fitIconSize(i_y.tex0, i_y.scalePct, iconRef, &yW, &yH);
    drawLayer(&sIconPic[1], &sIconT[1], i_y.tex0, i_y.black0, i_y.white0, slot0X, slot0Y, yW, yH);
    drawLayer(&sIconOverlay[1], &sIconOverlayT[1], i_y.tex1, i_y.black1, i_y.white1, slot0X, slot0Y,
              yW, yH);

    // X slot (bottom).
    f32 xW, xH;
    fitIconSize(i_x.tex0, i_x.scalePct, iconRef, &xW, &xH);
    drawLayer(&sIconPic[0], &sIconT[0], i_x.tex0, i_x.black0, i_x.white0, slot1X, slot1Y, xW, xH);
    drawLayer(&sIconOverlay[0], &sIconOverlayT[0], i_x.tex1, i_x.black1, i_x.white1, slot1X, slot1Y,
              xW, xH);
}

void dAlbwLopItemBelt_drawSword(J2DGrafContext* i_gctx, f32 i_cx, f32 i_cy, ResTIMG* i_swordTex,
                                bool i_faActive) {
    if (i_gctx == NULL || !ensureLoaded()) {
        return;
    }
    i_gctx->setup2D();

    // With FA active the panel grows downward by kFaStripH: the top edge and the sword
    // icon stay exactly where they are; the FA meter (drawn by the meter layer) occupies
    // the new bottom strip.
    const f32 frameH = i_faActive ? (kSwordBoxH + kFaStripH) : kSwordBoxH;
    const f32 frameCy = i_faActive ? (i_cy + kFaStripH * 0.5f) : i_cy;
    drawSlotFrame(i_cx, frameCy, kSwordBoxW, frameH);

    // Draw the blade at natural (square) aspect and rotate it flat — stretching a
    // diagonal icon across the wide panel would shear it. Size from the panel height
    // so the rotated blade's diagonal spans the landscape slot.
    const f32 swordSz = (kSwordBoxH - kIconInset) * kLopSwordIconScale;
    drawIcon(i_cx, i_cy, swordSz, swordSz, i_swordTex, 2, kLopSwordAngleDeg);
}

void dAlbwLopItemBelt_getSwordFaStrip(f32 i_cx, f32 i_cy, f32* o_left, f32* o_top, f32* o_width,
                                      f32* o_height) {
    // The strip is the extra band below the original (un-extended) panel bottom; the
    // FA bar sits inside it with a little horizontal padding.
    const f32 rowH = kFaStripH - 6.0f;
    if (o_left != NULL) {
        *o_left = i_cx - (kSwordBoxW * 0.5f - kEdgeThick - 3.0f);
    }
    if (o_top != NULL) {
        *o_top = i_cy + kSwordBoxH * 0.5f + (kFaStripH - rowH) * 0.5f - 1.0f;
    }
    if (o_width != NULL) {
        *o_width = kSwordBoxW - kEdgeThick * 2.0f - 6.0f;
    }
    if (o_height != NULL) {
        *o_height = rowH;
    }
}

void dAlbwLopItemBelt_cleanup() {
    if (sCorner != NULL) {
        JKR_DELETE(sCorner);
        sCorner = NULL;
    }
    if (sEdgeV != NULL) {
        JKR_DELETE(sEdgeV);
        sEdgeV = NULL;
    }
    if (sEdgeH != NULL) {
        JKR_DELETE(sEdgeH);
        sEdgeH = NULL;
    }
    if (sBase != NULL) {
        JKR_DELETE(sBase);
        sBase = NULL;
    }
    for (int i = 0; i < 3; i++) {
        if (sIconPic[i] != NULL) {
            JKR_DELETE(sIconPic[i]);
            sIconPic[i] = NULL;
        }
        sIconT[i] = NULL;
    }
    for (int i = 0; i < 2; i++) {
        if (sIconOverlay[i] != NULL) {
            JKR_DELETE(sIconOverlay[i]);
            sIconOverlay[i] = NULL;
        }
        sIconOverlayT[i] = NULL;
    }
    sCornerT = NULL;
    sEdgeVT = NULL;
    sEdgeHT = NULL;
    sBaseT = NULL;
}

#endif  // TARGET_PC

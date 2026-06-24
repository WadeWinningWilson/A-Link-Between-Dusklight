#ifndef D_ALBW_LOP_ITEM_BELT_H
#define D_ALBW_LOP_ITEM_BELT_H

// ============================================
// NEW CODE — ALBW Port (Lies of Link HUD — item belt slot frames)
// Self-contained: draws the carved "horiwaku" slot frames (from clctres.arc, mounted
// HUD-owned so the pause menu's removeResourceAll can't dangle it) for the bottom-left
// X/Y item belt. Revert = delete this file + d_albw_lop_item_belt.cpp + its files.cmake
// line + the call sites in d_meter2_draw.cpp.
// ============================================

#if TARGET_PC

#include "JSystem/JUtility/TColor.h"

class J2DGrafContext;
struct ResTIMG;

// One belt slot's icon, rendered as fresh quads (identity transform — never inherits
// the button ring's rotation/scale). Mirrors vanilla's two-layer item composite:
//   tex0 = base/content layer (potion liquid, item body), tinted black0/white0
//   tex1 = overlay layer      (bottle glass, etc.),       tinted black1/white1
// The tints are copied straight off the vanilla item panes (which set1st/2ndColor
// already applied), so bottles/potions look right without duplicating that color table.
// tex0 == NULL → empty slot (frame only). tex1 == NULL → single-layer item (most items).
// scalePct is the item's dItem_data texScale (0 → treated as 100) for native-aspect fit.
struct LopBeltIcon {
    ResTIMG* tex0;
    ResTIMG* tex1;
    JUtility::TColor black0;
    JUtility::TColor white0;
    JUtility::TColor black1;
    JUtility::TColor white1;
    u8 scalePct;
};

// Draws two stacked slot frames (Y on top, X below) above the Midna centre, with the
// assigned X/Y item icons composited fresh inside them — so we never fight the button
// ring's transform/clipping. Call after grafCtx is current.
void dAlbwLopItemBelt_draw(J2DGrafContext* i_gctx, f32 i_midnaX, f32 i_midnaY,
                           const LopBeltIcon& i_x, const LopBeltIcon& i_y);

// Draws a single framed slot at (i_cx, i_cy) with the equipped-sword icon inside
// (NULL = no sword, frame only). Used for the sword slot right of the B-button box.
// When i_faActive the panel extends downward to make room for the Focused Arts meter
// (drawn separately, by the meter layer, via dAlbwLopItemBelt_getSwordFaStrip) — the
// sword icon position and rotation are unchanged.
void dAlbwLopItemBelt_drawSword(J2DGrafContext* i_gctx, f32 i_cx, f32 i_cy, ResTIMG* i_swordTex,
                                bool i_faActive);

// Reports the global rect of the FA strip (the extra bottom area of the FA-active sword
// panel) so the meter layer can draw the kantera FA bar there. (i_cx,i_cy) is the sword
// slot centre, as passed to dAlbwLopItemBelt_drawSword.
void dAlbwLopItemBelt_getSwordFaStrip(f32 i_cx, f32 i_cy, f32* o_left, f32* o_top, f32* o_width,
                                      f32* o_height);

// Frees the frame + icon pictures. Call on meter-draw teardown.
void dAlbwLopItemBelt_cleanup();

#endif  // TARGET_PC

#endif  // D_ALBW_LOP_ITEM_BELT_H

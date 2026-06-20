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

class J2DGrafContext;
struct ResTIMG;

// Draws two stacked slot frames (X on top, Y below) above the Midna centre, with the
// assigned X/Y item icons rendered fresh inside them from their own textures — so we
// never fight the button ring's transform/clipping. Call after grafCtx is current.
// i_xTex / i_yTex are the current X / Y item textures (NULL = empty slot, no icon).
void dAlbwLopItemBelt_draw(J2DGrafContext* i_gctx, f32 i_midnaX, f32 i_midnaY, ResTIMG* i_xTex,
                           ResTIMG* i_yTex);

// Draws a single framed slot at (i_cx, i_cy) with the equipped-sword icon inside
// (NULL = no sword, frame only). Used for the sword slot right of the B-button box.
void dAlbwLopItemBelt_drawSword(J2DGrafContext* i_gctx, f32 i_cx, f32 i_cy, ResTIMG* i_swordTex);

// Frees the frame + icon pictures. Call on meter-draw teardown.
void dAlbwLopItemBelt_cleanup();

#endif  // TARGET_PC

#endif  // D_ALBW_LOP_ITEM_BELT_H

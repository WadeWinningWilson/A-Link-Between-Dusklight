// ============================================
// NEW CODE — ALBW Port (Native Shop Window)
// d_albw_shop.cpp — dALBWShop_c implementation.
// Loads letres.arc and draws the rental item list using the game's native
// letter-select screen layout. Registered via dComIfGd_set2DOpa() from
// the Postman actor's Draw() while dALBWRental_isOpen().
//
// Never call setPriority on getMsgCommonArchive() BLOs here — dALBWDialogue_c
// owns zelda_message_window_text.blo; a second load or JKR_DELETE of that
// screen corrupts the archive and can crash file-select / save load.
// ============================================
#include "d/d_albw_shop.h"

#if TARGET_PC_NATIVE_UI

#include "d/dolzel.h" // IWYU pragma: keep

#include "JSystem/J2DGraph/J2DGrafContext.h"
#include "JSystem/J2DGraph/J2DOrthoGraph.h"
#include "JSystem/J2DGraph/J2DPrint.h"
#include "JSystem/J2DGraph/J2DPane.h"
#include "JSystem/J2DGraph/J2DPicture.h"
#include "JSystem/J2DGraph/J2DWindow.h"
#include "JSystem/J2DGraph/J2DTextBox.h"
#include "JSystem/JSupport/JSUList.h"
#include "JSystem/JUtility/TColor.h"
#include "JSystem/JKernel/JKRMemArchive.h"
#include "d/d_albw_rental.h"
#include "d/d_albw_ui_text.h"
#include "d/d_com_inf_game.h"
#include "d/d_item.h"
#include "d/d_item_data.h"
#include "d/d_meter2_info.h"
#include "d/d_meter_HIO.h"
#include "dolphin/os/OSCache.h"
#include "JSystem/JKernel/JKRArchive.h"
#include "d/d_pane_class.h"
#include "dusk/frame_interpolation.h"
#include "m_Do/m_Do_dvd_thread.h"
#include "m_Do/m_Do_ext.h"
#include "m_Do/m_Do_graphic.h"
#include "os_report.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static const u64 kTagName[6] = {
    MULTI_CHAR('fenu_t0'), MULTI_CHAR('fenu_t1'), MULTI_CHAR('fenu_t2'),
    MULTI_CHAR('fenu_t3'), MULTI_CHAR('fenu_t4'), MULTI_CHAR('fenu_t5'),
};
static const u64 kTagPrice[6] = {
    MULTI_CHAR('fenu_t6'),  MULTI_CHAR('fenu_t7'),  MULTI_CHAR('fenu_t8'),
    MULTI_CHAR('fenu_t9'),  MULTI_CHAR('fenu_t10'), MULTI_CHAR('fenu_t11'),
};
static const u64 kTagSubShadow[6] = {
    MULTI_CHAR('fenu_t0s'), MULTI_CHAR('fenu_t1s'), MULTI_CHAR('fenu_t2s'),
    MULTI_CHAR('fenu_t3s'), MULTI_CHAR('fenu_t4s'), MULTI_CHAR('fenu_t5s'),
};
static const u64 kTagPriceShadow[6] = {
    MULTI_CHAR('fenu_t6s'), MULTI_CHAR('fenu_t7s'), MULTI_CHAR('fenu_t8s'),
    MULTI_CHAR('fenu_t9s'), MULTI_CHAR('fenu_10s'), MULTI_CHAR('fenu_11s'),
};
static const u64 kTagRowFrame[6] = {
    MULTI_CHAR('flame_00'), MULTI_CHAR('flame_01'), MULTI_CHAR('flame_02'),
    MULTI_CHAR('flame_03'), MULTI_CHAR('flame_04'), MULTI_CHAR('flame_05'),
};
static const u64 kTagRowLetter[6] = {
    MULTI_CHAR('let_00_n'), MULTI_CHAR('let_01_n'), MULTI_CHAR('let_02_n'),
    MULTI_CHAR('let_03_n'), MULTI_CHAR('let_04_n'), MULTI_CHAR('let_05_n'),
};
static const u64 kTagMidoku[6] = {
    MULTI_CHAR('midoku_0'), MULTI_CHAR('midoku_1'), MULTI_CHAR('midoku_2'),
    MULTI_CHAR('midoku_3'), MULTI_CHAR('midoku_4'), MULTI_CHAR('midoku_5'),
};

static constexpr f32 kTitleFontScale   = 0.65f;
// Applied once on t4_s at load; drawDescMesgText uses that size (no second multiply).
static constexpr f32 kDescFontScale       = 0.65f;
static constexpr f32 kDescLineSpaceScale  = 0.72f;  // ruled pitch is tall; tighten vs font size
static constexpr f32 kRowNameFontScale = 0.88f;  // long names (e.g. Double Clawshot) vs price column
// let_area is the whole letter-window interior (list + parchment), not the list column alone.
// Vanilla only draws the menu inside it; parchment in read mode is full-screen with a dim pass.
static constexpr f32 kPriceFontScale = 0.52f;
// List column width + gap before parchment (right).
static constexpr f32 kListColumnRatio = 0.47f;
static constexpr f32 kListDescGap     = 20.0f;
// let_*_n icons sit on the left edge of rows; extend list scissor so GX does not clip them.
static constexpr f32 kListIconScissorPad = 48.0f;
// Pause item wheel uses 48px TIMG baseline; let_*_n PAN2 bounds are the whole row, not icon size.
static constexpr f32 kWheelIconBaselinePx = 48.0f;
// let_*_n PAN2 row bounds are ~462×31; letter icon sits in a square on the left edge.
static constexpr f32 kRowLetterIconBoxMaxW = 34.0f;
static constexpr f32 kRowWheelIconInsetY   = 2.0f;
static constexpr f32 kRowFrameInsetX  = 6.0f;
// Gap (0-640 canvas px) the list scissor leaves before the parchment. The flame_* bars already
// span nearly the full area; the scissor — not the bar width — is what clips them short. Tunable.
static constexpr f32 kRowFrameGapBeforeParchment = 6.0f;
// Footer tagline / action-hint font height as a fraction of the footer bar height. Tunable.
static constexpr f32 kFooterFontHeightRatio = 0.42f;
// Footer content split: center tagline gets this fraction, right A/B hint the remainder. Tunable.
static constexpr f32 kFooterCenterFraction  = 0.70f;
// Uniform rightward nudge of the whole footer (stick + tagline + A/B), in 0-640 canvas px. Tunable.
static constexpr f32 kFooterContentShiftX   = 16.0f;
// Per-element fine positioning (0-640 canvas px). +x right, +y down. All tunable.
static constexpr f32 kFooterStickOffsetX    = 18.0f;   // nunchuk stick: nudge right
static constexpr f32 kFooterTaglineOffsetX  = 22.0f;   // "Hylians..." tagline: nudge right
static constexpr f32 kFooterTaglineOffsetY  = 7.0f;    // tagline: nudge down
static constexpr f32 kFooterActionsOffsetX  = -16.0f;  // A: Buy/ B: Exit: nudge left
static constexpr f32 kFooterActionsOffsetY  = 7.0f;    // A/B hints: nudge down

using LetAreaBounds = dALBWPaneBounds;

static LetAreaBounds calcPaneScreenBounds(J2DPane* pane) {
    return dALBW_calcPaneScreenBounds(pane);
}
static constexpr f32 kPriceRightInset  = 0.0f;
static constexpr f32 kPriceExtraNudgeX = 22.0f;
// paneTrans(x,y) adds x to the BLO init center — not an absolute screen position.
static constexpr f32 kPricePaneShiftX = -108.0f;
// Fallback text inset when t4_s / line09 bounds are unavailable.
static constexpr f32 kDescTextPadX    = 18.0f;
static constexpr f32 kDescTextPadY    = 8.0f;
// First-line vertical tune on line09 (n_e4line ruled layout).
static constexpr f32 kDescFirstLineNudgeY       = 0.0f;
static constexpr f32 kDescLockedLineNudgeDown   = 0.50f;  // locked copy sits ~half a line above rules
static constexpr f32 kDescRentableLineNudgeUp    = 1.70f;  // net up from line12 (lower = text sits on rules)
static constexpr f32 kDescRentablePixelNudgeY    = 5.0f;   // fine down onto rules (independent of pitch)
static constexpr f32 kDescBaselineNudgeY         = 4.0f;
static constexpr f32 kDescRentableBaselineNudgeY = 7.0f;
// Spot.blo gold corner flourishes extend past the parchment rect — widen scissor for frame only.
static constexpr f32 kDescDecorScissorPad     = 28.0f;

static s16 shopItemIconTexIdx(u8 itemNo) {
    if (itemNo == 0xff) {
        return -1;
    }
    u8 loadNo = itemNo;
    if (loadNo == (u8)dItemNo_DEITY_ARMOR_e) {
        loadNo = (u8)dItemNo_ARMOR_e;
    }
    s16 texIdx = dItem_data::getTexture(loadNo);
    if (loadNo == (u8)dItemNo_COPY_ROD_e) {
        texIdx = 0x57;  // ST_COPY_ROD_B in itemicon.arc (no daPy)
    }
    return texIdx;
}

// kItems[] stores bomb bags; the item wheel shows bombs / water bombs / bomblings.
static u8 shopWheelIconItemNo(u8 rentalItemNo) {
    switch (rentalItemNo) {
    case (u8)dItemNo_BOMB_BAG_LV1_e:
        return (u8)dItemNo_NORMAL_BOMB_e;
    case (u8)dItemNo_BOMB_BAG_LV2_e:
        return (u8)dItemNo_WATER_BOMB_e;
    default:
        return rentalItemNo;
    }
}

// Rental shop items → itemicon.arc BTI filenames (layout spreadsheet / wheel icons).
static const char* shopRentalItemBtiName(u8 itemNo) {
    switch (itemNo) {
    case (u8)dItemNo_PACHINKO_e:
        return "st_pachinko.bti";
    case (u8)dItemNo_BOOMERANG_e:
        return "tt_boomerang_05.bti";
    case (u8)dItemNo_COPY_ROD_e:
        return "st_copy_rod_b.bti";
    case (u8)dItemNo_HOOKSHOT_e:
        return "im_hookshot_48.bti";
    case (u8)dItemNo_BOW_e:
        return "tt_bow_06.bti";
    case (u8)dItemNo_W_HOOKSHOT_e:
        return "im_w_hookshot_48.bti";
    case (u8)dItemNo_SPINNER_e:
        return "im_sppiner_48.bti";
    case (u8)dItemNo_IRONBALL_e:
        return "im_ironball_48.bti";
    case (u8)dItemNo_ARMOR_e:
    case (u8)dItemNo_DEITY_ARMOR_e:
        return "ni_magicarmor_48.bti";
    case (u8)dItemNo_DUNGEON_BACK_e:
        return "im_musuko_48.bti";
    default:
        return nullptr;
    }
}

static ResTIMG* loadShopItemIconTimg(JKRArchive* arc, u8 itemNo, void* texBuf) {
    if (!arc || !texBuf || itemNo == 0xff) {
        return nullptr;
    }

    const u8 iconItemNo = shopWheelIconItemNo(itemNo);

    if (const char* name = shopRentalItemBtiName(iconItemNo)) {
        void* res = JKRGetNameResource(name, arc);
        if (res) {
            memcpy(texBuf, res, 0xC00);
            DCStoreRangeNoSync(texBuf, 0xC00);
            return (ResTIMG*)texBuf;
        }
    }

    const s16 texIdx = shopItemIconTexIdx(iconItemNo);
    if (texIdx < 0) {
        return nullptr;
    }

    u32 size = JKRReadIdxResource(texBuf, 0xC00, (u32)texIdx, arc);
    if (size == 0) {
        size = arc->readResource(texBuf, 0xC00, (u16)texIdx);
    }
    if (size == 0) {
        return nullptr;
    }
    DCStoreRangeNoSync(texBuf, 0xC00);
    return (ResTIMG*)texBuf;
}

static int countPicturesInPane(J2DPane* root) {
    if (!root) {
        return 0;
    }
    int        count = 0;
    struct StackEntry {
        J2DPane* pane;
    };
    StackEntry stack[48];
    int        sp = 0;
    stack[sp++].pane = root;
    while (sp > 0) {
        J2DPane* pane = stack[--sp].pane;
        if (!pane) {
            continue;
        }
        if (pane->getKind() == 'PIC1') {
            ++count;
        }
        for (J2DPane* child = pane->getFirstChildPane(); child; child = child->getNextChildPane()) {
            if (sp < (int)(sizeof(stack) / sizeof(stack[0]))) {
                stack[sp++].pane = child;
            }
        }
    }
    return count;
}

// Item-wheel path: swap TIMG on BLO letter-slot pictures before mpMenuScreen->draw().
static bool applyWheelIconsToRowPane(u8 itemNo, J2DPane* root, void* texBuf, void* texBuf2) {
    if (!root || itemNo == 0xff || !dComIfGp_getItemIconArchive()) {
        return false;
    }

    int param9 = -1;
    if (itemNo == (u8)dItemNo_COPY_ROD_e) {
        param9 = 0x57;
    }

    bool        ok = false;
    struct StackEntry {
        J2DPane* pane;
    };
    StackEntry stack[48];
    int        sp = 0;
    stack[sp++].pane = root;
    while (sp > 0) {
        J2DPane* pane = stack[--sp].pane;
        if (!pane) {
            continue;
        }
        if (pane->getKind() == 'PIC1') {
            J2DPicture* pic = (J2DPicture*)pane;
            const int   texNum =
                dMeter2Info_readItemTexture(itemNo, texBuf, pic, texBuf2, nullptr, nullptr,
                                            nullptr, nullptr, nullptr, param9);
            if (texNum > 0) {
                pic->setBlackWhite(JUtility::TColor(0, 0, 0, 0),
                                   JUtility::TColor(255, 255, 255, 255));
                pic->show();
                ok = true;
            }
        }
        for (J2DPane* child = pane->getFirstChildPane(); child; child = child->getNextChildPane()) {
            if (sp < (int)(sizeof(stack) / sizeof(stack[0]))) {
                stack[sp++].pane = child;
            }
        }
    }
    return ok;
}

static void restoreLetterIconsToPane(J2DPane* root, ResTIMG* letterTimg, f32 w, f32 h) {
    if (!root || !letterTimg) {
        return;
    }
    struct StackEntry {
        J2DPane* pane;
    };
    StackEntry stack[48];
    int        sp = 0;
    stack[sp++].pane = root;
    while (sp > 0) {
        J2DPane* pane = stack[--sp].pane;
        if (!pane) {
            continue;
        }
        if (pane->getKind() == 'PIC1') {
            J2DPicture* pic = (J2DPicture*)pane;
            pic->changeTexture(letterTimg, 0);
            pic->resize(w, h);
            pic->setBlackWhite(JUtility::TColor(0, 0, 0, 0),
                               JUtility::TColor(255, 255, 255, 255));
            pic->show();
        }
        for (J2DPane* child = pane->getFirstChildPane(); child; child = child->getNextChildPane()) {
            if (sp < (int)(sizeof(stack) / sizeof(stack[0]))) {
                stack[sp++].pane = child;
            }
        }
    }
}

static void hidePicturesInPane(J2DPane* root) {
    if (!root) {
        return;
    }
    struct StackEntry {
        J2DPane* pane;
    };
    StackEntry stack[48];
    int        sp = 0;
    stack[sp++].pane = root;
    while (sp > 0) {
        J2DPane* pane = stack[--sp].pane;
        if (!pane) {
            continue;
        }
        if (pane->getKind() == 'PIC1') {
            pane->hide();
        }
        for (J2DPane* child = pane->getFirstChildPane(); child; child = child->getNextChildPane()) {
            if (sp < (int)(sizeof(stack) / sizeof(stack[0]))) {
                stack[sp++].pane = child;
            }
        }
    }
}

// J2DWindow::drawContentsTexture uses a TEV path that ignores vertex alpha, so
// setContentsColor(0) does not hide the letter BTI — clear field_0x110 instead.
class J2DWindowContentsAccess : public J2DWindow {
public:
    static JUTTexture* getContents(J2DWindow* win) {
        return win ? reinterpret_cast<J2DWindowContentsAccess*>(win)->field_0x110 : nullptr;
    }
    static void setContents(J2DWindow* win, JUTTexture* tex) {
        if (win) {
            reinterpret_cast<J2DWindowContentsAccess*>(win)->field_0x110 = tex;
        }
    }
};

static bool paneOverlapsSlot(J2DPane* pane, const LetAreaBounds& slot) {
    if (!pane || !slot.valid) {
        return false;
    }
    const LetAreaBounds b = calcPaneScreenBounds(pane);
    if (!b.valid) {
        return false;
    }
    const f32 cx = b.x + b.w * 0.5f;
    const f32 cy = b.y + b.h * 0.5f;
    return cx >= slot.x && cx <= slot.x + slot.w && cy >= slot.y && cy <= slot.y + slot.h;
}

// let_*_n is PAN2 with no PIC children; the envelope WIN/PIC is usually a sibling in 6menu.blo.
static J2DPane* findLetterGfxPaneNear(J2DPane* screenRoot, ResTIMG* letterTimg, J2DPane* letPane,
                                      CPaneMgr* letMgr) {
    if (!screenRoot || !letterTimg || !letPane) {
        return nullptr;
    }

    LetAreaBounds slot = calcPaneScreenBounds(letMgr ? letMgr->getPanePtr() : letPane);
    if (slot.valid) {
        const f32 box = std::min(slot.h, kRowLetterIconBoxMaxW);
        if (box > 4.0f) {
            slot.w = box;
        }
    }

    J2DPane*    bestPane = nullptr;
    f32         bestArea = 1.0e9f;
    struct StackEntry {
        J2DPane* pane;
    };
    StackEntry stack[128];
    int        sp = 0;
    stack[sp++].pane = screenRoot;
    while (sp > 0) {
        J2DPane* pane = stack[--sp].pane;
        if (!pane || pane == letPane) {
            continue;
        }
        if (pane->isUsed(letterTimg) && paneOverlapsSlot(pane, slot)) {
            const LetAreaBounds b = calcPaneScreenBounds(pane);
            const f32           area = b.valid ? b.w * b.h : 1.0f;
            if (area < bestArea) {
                bestArea = area;
                bestPane = pane;
            }
        }
        for (J2DPane* child = pane->getFirstChildPane(); child; child = child->getNextChildPane()) {
            if (sp < (int)(sizeof(stack) / sizeof(stack[0]))) {
                stack[sp++].pane = child;
            }
        }
    }
    return bestPane;
}

// Fallback when isUsed(letterTimg) misses (shared TIMG / composite window).
static J2DPane* findIconGfxPaneNear(J2DPane* screenRoot, J2DPane* letPane, CPaneMgr* letMgr) {
    if (!screenRoot || !letPane) {
        return nullptr;
    }

    LetAreaBounds slot = calcPaneScreenBounds(letMgr ? letMgr->getPanePtr() : letPane);
    if (slot.valid) {
        const f32 box = std::min(slot.h, kRowLetterIconBoxMaxW);
        if (box > 4.0f) {
            slot.w = box;
        }
    }

    J2DPane* bestPane = nullptr;
    f32      bestArea = 1.0e9f;
    struct StackEntry {
        J2DPane* pane;
    };
    StackEntry stack[128];
    int        sp = 0;
    stack[sp++].pane = screenRoot;
    while (sp > 0) {
        J2DPane* pane = stack[--sp].pane;
        if (!pane || pane == letPane) {
            continue;
        }
        const u16 type = pane->getTypeID();
        if ((type == 17 || type == 18) && paneOverlapsSlot(pane, slot)) {
            const LetAreaBounds b = calcPaneScreenBounds(pane);
            const f32           area = b.valid ? b.w * b.h : 1.0f;
            if (area < bestArea) {
                bestArea = area;
                bestPane = pane;
            }
        }
        for (J2DPane* child = pane->getFirstChildPane(); child; child = child->getNextChildPane()) {
            if (sp < (int)(sizeof(stack) / sizeof(stack[0]))) {
                stack[sp++].pane = child;
            }
        }
    }
    return bestPane;
}

static void setRowLetterEnvelopeVisible(J2DWindow* win, JUTTexture* savedContents, bool show) {
    if (!win) {
        return;
    }
    if (show) {
        J2DWindowContentsAccess::setContents(win, savedContents);
        win->setContentsColor(JUtility::TColor(255, 255, 255, 255));
    } else {
        J2DWindowContentsAccess::setContents(win, nullptr);
        win->setContentsColor(JUtility::TColor(0, 0, 0, 0));
    }
}

static void showPaneAndAncestors(J2DPane* pane) {
    for (; pane; pane = pane->getParentPane()) {
        pane->show();
    }
}

static LetAreaBounds calcRowIconSlotBounds(CPaneMgr* letterMgr) {
    LetAreaBounds slot;
    if (!letterMgr) {
        return slot;
    }
    slot = calcPaneScreenBounds(letterMgr->getPanePtr());
    if (slot.valid) {
        const f32 box = std::min(slot.h - kRowWheelIconInsetY * 2.0f, kRowLetterIconBoxMaxW);
        if (box > 4.0f) {
            slot.w = box;
        }
    }
    return slot;
}

// Heap icon rect in gInf draw space (same coords as mpMenuScreen / getGlobalVtx).
static bool calcRowHeapIconDrawGInf(J2DPicture* rowFrame, CPaneMgr* letterMgr, f32 listIconLeft,
                                      f32* outX, f32* outY, f32* outW, f32* outH) {
    if (!rowFrame) {
        return false;
    }

    CPaneMgr scratch;
    Mtx     mtx;
    const Vec tl = scratch.getGlobalVtx(rowFrame, &mtx, 0, false, 0);
    const Vec br = scratch.getGlobalVtx(rowFrame, &mtx, 3, false, 0);

    const f32 scY    = mDoGph_gInf_c::getHeightF() / FB_HEIGHT;
    const f32 frameH = br.y - tl.y;
    if (frameH < 1.0f) {
        return false;
    }

    f32 boxH = std::min(frameH - kRowWheelIconInsetY * 2.0f * scY, kRowLetterIconBoxMaxW * scY);
    boxH     = std::max(boxH, 12.0f * scY);
    const f32 boxW = boxH;

    *outX = tl.x;
    *outY = tl.y + 0.5f * (frameH - boxH);
    *outW = boxW;
    *outH = boxH;
    (void)letterMgr;
    (void)listIconLeft;
    return true;
}

// Debug / fallback: normalized 640 bounds for dumpRowIconDebug.
static void calcRowHeapIconDrawPos(CPaneMgr* letterMgr, J2DPicture* rowFrame, f32 listIconLeft,
                                   f32* outX, f32* outY, f32* outW, f32* outH) {
    f32 gx = 0.0f;
    f32 gy = 0.0f;
    f32 gw = 0.0f;
    f32 gh = 0.0f;
    if (calcRowHeapIconDrawGInf(rowFrame, letterMgr, listIconLeft, &gx, &gy, &gw, &gh)) {
        const f32 scX = mDoGph_gInf_c::getWidthF() / FB_WIDTH;
        const f32 scY = mDoGph_gInf_c::getHeightF() / FB_HEIGHT;
        *outX         = (gx - mDoGph_gInf_c::getMinXF()) / scX;
        *outY         = (gy - mDoGph_gInf_c::getMinYF()) / scY;
        *outW         = gw / scX;
        *outH         = gh / scY;
        return;
    }

    const LetAreaBounds slot  = calcRowIconSlotBounds(letterMgr);
    const LetAreaBounds frame = rowFrame ? calcPaneScreenBounds(rowFrame) : LetAreaBounds{};

    f32 boxW = kRowLetterIconBoxMaxW;
    if (slot.valid && slot.w > 4.0f) {
        boxW = slot.w;
    }

    const f32 rowH = frame.valid ? frame.h : (slot.valid ? slot.h : kWheelIconBaselinePx);
    const f32 rowY = frame.valid ? frame.y : (slot.valid ? slot.y : 0.0f);

    f32 innerH = std::min(rowH - kRowWheelIconInsetY * 2.0f, kRowLetterIconBoxMaxW);
    innerH     = std::max(innerH, 12.0f);
    boxW       = std::min(boxW, innerH);

    *outW = boxW;
    *outH = innerH;
    *outY = rowY + 0.5f * (rowH - innerH);
    if (frame.valid) {
        *outX = frame.x;
    } else if (listIconLeft >= 0.0f) {
        *outX = listIconLeft;
    } else {
        *outX = slot.valid ? slot.x : 0.0f;
    }
}

// Match visible mail/envelope X on a locked row; flame_* left edge is a fallback only.
static f32 calcShopListIconLeft(J2DPicture* const* rowFrames, J2DPane* const* letterGfx,
                                 J2DPicture* const* letterPic, CPaneMgr* const* letterMgr,
                                 int scrollTop, int visCount, const dALBWVisibleEntry* visList) {
    for (int row = 0; row < 6; ++row) {
        const int itemIdx = scrollTop + row;
        if (itemIdx >= visCount || visList[itemIdx].purchasable) {
            continue;
        }
        if (letterGfx[row]) {
            const LetAreaBounds b = calcPaneScreenBounds(letterGfx[row]);
            if (b.valid && b.w < 80.0f && b.h < 80.0f) {
                return b.x;
            }
        }
        if (letterPic[row] && letterPic[row]->isVisible()) {
            const LetAreaBounds b = calcPaneScreenBounds(letterPic[row]);
            if (b.valid) {
                return b.x;
            }
        }
        const LetAreaBounds slot = calcRowIconSlotBounds(letterMgr[row]);
        if (slot.valid) {
            return slot.x;
        }
    }

    f32 left = -1.0f;
    for (int row = 0; row < 6; ++row) {
        const LetAreaBounds slot = calcRowIconSlotBounds(letterMgr[row]);
        if (slot.valid && (left < 0.0f || slot.x < left)) {
            left = slot.x;
        }
    }
    for (int row = 0; row < 6; ++row) {
        if (!rowFrames[row]) {
            continue;
        }
        const LetAreaBounds frame = calcPaneScreenBounds(rowFrames[row]);
        if (!frame.valid) {
            continue;
        }
        const f32 x = frame.x + kRowFrameInsetX;
        if (left < 0.0f || x < left) {
            left = x;
        }
    }
    return left;
}

// let_*_n is PAN2: row bar / name / price may share its JSUTree. Hiding the root suppresses
// flame_* and fenu_t* even after show() on those tags — only hide letter gfx in the icon slot.
static void walkLetRowSubtree(J2DPane* pane, ResTIMG* letterTimg, bool rental, JUTTexture* savedContents,
                              bool isLetRoot, const LetAreaBounds& iconSlot, f32 listIconLeft) {
    if (!pane) {
        return;
    }
    if (isLetRoot) {
        pane->show();
    } else if (rental) {
        const bool letterInSlot =
            letterTimg && pane->isUsed(letterTimg) && iconSlot.valid && paneOverlapsSlot(pane, iconSlot);
        if (letterInSlot) {
            if (pane->getTypeID() == 17) {
                setRowLetterEnvelopeVisible((J2DWindow*)pane, savedContents, false);
            }
            pane->hide();
        } else if (listIconLeft >= 0.0f) {
            const u16  type      = pane->getTypeID();
            const bool maybeIcon = pane->getKind() == 'PIC1' || type == 17 || type == 18;
            if (maybeIcon) {
                const LetAreaBounds b        = calcPaneScreenBounds(pane);
                const bool          widePane = b.valid && b.w >= 120.0f;
                if (widePane) {
                    pane->show();
                } else {
                    pane->hide();
                }
            } else {
                pane->show();
            }
        } else {
            pane->show();
        }
    } else {
        pane->show();
    }

    JSUTreeIterator<J2DPane> iter = pane->getPaneTree()->getFirstChild();
    while (iter) {
        walkLetRowSubtree(*iter, letterTimg, rental, savedContents, false, iconSlot, listIconLeft);
        ++iter;
    }
}

static void applyLetRowSubtreeRental(J2DPane* letRoot, ResTIMG* letterTimg, bool rental,
                                     JUTTexture* savedContents, const LetAreaBounds& iconSlot,
                                     f32 listIconLeft) {
    walkLetRowSubtree(letRoot, letterTimg, rental, savedContents, true, iconSlot, listIconLeft);
}

static void setRowLetterGraphicsVisible(J2DPane* gfxPane, J2DWindow* win, JUTTexture* savedContents,
                                        bool showEnvelope) {
    if (gfxPane && gfxPane->getTypeID() == 17) {
        gfxPane->show();
        setRowLetterEnvelopeVisible((J2DWindow*)gfxPane, savedContents, showEnvelope);
        return;
    }
    if (gfxPane) {
        if (showEnvelope) {
            gfxPane->show();
        } else {
            gfxPane->hide();
        }
    }
    setRowLetterEnvelopeVisible(win, savedContents, showEnvelope);
}

// Ring-style baseline, clamped to the letter icon square inside each row.
static void calcWheelIconDrawSize(u8 itemNo, const ResTIMG* timg, f32 slotH, f32* outW,
                                  f32* outH) {
    const f32 itemScale = dItem_data::getTexScale(itemNo) / 100.0f;
    f32       tw        = kWheelIconBaselinePx;
    f32       th        = kWheelIconBaselinePx;
    if (timg && timg->width > 0) {
        tw = (f32)timg->width;
    }
    if (timg && timg->height > 0) {
        th = (f32)timg->height;
    }

    f32 w = (tw / kWheelIconBaselinePx) * kWheelIconBaselinePx * itemScale;
    f32 h = (th / kWheelIconBaselinePx) * kWheelIconBaselinePx * itemScale;

    const f32 maxH = std::max(slotH - kRowWheelIconInsetY * 2.0f, 12.0f);
    const f32 maxW = std::min(maxH, kRowLetterIconBoxMaxW);
    const f32 s    = std::min(maxW / w, maxH / h);
    if (s < 1.0f) {
        w *= s;
        h *= s;
    }
    *outW = w;
    *outH = h;
}

static bool applyShopItemIcon(JKRArchive* arc, u8 itemNo, void* texBuf, J2DPicture* pic) {
    if (!pic || itemNo == 0xff) {
        return false;
    }
    if (!dComIfGp_getItemIconArchive() && arc) {
        dComIfGp_setItemIconArchive(arc);
    }

    int param9 = -1;
    if (itemNo == (u8)dItemNo_COPY_ROD_e) {
        param9 = 0x57;
    }
    if (texBuf && dComIfGp_getItemIconArchive()) {
        const int texNum =
            dMeter2Info_readItemTexture(itemNo, texBuf, pic, nullptr, nullptr, nullptr, nullptr,
                                        nullptr, nullptr, param9);
        if (texNum > 0) {
            pic->setBlackWhite(JUtility::TColor(0, 0, 0, 0),
                               JUtility::TColor(255, 255, 255, 255));
            return true;
        }
    }

    ResTIMG* img = loadShopItemIconTimg(arc, itemNo, texBuf);
    if (!img) {
        return false;
    }
    pic->changeTexture(img, 0);
    dMeter2Info_setItemColor(itemNo, pic, nullptr, nullptr, nullptr);
    pic->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(255, 255, 255, 255));
    return true;
}

static f32 calcDescLineSpace(const J2DTextBox* box, JUTFont* font, f32 ruledLinePitch) {
    if (ruledLinePitch > 1.0f) {
        return ruledLinePitch;
    }
    if (box) {
        const f32 bloLine = box->getLineSpace();
        if (bloLine > 1.0f) {
            return bloLine;
        }
    }
    if (font && font->getCellHeight() > 0) {
        f32 lineSpace = font->getLeading();
        if (box) {
            J2DTextBox::TFontSize fs;
            box->getFontSize(fs);
            if (fs.mSizeY > 0.0f) {
                lineSpace *= fs.mSizeY / (f32)font->getCellHeight();
            }
        }
        if (lineSpace > 1.0f) {
            return lineSpace;
        }
    }
    return 20.0f;
}
static void showPaneTree(J2DPane* pane) {
    if (!pane) {
        return;
    }
    if (!pane->isVisible()) {
        pane->show();
    }
    for (J2DPane* child = pane->getFirstChildPane(); child; child = child->getNextChildPane()) {
        showPaneTree(child);
    }
}

static J2DPicture* findPictureInPane(J2DPane* pane) {
    if (!pane) {
        return nullptr;
    }
    if (pane->getKind() == 'PIC1') {
        return (J2DPicture*)pane;
    }
    for (J2DPane* child = pane->getFirstChildPane(); child; child = child->getNextChildPane()) {
        J2DPicture* pic = findPictureInPane(child);
        if (pic) {
            return pic;
        }
    }
    return nullptr;
}

static void applyMesgFontScaled(J2DTextBox* box, f32 scale) {
    if (!box) return;
    box->setFont(mDoExt_getMesgFont());
    J2DTextBox::TFontSize naturalSize;
    box->getFontSize(naturalSize);
    if (naturalSize.mSizeX > 0.0f && naturalSize.mSizeY > 0.0f) {
        naturalSize.mSizeX *= scale;
        naturalSize.mSizeY *= scale;
        box->setFontSize(naturalSize);
    }
}

// Whole shop shell (base / menu / shadow) — d_meter_HIO defaults mWindowPosY = -20.
static void applySelectScreenTransform(CPaneMgr* mgr) {
    if (!mgr) return;
    const dMeter_drawLetterHIO_c& hio = g_drawHIO.mLetterSelectScreen;
    mgr->setAlphaRate(1.0f);
    mgr->scale(hio.mWindowScale, hio.mWindowScale);
    mgr->paneTrans(hio.mWindowPosX, hio.mWindowPosY);
}

// Read-mode parchment only (spot + window_base).
static void applyLetterWindowTransform(CPaneMgr* mgr) {
    if (!mgr) return;
    const dMeter_drawLetterHIO_c& hio = g_drawHIO.mLetterSelectScreen;
    mgr->setAlphaRate(1.0f);
    mgr->scale(hio.mLetterWindowScale, hio.mLetterWindowScale);
    mgr->paneTrans(hio.mLetterWindowPosX, hio.mLetterWindowPosY);
}

// Match d_menu_letter.cpp screenSetLetter() non-JPN + dALBWDialogue_c text layout.
static void configureLetterBaseTextScreen(J2DScreen* screen, J2DTextBox** outDescBox,
                                        J2DTextBox** outDescOutFontBox) {
    *outDescBox = nullptr;
    if (outDescOutFontBox) *outDescOutFontBox = nullptr;
    if (!screen) return;

    J2DPane* p;
    // n_3line / n_e4line toggled per selection in updateDescParchment().
    if ((p = screen->search(MULTI_CHAR('n_3fline')))) p->hide();
    if ((p = screen->search(MULTI_CHAR('t3f_s'))))     p->hide();
    if ((p = screen->search(MULTI_CHAR('mg_3flin'))))  p->hide();
    if ((p = screen->search(MULTI_CHAR('mg_3f_s'))))   p->hide();
    if ((p = screen->search(MULTI_CHAR('mg_3f'))))     p->hide();
    if ((p = screen->search('t3_s')))                 p->hide();
    // mg_3line holds the item-icon slot in n_3line layout; toggled in updateDescParchment().
    if ((p = screen->search(MULTI_CHAR('jp_fri_n'))))  p->hide();
    if ((p = screen->search('jp_n')))                 p->hide();
    if ((p = screen->search(MULTI_CHAR('p_texts'))))   p->hide();
    if ((p = screen->search(MULTI_CHAR('p_text'))))    p->hide();
    // Hide mg_e4lin — vanilla feeds it via OutFont; drawing it uninitialized crashes.
    if ((p = screen->search(MULTI_CHAR('mg_e4lin'))))   p->hide();
    // Item-box chrome / placeholder pics (uninitialized = solid black if drawn).
    static const u64 kHideItemBoxPics[] = {
        MULTI_CHAR('set_it_n'), MULTI_CHAR('o_gd_n'), MULTI_CHAR('item_p_n'),
        MULTI_CHAR('in_pic_n'), MULTI_CHAR('p_item_n'),
    };
    for (int i = 0; i < (int)(sizeof(kHideItemBoxPics) / sizeof(kHideItemBoxPics[0])); ++i) {
        if ((p = screen->search(kHideItemBoxPics[i]))) {
            p->hide();
        }
    }

    J2DTextBox* tb = (J2DTextBox*)screen->search('t4_s');
    if (!tb) {
        OSReport("ALBW Shop create: t4_s missing on zelda_letter_window_base.blo\n");
        return;
    }
    applyMesgFontScaled(tb, kDescFontScale);
    tb->setBlackWhite(JUtility::TColor(0, 0, 0, 0),
                      JUtility::TColor(65, 55, 30, 255));
    tb->setString(0x200, "");
    tb->show();
    *outDescBox = tb;
    if (outDescOutFontBox) {
        *outDescOutFontBox = nullptr;
    }
}

// Vanilla letter read uses line09–line20 on window_base (n_e4line layout).
static f32 measureParchmentLinePitch(J2DScreen* screen) {
    if (!screen) return 0.0f;
    J2DPane* line0 = screen->search(MULTI_CHAR('line09'));
    J2DPane* line1 = screen->search(MULTI_CHAR('line10'));
    if (!line0 || !line1) return 0.0f;

    const LetAreaBounds b0 = calcPaneScreenBounds(line0);
    const LetAreaBounds b1 = calcPaneScreenBounds(line1);
    if (!b0.valid || !b1.valid) return 0.0f;

    const f32 pitch = std::fabs(b0.y - b1.y);
    return (pitch > 1.0f) ? pitch : 0.0f;
}

// Ruled-line Y in parchment scissor space (line09 = locked body, line12 = below item box).
static f32 calcDescRuledLineY(J2DScreen* screen, CPaneMgr* textMgr, u64 lineTag, f32 descX, f32 descY,
                              f32 descH) {
    if (!screen || !textMgr) {
        return descY + kDescTextPadY;
    }
    J2DPane* linePane = screen->search(lineTag);
    if (!linePane) {
        return descY + kDescTextPadY;
    }
    const LetAreaBounds line = calcPaneScreenBounds(linePane);
    const LetAreaBounds root = calcPaneScreenBounds(textMgr->getPanePtr());
    if (!line.valid || !root.valid) {
        return descY + kDescTextPadY;
    }
    (void)root;
    if (line.y >= descY && line.y < descY + descH) {
        return line.y;
    }
    return descY + kDescTextPadY;
}

static f32 calcDescTextWidth(f32 descColumnW, J2DTextBox* descBox, CPaneMgr* textMgr) {
    const f32 colW = descColumnW - kDescTextPadX * 2.0f;
    if (!descBox || !textMgr || colW <= 1.0f) {
        return colW;
    }
    const LetAreaBounds root = calcPaneScreenBounds(textMgr->getPanePtr());
    const LetAreaBounds tb   = calcPaneScreenBounds(descBox);
    if (!root.valid || !tb.valid) {
        return colW;
    }
    const f32 relX = tb.x - root.x;
    const f32 insetW = tb.w;
    // t4_s spans the full letter window; only use its horizontal inset, not full width.
    if (relX >= 0.0f && relX < descColumnW && insetW > 1.0f && insetW < colW) {
        return insetW;
    }
    return colW;
}

static J2DPicture* findLargestIconPicture(J2DPane* root) {
    if (!root) {
        return nullptr;
    }

    const LetAreaBounds rootB = calcPaneScreenBounds(root);
    if (!rootB.valid) {
        return findPictureInPane(root);
    }

    const f32 maxCenterY = rootB.y + rootB.h * 0.58f;
    J2DPicture* best     = nullptr;
    f32         bestArea = 0.0f;

    struct StackEntry {
        J2DPane* pane;
    };
    StackEntry stack[48];
    int sp = 0;
    stack[sp++].pane = root;

    while (sp > 0) {
        J2DPane* pane = stack[--sp].pane;
        if (!pane) {
            continue;
        }
        if (pane->getKind() == 'PIC1') {
            const LetAreaBounds b = calcPaneScreenBounds(pane);
            if (b.valid) {
                const f32 centerY = b.y + b.h * 0.5f;
                const f32 area    = b.w * b.h;
                if (centerY <= maxCenterY && area > bestArea) {
                    bestArea = area;
                    best     = (J2DPicture*)pane;
                }
            }
        }
        for (J2DPane* child = pane->getFirstChildPane(); child; child = child->getNextChildPane()) {
            if (sp < (int)(sizeof(stack) / sizeof(stack[0]))) {
                stack[sp++].pane = child;
            }
        }
    }

    return best ? best : findPictureInPane(root);
}

static J2DPicture* resolveDescItemIconPicture(J2DScreen* textScreen, J2DPane* n3line,
                                              J2DPane* mg3line) {
    static const u64 kPicTags[] = {
        MULTI_CHAR('set_it_n'), MULTI_CHAR('o_gd_n'), MULTI_CHAR('item_p_n'),
        MULTI_CHAR('in_pic_n'), MULTI_CHAR('p_item_n'),
    };
    if (textScreen) {
        for (int i = 0; i < (int)(sizeof(kPicTags) / sizeof(kPicTags[0])); ++i) {
            if (J2DPicture* pic = (J2DPicture*)textScreen->search(kPicTags[i])) {
                return pic;
            }
        }
    }
    if (J2DPicture* pic = findLargestIconPicture(mg3line)) {
        return pic;
    }
    if (J2DPicture* pic = findLargestIconPicture(n3line)) {
        return pic;
    }
    return nullptr;
}

static void restoreScissor(J2DGrafContext* gfx, u32 l, u32 t, u32 w, u32 h) {
    gfx->scissor((f32)l, (f32)t, (f32)w, (f32)h);
    gfx->setScissor();
}

static void hidePaneTag(J2DScreen* screen, u64 tag) {
    if (!screen) {
        return;
    }
    if (J2DPane* p = screen->search(tag)) {
        p->hide();
    }
}

static void hidePaneSubtree(J2DPane* pane) {
    for (J2DPane* p = pane; p != nullptr; p = p->getNextChildPane()) {
        p->hide();
        hidePaneSubtree(p->getFirstChildPane());
    }
}

static bool paneIsAncestorOf(J2DPane* ancestor, J2DPane* node) {
    if (!ancestor || !node) {
        return false;
    }
    for (J2DPane* p = node; p != nullptr; p = p->getParentPane()) {
        if (p == ancestor) {
            return true;
        }
    }
    return false;
}

// menu_10n / fmenu_10 on 6menu share the 5th row Y with fenu_t10; hiding their subtree was
// suppressing the price pane. Hide only non-price siblings, or the whole hint if separate.
static void hideMenuHintOverlapKeepingPrice(J2DPane* hintRoot, J2DPane* pricePane) {
    if (!hintRoot || !pricePane) {
        return;
    }
    if (hintRoot == pricePane) {
        return;
    }
    if (!paneIsAncestorOf(hintRoot, pricePane)) {
        hintRoot->hide();
        return;
    }
    JSUTreeIterator<J2DPane> iter = hintRoot->getPaneTree()->getFirstChild();
    while (iter != nullptr) {
        J2DPane* child = *iter;
        if (child != pricePane && !paneIsAncestorOf(child, pricePane)) {
            hidePaneSubtree(child);
        }
        ++iter;
    }
    hintRoot->show();
    showPaneAndAncestors(pricePane);
}

static void suppressFifthRowFooterOverlap(J2DScreen* menuScreen, J2DTextBox* fifthRowPrice) {
    if (!menuScreen || !fifthRowPrice) {
        return;
    }
    static const u64 kOverlapTags[] = {
        MULTI_CHAR('menu_10n'), MULTI_CHAR('fmenu_10'), MULTI_CHAR('menu_t10'),
    };
    for (int i = 0; i < (int)(sizeof(kOverlapTags) / sizeof(kOverlapTags[0])); ++i) {
        hideMenuHintOverlapKeepingPrice(menuScreen->search(kOverlapTags[i]), fifthRowPrice);
    }
}

static void hideFooterSlotTree(J2DScreen* screen, u64 tag) {
    if (!screen) {
        return;
    }
    hidePaneSubtree(screen->search(tag));
}

// Footer hint tags on select_base.blo / shadow (NOT row prices).
static void hideFooterHintsOnBaseScreen(J2DScreen* screen) {
    if (!screen) {
        return;
    }
    static const u64 kHideTags[] = {
        MULTI_CHAR('menu_6n'),  MULTI_CHAR('menu_7n'),  MULTI_CHAR('menu_8n'),
        MULTI_CHAR('menu_9n'),  MULTI_CHAR('menu_10n'), MULTI_CHAR('fmenu_6n'),
        MULTI_CHAR('fmenu_7n'), MULTI_CHAR('fmenu_8n'), MULTI_CHAR('fmenu_9n'),
        MULTI_CHAR('fmenu_10'), MULTI_CHAR('menu_f6'),  MULTI_CHAR('menu_f7'),
        MULTI_CHAR('menu_f8'),  MULTI_CHAR('menu_t9'),  MULTI_CHAR('menu_t10'),
        MULTI_CHAR('menu_t11'), MULTI_CHAR('wi_btn_n'), MULTI_CHAR('w_btn_n'),
        MULTI_CHAR('abtn_n'),   MULTI_CHAR('gcabtn_n'), MULTI_CHAR('g_abtn_n'),
        MULTI_CHAR('wps_text'), MULTI_CHAR('w_p_text'), MULTI_CHAR('g_ps_txt'),
        MULTI_CHAR('g_p_text'), MULTI_CHAR('fgps_tx1'), MULTI_CHAR('fgp_tex1'),
        MULTI_CHAR('fwpstex1'), MULTI_CHAR('fwp_tex1'),
    };
    for (int i = 0; i < (int)(sizeof(kHideTags) / sizeof(kHideTags[0])); ++i) {
        hidePaneTag(screen, kHideTags[i]);
    }
}

// Footer hints on 6menu.blo — never tag-hide menu_10n / fmenu_10 (5th row overlaps fenu_t10).
static void hideFooterHintsOnMenuScreen(J2DScreen* screen) {
    if (!screen) {
        return;
    }
    static const u64 kHideTags[] = {
        MULTI_CHAR('menu_6n'), MULTI_CHAR('menu_7n'), MULTI_CHAR('menu_8n'),
        MULTI_CHAR('menu_9n'), MULTI_CHAR('fmenu_6n'), MULTI_CHAR('fmenu_7n'),
        MULTI_CHAR('fmenu_8n'), MULTI_CHAR('fmenu_9n'),
    };
    for (int i = 0; i < (int)(sizeof(kHideTags) / sizeof(kHideTags[0])); ++i) {
        hidePaneTag(screen, kHideTags[i]);
    }
}

static LetAreaBounds footerSlotFromTag(J2DScreen* screen, u64 tag) {
    if (!screen) {
        return {};
    }
    J2DPane* pane = screen->search(tag);
    const LetAreaBounds b = calcPaneScreenBounds(pane);
    if (b.valid) {
        hidePaneSubtree(pane);
    }
    return b;
}

static void rehideShopFooterPanes(J2DScreen* baseScreen, J2DScreen* sdwScreen, J2DScreen* menuScreen,
                                  J2DTextBox* fifthRowPrice) {
    hideFooterHintsOnBaseScreen(baseScreen);
    hideFooterHintsOnBaseScreen(sdwScreen);
    hideFooterHintsOnMenuScreen(menuScreen);
    suppressFifthRowFooterOverlap(menuScreen, fifthRowPrice);

    static const u64 kBaseSlotTags[] = {
        MULTI_CHAR('fwpstex1'), MULTI_CHAR('fwp_tex1'), MULTI_CHAR('fgp_tex1'),
        MULTI_CHAR('fgps_tx1'),
        // Hide ONLY the leaked glyphs/text of the vanilla page-nav footer —
        // the GC L/R buttons (g_lbtn_n / g_rbtn_n), the GC + Wii hint text
        // (g_text_n / w_text_n), and the Wii scroll arrows / cross (yaji_l /
        // yaji_r / wi_juji).  The g_base / w_base black slot backgrounds are
        // deliberately left visible so the footer keeps its dark backing.
        MULTI_CHAR('g_lbtn_n'), MULTI_CHAR('g_rbtn_n'), MULTI_CHAR('g_text_n'),
        MULTI_CHAR('w_text_n'), MULTI_CHAR('wi_juji'),  MULTI_CHAR('yaji_l'),
        MULTI_CHAR('yaji_r'),
    };
    for (int i = 0; i < (int)(sizeof(kBaseSlotTags) / sizeof(kBaseSlotTags[0])); ++i) {
        hideFooterSlotTree(baseScreen, kBaseSlotTags[i]);
        hideFooterSlotTree(sdwScreen, kBaseSlotTags[i]);
    }
}

// ============================================
// NEW CODE — ALBW Port (Footer bar — simplified single-rect setup)
// Replaces the old three-slot (stick / action / tagline) approach.
// Probes the actual base.blo footer textboxes BEFORE hiding so
// BLO-baked positions are valid, then collapses to one full-width bar.
// ============================================
static void setupShopFooterBar(J2DScreen* baseScreen, J2DScreen* sdwScreen, J2DScreen* menuScreen,
                                JKRArchive* arc, J2DPicture** outStickPic, LetAreaBounds* outBar) {
    // Step 1: read footer bar position from the first resolving base.blo footer pane.
    // Probe BEFORE any hide call so BLO-baked transforms are still accessible.
    if (outBar && baseScreen) {
        static const u64 kProbe[] = {
            MULTI_CHAR('fwpstex1'), MULTI_CHAR('fwp_tex1'),
            MULTI_CHAR('fgps_tx1'), MULTI_CHAR('fgp_tex1'),
        };
        for (int i = 0; i < 4 && !outBar->valid; ++i) {
            if (J2DPane* p = baseScreen->search(kProbe[i])) {
                const LetAreaBounds b = calcPaneScreenBounds(p);
                if (b.valid) {
                    outBar->x     = 0.0f;
                    outBar->y     = b.y;
                    outBar->w     = mDoGph_gInf_c::getWidthF();
                    outBar->h     = b.h;
                    outBar->valid = true;
                }
            }
        }
    }

    // Step 2: hide all footer chrome (safe split: base/shadow via base list, menu via menu list).
    hideFooterHintsOnBaseScreen(baseScreen);
    hideFooterHintsOnBaseScreen(sdwScreen);
    hideFooterHintsOnMenuScreen(menuScreen);

    // Step 3: load the nunchuk stick BTI (drawn as a square; see drawShopFooter).
    if (outStickPic && arc) {
        ResTIMG* timg = (ResTIMG*)JKRGetNameResource("im_wiicon_sthick_00.bti", arc);
        if (!timg)
            timg = (ResTIMG*)arc->getResource('TIMG', "im_wiicon_sthick_00.bti");
        if (timg) {
            *outStickPic = JKR_NEW J2DPicture(timg);
            if (*outStickPic) {
                (*outStickPic)->setBasePosition(J2DBasePosition_4);
                (*outStickPic)->setBlackWhite(JUtility::TColor(0, 0, 0, 0),
                                              JUtility::TColor(255, 255, 255, 255));
            }
        }
    }
}
// ============================================
// NEW CODE ENDS HERE
// ============================================

static f32 mesgLineWidth(JUTFont* font, const char* line, f32 fontSizeX) {
    f32 w = 0.0f;
    for (const u8* c = (const u8*)line; *c != '\0'; ++c) {
        JUTFont::TWidth cw;
        font->getWidthEntry(*c, &cw);
        w += (f32)cw.field_0x1 * (fontSizeX / (f32)font->getCellWidth());
    }
    return w;
}

static int countWrapLines(const char* text) {
    int lines = 0;
    for (const char* p = text; *p != '\0'; ++p) {
        if (*p == '\n') {
            ++lines;
        }
    }
    return lines + 1;
}

static void drawRowPriceMesg(J2DGrafContext* gfx, const LetAreaBounds& slot, const char* text,
                            JUtility::TColor ink) {
    if (!gfx || !slot.valid || text == nullptr || text[0] == '\0') {
        return;
    }
    JUTFont* font = mDoExt_getMesgFont();
    if (!font) {
        return;
    }
    const f32 fontSizeX = 8.5f * kPriceFontScale;
    const f32 fontSizeY = 8.5f * kPriceFontScale;
    const f32 lineW     = mesgLineWidth(font, text, fontSizeX);
    const f32 textX     = slot.x + slot.w - lineW - 2.0f;
    const f32 textY     = slot.y + 0.5f * (slot.h - fontSizeY);

    J2DPrint print(font, 0.0f, fontSizeY, ink, ink, JUtility::TColor(0, 0, 0, 0), ink);
    print.setFontSize(fontSizeX, fontSizeY);
    font->pushDrawState();
    print.initiate();
    print.locate(textX, textY);
    print.print(textX, textY, 255, "%s", text);
    font->popDrawState();
}

static void drawFooterTextBlock(J2DGrafContext* gfx, JUTFont* font, const LetAreaBounds& slot,
                                const char* text, f32 fontSizeX, f32 fontSizeY, bool goldTildes) {
    if (!gfx || !font || !slot.valid || !text || text[0] == '\0') {
        return;
    }

#ifdef TARGET_PC
    dusk::frame_interp::set_ui_tick_pending(true);
#endif

    J2DOrthoGraph* ortho = (J2DOrthoGraph*)gfx;
    ortho->setOrtho(mDoGph_gInf_c::getMinXF(), mDoGph_gInf_c::getMinYF(),
                    mDoGph_gInf_c::getWidthF(), mDoGph_gInf_c::getHeightF(), -1.0f, 1.0f);
    ortho->setup2D();
    ortho->setPort();

    const JUtility::TColor white(255, 255, 255, 255);
    const JUtility::TColor gold(210, 175, 55, 255);
    const f32              lineSpace = fontSizeY * 1.05f;
    const int              numLines  = countWrapLines(text);
    const f32              blockH    = (f32)numLines * lineSpace;
    f32                    curY      = slot.y + (slot.h - blockH) * 0.5f + 1.0f;

    J2DPrint print(font, 0.0f, lineSpace, white, white, JUtility::TColor(0, 0, 0, 0), white);
    print.setFontSize(fontSizeX, fontSizeY);
    font->pushDrawState();

    for (const char* p = text; *p != '\0' && curY < slot.y + slot.h; ) {
        char line[128];
        size_t len = 0;
        while (p[len] != '\0' && p[len] != '\n' && len < sizeof(line) - 1) {
            ++len;
        }
        memcpy(line, p, len);
        line[len] = '\0';
        p += len;
        if (*p == '\n') {
            ++p;
        }
        if (line[0] == '\0') {
            curY += lineSpace;
            continue;
        }

        const f32 textX = slot.x + 0.5f * (slot.w - mesgLineWidth(font, line, fontSizeX));
        print.initiate();
        print.locate(textX, curY);
        print.print(textX, curY, 255, "%s", line);

        if (goldTildes) {
            J2DPrint goldPrint(font, 0.0f, lineSpace, gold, gold, JUtility::TColor(0, 0, 0, 0), gold);
            goldPrint.setFontSize(fontSizeX, fontSizeY);
            for (const char* t = line; *t != '\0'; ++t) {
                if (*t == '~') {
                    f32 tildeX = textX;
                    for (const char* u = line; u < t; ++u) {
                        JUTFont::TWidth w;
                        font->getWidthEntry((u8)*u, &w);
                        tildeX += (f32)w.field_0x1 * (fontSizeX / (f32)font->getCellWidth());
                    }
                    goldPrint.initiate();
                    goldPrint.locate(tildeX, curY);
                    goldPrint.print(tildeX, curY, 255, "~");
                }
            }
        }

        curY += lineSpace;
    }

    font->popDrawState();
}


// ============================================
// NEW CODE — ALBW Port (Footer draw — single horizontal bar)
// bar = full-width strip derived from base.blo footer textbox Y/H.
// Stick icon: left-anchored square (min(bar.w, bar.h) * 0.92f).
// Remaining space (right of stick) is reserved for Cursor-defined content.
// Use drawFooterTextBlock(gfx, font, slot, text, szX, szY, goldTildes)
// to fill in button hints and tagline as sub-rects of bar.
// ============================================
static void drawShopFooter(J2DGrafContext* gfx, J2DPicture* stickPic, const LetAreaBounds& bar) {
    if (!gfx || !bar.valid) {
        return;
    }

    // Square nunchuk stick icon — left-anchored, height-constrained
    const f32 iconSide = std::min(bar.w, bar.h) * 0.92f;
    if (stickPic) {
        const f32 iconX = bar.x + 4.0f + kFooterStickOffsetX;
        const f32 iconY = bar.y + (bar.h - iconSide) * 0.5f;
        stickPic->show();
        stickPic->draw(iconX, iconY, iconSide, iconSide, false, false, false);
    }

    JUTFont* font = mDoExt_getMesgFont();
    if (!font) {
        return;
    }

    // ============================================
    // Footer content — centred tagline + right-side action hints.
    // Layout: [stick] [center: tagline] [right: A/B hints].  Each text zone
    // is a sub-rect of the bar; drawFooterTextBlock centres text within it.
    // Up/down scroll arrows beside the stick are a follow-up refinement.
    // ============================================
    const f32 contentX = bar.x + 4.0f + iconSide + 6.0f;
    const f32 contentW = (bar.x + bar.w) - contentX;
    if (contentW <= 8.0f) {
        return;
    }

    static constexpr const char* kFooterTagline = "\"Hylians are raving about our service!\"";
    static constexpr const char* kFooterActions = "A: Buy/ B: Exit";

    const f32 fontSz  = bar.h * kFooterFontHeightRatio;
    const f32 centerW = contentW * kFooterCenterFraction;
    const f32 rightW  = contentW - centerW;

    const LetAreaBounds centerSlot = { contentX + kFooterTaglineOffsetX,
                                       bar.y + kFooterTaglineOffsetY, centerW, bar.h, true };
    drawFooterTextBlock(gfx, font, centerSlot, kFooterTagline, fontSz, fontSz, false);

    const LetAreaBounds rightSlot = { contentX + centerW + kFooterActionsOffsetX,
                                      bar.y + kFooterActionsOffsetY, rightW, bar.h, true };
    drawFooterTextBlock(gfx, font, rightSlot, kFooterActions, fontSz, fontSz, false);
}
// ============================================
// NEW CODE ENDS HERE
// ============================================

static void hideSelectBasePaginationPanes(J2DScreen* baseScreen) {
    if (!baseScreen) return;
    static const u64 kPips[9] = {
        MULTI_CHAR('pi_00_n'), MULTI_CHAR('pi_01_n'), MULTI_CHAR('pi_02_n'),
        MULTI_CHAR('pi_03_n'), MULTI_CHAR('pi_04_n'), MULTI_CHAR('pi_05_n'),
        MULTI_CHAR('pi_06_n'), MULTI_CHAR('pi_07_n'), MULTI_CHAR('pi_08_n'),
    };
    static const u64 kLights[9] = {
        MULTI_CHAR('pi_l_00'), MULTI_CHAR('pi_l_01'), MULTI_CHAR('pi_l_02'),
        MULTI_CHAR('pi_l_03'), MULTI_CHAR('pi_l_04'), MULTI_CHAR('pi_l_05'),
        MULTI_CHAR('pi_l_06'), MULTI_CHAR('pi_l_07'), MULTI_CHAR('pi_l_08'),
    };
    for (int i = 0; i < 9; ++i) {
        J2DPane* p;
        if ((p = baseScreen->search(kPips[i])))   p->hide();
        if ((p = baseScreen->search(kLights[i]))) p->hide();
    }
    if (J2DPane* p = baseScreen->search(MULTI_CHAR('t_t00'))) p->hide();
    if (J2DPane* p = baseScreen->search(MULTI_CHAR('wi_btn_n'))) p->hide();
}

dALBWShop_c::dALBWShop_c() = default;

dALBWShop_c::~dALBWShop_c() {
    for (int i = 0; i < 6; ++i) {
        JKR_DELETE(mpRowLetterMgr[i]);
        JKR_DELETE(mpRowItemPic[i]);
    }
    JKR_DELETE(mpItemBoxPic);
    JKR_DELETE(mpFooterStickPic);
    JKR_DELETE(mpParent);
    JKR_DELETE(mpBaseMgr);
    JKR_DELETE(mpSdwMgr);
    JKR_DELETE(mpMenuScreen);
    JKR_DELETE(mpBaseScreen);
    JKR_DELETE(mpSdwScreen);
    JKR_DELETE(mpDescSpotMgr);
    JKR_DELETE(mpDescTextMgr);
    JKR_DELETE(mpDescBoxMgr);
    JKR_DELETE(mpDescSpotScreen);
    JKR_DELETE(mpDescTextScreen);
    for (int i = 0; i < 6; ++i) {
        JKR_DELETE(mpRowPriceMgr[i]);
    }
    mpDescBox = nullptr;
    mpDescOutFontBox = nullptr;
    if (mpMount) {
        mpMount->destroy();
        mpMount = nullptr;
    }
    if (mpArchive) {
        JKRUnmountArchive(mpArchive);
        mpArchive = nullptr;
    }
    if (mpItemIconMount) {
        mpItemIconMount->destroy();
        mpItemIconMount = nullptr;
    }
    mpItemIconArchive = nullptr;
}

JKRArchive* dALBWShop_c::ensureItemIconArchive() {
    JKRArchive* arc = dComIfGp_getItemIconArchive();
    if (arc) {
        return arc;
    }
    if (mpItemIconArchive) {
        return mpItemIconArchive;
    }
    if (!mpItemIconMount) {
        mpItemIconMount = mDoDvdThd_mountArchive_c::create("/res/Layout/itemicon.arc", 0, nullptr);
    }
    if (!mpItemIconMount || mpItemIconMount->sync() == 0) {
        return nullptr;
    }
    mpItemIconArchive = (JKRArchive*)mpItemIconMount->getArchive();
    if (mpItemIconArchive) {
        dComIfGp_setItemIconArchive(mpItemIconArchive);
        OSReport("ALBW Shop: mounted itemicon.arc\n");
    }
    return mpItemIconArchive;
}

bool dALBWShop_c::update() {
    if (mReady) {
        ensureItemIconArchive();
        return true;
    }
    if (!mpMount)
        mpMount = mDoDvdThd_mountArchive_c::create("/res/Layout/letres.arc", 0, NULL);
    if (!mpArchive && mpMount && mpMount->sync() != 0) {
        mpArchive = (JKRArchive*)mpMount->getArchive();
        JKR_DELETE(mpMount);
        mpMount = nullptr;
        create();
        mReady = true;
    }
    return mReady;
}

void dALBWShop_c::create() {
    mpMenuScreen = JKR_NEW J2DScreen();
    mpMenuScreen->setPriority("zelda_letter_select_6menu.blo", 0x20000, mpArchive);
    dPaneClass_showNullPane(mpMenuScreen);
    if (mpArchive) {
        mRowLetterTimg = (ResTIMG*)JKRGetNameResource("ni_item_icon_letter.bti", mpArchive);
        mItemBoxTimg   = (ResTIMG*)JKRGetNameResource("tt_do_icon7_160_174.bti", mpArchive);
    }
    mpParent = JKR_NEW CPaneMgr(mpMenuScreen, MULTI_CHAR('n_all'), 2, NULL);
    applySelectScreenTransform(mpParent);

    // Vanilla 6menu layout (non-JPN): fenu_t0 = letter subject (item name),
    // fenu_t6 = sender name (item price).  Shadow panes fenu_t0s / fenu_t6s hidden.
    for (int i = 0; i < 6; ++i) {
        mpRowName[i]  = (J2DTextBox*)mpMenuScreen->search(kTagName[i]);
        mpRowPrice[i] = (J2DTextBox*)mpMenuScreen->search(kTagPrice[i]);
        mpRowSub[i]   = nullptr;
        if (mpRowName[i]) {
            applyMesgFontScaled(mpRowName[i], kRowNameFontScale);
            mpRowName[i]->setString(0x40, "");
        }
        if (mpRowPrice[i]) {
            applyMesgFontScaled(mpRowPrice[i], kPriceFontScale);
            mpRowPrice[i]->setString(0x20, "");
            mpRowPrice[i]->show();
            mpRowPriceMgr[i] = JKR_NEW CPaneMgr(mpMenuScreen, kTagPrice[i], 0, NULL);
            if (mpRowPriceMgr[i]) {
                mPriceInitCenterX[i] = mpRowPriceMgr[i]->getInitGlobalCenterPosX();
            }
        }
        mpRowFrame[i] = (J2DPicture*)mpMenuScreen->search(kTagRowFrame[i]);
        if (mpRowFrame[i]) {
            mRowFrameBaseW[i] = mpRowFrame[i]->getWidth();
            mRowFrameBaseH[i] = mpRowFrame[i]->getHeight();
        }
        if (J2DPane* sh = mpMenuScreen->search(kTagSubShadow[i])) sh->hide();
        if (J2DPane* sh = mpMenuScreen->search(kTagPriceShadow[i])) sh->hide();
        if (J2DPane* md = mpMenuScreen->search(kTagMidoku[i])) md->hide();
    }
    // Non-JPN letter select hides menu_t* row bars; leaving them visible draws full-width
    // chrome that gets scissor-clipped (black bar on the right) and shifts wheel icons.
    static const u64 kHideMenuRowTags[] = {
        MULTI_CHAR('menu_t0'),  MULTI_CHAR('menu_t1'),  MULTI_CHAR('menu_t2'),
        MULTI_CHAR('menu_t3'),  MULTI_CHAR('menu_t4'),  MULTI_CHAR('menu_t5'),
        MULTI_CHAR('menu_t0s'), MULTI_CHAR('menu_t1s'), MULTI_CHAR('menu_t2s'),
        MULTI_CHAR('menu_t3s'), MULTI_CHAR('menu_t4s'), MULTI_CHAR('menu_t5s'),
    };
    for (int i = 0; i < (int)(sizeof(kHideMenuRowTags) / sizeof(kHideMenuRowTags[0])); ++i) {
        hidePaneTag(mpMenuScreen, kHideMenuRowTags[i]);
    }
    for (int i = 0; i < 6; ++i) {
        mpRowLetterPane[i] = mpMenuScreen->search(kTagRowLetter[i]);
        mpRowLetterMgr[i]  = JKR_NEW CPaneMgr(mpMenuScreen, kTagRowLetter[i], 0, NULL);
        mpRowLetterWin[i]     = nullptr;
        mpRowLetterGfx[i]     = nullptr;
        mRowLetterContentsTex[i] = nullptr;
        if (mpRowLetterPane[i]) {
            J2DPane* screenRoot = mpMenuScreen->search('ROOT');
            mpRowLetterGfx[i] = findLetterGfxPaneNear(screenRoot, mRowLetterTimg, mpRowLetterPane[i],
                                                      mpRowLetterMgr[i]);
            mpRowBloItemGfx[i] =
                findIconGfxPaneNear(screenRoot, mpRowLetterPane[i], mpRowLetterMgr[i]);
            if (mpRowBloItemGfx[i] == mpRowLetterGfx[i]) {
                mpRowBloItemGfx[i] = nullptr;
            }
            if (mpRowLetterGfx[i] && mpRowLetterGfx[i]->getTypeID() == 17) {
                mpRowLetterWin[i] = (J2DWindow*)mpRowLetterGfx[i];
                mRowLetterContentsTex[i] =
                    J2DWindowContentsAccess::getContents(mpRowLetterWin[i]);
            }
            mpRowLetter[i] = findLargestIconPicture(mpRowLetterPane[i]);
            if (!mpRowLetter[i]) {
                mpRowLetter[i] = findPictureInPane(mpRowLetterPane[i]);
            }
            if (!mpRowLetter[i] && mpRowLetterPane[i]->getKind() == 'PIC1') {
                mpRowLetter[i] = (J2DPicture*)mpRowLetterPane[i];
            }
        }
        if (mpRowLetterMgr[i]) {
            mRowLetterInitCenterX[i] = mpRowLetterMgr[i]->getInitGlobalCenterPosX();
            mpRowLetterMgr[i]->setAlphaRate(1.0f);
            const f32 iconScale = g_drawHIO.mLetterSelectScreen.mUnselectBarScale;
            mpRowLetterMgr[i]->scale(iconScale, iconScale);
        }
        if (mpRowLetter[i]) {
            mRowLetterBaseW[i] = mpRowLetter[i]->getWidth();
            mRowLetterBaseH[i] = mpRowLetter[i]->getHeight();
        }
        // Letter-envelope reference size in letres BLO (not the PAN2 row container extent).
        mRowLetterBaseW[i] = kWheelIconBaselinePx;
        mRowLetterBaseH[i] = kWheelIconBaselinePx;
        if (mpRowLetter[i]) {
            mpRowLetter[i]->setBlackWhite(JUtility::TColor(0, 0, 0, 0),
                                          JUtility::TColor(255, 255, 255, 255));
        }
    }

    if (mpRowLetterPane[0]) {
        const u32 kind = mpRowLetterPane[0]->getKind();
        OSReport(
            "ALBW Shop create: let0 kind=%c%c%c%c pane=%p pic=%p gfx=%p win=%p contents=%p "
            "letterTimg=%p\n",
            (char)(kind >> 24), (char)(kind >> 16), (char)(kind >> 8), (char)kind,
            (void*)mpRowLetterPane[0], (void*)mpRowLetter[0], (void*)mpRowLetterGfx[0],
            (void*)mpRowLetterWin[0], (void*)mRowLetterContentsTex[0], (void*)mRowLetterTimg);
    } else {
        OSReport("ALBW Shop create: let0 pane=null\n");
    }

    // Start itemicon.arc load in parallel with first shop frames (draw uses cached slot bounds).
    ensureItemIconArchive();

    mpBaseScreen = JKR_NEW J2DScreen();
    mpBaseScreen->setPriority("zelda_letter_select_base.blo", 0x20000, mpArchive);
    dPaneClass_showNullPane(mpBaseScreen);
    hideSelectBasePaginationPanes(mpBaseScreen);
    mpBaseMgr = JKR_NEW CPaneMgr(mpBaseScreen, MULTI_CHAR('n_all'), 2, NULL);
    applySelectScreenTransform(mpBaseMgr);
    mLetAreaPane = mpBaseScreen->search(MULTI_CHAR('let_area'));
    OSReport("ALBW Shop create: let_area=%p\n", (void*)mLetAreaPane);

    mpSdwScreen = JKR_NEW J2DScreen();
    mpSdwScreen->setPriority("zelda_letter_select_shadow.blo", 0x20000, mpArchive);
    dPaneClass_showNullPane(mpSdwScreen);
    mpSdwMgr = JKR_NEW CPaneMgr(mpSdwScreen, MULTI_CHAR('n_all'), 2, NULL);
    applySelectScreenTransform(mpSdwMgr);
    setupShopFooterBar(mpBaseScreen, mpSdwScreen, mpMenuScreen, mpArchive,
                       &mpFooterStickPic, &mFooterBar);

// ============================================
// NEW CODE — ALBW Port (Shop Description Parchment)
// Vanilla letter read UI uses TWO BLOs (d_menu_letter.cpp screenSetLetter):
//   zelda_letter_window_spot.blo — parchment frame
//   zelda_letter_window_base.blo — t4_s body text (NOT on spot.blo)
// Both are positioned via CPaneMgr on n_all with mLetterWindowPos/Scale.
// ============================================
    mpDescSpotScreen = JKR_NEW J2DScreen();
    if (mpDescSpotScreen->setPriority("zelda_letter_window_spot.blo", 0x20000, mpArchive)) {
        dPaneClass_showNullPane(mpDescSpotScreen);
        mpDescSpotMgr = JKR_NEW CPaneMgr(mpDescSpotScreen, MULTI_CHAR('n_all'), 2, NULL);
        applyLetterWindowTransform(mpDescSpotMgr);
        OSReport("ALBW Shop create: spot OK scr=%p mgr=%p\n",
                 (void*)mpDescSpotScreen, (void*)mpDescSpotMgr);
    } else {
        OSReport("ALBW Shop create: FAILED zelda_letter_window_spot.blo\n");
        JKR_DELETE(mpDescSpotScreen);
        mpDescSpotScreen = nullptr;
    }

    mpDescTextScreen = JKR_NEW J2DScreen();
    if (mpDescTextScreen->setPriority("zelda_letter_window_base.blo", 0x20000, mpArchive)) {
        dPaneClass_showNullPane(mpDescTextScreen);
        configureLetterBaseTextScreen(mpDescTextScreen, &mpDescBox, &mpDescOutFontBox);
        mpDescTextMgr = JKR_NEW CPaneMgr(mpDescTextScreen, MULTI_CHAR('n_all'), 2, NULL);
        applyLetterWindowTransform(mpDescTextMgr);
        if (mpDescBox) {
            mpDescBoxMgr = JKR_NEW CPaneMgr(mpDescTextScreen, 't4_s', 0, NULL);
        }
        mpDescN3linePane  = mpDescTextScreen->search(MULTI_CHAR('n_3line'));
        mpDescN4linePane  = mpDescTextScreen->search(MULTI_CHAR('n_e4line'));
        mpDescMg3linePane = mpDescTextScreen->search(MULTI_CHAR('mg_3line'));
        mpDescItemPic     = resolveDescItemIconPicture(mpDescTextScreen, mpDescN3linePane,
                                                       mpDescMg3linePane);
        if (mpDescItemPic) {
            mDescItemPicBaseW = mpDescItemPic->getWidth();
            mDescItemPicBaseH = mpDescItemPic->getHeight();
            if (mDescItemPicBaseW <= 0.0f) mDescItemPicBaseW = 1.0f;
            if (mDescItemPicBaseH <= 0.0f) mDescItemPicBaseH = 1.0f;
            mpDescItemPic->hide();
        }
        if (mpDescMg3linePane) mpDescMg3linePane->hide();
        if (mpDescN3linePane) mpDescN3linePane->hide();
        if (mpDescN4linePane) mpDescN4linePane->show();
        OSReport("ALBW Shop create: base OK t4_s=%p n3=%p mg3=%p itempic=%p\n", (void*)mpDescBox,
                 (void*)mpDescN3linePane, (void*)mpDescMg3linePane, (void*)mpDescItemPic);
    } else {
        OSReport("ALBW Shop create: FAILED zelda_letter_window_base.blo\n");
        JKR_DELETE(mpDescTextScreen);
        mpDescTextScreen = nullptr;
    }
// ============================================
// NEW CODE ENDS HERE
// ============================================

    mpTitleBox = (J2DTextBox*)mpBaseScreen->search(MULTI_CHAR('f_t_00'));
    if (!mpTitleBox)
        mpTitleBox = (J2DTextBox*)mpBaseScreen->search(MULTI_CHAR('t_t00'));
    if (mpTitleBox) {
        applyMesgFontScaled(mpTitleBox, kTitleFontScale);
        mpTitleBox->setString("Postman's Lending Service");
    }
}

void dALBWShop_c::applyListColumnLayout(f32 areaX, f32 listW) {
    // Extend the row bars toward the parchment. flame_* draws its window to the
    // resized local width frameW; its on-screen right edge works out to
    //   nb.x + (frameW / baseW) * nb.w   (nb = native screen bounds).
    // The old frameW = listW - inset tied the bar to the narrow list column,
    // landing the right edge well short of the parchment. Solve frameW so the
    // bar lands just before the parchment instead. Price layout below is
    // unchanged — it reads the native (pre-resize) frame bounds, so the price
    // keeps its tuned position regardless of the new bar length.
    // Target bar width is computed ONCE per row and cached.  calcPaneScreenBounds()
    // reflects the prior frame's resize(), so recomputing frameW from the live
    // bounds every frame forms a feedback loop that makes the bars (and the price
    // that tracks them) flicker.  The first frame reads the native (pre-resize)
    // bounds; every frame after reuses the cached width so resize() is constant.
    static f32 sCachedFrameW[6] = {-1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f};
    const f32 descX        = areaX + listW + kListDescGap;
    const f32 barDrawRight = descX - kRowFrameGapBeforeParchment;  // 0-640 canvas
    for (int i = 0; i < 6; ++i) {
        f32 frameW = listW - kRowFrameInsetX * 2.0f;  // fallback (old behaviour)
        if (mpRowFrame[i] && mRowFrameBaseW[i] > 1.0f) {
            if (sCachedFrameW[i] <= 0.0f) {
                const LetAreaBounds nb = calcPaneScreenBounds(mpRowFrame[i]);  // native on first frame
                if (nb.valid && nb.w > 1.0f) {
                    const f32 frac = (barDrawRight - nb.x) / nb.w;
                    if (frac > 0.05f) {
                        sCachedFrameW[i] = frac * mRowFrameBaseW[i];
                    }
                }
            }
            if (sCachedFrameW[i] > 0.0f) {
                frameW = sCachedFrameW[i];
            }
            mpRowFrame[i]->resize(frameW, mRowFrameBaseH[i]);
        }
        if (mpRowPriceMgr[i]) {
            f32 targetCenterX = areaX + listW - kPriceRightInset + kPriceExtraNudgeX;
            if (mpRowFrame[i]) {
                const LetAreaBounds frame = calcPaneScreenBounds(mpRowFrame[i]);
                if (frame.valid) {
                    targetCenterX = frame.x + frame.w - kPriceRightInset + kPriceExtraNudgeX;
                }
            }
            f32 offsetX = targetCenterX - mPriceInitCenterX[i];
            if (mPriceInitCenterX[i] <= 0.0f) {
                offsetX = kPricePaneShiftX + (mRowFrameBaseW[i] - frameW);
            }
            mpRowPriceMgr[i]->paneTrans(offsetX, 0.0f);
        }
    }
}

void dALBWShop_c::applyDescColumnShift(f32 shiftX) {
    if (mDescColumnShifted || shiftX <= 0.0f) return;
    if (mpDescTextMgr) mpDescTextMgr->paneTrans(shiftX, 0.0f);
    if (mpDescSpotMgr) mpDescSpotMgr->paneTrans(shiftX, 0.0f);
    mDescColumnShifted = true;
}

void dALBWShop_c::drawDescMesgText(J2DGrafContext* gfx, f32 x, f32 y, f32 w, f32 h,
                                   f32 ruledLinePitch) {
    if (!gfx || mDescBuf[0] == '\0' || w <= 1.0f || h <= 1.0f) {
        return;
    }

    JUTFont* font = mDoExt_getMesgFont();
    if (!font) {
        return;
    }

    // Draw inside the parchment scissor (x,y,w,h). Wrap width must be the column width,
    // not t4_s (full letter-window width) or text runs in one line until scissor clip.
    f32 textX = x + kDescTextPadX;
    const u64 descLineTag = mDescShowsItemBox ? MULTI_CHAR('line12') : MULTI_CHAR('line09');
    f32 textY = calcDescRuledLineY(mpDescTextScreen, mpDescTextMgr, descLineTag, x, y, h);
    if (ruledLinePitch > 1.0f) {
        if (mDescShowsItemBox) {
            textY -= ruledLinePitch * kDescRentableLineNudgeUp;
        } else {
            textY += ruledLinePitch * kDescLockedLineNudgeDown;
        }
    }
    if (mDescShowsItemBox) {
        textY += kDescRentablePixelNudgeY;
    }
    textY += kDescFirstLineNudgeY;
    const f32 textW = calcDescTextWidth(w, mpDescBox, mpDescTextMgr);
    f32 textH = h - (textY - y) - kDescTextPadY;
    f32 fontSizeX = 14.0f;
    f32 fontSizeY = 14.0f;

    if (mpDescBox && mpDescTextMgr) {
        const LetAreaBounds root = calcPaneScreenBounds(mpDescTextMgr->getPanePtr());
        const LetAreaBounds tb   = calcPaneScreenBounds(mpDescBox);
        if (root.valid && tb.valid) {
            const f32 relX = tb.x - root.x;
            if (relX >= 0.0f && relX <= kDescTextPadX + 6.0f) {
                textX = x + relX;
            }
        }
        J2DTextBox::TFontSize fs;
        mpDescBox->getFontSize(fs);
        if (fs.mSizeX > 0.0f && fs.mSizeY > 0.0f) {
            fontSizeX = fs.mSizeX;
            fontSizeY = fs.mSizeY;
        }
    }

#ifdef TARGET_PC
    dusk::frame_interp::set_ui_tick_pending(true);
#endif

    J2DOrthoGraph* ortho = (J2DOrthoGraph*)gfx;
    ortho->setOrtho(mDoGph_gInf_c::getMinXF(), mDoGph_gInf_c::getMinYF(),
                    mDoGph_gInf_c::getWidthF(), mDoGph_gInf_c::getHeightF(), -1.0f, 1.0f);
    ortho->setup2D();
    ortho->setPort();

    const JUtility::TColor ink(65, 55, 30, 255);
    f32 lineSpace = calcDescLineSpace(mpDescBox, font, ruledLinePitch);
    if (ruledLinePitch > 1.0f) {
        lineSpace = ruledLinePitch * kDescLineSpaceScale;
    }
    dALBW_wrapMesgWords(mDescWrapBuf, sizeof(mDescWrapBuf), mDescBuf, textW, font, fontSizeX);

    J2DPrint print(font, mpDescBox ? mpDescBox->getCharSpace() : 0.0f, lineSpace, ink, ink,
                   JUtility::TColor(0, 0, 0, 0), ink);
    print.setFontSize(fontSizeX, fontSizeY);
    font->pushDrawState();

    // printReturn re-wraps per character and splits words mid-glyph; draw our pre-wrapped lines.
    f32 curY = textY;
    for (const char* p = mDescWrapBuf; *p != '\0' && curY < y + textH; ) {
        char line[256];
        size_t len = 0;
        while (p[len] != '\0' && p[len] != '\n' && len < sizeof(line) - 1) {
            ++len;
        }
        memcpy(line, p, len);
        line[len] = '\0';
        p += len;
        if (*p == '\n') {
            ++p;
        }
        if (line[0] == '\0') {
            curY += lineSpace;
            continue;
        }

        const f32 baselineNudge =
            mDescShowsItemBox ? kDescRentableBaselineNudgeY : kDescBaselineNudgeY;
        print.initiate();
        print.locate(textX, curY);
        print.print(textX, curY + baselineNudge, 255, "%s", line);
        curY += lineSpace;
    }

    font->popDrawState();
}

void dALBWShop_c::updateDescParchment(bool showItemBox, u8 itemNo) {
    (void)itemNo;
    mDescShowsItemBox = showItemBox;

    // Never show letter-read item-box panes via BLO draw — they render as solid black quads
    // over the list / parchment when shifted into the shop layout.
    if (mpDescN3linePane) {
        mpDescN3linePane->hide();
    }
    if (mpDescMg3linePane) {
        mpDescMg3linePane->hide();
    }
    if (mpDescItemPic) {
        mpDescItemPic->hide();
    }
    if (mpDescN4linePane) {
        if (showItemBox) {
            mpDescN4linePane->hide();
        } else {
            showPaneTree(mpDescN4linePane);
        }
    }
}

void dALBWShop_c::populateRows() {
    int visCount = 0;
    const dALBWVisibleEntry* visList = dALBWRental_getVisibleList(&visCount);
    const int sel = dALBWRental_getSelectedIdx();

    if (sel < mScrollTop)
        mScrollTop = sel;
    else if (sel >= mScrollTop + 6)
        mScrollTop = sel - 5;
    const int maxScroll = (visCount > 6) ? (visCount - 6) : 0;
    if (mScrollTop > maxScroll) mScrollTop = maxScroll;
    if (mScrollTop < 0)         mScrollTop = 0;

    const dMeter_drawLetterHIO_c& hio = g_drawHIO.mLetterSelectScreen;
    char buf[64];
    for (int row = 0; row < 6; ++row) {
        if (!mpRowName[row]) continue;
        const int itemIdx = mScrollTop + row;
        if (itemIdx < visCount) {
            const dALBWVisibleEntry& ve    = visList[itemIdx];
            const bool               isSel = (itemIdx == sel);
            const char* name = (ve.purchasable || ve.showNameWhenSoldOut) ? ve.name : "?????";
            if (isSel) {
                snprintf(buf, sizeof(buf), "> %s", name);
            } else {
                snprintf(buf, sizeof(buf), "  %s", name);
            }
            mpRowName[row]->setString(buf);
            mpRowName[row]->show();
            if (mpRowPrice[row]) {
                snprintf(buf, sizeof(buf), "%d Rupees", ve.price);
                mpRowPrice[row]->setString(buf);
                mpRowPrice[row]->show();
            }
            if (isSel) {
                mpRowName[row]->setBlackWhite(hio.mSelectTextBack, hio.mSelectTextFront);
                if (mpRowPrice[row])
                    mpRowPrice[row]->setBlackWhite(hio.mSelectTextBack, hio.mSelectTextFront);
            } else {
                mpRowName[row]->setBlackWhite(JUtility::TColor(31, 24, 12, 0),
                                               JUtility::TColor(200, 180, 135, 255));
                if (mpRowPrice[row])
                    mpRowPrice[row]->setBlackWhite(JUtility::TColor(31, 24, 12, 0),
                                                   JUtility::TColor(180, 165, 120, 255));
            }
            if (mpRowFrame[row]) {
                if (isSel) {
                    mpRowFrame[row]->setBlackWhite(hio.mSelectBarBack, hio.mSelectBarFront);
                } else {
                    mpRowFrame[row]->setBlackWhite(JUtility::TColor(105, 95, 55, 255),
                                                   JUtility::TColor(200, 180, 135, 255));
                }
            }
        } else {
            mpRowName[row]->setString("");
            if (mpRowPrice[row]) mpRowPrice[row]->setString("");
        }
    }

    bool showItemBox = false;
    u8   itemNo      = 0xff;
    if (sel >= 0 && sel < visCount) {
        const dALBWVisibleEntry& ve = visList[sel];
        itemNo = ve.itemNo;
        if ((ve.purchasable || ve.showNameWhenSoldOut) && ve.desc) {
            snprintf(mDescBuf, sizeof(mDescBuf), "%s\n\nPrice:  %d Rupees", ve.desc, ve.price);
            showItemBox = true;
        } else {
            snprintf(mDescBuf, sizeof(mDescBuf), "Not in stock yet - Come back soon!!!");
        }
    } else {
        snprintf(mDescBuf, sizeof(mDescBuf), "Not in stock yet - Come back soon!!!");
    }
    if (mpDescBox) {
        mpDescBox->hide();
    }

    // ============================================
    // NEW CODE — ALBW Port (Shop Category Pages)
    // Repurpose the title pane as the page heading + counter, e.g. "Items (2/4)".
    // Shows the page title alone when only one page has content.
    // ============================================
    if (mpTitleBox) {
        char      titleBuf[64];
        const int pageNum   = dALBWRental_getPageNumber();
        const int pageCount = dALBWRental_getPageCount();
        if (pageCount > 1) {
            snprintf(titleBuf, sizeof(titleBuf), "%s   (%d/%d)",
                     dALBWRental_getPageTitle(), pageNum, pageCount);
        } else {
            snprintf(titleBuf, sizeof(titleBuf), "%s", dALBWRental_getPageTitle());
        }
        mpTitleBox->setString(titleBuf);
    }
    // ============================================
    // NEW CODE ENDS HERE
    // ============================================

    updateDescParchment(showItemBox, itemNo);
}

bool dALBWShop_c::ensureRowWheelPic(int row, u8 itemNo, JKRArchive* arc) {
    if (!arc || itemNo == 0xff || row < 0 || row >= 6) {
        return false;
    }

    if (!dComIfGp_getItemIconArchive()) {
        dComIfGp_setItemIconArchive(arc);
    }

    if (mpRowItemPic[row] && mRowItemPicItemNo[row] == itemNo) {
        mpRowItemPic[row]->setBasePosition(J2DBasePosition_0);
        return true;
    }

    JKR_DELETE(mpRowItemPic[row]);
    mpRowItemPic[row]       = nullptr;
    mRowItemPicItemNo[row]  = 0xff;

    const u8 iconItemNo = shopWheelIconItemNo(itemNo);
    ResTIMG* timg       = loadShopItemIconTimg(arc, itemNo, mRowItemTexBuf[row]);
    if (!timg) {
        OSReport("ALBW Shop: ensureRowWheelPic failed row=%d itemNo=%u iconNo=%u idx=%d\n", row,
                 (unsigned)itemNo, (unsigned)iconItemNo, (int)shopItemIconTexIdx(iconItemNo));
        return false;
    }

    mpRowItemPic[row] = JKR_NEW J2DPicture(timg);
    if (!mpRowItemPic[row]) {
        return false;
    }

    mpRowItemPic[row]->setBasePosition(J2DBasePosition_0);
    dMeter2Info_setItemColor(iconItemNo, mpRowItemPic[row], nullptr, nullptr, nullptr);
    mpRowItemPic[row]->setBlackWhite(JUtility::TColor(0, 0, 0, 0),
                                     JUtility::TColor(255, 255, 255, 255));
    mRowItemPicItemNo[row] = itemNo;
    return true;
}

bool dALBWShop_c::ensureItemBoxPic() {
    if (mpItemBoxPic) {
        mpItemBoxPic->setBasePosition(J2DBasePosition_0);
        return true;
    }
    if (!mItemBoxTimg) {
        return false;
    }
    mpItemBoxPic = JKR_NEW J2DPicture(mItemBoxTimg);
    if (!mpItemBoxPic) {
        return false;
    }
    mpItemBoxPic->setBasePosition(J2DBasePosition_0);
    mpItemBoxPic->setBlackWhite(JUtility::TColor(0, 0, 0, 0),
                                 JUtility::TColor(255, 255, 255, 255));
    mpItemBoxPic->hide();
    return true;
}

void dALBWShop_c::cacheRowLetterSlotBounds(int row) {
    const LetAreaBounds slot =
        calcRowIconSlotBounds(mpRowLetterMgr[row]);
    mRowIconSlot[row].x       = slot.x;
    mRowIconSlot[row].y       = slot.y;
    mRowIconSlot[row].w       = slot.w;
    mRowIconSlot[row].h       = slot.h;
    mRowIconSlot[row].valid   = slot.valid;
}

void dALBWShop_c::dumpRowIconDebug(JKRArchive* iconArc, int visCount,
                                   const dALBWVisibleEntry* visList, f32 listIconLeft) {
    char path[512];
    path[0] = '\0';
    const char* user = getenv("USERPROFILE");
    if (user && user[0] != '\0') {
        snprintf(path, sizeof(path), "%s/Documents/dusklight/albw_shop_debug.txt", user);
    } else {
        strncpy(path, "albw_shop_debug.txt", sizeof(path) - 1);
    }

    FILE* fp = fopen(path, "w");
    if (!fp) {
        fp = fopen("albw_shop_debug.txt", "w");
    }
    if (!fp) {
        return;
    }

    fprintf(fp, "path=%s\n", path);
    const int sel = dALBWRental_getSelectedIdx();
    fprintf(fp,
            "native_ui=1 globalIconArc=%p shopIconArc=%p mReady=%d visCount=%d sel=%d scrollTop=%d "
            "maxScroll=%d\n",
            (void*)dComIfGp_getItemIconArchive(), (void*)iconArc, mReady ? 1 : 0, visCount, sel,
            mScrollTop, (visCount > 6) ? (visCount - 6) : 0);
    fprintf(fp, "letterTimg=%p itemBoxTimg=%p itemBoxPic=%p listIconLeft=%.1f draw_pass=gInf_vertex\n",
            (void*)mRowLetterTimg, (void*)mItemBoxTimg, (void*)mpItemBoxPic, listIconLeft);
    fprintf(fp, "gInf min=%.1f,%.1f size=%.1f,%.1f\n", mDoGph_gInf_c::getMinXF(),
            mDoGph_gInf_c::getMinYF(), mDoGph_gInf_c::getWidthF(), mDoGph_gInf_c::getHeightF());
    {
        J2DGrafContext* cur = dComIfGp_getCurrentGrafPort();
        if (cur && cur->getGrafType() == 1) {
            const JGeometry::TBox2<f32>* ob = ((J2DOrthoGraph*)cur)->getOrtho();
            fprintf(fp, "curOrtho=%.1f,%.1f -> %.1f,%.1f\n", ob->i.x, ob->i.y, ob->f.x, ob->f.y);
        }
    }
    fprintf(fp, "--- catalog (all %d items; UI shows 6 at scrollTop+row) ---\n", visCount);
    for (int i = 0; i < visCount && visList; ++i) {
        fprintf(fp, "itemIdx=%d purch=%d price=%d itemNo=%u name=%s\n", i,
                visList[i].purchasable ? 1 : 0, visList[i].price, (unsigned)visList[i].itemNo,
                visList[i].name ? visList[i].name : "?");
    }
    fprintf(fp, "--- viewport rows (on screen now) ---\n");
    for (int row = 0; row < 6; ++row) {
        const int itemIdx = mScrollTop + row;
        u8        itemNo  = 0xff;
        int       purch   = 0;
        if (itemIdx < visCount && visList) {
            purch  = visList[itemIdx].purchasable ? 1 : 0;
            itemNo = visList[itemIdx].itemNo;
        }
        int ensure = 0;
        if (purch && itemNo != 0xff && dComIfGp_getItemIconArchive()) {
            cacheRowLetterSlotBounds(row);
            ensure = ensureRowWheelPic(row, itemNo, dComIfGp_getItemIconArchive()) ? 1 : 0;
        }
        u32 kind = 0;
        if (mpRowLetterPane[row]) {
            kind = mpRowLetterPane[row]->getKind();
        }
        const int picCount = mpRowLetterPane[row] ? countPicturesInPane(mpRowLetterPane[row]) : 0;
        int priceSet = 0;
        if (itemIdx < visCount && visList && mpRowPrice[row]) {
            priceSet = visList[itemIdx].price;
        }
        fprintf(fp,
                "row=%d itemIdx=%d purch=%d itemNo=%u price=%d ensure=%d letVis=%d frameVis=%d "
                "priceVis=%d "
                "kind=%c%c%c%c picCount=%d pane=%p letterGfx=%p bloItemGfx=%p "
                "slot=%d slotXYWH=%.1f,%.1f,%.1f,%.1f\n",
                row, itemIdx, purch, (unsigned)itemNo, priceSet, ensure,
                mpRowLetterPane[row] && mpRowLetterPane[row]->isVisible() ? 1 : 0,
                mpRowFrame[row] && mpRowFrame[row]->isVisible() ? 1 : 0,
                mpRowPrice[row] && mpRowPrice[row]->isVisible() ? 1 : 0,
                (char)(kind >> 24), (char)(kind >> 16), (char)(kind >> 8), (char)kind, picCount,
                (void*)mpRowLetterPane[row], (void*)mpRowLetterGfx[row],
                (void*)mpRowBloItemGfx[row], mRowIconSlot[row].valid ? 1 : 0, mRowIconSlot[row].x,
                mRowIconSlot[row].y, mRowIconSlot[row].w, mRowIconSlot[row].h);
        if (purch && ensure && mRowItemTexBuf[row]) {
            const ResTIMG* timg = (const ResTIMG*)mRowItemTexBuf[row];
            f32          dw = 0.0f;
            f32          dh = 0.0f;
            calcWheelIconDrawSize(itemNo, timg, mRowIconSlot[row].h, &dw, &dh);
            fprintf(fp, "  drawWH=%.1f,%.1f\n", dw, dh);
        }

        LetAreaBounds gfxB;
        if (mpRowLetterGfx[row]) {
            gfxB = calcPaneScreenBounds(mpRowLetterGfx[row]);
        }
        LetAreaBounds frameB;
        if (mpRowFrame[row]) {
            frameB = calcPaneScreenBounds(mpRowFrame[row]);
        }
        f32 hx = 0.0f, hy = 0.0f, hw = 0.0f, hh = 0.0f;
        calcRowHeapIconDrawPos(mpRowLetterMgr[row], mpRowFrame[row], listIconLeft, &hx, &hy, &hw,
                               &hh);
        fprintf(fp,
                "  gfxB=%d:%.1f,%.1f,%.1f,%.1f frameB=%d:%.1f,%.1f,%.1f,%.1f heapDraw=%.1f,%.1f,"
                "%.1f,%.1f itemPic=%p boxPic=%p\n",
                gfxB.valid ? 1 : 0, gfxB.x, gfxB.y, gfxB.w, gfxB.h, frameB.valid ? 1 : 0, frameB.x,
                frameB.y, frameB.w, frameB.h, hx, hy, hw, hh, (void*)mpRowItemPic[row],
                (void*)mpItemBoxPic);
    }
    fclose(fp);
}

void dALBWShop_c::drawRowWheelIcons(J2DGrafContext* gfx, int visCount) {
    JKRArchive* iconArc = dComIfGp_getItemIconArchive();
    if (!iconArc) {
        iconArc = ensureItemIconArchive();
    }
    if (!gfx || !iconArc) {
        return;
    }

    const dALBWVisibleEntry* visList = dALBWRental_getVisibleList(&visCount);

#ifdef TARGET_PC
    dusk::frame_interp::set_ui_tick_pending(true);
#endif

    // Draw in gInf vertex space (same as mpMenuScreen). Normalized 640 coords + a 640 ortho pass
    // misalign Y against the list scissor; getGlobalVtx on flame_* keeps middle-left placement.
    gfx->setup2D();

    const f32 listIconLeft =
        calcShopListIconLeft(mpRowFrame, mpRowLetterGfx, mpRowLetter, mpRowLetterMgr, mScrollTop,
                             visCount, visList);

    for (int row = 0; row < 6; ++row) {
        const int itemIdx = mScrollTop + row;
        if (itemIdx >= visCount || !visList[itemIdx].purchasable) {
            continue;
        }

        const u8 itemNo     = visList[itemIdx].itemNo;
        const u8 iconItemNo = shopWheelIconItemNo(itemNo);
        if (!ensureRowWheelPic(row, itemNo, iconArc) || !ensureItemBoxPic()) {
            continue;
        }

        f32 boxXg = 0.0f;
        f32 boxYg = 0.0f;
        f32 boxWg = 0.0f;
        f32 boxHg = 0.0f;
        if (!calcRowHeapIconDrawGInf(mpRowFrame[row], mpRowLetterMgr[row], listIconLeft, &boxXg,
                                     &boxYg, &boxWg, &boxHg)) {
            continue;
        }

        mpItemBoxPic->show();
        mpItemBoxPic->setBasePosition(J2DBasePosition_0);
        mpItemBoxPic->draw(boxXg, boxYg, boxWg, boxHg, false, false, false);

        J2DPicture* pic = mpRowItemPic[row];
        if (!pic || pic->getTextureCount() == 0) {
            continue;
        }

        const ResTIMG* timg  = (const ResTIMG*)mRowItemTexBuf[row];
        f32            drawW = 0.0f;
        f32            drawH = 0.0f;
        const f32      slotHNorm =
            boxHg / (mDoGph_gInf_c::getHeightF() / FB_HEIGHT);
        calcWheelIconDrawSize(iconItemNo, timg, slotHNorm, &drawW, &drawH);

        const f32 scY = mDoGph_gInf_c::getHeightF() / FB_HEIGHT;
        const f32 drawWg = drawW * scY;
        const f32 drawHg = drawH * scY;
        const f32 iconXg = boxXg + 0.5f * (boxWg - drawWg);
        const f32 iconYg = boxYg + 0.5f * (boxHg - drawHg);

        pic->show();
        pic->setBasePosition(J2DBasePosition_0);
        pic->draw(iconXg, iconYg, drawWg, drawHg, false, false, false);
    }
}

void dALBWShop_c::drawRowListText(J2DGrafContext* gfx, int visCount) {
    if (!gfx) {
        return;
    }
    const dALBWVisibleEntry* visList = dALBWRental_getVisibleList(&visCount);
    const int                  sel   = dALBWRental_getSelectedIdx();
    const dMeter_drawLetterHIO_c& hio = g_drawHIO.mLetterSelectScreen;
    char                       buf[64];

    for (int row = 0; row < 6; ++row) {
        const int itemIdx = mScrollTop + row;
        if (itemIdx >= visCount) {
            continue;
        }
        if (visList[itemIdx].purchasable && mpRowName[row]) {
            showPaneAndAncestors(mpRowName[row]);
            mpRowName[row]->J2DPane::draw(0.0f, 0.0f, gfx, true, true);
        }
        if (!mpRowPrice[row]) {
            continue;
        }
        snprintf(buf, sizeof(buf), "%d Rupees", visList[itemIdx].price);
        const JUtility::TColor ink =
            (itemIdx == sel) ? hio.mSelectTextFront
                             : JUtility::TColor(180, 165, 120, 255);
        if (row == 4) {
            const LetAreaBounds slot = calcPaneScreenBounds(mpRowPrice[row]);
            drawRowPriceMesg(gfx, slot, buf, ink);
            continue;
        }
        showPaneAndAncestors(mpRowPrice[row]);
        mpRowPrice[row]->show();
        mpRowPrice[row]->J2DPane::draw(0.0f, 0.0f, gfx, true, true);
    }
}

void dALBWShop_c::updateRowLetters(int visCount, int sel) {
    const dALBWVisibleEntry* visList = dALBWRental_getVisibleList(&visCount);
    const dMeter_drawLetterHIO_c& hio = g_drawHIO.mLetterSelectScreen;
    (void)ensureItemIconArchive();
    (void)ensureItemBoxPic();

    for (int row = 0; row < 6; ++row) {
        J2DPane*  pane = mpRowLetterPane[row];
        CPaneMgr* mgr  = mpRowLetterMgr[row];
        if (!pane) {
            continue;
        }

        const int itemIdx = mScrollTop + row;
        if (itemIdx >= visCount) {
            pane->hide();
            continue;
        }

        const dALBWVisibleEntry& ve    = visList[itemIdx];
        const bool               isSel = (itemIdx == sel);

        if (mgr) {
            mgr->setAlphaRate(1.0f);
            // fenu_t* name panes share let_*'s transform; select-bar scale shifts subject text right.
            // Rentable rows draw the wheel icon in heap — keep let_* at unselect scale (flame_* shows selection).
            const bool rentableRow =
                ve.purchasable && dComIfGp_getItemIconArchive() &&
                ensureRowWheelPic(row, ve.itemNo, dComIfGp_getItemIconArchive());
            const f32 scale =
                (isSel && !rentableRow) ? hio.mSelectBarScale : hio.mUnselectBarScale;
            mgr->scale(scale, scale);
            // let_* holds flame_* — never paneTrans the row container (shifts bars off-screen).
            mgr->paneTrans(0.0f, 0.0f);
        }

        pane->show();

        const LetAreaBounds iconSlot = calcRowIconSlotBounds(mgr);
        cacheRowLetterSlotBounds(row);

        if (mpRowBloItemGfx[row]) {
            mpRowBloItemGfx[row]->hide();
        }

        if (ve.purchasable && dComIfGp_getItemIconArchive() &&
            ensureRowWheelPic(row, ve.itemNo, dComIfGp_getItemIconArchive())) {
            const f32 listIconLeft =
                calcShopListIconLeft(mpRowFrame, mpRowLetterGfx, mpRowLetter, mpRowLetterMgr,
                                     mScrollTop, visCount, visList);
            applyLetRowSubtreeRental(pane, mRowLetterTimg, true, mRowLetterContentsTex[row],
                                     iconSlot, listIconLeft);
            setRowLetterGraphicsVisible(mpRowLetterGfx[row], mpRowLetterWin[row],
                                        mRowLetterContentsTex[row], false);
            if (mpRowLetterGfx[row] && mRowLetterTimg && !mpRowLetterGfx[row]->isUsed(mRowLetterTimg)) {
                mpRowLetterGfx[row]->hide();
            }
            if (mpRowLetter[row]) {
                mpRowLetter[row]->hide();
            }
            if (mpRowFrame[row]) {
                showPaneAndAncestors(mpRowFrame[row]);
                mpRowFrame[row]->show();
            }
            if (mpRowName[row]) {
                showPaneAndAncestors(mpRowName[row]);
            }
            if (mpRowPrice[row]) {
                showPaneAndAncestors(mpRowPrice[row]);
            }
            if (mpRowItemPic[row]) {
                mpRowItemPic[row]->hide();
            }
        } else {
            mRowIconSlot[row].valid = false;
            if (mpRowItemPic[row]) {
                mpRowItemPic[row]->hide();
            }
            mRowItemPicItemNo[row] = 0xff;
            restoreLetterIconsToPane(pane, mRowLetterTimg, mRowLetterBaseW[row], mRowLetterBaseH[row]);
            applyLetRowSubtreeRental(pane, mRowLetterTimg, false, mRowLetterContentsTex[row], iconSlot,
                                     -1.0f);
            setRowLetterGraphicsVisible(mpRowLetterGfx[row], mpRowLetterWin[row],
                                        mRowLetterContentsTex[row], true);
            if (mpRowLetterWin[row]) {
                setRowLetterEnvelopeVisible(mpRowLetterWin[row], mRowLetterContentsTex[row],
                                            true);
            }
            if (mpRowLetter[row]) {
                mpRowLetter[row]->show();
            }
            if (mpRowFrame[row]) {
                mpRowFrame[row]->show();
            }
            if (mpRowPrice[row]) {
                mpRowPrice[row]->show();
            }
        }
    }
}

void dALBWShop_c::hideRentableCenterBloIcons(int visCount, f32 listIconLeft) {
    const dALBWVisibleEntry* visList = dALBWRental_getVisibleList(&visCount);

    for (int row = 0; row < 6; ++row) {
        J2DPane* pane = mpRowLetterPane[row];
        if (!pane || !mpRowLetterMgr[row]) {
            continue;
        }

        const int itemIdx = mScrollTop + row;
        if (itemIdx >= visCount) {
            continue;
        }

        const dALBWVisibleEntry& ve = visList[itemIdx];
        if (!ve.purchasable || !dComIfGp_getItemIconArchive() ||
            !ensureRowWheelPic(row, ve.itemNo, dComIfGp_getItemIconArchive())) {
            continue;
        }

        if (mpRowBloItemGfx[row]) {
            mpRowBloItemGfx[row]->hide();
        }

        const LetAreaBounds iconSlot = calcRowIconSlotBounds(mpRowLetterMgr[row]);
        applyLetRowSubtreeRental(pane, mRowLetterTimg, true, mRowLetterContentsTex[row], iconSlot,
                                 listIconLeft);
    }
}

void dALBWShop_c::registerDraw() {
    if (mReady) dComIfGd_set2DOpa(this);
}

void dALBWShop_c::draw() {
    if (!mReady || !mpMenuScreen || !mpBaseScreen) return;
    populateRows();
    int         visCount = 0;
    const int   sel      = dALBWRental_getSelectedIdx();
    const dALBWVisibleEntry* visList = dALBWRental_getVisibleList(&visCount);
    (void)sel;

    J2DGrafContext* gfx = dComIfGp_getCurrentGrafPort();

    rehideShopFooterPanes(mpBaseScreen, mpSdwScreen, mpMenuScreen, mpRowPrice[4]);
    for (int i = 0; i < 6; ++i) {
        if (mpRowPrice[i]) {
            showPaneAndAncestors(mpRowPrice[i]);
            mpRowPrice[i]->show();
        }
    }

    if (mpSdwScreen) mpSdwScreen->draw(0.0f, 0.0f, gfx);
    mpBaseScreen->draw(0.0f, 0.0f, gfx);
    rehideShopFooterPanes(mpBaseScreen, mpSdwScreen, mpMenuScreen, mpRowPrice[4]);
    for (int i = 0; i < 6; ++i) {
        if (mpRowPrice[i]) {
            showPaneAndAncestors(mpRowPrice[i]);
            mpRowPrice[i]->show();
        }
    }
    // Vanilla letter UI: list scissored to let_area (left); parchment only on the right.
    // Without both scissors, letter_window_* BLOs cover the full frame as an overlay.
    u32 savedL = 0, savedT = 0, savedW = 0, savedH = 0;
    GXGetScissor(&savedL, &savedT, &savedW, &savedH);

    const LetAreaBounds area = calcPaneScreenBounds(mLetAreaPane);

    // ============================================
    // NEW CODE — ALBW Port (Footer anchor — Bug 1 fix)
    // Anchor the footer strip to the let_area interior, not the full screen,
    // so the stick + content stay inside the ornate frame.  Keep the probed
    // Y/H (vanilla footer band).  Drawn last (below) so it sits on top.
    // ============================================
    LetAreaBounds footerBar = mFooterBar;
    if (area.valid && footerBar.valid) {
        footerBar.x = area.x;
        footerBar.w = area.w;
    }
    // Nudge the whole footer (stick + tagline + A/B) right to sit over the
    // vanilla footer slot region rather than hugging the let_area left edge.
    footerBar.x += kFooterContentShiftX;

    if (area.valid) {
        const f32 listW = area.w * kListColumnRatio;
        const f32 descX = area.x + listW + kListDescGap;
        const f32 descW = area.w - listW - kListDescGap;

        applyListColumnLayout(area.x, listW);
        for (int row = 0; row < 6; ++row) {
            cacheRowLetterSlotBounds(row);
        }
        updateRowLetters(visCount, sel);

        // Item list: left column + icon column (icons sit slightly left of let_area origin).
        // The flame_* bars span nearly the whole area; extend the scissor right edge to just
        // before the parchment so they read full-length instead of being clipped at listW.
        const f32 listScissorX     = area.x - kListIconScissorPad;
        const f32 listScissorRight = descX - kRowFrameGapBeforeParchment;
        const f32 listScissorW     = listScissorRight - listScissorX;
        gfx->scissor(listScissorX, area.y, listScissorW, area.h);
        gfx->setScissor();
        suppressFifthRowFooterOverlap(mpMenuScreen, mpRowPrice[4]);
        const f32 listIconLeft =
            calcShopListIconLeft(mpRowFrame, mpRowLetterGfx, mpRowLetter, mpRowLetterMgr, mScrollTop,
                                 visCount, visList);
        hideRentableCenterBloIcons(visCount, listIconLeft);
        mpMenuScreen->draw(0.0f, 0.0f, gfx);
        static u32 sIconDbgFrame = 0;
        if ((sIconDbgFrame++ % 90) == 0) {
            dumpRowIconDebug(dComIfGp_getItemIconArchive(), visCount, visList, listIconLeft);
        }
        drawRowListText(gfx, visCount);
        drawRowWheelIcons(gfx, visCount);
        restoreScissor(gfx, savedL, savedT, savedW, savedH);

        // Parchment on the right column (shifted past list + gap).
        applyDescColumnShift(listW + kListDescGap);

        gfx->scissor(descX, area.y, descW, area.h);
        gfx->setScissor();
        const f32 ruledPitch = measureParchmentLinePitch(mpDescTextScreen);
        if (mpDescSpotScreen) {
            mpDescSpotScreen->draw(0.0f, 0.0f, gfx);
        }
        if (mpDescTextScreen) {
            mpDescTextScreen->draw(0.0f, 0.0f, gfx);
        }
        drawDescMesgText(gfx, descX, area.y, descW, area.h, ruledPitch);
        restoreScissor(gfx, savedL, savedT, savedW, savedH);
    } else {
        updateRowLetters(visCount, sel);
        suppressFifthRowFooterOverlap(mpMenuScreen, mpRowPrice[4]);
        const f32 listIconLeft =
            calcShopListIconLeft(mpRowFrame, mpRowLetterGfx, mpRowLetter, mpRowLetterMgr, mScrollTop,
                                 visCount, visList);
        hideRentableCenterBloIcons(visCount, listIconLeft);
        mpMenuScreen->draw(0.0f, 0.0f, gfx);
        static u32 sIconDbgFrameNoArea = 0;
        if ((sIconDbgFrameNoArea++ % 90) == 0) {
            dumpRowIconDebug(dComIfGp_getItemIconArchive(), visCount, visList, listIconLeft);
        }
        drawRowListText(gfx, visCount);
        drawRowWheelIcons(gfx, visCount);
        const f32 ruledPitch = measureParchmentLinePitch(mpDescTextScreen);
        if (mpDescSpotScreen) {
            mpDescSpotScreen->draw(0.0f, 0.0f, gfx);
        }
        if (mpDescTextScreen) {
            mpDescTextScreen->draw(0.0f, 0.0f, gfx);
        }
        if (area.valid) {
            drawDescMesgText(gfx, area.x, area.y, area.w, area.h, ruledPitch);
        }
    }

    // Custom footer last so it draws on top of the rest of the HUD. Scissor is
    // already restored here, so the stick + text are unclipped.
    drawShopFooter(gfx, mpFooterStickPic, footerBar);
}

#endif // TARGET_PC_NATIVE_UI

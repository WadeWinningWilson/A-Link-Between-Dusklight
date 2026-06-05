#pragma once
#if TARGET_PC_NATIVE_UI

#include "d/d_albw_ui_text.h"
#include "d/d_drawlist.h"

class mDoDvdThd_mountArchive_c;
class J2DGrafContext;
class J2DScreen;
class J2DTextBox;
class J2DPicture;
class J2DPane;
class J2DWindow;
class JUTTexture;
class CPaneMgr;
class JKRArchive;
struct ResTIMG;
struct dALBWVisibleEntry;

// ============================================
// NEW CODE — ALBW Port (Native Shop Window)
// Renders the rental shop using the game's native letter-select screen
// (letres.arc). Owned by the rental Postman actor; drawn via
// dComIfGd_set2DOpa() from actor Draw() when dALBWRental_isOpen().
// ============================================
class dALBWShop_c : public dDlst_base_c {
public:
    dALBWShop_c();
    ~dALBWShop_c();

    // Drive async archive load; returns true once ready to draw.
    bool update();

    // Register for drawing this frame (call from Postman Draw()).
    void registerDraw();

    virtual void draw() override;

    bool isReady() const { return mReady; }

    // Return the loaded letres.arc for use by dALBWDialogue_c.
    // Null until update() returns true.
    JKRArchive* getArchive() const { return mpArchive; }

private:
    void create();
    void applyListColumnLayout(f32 areaX, f32 listW);
    void applyDescColumnShift(f32 shiftX);
    void populateRows();
    void updateRowLetters(int visCount, int sel);
    void hideRentableCenterBloIcons(int visCount, f32 listIconLeft);
    bool ensureRowWheelPic(int row, u8 itemNo, JKRArchive* arc);
    bool ensureItemBoxPic();
    void cacheRowLetterSlotBounds(int row);
    void drawRowListText(J2DGrafContext* gfx, int visCount);
    void drawRowWheelIcons(J2DGrafContext* gfx, int visCount);
    void dumpRowIconDebug(JKRArchive* iconArc, int visCount, const dALBWVisibleEntry* visList);
    void updateDescParchment(bool showItemBox, u8 itemNo);
    void drawDescMesgText(J2DGrafContext* gfx, f32 x, f32 y, f32 w, f32 h, f32 ruledLinePitch);
    JKRArchive* ensureItemIconArchive();

    J2DPicture*     mpFooterStickPic = nullptr;
    dALBWPaneBounds mFooterBar;          ///< single full-width footer strip (Cursor-defined content)

    mDoDvdThd_mountArchive_c* mpMount          = nullptr;
    mDoDvdThd_mountArchive_c* mpItemIconMount  = nullptr;
    JKRArchive*               mpArchive        = nullptr;
    JKRArchive*               mpItemIconArchive = nullptr;
    J2DScreen*                mpMenuScreen = nullptr;
    J2DScreen*                mpBaseScreen = nullptr;
    J2DScreen*                mpSdwScreen  = nullptr;
    // ============================================
    // NEW CODE — ALBW Port (Shop Description + Title)
    // Right-side parchment (vanilla letter read layout — two BLOs):
    //   mpDescSpotScreen — zelda_letter_window_spot.blo (parchment frame)
    //   mpDescTextScreen — zelda_letter_window_base.blo (t4_s body text)
    //   mpDescSpotMgr / mpDescTextMgr — CPaneMgr on n_all (pos/scale/alpha)
    // mpDescBox — t4_s on mpDescTextScreen; updated in populateRows().
    // mpTitleBox   — f_t_00 / t_t00 page-counter pane in mpBaseScreen,
    //   repurposed as the persistent shop heading.
    // ============================================
    J2DScreen*                mpDescSpotScreen = nullptr;
    J2DScreen*                mpDescTextScreen = nullptr;
    CPaneMgr*                 mpDescSpotMgr    = nullptr;
    CPaneMgr*                 mpDescTextMgr    = nullptr;
    CPaneMgr*                 mpDescBoxMgr     = nullptr;  ///< positions t4_s in the right column
    J2DTextBox*               mpDescBox        = nullptr;  ///< t4_s — mesg font body text on parchment
    J2DTextBox*               mpDescOutFontBox = nullptr; ///< mg_e4lin (vanilla OutFont target; unused for C strings)
    J2DPane*                  mpDescN3linePane = nullptr; ///< upper item-box frame (rentable items)
    J2DPane*                  mpDescN4linePane = nullptr; ///< 4-line description layout (locked rows)
    J2DPane*                  mpDescMg3linePane = nullptr; ///< 3-line text/icon container (item box)
    J2DPicture*               mpDescItemPic    = nullptr; ///< wheel icon inside n_3line / mg_3line
    f32                       mDescItemPicBaseW = 0.0f;
    f32                       mDescItemPicBaseH = 0.0f;
    u8                        mDescItemTexBuf[0xC00]   = {};
    u8                        mRowItemTexBuf[6][0xC00]  = {};
    u8                        mRowItemTexBuf2[6][0xC00] = {};
    bool                      mDescShowsItemBox  = false;
    bool                      mDescColumnShifted  = false;
    J2DTextBox*               mpTitleBox       = nullptr;
    J2DPane*                  mLetAreaPane     = nullptr; ///< list bounds; desc scissor starts at right edge
    // ============================================
    // NEW CODE ENDS HERE
    // ============================================
    CPaneMgr*                 mpParent     = nullptr;  ///< menu 6menu n_all
    CPaneMgr*                 mpBaseMgr    = nullptr;  ///< select_base n_all (mWindowPos/Scale)
    CPaneMgr*                 mpSdwMgr     = nullptr;  ///< shadow n_all
    J2DTextBox*               mpRowName[6]  = {};  ///< fenu_t0–t5 letter subject → item name
    J2DTextBox*               mpRowPrice[6] = {};  ///< fenu_t6–t11 sender slot → rupee price
    J2DTextBox*               mpRowSub[6]      = {};  ///< unused (shadow panes hidden)
    J2DPicture*               mpRowFrame[6]    = {};  ///< flame_00–05 row borders (clipped left)
    J2DPane*                  mpRowLetterPane[6] = {};  ///< let_00_n–05_n (window or picture)
    J2DPicture*               mpRowLetter[6]   = {};  ///< PIC1 inside let_*_n (often null; letter is WIN contents)
    J2DWindow*                mpRowLetterWin[6] = {};  ///< WIN using letter TIMG near let_* (often a sibling)
    J2DPane*                  mpRowLetterGfx[6] = {};  ///< PIC/WIN pane that draws the envelope
    J2DPane*                  mpRowBloItemGfx[6] = {}; ///< 6menu sibling item-icon pane (not the envelope)
    JUTTexture*               mRowLetterContentsTex[6] = {};  ///< saved field_0x110 while wheel icon shown
    J2DPicture*               mpRowItemPic[6]  = {};  ///< heap wheel icon (rentable rows)
    J2DPicture*               mpItemBoxPic      = nullptr; ///< shared empty item-slot frame
    ResTIMG*                  mItemBoxTimg      = nullptr;
    u8                        mRowItemPicItemNo[6] = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    };  ///< itemNo cached in mpRowItemPic[row]
    CPaneMgr*                 mpRowLetterMgr[6] = {};  ///< vanilla letter-icon parent mgr
    ResTIMG*                  mRowLetterTimg    = nullptr;
    f32                       mRowLetterBaseW[6] = {};
    f32                       mRowLetterBaseH[6] = {};
    struct {
        f32  x = 0.0f;
        f32  y = 0.0f;
        f32  w = 0.0f;
        f32  h = 0.0f;
        bool valid = false;
    }                         mRowIconSlot[6] = {};  ///< cached before hidePicturesInPane
    CPaneMgr*                 mpRowPriceMgr[6] = {};  ///< shifts fenu_t6 into list column
    f32                       mRowFrameBaseW[6] = {}; ///< flame_00–05 width before list-column resize
    f32                       mRowFrameBaseH[6] = {};
    f32                       mPriceInitCenterX[6] = {}; ///< fenu_t6 default center (paneTrans offset base)
    f32                       mRowLetterInitCenterX[6] = {}; ///< let_*_n default center (icon column paneTrans base)
    int                       mScrollTop   = 0;    ///< first visible row index in the 6-row viewport
    char                      mDescBuf[256]      = {}; ///< body text for right parchment pane
    char                      mDescWrapBuf[512] = {}; ///< word-wrapped copy for J2DPrint
    bool                      mReady       = false;
};
// ============================================
// NEW CODE ENDS HERE
// ============================================

#endif // TARGET_PC_NATIVE_UI

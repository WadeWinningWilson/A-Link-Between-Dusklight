// ============================================
// NEW CODE — ALBW Port (Native Dialogue Box)
// ============================================
#include "d/d_albw_dialogue.h"

#if TARGET_PC_NATIVE_UI

#include "os_report.h"

#include "JSystem/J2DGraph/J2DGrafContext.h"
#include "JSystem/J2DGraph/J2DPane.h"
#include "JSystem/J2DGraph/J2DScreen.h"
#include "JSystem/J2DGraph/J2DTextBox.h"
#include "JSystem/JUtility/TColor.h"
#include <algorithm>
#include <cstring>
#include "d/d_albw_ui_text.h"
#include "d/d_com_inf_game.h"
#include "d/d_msg_object.h"
#include "d/d_msg_scrn_arrow.h"
#include "d/d_pane_class.h"
#include "d/dolzel.h" // IWYU pragma: keep
#include "dusk/string.hpp"
#include "m_Do/m_Do_controller_pad.h"
#include "m_Do/m_Do_ext.h"

#ifdef TARGET_PC
#include "dusk/frame_interpolation.h"
#endif

static constexpr f32 kFontScale      = 1.0f;
static constexpr f32 kWrapMarginPx   = 22.0f;

dALBWDialogue_c::dALBWDialogue_c() {}

dALBWDialogue_c::~dALBWDialogue_c() {
    JKR_DELETE(mpArrow);
    JKR_DELETE(mpArwAnchor);
    JKR_DELETE(mpFrameMgr);
    JKR_DELETE(mpScreen);
    JKR_DELETE(mpT4Mgr);
    mpT4Mgr = nullptr;
    JKR_DELETE(mpTextMgr);
    mpTextMgr = nullptr;
    JKR_DELETE(mpTxScreen);
    mpTxScreen = nullptr;
}

static bool pageHasText(const char* text) {
    if (!text) {
        return false;
    }
    for (const char* p = text; *p != '\0'; ++p) {
        if (*p != ' ' && *p != '\n' && *p != '\r' && *p != '\t') {
            return true;
        }
    }
    return false;
}

bool dALBWDialogue_c::tryCreate() {
    if (mReady) {
        return true;
    }

    JKRArchive* commonArc = dComIfGp_getMsgCommonArchive();
    if (!commonArc) {
        return false;
    }

    J2DScreen* textScr = JKR_NEW J2DScreen();
    if (!textScr->setPriority("zelda_message_window_text.blo", 0x20000, commonArc)) {
        JKR_DELETE(textScr);
        OSReport("ALBW Dlg tryCreate: FAILED to load zelda_message_window_text.blo\n");
        return false;
    }
    dPaneClass_showNullPane(static_cast<J2DPane*>(textScr));

    {
        J2DPane* p3  = textScr->search(MULTI_CHAR('n_3line'));
        J2DPane* p3f = textScr->search(MULTI_CHAR('n_3fline'));
        J2DPane* p4  = textScr->search(MULTI_CHAR('n_e4line'));
        if (p3)  p3->hide();
        if (p3f) p3f->hide();
        if (p4)  p4->show();
    }

    J2DTextBox* tb = (J2DTextBox*)textScr->search('t4_s');
    if (!tb) {
        JKR_DELETE(textScr);
        OSReport("ALBW Dlg tryCreate: FAILED — t4_s pane not found\n");
        return false;
    }

    tb->setFont(mDoExt_getMesgFont());
    tb->setBlackWhite(JUtility::TColor(0, 0, 0, 0), JUtility::TColor(255, 255, 255, 255));

    {
        J2DTextBox::TFontSize naturalSize;
        tb->getFontSize(naturalSize);
        if (naturalSize.mSizeX > 0.0f && naturalSize.mSizeY > 0.0f) {
            J2DTextBox::TFontSize scaledSize;
            scaledSize.mSizeX = naturalSize.mSizeX * kFontScale;
            scaledSize.mSizeY = naturalSize.mSizeY * kFontScale;
            tb->setFontSize(scaledSize);
            mFontSizeX = scaledSize.mSizeX;
            mFontSizeY = scaledSize.mSizeY;
        }
    }

    mLineSpace = tb->getLineSpace();
    if (mLineSpace <= 0.0f && mFontSizeY > 0.0f) {
        mLineSpace = mFontSizeY * 1.05f;
    }
    mCharSpace = tb->getCharSpace();

    // Override BLO VBIND_BOTTOM; vanilla keeps field_0x10c/110 at 0 (cursor nudge only).
    tb->mFlags      = (HBIND_LEFT << 2) | VBIND_TOP;
    tb->field_0x10c = 0.0f;
    tb->field_0x110 = 0.0f;
    tb->setString(0x200, "");
    tb->show();

    {
        J2DPane* mgNull = textScr->search(MULTI_CHAR('mg_null'));
        if (mgNull) {
            mgNull->move(mgNull->getBounds().i.x + g_MsgObject_HIO_c.mTextPosX,
                         mgNull->getBounds().i.y + g_MsgObject_HIO_c.mTextPosY);
        }
    }

    // Mirror d_msg_scrn_talk.cpp: resize mg_e4lin (mpTm_c[0]) and t4_s (mpTm_c[1]) to 1.2× width.
    mpTextMgr = JKR_NEW CPaneMgr(textScr, MULTI_CHAR('mg_e4lin'), 0, NULL);
    mpT4Mgr   = JKR_NEW CPaneMgr(textScr, 't4_s', 0, NULL);
    if (mpTextMgr) {
        mpTextMgr->resize(1.2f * mpTextMgr->getSizeX(), mpTextMgr->getSizeY());
    }
    if (mpT4Mgr) {
        mpT4Mgr->resize(1.2f * mpT4Mgr->getSizeX(), mpT4Mgr->getSizeY());
    }

    mTextBoxWidth  = mpT4Mgr ? mpT4Mgr->getSizeX() : tb->getWidth();
    mTextBoxHeight = mpT4Mgr ? mpT4Mgr->getSizeY() : tb->getHeight();
    // Pre-wrap width matches drawSelf wrap (post-resize t4_s) minus filigree margin.
    mWrapMaxWidth = std::max(1.0f, mTextBoxWidth + 0.0001f - kWrapMarginPx);

    mTextBounds = dALBW_calcPaneScreenBounds(tb);
    if (!mTextBounds.valid) {
        mTextBounds = dALBW_calcPaneScreenBounds(mpTextMgr ? mpTextMgr->getPanePtr() : nullptr);
    }

    const f32 lineH = mLineSpace > 0.0f ? mLineSpace : mFontSizeY * 1.05f;
    mMaxLinesPerPage = kVanillaMaxLines;
    if (mTextBoxHeight > lineH) {
        const int fit = (int)(mTextBoxHeight / lineH);
        mMaxLinesPerPage = std::min(kVanillaMaxLines, std::max(1, fit));
    }

    OSReport("ALBW Dlg tryCreate: t4=(%.1f,%.1f) wrap=%.1f maxLines=%d vbind=%d\n",
             mTextBoxWidth, mTextBoxHeight, mWrapMaxWidth, mMaxLinesPerPage,
             (int)tb->getVBinding());

    mpTxScreen = textScr;
    mpTextBox  = tb;
    mReady     = true;
    return true;
}

void dALBWDialogue_c::buildPages(const char* text) {
    mPageCount = 0;
    mPageIndex = 0;

    if (!text || text[0] == '\0') {
        return;
    }

    JUTFont* font = mDoExt_getMesgFont();
    if (!font || mFontSizeX <= 0.0f) {
        strncpy(mPages[0], text, kPageBufSize - 1);
        mPages[0][kPageBufSize - 1] = '\0';
        mPageCount = 1;
        return;
    }

    char wrapped[kMaxPages * kPageBufSize];
    wrapped[0] = '\0';
    dALBW_wrapMesgWords(wrapped, sizeof(wrapped), text, mWrapMaxWidth, font, mFontSizeX, mCharSpace);

    char* pageDst  = mPages[0];
    int   pageIdx  = 0;
    int   lineInPg = 0;

    auto startNewPage = [&]() {
        if (pageIdx + 1 >= kMaxPages) {
            return false;
        }
        pageDst[0]     = '\0';
        pageDst        = mPages[++pageIdx];
        pageDst[0]     = '\0';
        lineInPg       = 0;
        mPageCount     = pageIdx + 1;
        return true;
    };

    mPageCount = 1;
    pageDst[0] = '\0';

    // Must match drawSelf: mBounds.getWidth() + 0.0001f (post-resize t4_s, no field_0x10c deduction).
    const f32 drawWrapW   = mTextBoxWidth + 0.0001f;
    const f32 heightSlack = mLineSpace * 0.2f + 1.0f;

    auto pageFitsBox = [&](const char* page) -> bool {
        if (!pageHasText(page)) {
            return true;
        }
        const f32 h = dALBW_mesgParseHeight(page, drawWrapW, font, mFontSizeX, mFontSizeY,
                                          mCharSpace, mLineSpace);
        return h <= mTextBoxHeight - heightSlack;
    };

    for (const char* p = wrapped; *p != '\0'; ) {
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
            continue;
        }

        char trial[kPageBufSize];
        if (pageDst[0] != '\0') {
            snprintf(trial, sizeof(trial), "%s\n%s", pageDst, line);
        } else {
            snprintf(trial, sizeof(trial), "%s", line);
        }

        if (pageHasText(pageDst) && (lineInPg >= mMaxLinesPerPage || !pageFitsBox(trial))) {
            if (!startNewPage()) {
                break;
            }
            snprintf(trial, sizeof(trial), "%s", line);
        }

        if (pageDst[0] != '\0') {
            const size_t pl = strlen(pageDst);
            pageDst[pl]     = '\n';
            pageDst[pl + 1] = '\0';
        }
        const size_t pl = strlen(pageDst);
        strncpy(pageDst + pl, line, kPageBufSize - pl - 1);
        pageDst[kPageBufSize - 1] = '\0';
        ++lineInPg;
    }

    int compact = 0;
    for (int i = 0; i < mPageCount; ++i) {
        if (pageHasText(mPages[i])) {
            if (compact != i) {
                memcpy(mPages[compact], mPages[i], kPageBufSize);
            }
            ++compact;
        }
    }
    if (compact == 0) {
        strncpy(mPages[0], text, kPageBufSize - 1);
        mPages[0][kPageBufSize - 1] = '\0';
        mPageCount = 1;
    } else {
        mPageCount = compact;
    }
    mPageIndex = 0;
}

bool dALBWDialogue_c::advancePage() {
    while (mPageIndex + 1 < mPageCount) {
        ++mPageIndex;
        if (pageHasText(mPages[mPageIndex])) {
            mCooldown = 4;
            return true;
        }
    }
    return false;
}

void dALBWDialogue_c::ensureArrow() {
    J2DScreen* anchorScr = mpScreen;
    if (!anchorScr || !anchorScr->search(MULTI_CHAR('set_ya_n'))) {
        anchorScr = mpTxScreen;
    }
    if (!anchorScr || !anchorScr->search(MULTI_CHAR('set_ya_n'))) {
        return;
    }
    if (!mpArwAnchor) {
        mpArwAnchor = JKR_NEW CPaneMgr(anchorScr, MULTI_CHAR('set_ya_n'), 0, NULL);
    }
    if (!mpArrow) {
        mpArrow = JKR_NEW dMsgScrnArrow_c();
    }
    if (mpArrow) {
        mpArrow->arwAnimeInit();
    }
}

void dALBWDialogue_c::showWithText(const char* text) {
    JKR_DELETE(mpFrameMgr);
    mpFrameMgr = nullptr;
    JKR_DELETE(mpArwAnchor);
    mpArwAnchor = nullptr;
    JKR_DELETE(mpScreen);
    mpScreen = nullptr;

    JKRArchive* msgArc = dComIfGp_getMsgArchive(1);
    if (msgArc) {
        J2DScreen* scr = JKR_NEW J2DScreen();
        if (scr->setPriority("zelda_message_window_new.blo", 0x20000, msgArc)) {
            dPaneClass_showNullPane(static_cast<J2DPane*>(scr));
            CPaneMgr* mgr = JKR_NEW CPaneMgr(scr, MULTI_CHAR('n_all'), 3, NULL);
            mgr->setAlphaRate(1.0f);
            mpScreen   = scr;
            mpFrameMgr = mgr;
        } else {
            JKR_DELETE(scr);
        }
    }

    if (!mReady) {
        tryCreate();
    }
    if (mReady) {
        buildPages(text);
    } else if (text) {
        strncpy(mPages[0], text, kPageBufSize - 1);
        mPages[0][kPageBufSize - 1] = '\0';
        mPageCount = 1;
        mPageIndex = 0;
    }

    ensureArrow();

    mVisible  = true;
    mCooldown = 4;
}

void dALBWDialogue_c::hide() {
    if (mpTextBox) {
        SAFE_STRCPY(mpTextBox->getStringPtr(), "");
    }
    JKR_DELETE(mpArrow);
    mpArrow = nullptr;
    JKR_DELETE(mpArwAnchor);
    mpArwAnchor = nullptr;
    JKR_DELETE(mpFrameMgr);
    mpFrameMgr = nullptr;
    JKR_DELETE(mpScreen);
    mpScreen = nullptr;
    mVisible   = false;
    mPageCount = 0;
    mPageIndex = 0;
}

void dALBWDialogue_c::registerDraw() {
    if (!mReady) {
        tryCreate();
    }
    if (mReady && mVisible) {
        dComIfGd_set2DOpa(this);
    }
}

bool dALBWDialogue_c::checkDismiss() {
    if (!mVisible) {
        return false;
    }
    if (mCooldown > 0) {
        --mCooldown;
        return false;
    }
    if (!mReady || mPageCount <= 0) {
        return true;
    }
    if (!pageHasText(mPages[mPageIndex])) {
        return true;
    }
    if (!(mDoCPd_c::getTrigA(PAD_1) || mDoCPd_c::getTrigB(PAD_1))) {
        return false;
    }
    if (advancePage()) {
        if (mpArrow) {
            mpArrow->arwAnimeInit();
        }
        return false;
    }
    return true;
}

void dALBWDialogue_c::draw() {
    if (!mReady || !mVisible || !mpTxScreen || mPageCount <= 0 || !mpTextBox) {
        return;
    }

    if (mPageIndex < 0 || mPageIndex >= mPageCount) {
        return;
    }

    const char* pageText = mPages[mPageIndex];
    if (!pageHasText(pageText)) {
        return;
    }

#ifdef TARGET_PC
    dusk::frame_interp::set_ui_tick_pending(true);
#endif

    J2DGrafContext* gfx = dComIfGp_getCurrentGrafPort();
    gfx->setup2D();

    if (mpScreen) {
        mpScreen->draw(0.0f, 0.0f, gfx);
    }

    mpTextBox->mFlags      = (HBIND_LEFT << 2) | VBIND_TOP;
    mpTextBox->field_0x10c = 0.0f;
    mpTextBox->field_0x110 = 0.0f;
    mpTextBox->show();
    SAFE_STRCPY(mpTextBox->getStringPtr(), pageText);
    mpTxScreen->draw(0.0f, 0.0f, gfx);

    if (mpArrow && mpArwAnchor && mCooldown == 0) {
        const Vec arwCenter = mpArwAnchor->getGlobalVtxCenter(false, 0);
        mpArrow->setPos(arwCenter.x, arwCenter.y);
        mpArrow->arwAnimeMove();
        mpArrow->draw();
    }
}

#endif // TARGET_PC_NATIVE_UI

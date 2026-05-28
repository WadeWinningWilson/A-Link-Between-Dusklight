// ============================================
// NEW CODE — ALBW Port (Native Shop Window)
// d_albw_shop.cpp — dALBWShop_c implementation.
// Loads letres.arc and draws the rental item list using the game's native
// letter-select screen layout. Registered via dComIfGd_set2DOpa() from
// the Postman actor's Draw() while dALBWRental_isOpen().
// ============================================
#include "d/d_albw_shop.h"

#if TARGET_PC_NATIVE_UI

#include "JSystem/J2DGraph/J2DGrafContext.h"
#include "JSystem/J2DGraph/J2DTextBox.h"
#include "JSystem/JKernel/JKRMemArchive.h"
#include "d/d_albw_rental.h"
#include "d/d_com_inf_game.h"
#include "d/d_pane_class.h"
#include "m_Do/m_Do_dvd_thread.h"
#include "m_Do/m_Do_graphic.h"
#include <cstdio>

static const u64 kTagName[6] = {
    MULTI_CHAR('fenu_t0'), MULTI_CHAR('fenu_t1'), MULTI_CHAR('fenu_t2'),
    MULTI_CHAR('fenu_t3'), MULTI_CHAR('fenu_t4'), MULTI_CHAR('fenu_t5'),
};
static const u64 kTagPrice[6] = {
    MULTI_CHAR('fenu_t6'),  MULTI_CHAR('fenu_t7'),  MULTI_CHAR('fenu_t8'),
    MULTI_CHAR('fenu_t9'),  MULTI_CHAR('fenu_t10'), MULTI_CHAR('fenu_t11'),
};

dALBWShop_c::dALBWShop_c() = default;

dALBWShop_c::~dALBWShop_c() {
    JKR_DELETE(mpParent);
    JKR_DELETE(mpMenuScreen);
    JKR_DELETE(mpBaseScreen);
    JKR_DELETE(mpSdwScreen);
    if (mpMount) {
        mpMount->destroy();
        mpMount = nullptr;
    }
    if (mpArchive) {
        JKRUnmountArchive(mpArchive);
        mpArchive = nullptr;
    }
}

bool dALBWShop_c::update() {
    if (mReady) return true;
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
    mpParent = JKR_NEW CPaneMgr(mpMenuScreen, MULTI_CHAR('n_all'), 2, NULL);
    mpParent->setAlphaRate(1.0f);

    for (int i = 0; i < 6; ++i) {
        mpRowName[i]  = (J2DTextBox*)mpMenuScreen->search(kTagName[i]);
        mpRowPrice[i] = (J2DTextBox*)mpMenuScreen->search(kTagPrice[i]);
        if (mpRowName[i])  { mpRowName[i]->setFont(mDoExt_getMesgFont());  mpRowName[i]->setString(0x40, ""); }
        if (mpRowPrice[i]) { mpRowPrice[i]->setFont(mDoExt_getMesgFont()); mpRowPrice[i]->setString(0x20, ""); }
    }

    mpBaseScreen = JKR_NEW J2DScreen();
    mpBaseScreen->setPriority("zelda_letter_select_base.blo", 0x20000, mpArchive);
    dPaneClass_showNullPane(mpBaseScreen);

    mpSdwScreen = JKR_NEW J2DScreen();
    mpSdwScreen->setPriority("zelda_letter_select_shadow.blo", 0x20000, mpArchive);
    dPaneClass_showNullPane(mpSdwScreen);
}

void dALBWShop_c::populateRows() {
    int visCount = 0;
    const dALBWVisibleEntry* visList = dALBWRental_getVisibleList(&visCount);
    for (int row = 0; row < 6; ++row) {
        if (!mpRowName[row] || !mpRowPrice[row]) continue;
        if (row < visCount) {
            const dALBWVisibleEntry& ve = visList[row];
            mpRowName[row]->setString(ve.purchasable ? ve.name : "?????");
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", ve.price);
            mpRowPrice[row]->setString(buf);
        } else {
            mpRowName[row]->setString("");
            mpRowPrice[row]->setString("");
        }
    }
}

void dALBWShop_c::registerDraw() {
    if (mReady) dComIfGd_set2DOpa(this);
}

void dALBWShop_c::draw() {
    if (!mReady || !mpMenuScreen || !mpBaseScreen) return;
    populateRows();
    J2DGrafContext* gfx = dComIfGp_getCurrentGrafPort();
    if (mpSdwScreen)  mpSdwScreen->draw(0.0f, 0.0f, gfx);
    mpBaseScreen->draw(0.0f, 0.0f, gfx);
    mpMenuScreen->draw(0.0f, 0.0f, gfx);
}
// ============================================
// NEW CODE ENDS HERE
// ============================================

#endif // TARGET_PC_NATIVE_UI

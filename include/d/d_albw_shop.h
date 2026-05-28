#pragma once
#if TARGET_PC_NATIVE_UI

#include "d/d_drawlist.h"

class mDoDvdThd_mountArchive_c;
class J2DScreen;
class J2DTextBox;
class CPaneMgr;
class JKRArchive;

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

private:
    void create();
    void populateRows();

    mDoDvdThd_mountArchive_c* mpMount      = nullptr;
    JKRArchive*               mpArchive    = nullptr;
    J2DScreen*                mpMenuScreen = nullptr;
    J2DScreen*                mpBaseScreen = nullptr;
    J2DScreen*                mpSdwScreen  = nullptr;
    CPaneMgr*                 mpParent     = nullptr;
    J2DTextBox*               mpRowName[6] = {};
    J2DTextBox*               mpRowPrice[6]= {};
    bool                      mReady       = false;
};
// ============================================
// NEW CODE ENDS HERE
// ============================================

#endif // TARGET_PC_NATIVE_UI

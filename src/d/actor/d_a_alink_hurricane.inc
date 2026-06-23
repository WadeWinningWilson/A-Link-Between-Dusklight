/**

 * d_a_alink_hurricane.inc

 * PC-only Hurricane Spin test proc (Focused Arts finisher prototype).

 */



#if TARGET_PC



#include "dusk/hurricane_test.h"



static constexpr int l_hurricaneDurationFrames = 300;
static constexpr int l_hurricaneTiredLockFrames = 60;
static constexpr f32 l_hurricaneCruiseSpeed = 15.0f;
static constexpr f32 l_hurricaneAnimRateScale = 3.5f;



// false = original looping BCK attack-window spin (revert target).

static constexpr bool l_hurricaneFrozenModelSpin = true;



static f32 l_hurricaneFrozenPoseFrame = 0.0f;



static void hurricaneInitAnimLoopSpin(daAlink_c* i_link, daAlink_c::daAlink_ANM anmID,

                                      const daAlinkHIO_anm_c* anm_data, f32 start_frame) {

    i_link->setSingleAnime(anmID, anm_data->mSpeed * l_hurricaneAnimRateScale, start_frame,

                           anm_data->mEndFrame, anm_data->mInterpolation);

    i_link->mUnderFrameCtrl[0].setFrame(i_link->field_0x3484);

    i_link->getNowAnmPackUnder(daAlink_c::UNDER_0)->setFrame(i_link->field_0x3484);

}



static void hurricaneInitFrozenSpin(daAlink_c* i_link, daAlink_c::daAlink_ANM anmID,

                                    const daAlinkHIO_anm_c* anm_data, f32 start_frame,

                                    int param_direction) {

    l_hurricaneFrozenPoseFrame = start_frame;



    i_link->setSingleAnime(anmID, 0.0f, start_frame, anm_data->mEndFrame,

                           anm_data->mInterpolation);

    i_link->mUnderFrameCtrl[0].setRate(0.0f);

    i_link->mUnderFrameCtrl[0].setFrame(start_frame);

    i_link->mUnderFrameCtrl[0].offEndFlg();

    i_link->getNowAnmPackUnder(daAlink_c::UNDER_0)->setFrame(start_frame);



    const f32 attackSpan = i_link->field_0x3488 - i_link->field_0x3484;

    if (attackSpan > 0.0f) {

        const f32 rotPerFrame =

            65536.0f * anm_data->mSpeed * l_hurricaneAnimRateScale / attackSpan;

        i_link->mProcVar2.field_0x300c =

            (s16)(param_direction == 1 ? -rotPerFrame : rotPerFrame);

    } else {

        i_link->mProcVar2.field_0x300c = 0;

    }

}



static void hurricaneUpdateFrozenSpin(daAlink_c* i_link) {

    daPy_frameCtrl_c* frameCtrl = &i_link->mUnderFrameCtrl[0];

    frameCtrl->setRate(0.0f);

    frameCtrl->setFrame(l_hurricaneFrozenPoseFrame);

    frameCtrl->offEndFlg();

    i_link->getNowAnmPackUnder(daAlink_c::UNDER_0)->setFrame(l_hurricaneFrozenPoseFrame);



    i_link->shape_angle.y += i_link->mProcVar2.field_0x300c;

}



static void hurricaneUpdateLoopSpin(daAlink_c* i_link) {

    daPy_frameCtrl_c* frameCtrl = &i_link->mUnderFrameCtrl[0];



    if (frameCtrl->getFrame() >= i_link->field_0x3488) {

        frameCtrl->offEndFlg();

        frameCtrl->setFrame(i_link->field_0x3484);

        i_link->getNowAnmPackUnder(daAlink_c::UNDER_0)->setFrame(i_link->field_0x3484);

    }

}



int daAlink_c::procCutGsHurricaneInit(int param_direction) {

    if (!dusk::isHurricaneTestEnabled()) {

        return 0;

    }



    const daAlinkHIO_cutTurn_c1* cutData = &mpHIO->mCut.mCutTurn.m;

    const daAlinkHIO_anm_c* anm_data;

    daAlink_ANM anmID;

    f32 start_frame;



    commonProcInit(PROC_CUT_GS_HURRICANE);

    resetCombo(TRUE);



    if (param_direction == 1) {

        anmID = ANM_CUT_TURN_RIGHT;

        anm_data = &cutData->mRightTurnAnm;

        start_frame = cutData->mRightTurnInputStartFrame;

        field_0x3484 = cutData->mRightAttackStartFrame;

        field_0x3488 = cutData->mRightAttackEndFrame;

        mProcVar1.field_0x300a = 6;

        mProcVar4.field_0x3010 = 1;

        setCutType(CUT_TYPE_LARGE_TURN_RIGHT);

    } else {

        anmID = ANM_CUT_TURN_LEFT;

        anm_data = &cutData->mLeftTurnAnm;

        start_frame = cutData->mLeftTurnInputStartFrame;

        field_0x3484 = cutData->mLeftAttackStartFrame;

        field_0x3488 = cutData->mLeftAttackEndFrame;

        mProcVar1.field_0x300a = 8;

        mProcVar4.field_0x3010 = 0;

        setCutType(CUT_TYPE_LARGE_TURN_LEFT);

    }



    field_0x3478 = cutData->mLargeAttackRadius;

    field_0x348c = cutData->mLargeAttackAccel;

    field_0x347c = field_0x3478;

    field_0x3480 = anm_data->mCancelFrame;

    field_0x3588 = l_halfAtnWaitBaseAnime;

    field_0x3180 = 0;

    current.angle.y = shape_angle.y;

    field_0x2fe4 = shape_angle.y;

    field_0x2f98 = 0;

    mMaxSpeed = mpHIO->mItem.mSpinner.m.mRideSpeed;

    mNormalSpeed = l_hurricaneCruiseSpeed;

    mProcVar3.field_0x300e = l_hurricaneDurationFrames;

    mProcVar2.field_0x300c = 0;

    mProcVar5.field_0x3012 = param_direction;

    mLeftHandIndex = 100;

    onModeFlg(1);

    mSpeedModifier = 0.0f;



    if (l_hurricaneFrozenModelSpin) {

        hurricaneInitFrozenSpin(this, anmID, anm_data, field_0x3484, param_direction);

    } else {

        hurricaneInitAnimLoopSpin(this, anmID, anm_data, start_frame);

    }



    if (!checkWoodSwordEquip()) {

        simpleAnmPlay(m_nSwordBtk);

    }



    setCutWaterDropEffect();

    initCutTurnAt(field_0x347c, 4);

    onNoResetFlg1(FLG1_UNK_10000000);

    dComIfGp_setPlayerStatus0(0, 0x8000);

    onResetFlg0(RFLG0_UNK_2);

    dusk::initHurricaneSpinSe(this);

    return 1;

}



static void hurricaneApplyMovement(daAlink_c* i_link) {
    // WW-lite: mandatory cruise. Stick steers heading and can add speed above the floor,
    // but never brake below it — neutral stick keeps sliding.
    f32 targetSpeed = l_hurricaneCruiseSpeed;

    if (i_link->checkInputOnR()) {
        cLib_addCalcAngleS(&i_link->current.angle.y, i_link->mMoveAngle, 4, 0x3000, 0x400);
        const f32 stickSpeed = i_link->mMaxSpeed * i_link->mMoveValue;
        if (stickSpeed > targetSpeed) {
            targetSpeed = stickSpeed;
        }
    }

    i_link->mNormalSpeed = targetSpeed;
    i_link->onModeFlg(1);
    i_link->mSpeedModifier = 0.0f;
}



int daAlink_c::procCutGsHurricane() {

    if (checkGroundSpecialMode()) {

        return 1;

    }



    setShapeAngleToAtnActor(0);

    onNoResetFlg1(FLG1_UNK_10000000);

    field_0x2f99 = 4;

    onEndResetFlg0(ERFLG0_UNK_8000000);



    if (!checkWoodSwordEquip()) {

        simpleAnmPlay(m_nSwordBtk);

    }



    if (l_hurricaneFrozenModelSpin) {

        hurricaneUpdateFrozenSpin(this);

    } else {

        hurricaneUpdateLoopSpin(this);

    }



    dusk::maintainHurricaneSpinSe(this);

    hurricaneApplyMovement(this);

    field_0x2fe4 = shape_angle.y;



    mAtSph.ResetAtHit();

    onResetFlg0(RFLG0_UNK_2);

    field_0x347c = field_0x3478;

    mAtSph.SetR(field_0x347c);



    mProcVar3.field_0x300e--;

    if (mProcVar3.field_0x300e <= 0) {

        dusk::stopHurricaneSpinSe(this);

        mAtSph.OffAtSetBit();

        field_0x2fd0 = 0;

        return procCutGsHurricaneTiredInit();

    }



    return 1;

}



int daAlink_c::procCutGsHurricaneTiredInit() {

    if (!commonProcInitNotSameProc(PROC_CUT_GS_HURRICANE_TIRED)) {

        return 0;

    }



    mNormalSpeed = 0.0f;

    mProcVar3.field_0x300e = l_hurricaneTiredLockFrames;

    setSingleAnimeBase(ANM_WAIT_TO_TIRED);

    return 1;

}



int daAlink_c::procCutGsHurricaneTired() {

    daPy_frameCtrl_c* frameCtrl_p = mUnderFrameCtrl;



    if (checkGroundSpecialMode()) {

        return 1;

    }



    mNormalSpeed = 0.0f;



    if (mProcVar3.field_0x300e > 0) {

        mProcVar3.field_0x300e--;



        if (checkAnmEnd(frameCtrl_p)) {

            setSingleAnimeBase(ANM_WAIT_TIRED);

        } else if (frameCtrl_p->getAttribute() == 2) {

            setTiredVoice(frameCtrl_p);

        }



        return 1;

    }



    return procWaitInit();

}



#endif



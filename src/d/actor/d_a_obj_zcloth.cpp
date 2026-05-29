/**
 * d_a_obj_zcloth.cpp
 * Object - Zora Armor
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep

#include "d/actor/d_a_obj_zcloth.h"
#include "d/d_com_inf_game.h"
#include "d/d_item_data.h"
#include "d/d_a_itembase_static.h"
#include "f_pc/f_pc_name.h"

#if TARGET_PC
#include "dusk/randomizer/game/verify_item_functions.h"
#endif

void daObjZCloth_c::initBaseMtx() {
    mpModel->setBaseScale(scale);
    setBaseMtx();
}

void daObjZCloth_c::setBaseMtx() {
    mDoMtx_stack_c::transS(current.pos.x, current.pos.y, current.pos.z);
    mDoMtx_stack_c::ZXYrotM(shape_angle.x, shape_angle.y, shape_angle.z);
    mpModel->setBaseTRMtx(mDoMtx_stack_c::get());
}

int daObjZCloth_c::Create() {
    initBaseMtx();
    fopAcM_SetMtx(this, mpModel->getBaseTRMtx());
    fopAcM_setCullSizeBox2(this, mpModel->getModelData());
    return 1;
}

int daObjZCloth_c::create() {
    fopAcM_ct(this, daObjZCloth_c);
    m_itemNo = 0x31;
#if TARGET_PC
    // Override the item id in randomizer
    if (randomizer_IsActive()) {
        m_itemNo = verifyProgressiveItem(randomizer_getItemAtLocation("Rutelas Blessing"));
        // Use home.angle.x as a check to see if we've set the foolish item model or not
        // Otherwise we end up setting it twice
        if (m_itemNo == dItemNo_Randomizer_FOOLISH_ITEM_e && home.angle.x != 0) {
            home.angle.z = randomizer_getRandomFoolishItemModelID();
            home.angle.x = 0;
        }
        // TODO: Set rotation/height/scale depending on the item (low prio, can figure this out later)
        scale.setall(1.5f);
        switch (M_ITEMNO_MODEL_ITEM_ID) {
        case dItemNo_Randomizer_WOOD_STICK_e:
            shape_angle.x = 0x4000;
            break;
        case dItemNo_Randomizer_MASTER_SWORD_e:
        case dItemNo_Randomizer_LIGHT_SWORD_e:
            shape_angle.x = 0x4000;
            current.pos.z -= 40.0f;
            scale.setall(1.0f);
            break;
        default:
            break;
        }
    }
#endif
    int phase = dComIfG_resLoad(&mPhase, dItem_data::getFieldArc(M_ITEMNO_MODEL_ITEM_ID));
    if (phase == cPhs_COMPLEATE_e) {
        if (!fopAcM_entrySolidHeap(this, (heapCallbackFunc)CheckFieldItemCreateHeap, 0x2fb0)) {
            return cPhs_ERROR_e;
        }
        else if (!Create()) {
            return cPhs_ERROR_e;
        } 
    }
    return phase;
}

int daObjZCloth_c::execute() {
    setBaseMtx();
    return 1;
}

int daObjZCloth_c::draw() {
    DrawBase();
    return 1;
}

int daObjZCloth_c::_delete() {
    dComIfG_resDelete(&mPhase, dItem_data::getFieldArc(m_itemNo)); 
    return 1;
}  

static int daObjZCloth_Draw(daObjZCloth_c* i_this) {
    return i_this->draw();
}

static int daObjZCloth_Execute(daObjZCloth_c* i_this) {
    return i_this->execute();
}

static int daObjZCloth_Delete(daObjZCloth_c* i_this) {
    const fpc_ProcID procID = fopAcM_GetID(i_this);
    return i_this->_delete();
}

static int daObjZCloth_Create(fopAc_ac_c* i_this) {
    daObjZCloth_c* a_this = static_cast<daObjZCloth_c*>(i_this);
    const fpc_ProcID procID = fopAcM_GetID(i_this);
    return a_this->create();
}

static DUSK_CONST actor_method_class l_daObjZCloth_Method = {
    (process_method_func)daObjZCloth_Create,
    (process_method_func)daObjZCloth_Delete,
    (process_method_func)daObjZCloth_Execute,
    (process_method_func)NULL,
    (process_method_func)daObjZCloth_Draw,
};

DUSK_PROFILE actor_process_profile_definition DUSK_CONST g_profile_Obj_ZoraCloth = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 7,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_Obj_ZoraCloth_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daObjZCloth_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_Obj_ZoraCloth_e,
    /* Actor SubMtd */ &l_daObjZCloth_Method,
    /* Status       */ fopAcStts_UNK_0x40000_e | fopAcStts_CULL_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};

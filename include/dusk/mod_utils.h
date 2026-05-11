#pragma once

#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_layer.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_node.h"
#include "m_Do/m_Do_controller_pad.h"

// Remove a button from this frame's trigger state so the game won't see it
// Call after detecting a combo in mod_tick to prevent double-processing
inline void consumeInput(u32 pad, u32 buttonMask) {
    mDoCPd_c::getCpadInfo(pad).mPressedButtonFlags &= ~buttonMask;
}

// Spawn an actor in the play scene layer
// calling fopAcM_create directly outside game simulation context creates the actor in the wrong
// layer, corrupting its first-frame rendering setup
inline fpc_ProcID fopAcM_createInPlayScene(s16 proc_name, u32 params, const cXyz* pos, int room_no,
    const csXyz* angle, const cXyz* scale, s8 argument) {
    layer_class* savedLayer = fpcLy_CurrentLayer();
    base_process_class* playScene = fpcM_SearchByName(fpcNm_PLAY_SCENE_e);
    if (playScene != nullptr) {
        fpcLy_SetCurrentLayer(&((process_node_class*)playScene)->layer);
    }
    fpc_ProcID result = fopAcM_create(proc_name, params, pos, room_no, angle, scale, argument);
    fpcLy_SetCurrentLayer(savedLayer);
    return result;
}

#ifndef D_A_D_A_ITEMBASE_STATIC_H
#define D_A_D_A_ITEMBASE_STATIC_H


class fopAc_ac_c;

int CheckFieldItemCreateHeap(fopAc_ac_c* actor);
int CheckItemCreateHeap(fopAc_ac_c* i_this);

#if TARGET_PC
// Used for foolish items in rando. Foolish item model id is saved to home.angle.z
#define M_ITEMNO_MODEL_ITEM_ID (IF_DUSK(randomizer_IsActive() && m_itemNo == dItemNo_Randomizer_FOOLISH_ITEM_e && home.angle.z != 0 ? home.angle.z :) m_itemNo)
#endif

#endif /* D_A_D_A_ITEMBASE_STATIC_H */

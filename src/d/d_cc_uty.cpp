/**
 * d_cc_uty.cpp
 * Collider Utilties
 */

#include "d/dolzel.h" // IWYU pragma: keep

#include "d/d_cc_uty.h"
#include "SSystem/SComponent/c_math.h"
#include "Z2AudioLib/Z2Creature.h"
#include "d/d_cc_d.h"
#include "d/actor/d_a_player.h"
#include "d/d_s_play.h"
#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor_mng.h"
#if TARGET_PC
#include "dusk/achievements.h"
#include "dusk/settings.h"
#include "d/actor/d_a_alink.h"
#include "d/d_albw_hp_mult.h"
#include "d/d_albw_lockout.h"
#include "d/d_meter2_info.h"
#include "d/d_albw_wolf_stun.h"
#include "d/d_albw_wolf_combat.h"
#include "d/d_albw_wolf_charge_hud.h"
#include "d/d_focused_arts.h"
#include "dusk/hurricane_test.h"
#include "d/d_albw_enemy_rupee.h"
#include "d/d_albw_boss.h"
#endif

static int plCutLRC[58] = {
    0,  //
    0,  // CUT_VERTICAL
    1,  // CUT_STAB
    1,  // CUT_SWEEP
    2,  // CUT_HORIZONTAL
    0,  // CUT_HEAD
    2,  // CUT_LEFT_SWEEP_FINISH
    1,  // CUT_DOWN_FINISH
    0,  // CUT_TURN_RIGHT
    0,  //
    0,  // CUT_JUMP
    2,  //
    1,  //
    1,  //
    1,  //
    2,  //
    2,  // CUT_AIR
    1,  //
    1,  // CUT_LARGE_JUMP_INIT
    0,  // CUT_LARGE_JUMP
    0,  // CUT_LARGE_JUMP_FINISH
    1,  // CUT_RIGHT_SWEEP_FINISH
    0,  // CUT_TURN_LEFT
    0,  // CUT_LARGE_TURN_LEFT
    0,  // CUT_LARGE_TURN_RIGHT
    0,  //
    0,  // CUT_FAST_MOVE
    0,  //
    0,  //
    0,  //
    0,  // CUT_TWIRL
    0,  // CUT_FAST
    0,  // CUT_STAB_FINISH
    2,  // CUT_STAB_COMBO
    0,  //
    0,  //
    0,  //
    2,  //
    1,  //
    0,  //
    0,  //
    0,  //
    0,  //
    0,  //
    0,  //
    0,  //
    0,  //
    0,  //
    0,  //
    0,  //
    0,  //
    2,  //
    1,  //
    0,  //
    0,  //
    0,  //
    0,  //
    0,  //
};

int pl_cut_LRC(int index) {
    if (index >= 58) {
        return 0;
    }

    return plCutLRC[index];
}

u16 cc_pl_cut_bit_get() {
    daPy_py_c* player_p = (daPy_py_c*)dComIfGp_getPlayer(0);

    u16 bit = 0;
    if (player_p->getCutType() == daPy_py_c::CUT_TYPE_NM_VERTICAL) {
        bit = 0x1;
    } else if (player_p->getCutType() == daPy_py_c::CUT_TYPE_NM_STAB) {
        bit = 0x2;
    } else if (player_p->getCutType() == daPy_py_c::CUT_TYPE_COMBO_STAB) {
        bit = 0x200;
    } else if (player_p->getCutType() == daPy_py_c::CUT_TYPE_NM_RIGHT) {
        bit = 0x4;
    } else if (player_p->getCutType() == daPy_py_c::CUT_TYPE_NM_LEFT) {
        bit = 0x8;
    } else if (player_p->getCutType() == daPy_py_c::CUT_TYPE_FINISH_LEFT) {
        bit = 0x20;
    } else if (player_p->getCutType() == daPy_py_c::CUT_TYPE_FINISH_VERTICAL) {
        bit = 0x40;
    } else if (player_p->getCutType() == daPy_py_c::CUT_TYPE_TURN_RIGHT ||
               player_p->getCutType() == daPy_py_c::CUT_TYPE_UNK_9 || player_p->getCutType() == daPy_py_c::CUT_TYPE_HORSE_TURN ||
               player_p->getCutType() == daPy_py_c::CUT_TYPE_LARGE_TURN_RIGHT)
    {
        bit = 0x80;
    } else if (player_p->getCutType() == daPy_py_c::CUT_TYPE_TURN_LEFT ||
               player_p->getCutType() == daPy_py_c::CUT_TYPE_LARGE_TURN_LEFT)
    {
        bit = 0x800;
    } else if (player_p->getCutType() == daPy_py_c::CUT_TYPE_JUMP) {
        bit = 0x100;
    } else if (player_p->getCutType() == daPy_py_c::CUT_TYPE_UNK_28 || player_p->getCutType() == daPy_py_c::CUT_TYPE_GUARD_ATTACK) {
        bit = 0x400;
    }

    return bit;
}

static int getMapInfo(s8 param_0) {
    int map_info = 30;
    if (param_0 == 1) {
        map_info = 31;
    } else if (param_0 == 2) {
        map_info = 32;
    }

    return map_info;
}

static u32 getHitId(cCcD_Obj* i_ccObj, int i_useReboundSe) {
    dCcD_GObjInf* dObj = static_cast<dCcD_GObjInf*>(i_ccObj);
    return dObj->getHitSeID(dObj->GetAtSe(), i_useReboundSe);
}

void def_se_set(Z2Creature* i_sound, cCcD_Obj* i_ccObj, u32 i_mapInfo, fopAc_ac_c* i_actor) {
    if (i_sound != NULL) {
        int useReboundSe;
        if (i_actor != NULL && fopAcM_CheckStatus(i_actor, 0x280000)) {
            useReboundSe = false;
        } else {
            useReboundSe = true;
        }
        i_sound->startCollisionSE(getHitId(i_ccObj, useReboundSe), i_mapInfo);
    }
}

static u8 at_power_get(dCcU_AtInfo* i_AtInfo) {
    u8 power = i_AtInfo->mpCollider->GetAtAtp();

    if (i_AtInfo->mPowerType == 0) {
        if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_10000000)) {
            power = 0;
        } else if (power == 1) {
            power = 1;
        } else if (power == 2) {
            power = 10;
        } else if (power == 3) {
            power *= 10;
        } else if (power == 6) {
            power = 80;
        } else if (power >= 4) {
            power = 200;
        }
    } else if (i_AtInfo->mPowerType == 6) {
        if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_10000000)) {
            power = 0;
        } else if (power == 1) {
            power = 1;
        } else if (power == 2) {
            power = 10;
        } else if (power == 6) {
            power = 80;
        } else if (power >= 3) {
            power *= 10;
        }
    } else if (i_AtInfo->mPowerType == 4) {
        if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_IRON_BALL)) {
            power = 200;
        } else if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_10000000)) {
            power = 0;
        } else if (power == 1) {
            power = 1;
        } else if (power == 2) {
            power = 10;
        } else if (power >= 3) {
            power *= 10;
        } else if (power == 6) {
            power = 80;
        } else if (power >= 4) {
            power = 200;
        }
    } else if (i_AtInfo->mPowerType == 5) {
        if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_ARROW)) {
            power = 0;
        } else if (power == 1) {
            power = 1;
        } else if (power == 2) {
            power = 3;
        } else if (power >= 3) {
            power = 4;
        }
    } else if (i_AtInfo->mPowerType == 2) {
        if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_IRON_BALL)) {
            power = 200;
        } else if (power == 1) {
            power = 1;
        } else if (power == 2) {
            power = 10;
        } else if (power == 3) {
            power = 20;
        } else if (power == 6) {
            power = 80;
        } else if (power >= 4) {
            power = 200;
        }
    } else if (i_AtInfo->mPowerType == 1) {
        if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_IRON_BALL) || power >= 4) {
            power = 200;
        } else {
            power *= 10;
        }
    } else if (i_AtInfo->mPowerType == 3) {
        if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_IRON_BALL) || power >= 4) {
            power = 200;
        } else if (power == 1) {
            power = 10;
        } else if (power == 2) {
            power = 20;
        } else if (power >= 3) {
            power = 40;
        }
    } else if (i_AtInfo->mPowerType == 7) {
        if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_THROW_OBJ | AT_TYPE_20000 | AT_TYPE_IRON_BALL)) {
            power = 50;
        } else if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_ARROW)) {
            power = 27;
        } else if (power == 1) {
            power = 1;
        } else if (power == 2) {
            power = 10;
        } else if (power == 3) {
            power *= 10;
        } else if (power == 6) {
            power = 80;
        } else if (power >= 4) {
            power = 200;
        }
    } else if (i_AtInfo->mPowerType == 8) {
        if (power == 1) {
            power = 3;
        } else if (power == 2) {
            power = 10;
        } else if (power == 6) {
            power = 80;
        } else if (power >= 3) {
            power *= 10;
        }
    } else if (i_AtInfo->mPowerType == 9 || i_AtInfo->mPowerType == 10) {
        if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_HOOKSHOT)) {
            power = 0;
        }
        if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_IRON_BALL)) {
            power = 19;
        }

        if (power == 1) {
            power = 4;
        } else if (power == 2) {
            power = 8;
        } else if (power == 3) {
            power *= 10;
        } else if (power == 6) {
            power = 80;
        } else if (power >= 4) {
            power = 200;
        }
    } else if (i_AtInfo->mPowerType == 11) {
        if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_HOOKSHOT)) {
            power = 0;
        }

        if (power == 1) {
            power = 10;
        } else if (power == 2) {
            power = 20;
        } else if (power == 3) {
            power = 30;
        } else if (power >= 4) {
            power = 100;
        }
    } else if (i_AtInfo->mPowerType == 12) {
        if (power == 1) {
            power = 10;
        } else if (power == 2) {
            power = 20;
        } else if (power == 3) {
            power = 30;
        } else if (power >= 4) {
            power = 100;
        }
    }

    return power;
}

fopAc_ac_c* at_power_check(dCcU_AtInfo* i_AtInfo) {
    if (i_AtInfo->mpCollider == NULL) {
        return NULL;
    }

    i_AtInfo->mpActor = i_AtInfo->mpCollider->GetAc();
    i_AtInfo->mHitType = 12;
    i_AtInfo->mAttackPower = 0;
    i_AtInfo->mHitBit = 0;

    if (i_AtInfo->mpActor != NULL) {
        i_AtInfo->mAttackPower = at_power_get(i_AtInfo);

        s16 ac_name = fopAcM_GetName(i_AtInfo->mpActor);
        if (ac_name == fpcNm_ALINK_e || ac_name == fpcNm_ALINK_e) {
            if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_8000) ||
                i_AtInfo->mpCollider->ChkAtType(AT_TYPE_IRON_BALL))
            {
                i_AtInfo->mHitType = HIT_TYPE_LINK_HEAVY_ATTACK;
            } else {
                i_AtInfo->mHitType = HIT_TYPE_LINK_NORMAL_ATTACK;
                i_AtInfo->mHitBit = cc_pl_cut_bit_get();
            }
        } else if (ac_name == fpcNm_NBOMB_e) {
            i_AtInfo->mHitType = HIT_TYPE_BOMB;
            i_AtInfo->mHitBit = 0x10000000;
        } else if (ac_name == fpcNm_BOOMERANG_e) {
            i_AtInfo->mHitType = HIT_TYPE_BOOMERANG;
            i_AtInfo->mHitBit = 0x40000000;
        } else if (ac_name == fpcNm_ARROW_e) {
            i_AtInfo->mHitType = HIT_TYPE_ARROW;
            i_AtInfo->mHitBit = 0x80000000;
        }
    }

    i_AtInfo->mHitStatus = 0;
    if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_40) ||
        i_AtInfo->mpCollider->ChkAtType(AT_TYPE_BOOMERANG) ||
        i_AtInfo->mpCollider->ChkAtType(AT_TYPE_SLINGSHOT) ||
        i_AtInfo->mpCollider->ChkAtType(AT_TYPE_SHIELD_ATTACK))
    {
        i_AtInfo->mHitType = HIT_TYPE_STUN;
    } else if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_HOOKSHOT)) {
        i_AtInfo->mHitBit = 0x1000;
    } else {
        s16 ac_name = fopAcM_GetName(i_AtInfo->mpActor);
        if (ac_name == fpcNm_ALINK_e || ac_name == fpcNm_ALINK_e) {
            if (dCcD_GetGObjInf(i_AtInfo->mpCollider)->GetAtSpl() == 1) {
                i_AtInfo->mHitStatus = 1;
            }
        } else if (i_AtInfo->mAttackPower >= 3) {
            i_AtInfo->mHitStatus = 1;
        }
    }

    return i_AtInfo->mpActor;
}

fopAc_ac_c* cc_at_check(fopAc_ac_c* i_enemy, dCcU_AtInfo* i_AtInfo) {
    daPy_py_c* player_p = (daPy_py_c*)dComIfGp_getPlayer(0);
    i_AtInfo->mpActor = at_power_check(i_AtInfo);

    f32 x_diff;
    f32 z_diff;
    if (i_AtInfo->mpActor != NULL) {
#if TARGET_PC
        if (dMeter2_isALBWLocked() && i_AtInfo->mpCollider != NULL &&
            fopAcM_GetGroup(i_enemy) == fopAc_ENEMY_e &&
            (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_BOOMERANG) ||
             i_AtInfo->mpCollider->ChkAtType(AT_TYPE_40))) {
            dAlbwLockout_onBoomerangHit(i_enemy);
        }
#endif
        cXyz tmp = i_AtInfo->mpActor->speed;
        tmp.y = 0.0f;
        if (tmp.abs() > 100.0f) {
            f32 x = i_AtInfo->mpActor->speed.x;
            f32 z = i_AtInfo->mpActor->speed.z;
            i_AtInfo->mHitDirection.y = cM_atan2s(-x, -z) + (s16)cM_rndFX(4000.0f);
        } else {
            if (fopAcM_GetName(i_AtInfo->mpActor) == fpcNm_BOOMERANG_e) {
                x_diff = i_enemy->current.pos.x - player_p->current.pos.x;
                z_diff = i_enemy->current.pos.z - player_p->current.pos.z;
                i_AtInfo->mHitDirection.y = cM_atan2s(-x_diff, -z_diff) + (s16)cM_rndFX(10000.0f);
            } else {
                x_diff = i_enemy->current.pos.x - i_AtInfo->mpActor->current.pos.x;
                z_diff = i_enemy->current.pos.z - i_AtInfo->mpActor->current.pos.z;
                i_AtInfo->mHitDirection.y = cM_atan2s(-x_diff, -z_diff);
            }
        }

        if (i_AtInfo->mHitType == HIT_TYPE_LINK_NORMAL_ATTACK &&
            player_p->getCutType() == daPy_py_c::CUT_TYPE_HEAD_JUMP)
        {
            i_AtInfo->mHitDirection.y = player_p->shape_angle.y;
        }

        if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_HOOKSHOT) &&
            fopAcM_CheckStatus(i_enemy, 0x380000))
        {
            i_AtInfo->mAttackPower = 0;
        }

        if (static_cast<dCcD_GObjInf*>(i_AtInfo->mpCollider)->GetAtMtrl() == dCcD_MTRL_LIGHT) {
            if (fopAcM_GetName(i_enemy) == fpcNm_B_GND_e) {
                i_AtInfo->mAttackPower = 0;
            } else if (fopAcM_GetName(i_enemy) != fpcNm_B_ZANT_e) {
                i_AtInfo->mAttackPower = 100;
            }
        }

        if (i_AtInfo->mHitType == HIT_TYPE_LINK_NORMAL_ATTACK) {
            if (!daPy_py_c::checkNowWolf()) {
                if (player_p->checkMasterSwordEquip()) {
                    i_AtInfo->mAttackPower *= 2;
                }

                if (daPy_py_c::checkWoodSwordEquip()) {
                    i_AtInfo->mAttackPower /= 2;
                }
            }

            if (player_p->getSwordAtUpTime()) {
                i_AtInfo->mAttackPower *= 2;
                i_AtInfo->mHitStatus = 1;
            }

            if (!daPy_py_c::checkNowWolf()) {
                if (dusk::isLinkHurricaneProc((daAlink_c*)player_p)) {
                    i_AtInfo->mAttackPower = dFocusedArts_getGsHurricaneHitPower();
                } else {
                    i_AtInfo->mAttackPower =
                        dFocusedArts_resolveMeleeDamage(i_AtInfo->mAttackPower, player_p->getCutType());
                }
            }

            if (dFocusedArts_isEndingBlowGreatSpinAoeActive()) {
                daAlink_c* link_p = (daAlink_c*)player_p;
                if (i_AtInfo->mpCollider == static_cast<cCcD_Obj*>(&link_p->mAtSph)) {
                    i_AtInfo->mAttackPower =
                        dFocusedArts_getEndingBlowGreatSpinAoePower(i_AtInfo->mAttackPower);
                }
            }
        }

#if TARGET_PC
        if (dusk::getSettings().game.invincibleEnemies &&
            fopAcM_GetGroup(i_enemy) == fopAc_ENEMY_e) {
            i_AtInfo->mAttackPower = 0;
        }
        // ============================================
        // NEW CODE — ALBW Port
        // Link damage decrease intercept. Runs after all attack-power
        // modifiers and after the invincibleEnemies zero-out above.
        // Divides mAttackPower by linkDamageDecreaseMult (global, default 1×).
        // Independent of per-category true max-HP sliders.
        // ============================================
        if (i_AtInfo->mAttackPower > 0 && fopAcM_GetGroup(i_enemy) == fopAc_ENEMY_e) {
            i_AtInfo->mAttackPower =
                dAlbwHP_applyMult(fopAcM_GetName(i_enemy), i_AtInfo->mAttackPower);
        }
        if (dMeter2_isALBWLocked() && i_AtInfo->mAttackPower > 0 &&
            fopAcM_GetGroup(i_enemy) == fopAc_ENEMY_e && i_AtInfo->mpCollider != NULL) {
            u32 atType = 0;
            if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_ARROW)) {
                atType = AT_TYPE_ARROW;
            } else if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_BOMB)) {
                atType = AT_TYPE_BOMB;
            } else if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_IRON_BALL)) {
                atType = AT_TYPE_IRON_BALL;
            } else if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_SLINGSHOT)) {
                atType = AT_TYPE_SLINGSHOT;
            }
            if (atType != 0) {
                dFocusedArts_applyItemDamageBoost(i_AtInfo->mAttackPower);
                dAlbwLockout_applyAttackPowerBoost(i_AtInfo->mAttackPower, atType);
            }
        } else if (i_AtInfo->mAttackPower > 0 && fopAcM_GetGroup(i_enemy) == fopAc_ENEMY_e &&
                   i_AtInfo->mpCollider != NULL) {
            u32 atType = 0;
            if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_ARROW)) {
                atType = AT_TYPE_ARROW;
            } else if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_BOMB)) {
                atType = AT_TYPE_BOMB;
            } else if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_IRON_BALL)) {
                atType = AT_TYPE_IRON_BALL;
            } else if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_SLINGSHOT)) {
                atType = AT_TYPE_SLINGSHOT;
            }
            if (atType != 0) {
                dFocusedArts_applyItemDamageBoost(i_AtInfo->mAttackPower);
            }
        }
        // Wolf field attack damage modifier.
        // Runs after all other attack-power adjustments (including Link
        // damage decrease above) so the split is applied to the
        // fully-resolved value.
        //
        // Link damage decrease already divided mAttackPower by rawMult.
        // To express charge damage as  base × rawMult × fraction  we
        // multiply by rawMult² here, which cancels the division and then
        // scales forward:  (base / rawMult) × rawMult² × fraction
        //                = base × rawMult × fraction.
        //
        // Twilight enemies (shadow/dusk forms):  70 % of scaled damage,
        //   no stun; these enemies feel weaker against wolf form.
        // Non-twilight enemies:                  50 % of scaled damage,
        //   compensated by the 300-frame stun dispatched after deduction.
        //
        // Zant excluded — his fight's phase scripts check AT_TYPE_WOLF_*
        // for hit-timing hooks and must not have damage or stun modified.
        // ============================================
        if (dAlbwWolfCombat_isEnabled() &&
            i_AtInfo->mAttackPower > 0 &&
            i_AtInfo->mpCollider->ChkAtType(AT_TYPE_MIDNA_LOCK) &&
            daPy_py_c::checkNowWolf() &&
            fopAcM_GetGroup(i_enemy) == fopAc_ENEMY_e &&
            fopAcM_GetName(i_enemy) != fpcNm_B_ZANT_e)
        {
            const int rawMult = dAlbwHP_getRawMult(fopAcM_GetName(i_enemy));
            if (dAlbwWolfStun_isTwilightEnemy(fopAcM_GetName(i_enemy))) {
                // Twilight: base × rawMult × 0.70, no stun
                i_AtInfo->mAttackPower =
                    (i_AtInfo->mAttackPower * rawMult * rawMult * 7) / 10;
                if (i_AtInfo->mAttackPower < 1) {
                    i_AtInfo->mAttackPower = 1;
                }
            } else {
                // Non-twilight: base × rawMult × 0.50, stun applied below
                i_AtInfo->mAttackPower =
                    (i_AtInfo->mAttackPower * rawMult * rawMult * 5) / 10;
                if (i_AtInfo->mAttackPower < 1) {
                    i_AtInfo->mAttackPower = 1;
                }
            }
        }
        // Boss Refinement: Armogohma core arrow hits chip 4% in dAlbwBoss_armogohmaOnBowCoreHit().
        if (dAlbwBoss_armogohmaShouldSuppressVanillaArrowDamage(i_enemy, i_AtInfo)) {
            i_AtInfo->mAttackPower = 0;
        }
        // ============================================
        // NEW CODE ENDS HERE
        // ============================================
#endif

        if (i_AtInfo->mAttackPower != 0) {
            i_enemy->health -= i_AtInfo->mAttackPower;
#if TARGET_PC
            if (fopAcM_GetGroup(i_enemy) == fopAc_ENEMY_e) {
                dAlbwEnemyRupees_tryKillAfterDamage(i_enemy, i_AtInfo->mAttackPower);
                if (i_AtInfo->mpCollider != NULL &&
                    !i_AtInfo->mpCollider->ChkAtType(AT_TYPE_NORMAL_SWORD) &&
                    !i_AtInfo->mpCollider->ChkAtType(AT_TYPE_MASTER_SWORD) &&
                    !i_AtInfo->mpCollider->ChkAtType(AT_TYPE_WOLF_ATTACK) &&
                    !i_AtInfo->mpCollider->ChkAtType(AT_TYPE_WOLF_CUT_TURN) &&
                    !i_AtInfo->mpCollider->ChkAtType(AT_TYPE_MIDNA_LOCK) &&
                    (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_ARROW) ||
                     i_AtInfo->mpCollider->ChkAtType(AT_TYPE_BOMB) ||
                     i_AtInfo->mpCollider->ChkAtType(AT_TYPE_SLINGSHOT) ||
                     i_AtInfo->mpCollider->ChkAtType(AT_TYPE_IRON_BALL) ||
                     i_AtInfo->mpCollider->ChkAtType(AT_TYPE_40)))
                {
                    dFocusedArts_onConnectedItemHit();
                }
            }
#endif
        }

        s8 pause_time = 0;
        if (i_AtInfo->mAttackPower != 0 && i_enemy->health <= 0) {
            i_AtInfo->mHitStatus = 2;
            i_enemy->health = 0;
#if TARGET_PC
            if (fopAcM_GetGroup(i_enemy) == fopAc_ENEMY_e) {
                dusk::AchievementSystem::get().signal("enemy_killed");
                const auto* link = static_cast<const daAlink_c*>(daPy_getPlayerActorClass());
                if (link != nullptr && link->mProcID == daAlink_c::PROC_CUT_FINISH && link->mIsRollstab) {
                    dusk::AchievementSystem::get().signal("rollstab_kill");
                }
                // ============================================
                // NEW CODE — ALBW Port
                // Magic jar bonus drop on common enemy kill. Fires for all
                // actors in fopAc_ENEMY_e group — excludes bosses, traps,
                // and final bosses which live in different actor groups.
                // 10% drop rate (matches original orange rupee ALBW bonus drop).
                // ============================================
                if (cM_rndF(1.0f) < 0.10f) {
                    static const cXyz s_jarDropScale(1.3f, 1.3f, 1.3f);
                    fopAcM_createItem(&i_enemy->current.pos, dItemNo_L_MAGIC_e, -1,
                                      fopAcM_GetRoomNo(i_enemy), NULL, &s_jarDropScale, 0);
                }
                // Rupee grant handled by tryKillAfterDamage() above (includes lethal thresholds).
                // ============================================
                // NEW CODE ENDS HERE
                // ============================================
            }
#endif
        }

#if TARGET_PC
        // ============================================
        // NEW CODE — ALBW Port
        // Wolf bite charge accumulation and field attack stun dispatch.
        //
        // Bite tracking: 5 normal wolf bites (AT not MIDNA_LOCK) grant
        // +1 charge (cap 2) and heal 1/4 heart when Link is at or below
        // 50 % max HP.  Bites that deal 0 damage are ignored.
        //
        // Field attack stun: non-twilight enemies that survived the hit
        // (health > 0 after deduction) are frozen via fpcM_PauseEnable
        // for WOLF_STUN_FRAMES (300 frames).  Twilight enemies and Zant
        // are excluded.
        // ============================================
        if (dAlbwWolfCombat_isEnabled() &&
            fopAcM_GetGroup(i_enemy) == fopAc_ENEMY_e &&
            fopAcM_GetName(i_enemy) != fpcNm_B_ZANT_e)
        {
            auto* link = static_cast<daAlink_c*>(daPy_getPlayerActorClass());
            if (link != NULL && daPy_py_c::checkNowWolf()) {

                // --- Wolf normal bite: charge accumulation ---
                if (i_AtInfo->mHitType == HIT_TYPE_LINK_NORMAL_ATTACK &&
                    !i_AtInfo->mpCollider->ChkAtType(AT_TYPE_MIDNA_LOCK) &&
                    i_AtInfo->mAttackPower > 0)
                {
                    link->mWolfBiteCount++;
                    if (link->mWolfBiteCount >= 5) {
                        link->mWolfBiteCount = 0;
                        if (link->mWolfChargeCount < 2) {
                            link->mWolfChargeCount++;
                            dAlbwWolfChargeHud_notify();
                        }
                        // Heal 1/4 heart when at or below 50 % max HP
                        const u16 curHP = dComIfGs_getLife();
                        const u16 maxHP = dComIfGs_getMaxLifeGauge();
                        if (curHP * 2 <= maxHP) {
                            dComIfGp_setItemLifeCount(1.0f, 0);
                        }
                    }
                }

                // --- Wolf field attack: stun non-twilight survivors ---
                if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_MIDNA_LOCK) &&
                    i_AtInfo->mAttackPower > 0 &&
                    i_enemy->health > 0 &&
                    !dAlbwWolfStun_isTwilightEnemy(fopAcM_GetName(i_enemy)))
                {
                    dAlbwWolfStun_apply(i_enemy);
                }
            }
        }
        // ============================================
        // NEW CODE ENDS HERE
        // ============================================
#endif

        int uvar8;
        if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_HOOKSHOT) &&
            !fopAcM_CheckStatus(i_enemy, 0x280000))
        {
            uvar8 = 1;
        } else {
            uvar8 = 0;
        }

        if (i_AtInfo->mpSound != NULL) {
            if (i_AtInfo->field_0x18 != 0) {
                i_AtInfo->mpSound->startCollisionSE(getHitId(i_AtInfo->mpCollider, uvar8),
                                                    i_AtInfo->field_0x18);
            } else {
                i_AtInfo->mpSound->startCollisionSE(getHitId(i_AtInfo->mpCollider, uvar8),
                                                    getMapInfo(i_AtInfo->mHitStatus));
            }
        }

        if (i_AtInfo->mHitStatus != 0) {
            pause_time = 5;
        } else {
            if (i_AtInfo->mAttackPower > 1) {
                pause_time = 2;
            }
        }

        if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_MIDNA_LOCK) ||
            ((daPy_py_c*)dComIfGp_getPlayer(0))->checkHorseRide())
        {
            // actor is Bulblin or Horseback Ganon
            if ((fopAcM_GetName(i_enemy) == fpcNm_E_RD_e && ((s8*)i_enemy)[0x129a] != 0) ||
                fopAcM_GetName(i_enemy) == fpcNm_B_GND_e)
            {
                pause_time = 3;
            } else {
                pause_time = 0;
            }
        }

        s16 ac_name = fopAcM_GetName(i_enemy);
        // actor is Stalkin, Chu, Keese, Shadow Keese, Shadow Vermin, Baby Gohma, or Rat
        if (ac_name == fpcNm_E_BS_e || ac_name == fpcNm_E_SM2_e || ac_name == fpcNm_E_BA_e ||
            ac_name == fpcNm_E_YK_e || ac_name == fpcNm_E_YG_e || ac_name == fpcNm_E_GM_e ||
            ac_name == fpcNm_E_MS_e)
        {
            pause_time = 0;
        }

        if ((i_AtInfo->mHitType == HIT_TYPE_LINK_NORMAL_ATTACK ||
             i_AtInfo->mpCollider->ChkAtType(AT_TYPE_THROW_OBJ)) &&
            !player_p->checkCutJumpCancelTurn())
        {
            if (i_AtInfo->mpCollider->ChkAtType(AT_TYPE_THROW_OBJ)) {
                pause_time = 4;
            }
            dScnPly_c::setPauseTimer(pause_time);
        }
    }

    return i_AtInfo->mpActor;
}

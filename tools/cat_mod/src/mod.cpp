#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_npc_ne.h"
#include "d/d_com_inf_game.h"
#include "d/d_meter2_info.h"
#include "d/d_msg_object.h"
#include "dusk/hook.hpp"
#include "dusk/mod_api.h"
#include "dusk/mod_utils.h"
#include "f_op/f_op_actor.h"
#include "f_op/f_op_actor_mng.h"
#include "f_op/f_op_overlap_mng.h"
#include "m_Do/m_Do_audio.h"
#include "m_Do/m_Do_controller_pad.h"

#include <cstdio>
#include <cstring>

static constexpr s16 ACTOR_NPC_NE    = 269;
static constexpr u16 NOTIFY_MSG_ID   = 0xFFFE;
static const char*   DEATH_MSG_TEXT  = "It seems Chloe has died...";

using GetStringEntry = dusk::HookEntry<&dMsgObject_c::getString>;

static void on_getString_post(void* args, void* retval) {
    if (dusk::arg<u32>(args, 0) != NOTIFY_MSG_ID) {
        return;
    }

    strcpy(dusk::arg<char*>(args, 5), DEATH_MSG_TEXT);
    strcpy(dusk::arg<char*>(args, 7), DEATH_MSG_TEXT);

    if (retval) {
        *static_cast<bool*>(retval) = true;
    }
}
static constexpr int CAT_MAX_HP   = 1;

static fpc_ProcID  s_cat_id   = fpcM_ERROR_PROCESS_ID_e;
static int         s_cat_hp   = CAT_MAX_HP;
static bool        s_cat_dead = false;

static bool  s_summon_carry = false;

static bool  s_has_spawn          = false;
static cXyz  s_spawn_pos          = {};
static s8    s_spawn_room         = -1;
static char  s_spawn_stage[8]     = {};

static DuskElemHandle s_el_hp       = nullptr;
static DuskElemHandle s_el_hp_bar   = nullptr;
static DuskElemHandle s_el_status   = nullptr;

static fopAc_ac_c* getCat() {
    if (s_cat_id == fpcM_ERROR_PROCESS_ID_e) {
        return nullptr;
    }
    fopAc_ac_c* cat = fopAcM_SearchByID(s_cat_id);
    if (!cat) {
        s_cat_id = fpcM_ERROR_PROCESS_ID_e;
    }
    return cat;
}

static void killCat() {
    fopAc_ac_c* cat = getCat();
    if (cat) {
        fopAcM_delete(cat);
        s_cat_id = fpcM_ERROR_PROCESS_ID_e;
    }
    mDoAud_seStartMenu(Z2SE_CAT_CRY_ANNOY);
    s_cat_dead = true;
    dMeter2Info_setFloatingMessage(NOTIFY_MSG_ID, 150, false);
    dusk::g_api->log_info("cat_mod: the cat has died");
}

static bool inSpawnStage() {
    return strncmp(dComIfGp_getStartStageName(), s_spawn_stage, sizeof(s_spawn_stage)) == 0;
}

static void spawnCat(bool carry = false) {
    if (s_cat_dead || dComIfGp_event_runCheck()) {
        return;
    }
    daAlink_c* link = daAlink_getAlinkActorClass();
    if (!link) {
        return;
    }

    cXyz pos;
    s8 roomNo;
    csXyz angle = {};
    if (s_has_spawn && inSpawnStage()) {
        pos    = s_spawn_pos;
        roomNo = s_spawn_room;
    } else {
        f32 yaw = link->shape_angle.y;
        pos = link->current.pos;
        pos.x += cM_ssin(yaw) * 30.0f;
        pos.z += cM_scos(yaw) * 30.0f;
        roomNo = link->current.roomNo;
        angle.y = (s16)(link->shape_angle.y + (s16)0x8000);
    }
    cXyz scale = {1.0f, 1.0f, 1.0f};

    s_cat_id = fopAcM_createInPlayScene(
        ACTOR_NPC_NE, -1, &pos, roomNo, &angle, &scale, -1);

    if (s_cat_id != fpcM_ERROR_PROCESS_ID_e) {
        dusk::g_api->log_info("cat_mod: cat spawned (hp %d/%d)", s_cat_hp, CAT_MAX_HP);
        s_summon_carry = carry;
    }
}

static void on_setDamagePoint_post(void* args, void* /*retval*/) {
    if (s_cat_dead) {
        return;
    }
    int dmg = dusk::arg<int>(args, 1);
    if (dmg <= 0) {
        return;
    }
    fopAc_ac_c* cat = getCat();
    bool cat_free = cat != nullptr && !fopAcM_checkCarryNow(cat);
    if (cat_free) {
        return;
    }
    s_cat_hp -= dmg;
    dusk::g_api->log_info("cat_mod: cat took %d damage (hp %d/%d)", dmg, s_cat_hp, CAT_MAX_HP);
    if (s_cat_hp <= 0) {
        s_cat_hp = 0;
        killCat();
    }
    else {
        mDoAud_seStartMenu(Z2SE_CAT_CRY_CARRY);
    }
}

static void BuildPanel(DuskPanelHandle panel, void*) {
    DuskModAPI* api = dusk::g_api;
    api->panel_add_section(panel, "Cat");
    s_el_status = api->panel_add_dyn_text(panel, s_cat_dead ? "Dead" : "Alive");

    float fraction = static_cast<float>(s_cat_hp) / CAT_MAX_HP;
    s_el_hp_bar = api->panel_add_progress(panel, fraction);

    char buf[32];
    snprintf(buf, sizeof(buf), "%d / %d HP", s_cat_hp, CAT_MAX_HP);
    s_el_hp = api->panel_add_dyn_text(panel, buf);
}

static void UpdatePanel(void*) {
    DuskModAPI* api = dusk::g_api;
    api->elem_set_text(s_el_status, s_cat_dead ? "Dead" : "Alive");

    float fraction = static_cast<float>(s_cat_hp) / CAT_MAX_HP;
    api->elem_set_progress(s_el_hp_bar, fraction);

    char buf[32];
    snprintf(buf, sizeof(buf), "%d / %d HP", s_cat_hp, CAT_MAX_HP);
    api->elem_set_text(s_el_hp, buf);
}

extern "C" {

void mod_init(DuskModAPI* api) {
    dusk::init(api);
    dusk::hookAddPost<&dMsgObject_c::getString>(on_getString_post);
    dusk::hookAddPost<&daAlink_c::setDamagePoint>(on_setDamagePoint_post);
    api->register_tab_content(BuildPanel, nullptr);
    api->register_tab_update(UpdatePanel, nullptr);
    api->log_info("cat_mod: ready");
}

void mod_tick(DuskModAPI* api) {
    (void)api;

    if (s_cat_dead) {
        return;
    }

    fopAc_ac_c* cat = getCat();

    // Load zone detected: dismiss cat into inventory before the area unloads.
    if (cat && fopAcM_checkCarryNow(cat) && fopOvlpM_IsDoingReq()) {
        fopAcM_delete(cat);
        s_cat_id    = fpcM_ERROR_PROCESS_ID_e;
        s_has_spawn = false;
        daAlink_c* link = daAlink_getAlinkActorClass();
        if (link) {
            link->procPreActionUnequipInit(0, nullptr);
        }
        return;
    }

    if (!cat) {
        if (s_has_spawn && inSpawnStage() && !dComIfGp_event_runCheck()) {
            spawnCat();
        } else if (mDoCPd_c::getHoldR(PAD_1) && mDoCPd_c::getTrigZ(PAD_1)) {
            consumeInput(PAD_1, PAD_TRIGGER_Z);
            spawnCat(true);
        }
        return;
    }

    if (s_summon_carry) {
        s_summon_carry = false;
        daAlink_c* link = daAlink_getAlinkActorClass();
        if (link) {
            link->field_0x27f4 = cat;
            link->procGrabReadyInit();
        }
    }

    if (!fopAcM_checkCarryNow(cat)) {
        memcpy(s_spawn_stage, dComIfGp_getStartStageName(), sizeof(s_spawn_stage));
        s_spawn_room = cat->current.roomNo;
        s_spawn_pos  = cat->current.pos;
        s_has_spawn  = true;
    }

    npc_ne_class* ne = static_cast<npc_ne_class*>(cat);
    ne->mBehavior = npc_ne_class::BHV_TAME;
    ne->mNoFollow = 0;
    ne->mTexture  = 0;
    ne->mBtkFrame = 0;

    if (mDoCPd_c::getHoldR(PAD_1) && mDoCPd_c::getTrigZ(PAD_1) && fopAcM_checkCarryNow(cat)) {
        consumeInput(PAD_1, PAD_TRIGGER_Z);
        fopAcM_delete(cat);
        s_cat_id    = fpcM_ERROR_PROCESS_ID_e;
        s_has_spawn = false;
        daAlink_c* link = daAlink_getAlinkActorClass();
        if (link) {
            link->procPreActionUnequipInit(0, nullptr);
        }
    }
}

void mod_cleanup(DuskModAPI* api) {
    (void)api;
    s_cat_id       = fpcM_ERROR_PROCESS_ID_e;
    s_cat_hp       = CAT_MAX_HP;
    s_cat_dead     = false;
    s_summon_carry = false;
    s_el_hp        = nullptr;
    s_el_hp_bar    = nullptr;
    s_el_status    = nullptr;
}

}

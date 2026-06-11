// ============================================
// NEW CODE — ALBW Port
// Death rupee halving + recovery orb at death XYZ.
// ============================================
#include "d/d_albw_death_rupee.h"

#if TARGET_PC

#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_player.h"
#include "d/actor/d_a_obj_drop.h"
#include "d/d_bg_s.h"
#include "d/d_com_inf_game.h"
#include "dusk/settings.h"
#include "f_op/f_op_actor.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_name.h"
#include "Z2AudioLib/Z2Instances.h"
#include <cstdarg>
#include <cstdlib>
#include <cstdio>
#include <cstring>

namespace {

static u16       sLastLostRupees = 0;
static u16       sOrbRecovery    = 0;
static bool      sOrbPending     = false;
static bool      sOrbSpawned     = false;
static bool      sOrbCreateGate       = false;
static bool      sOrbDropCreatePending = false;
static int       sOrbMissingFrames    = 0;
static int       sOrbSpawnCooldown    = 0;
static fpc_ProcID sOrbActorId         = fpcM_ERROR_PROCESS_ID_e;
static char      sOrbStage[32]   = {};
static int       sOrbRoom        = -1;
static cXyz      sOrbPos         = { 0.0f, 0.0f, 0.0f };

// ym_swbit=0xFF → daObjDrop_c::modeInit spawns floating tear without a shadow insect parent.
static constexpr u32 kRecoveryDropParams = 0x0000FF00u;
// Slightly above Link's head so the particle glow reads clearly at a distance.
static constexpr f32 kOrbFloatAboveGround = 135.0f;

static bool sTearRenderFlagWasSet[3] = {};

static void pushTearRenderFlags() {
    for (int i = 0; i < 3; ++i) {
        sTearRenderFlagWasSet[i] = dComIfGs_isLightDropGetFlag((u8)i) != FALSE;
        dComIfGs_onLightDropGetFlag((u8)i);
    }
}

static void popTearRenderFlags() {
    for (int i = 0; i < 3; ++i) {
        if (!sTearRenderFlagWasSet[i]) {
            dComIfGs_offLightDropGetFlag((u8)i);
        }
        sTearRenderFlagWasSet[i] = false;
    }
}

static void orbDebugReset() {
    // Truncate once per session so a multi-death test (dungeon + overworld)
    // keeps every run in the log.
    static bool sResetDone = false;
    if (sResetDone) {
        return;
    }
    sResetDone = true;

    char path[512];
    path[0] = '\0';
    const char* user = getenv("USERPROFILE");
    if (user && user[0] != '\0') {
        snprintf(path, sizeof(path), "%s/Documents/dusklight/albw_orb_debug.txt", user);
    } else {
        strncpy(path, "albw_orb_debug.txt", sizeof(path) - 1);
    }

    FILE* fp = fopen(path, "w");
    if (!fp) {
        fp = fopen("albw_orb_debug.txt", "w");
    }
    if (fp) {
        fprintf(fp, "--- ALBW death recovery orb ---\n");
        fclose(fp);
    }
}

static void orbDebugLog(const char* fmt, ...) {
    char path[512];
    path[0] = '\0';
    const char* user = getenv("USERPROFILE");
    if (user && user[0] != '\0') {
        snprintf(path, sizeof(path), "%s/Documents/dusklight/albw_orb_debug.txt", user);
    } else {
        strncpy(path, "albw_orb_debug.txt", sizeof(path) - 1);
    }

    FILE* fp = fopen(path, "a");
    if (!fp) {
        fp = fopen("albw_orb_debug.txt", "a");
    }
    if (!fp) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    vfprintf(fp, fmt, args);
    va_end(args);
    fclose(fp);
}

static void destroySpawnedOrbActor() {
    if (sOrbActorId == fpcM_ERROR_PROCESS_ID_e) {
        return;
    }
    if (fopAc_ac_c* actor = fopAcM_SearchByID(sOrbActorId)) {
        orbDebugLog("destroy orb actor id=%u\n", (unsigned)sOrbActorId);
        fopAcM_delete(actor);
    }
    sOrbActorId = fpcM_ERROR_PROCESS_ID_e;
}

static void resetOrbState(bool deleteActor) {
    if (deleteActor) {
        destroySpawnedOrbActor();
    } else {
        sOrbActorId = fpcM_ERROR_PROCESS_ID_e;
    }
    popTearRenderFlags();
    sOrbPending            = false;
    sOrbSpawned            = false;
    sOrbRecovery           = 0;
    sOrbDropCreatePending  = false;
    sOrbMissingFrames      = 0;
    sOrbSpawnCooldown      = 0;
    sOrbStage[0]           = '\0';
    sOrbRoom               = -1;
}

static void clearOrbState() {
    resetOrbState(true);
}

// Vanilla tear pickup sets FLG3_UNK_200000 + field_0x346c; in overworld that glow never clears.
static void clearLinkTearCollectEffect() {
    if (daPy_py_c* py = daPy_getLinkPlayerActorClass()) {
        py->offNoResetFlg3(daPy_py_c::FLG3_UNK_200000);
    }
    if (daAlink_c* alink = daAlink_getAlinkActorClass()) {
        alink->field_0x346c = -1.0f;
    }
}

}  // namespace

bool dALBWDeathRupees_isEnabled() {
    return dusk::getSettings().game.deathRecoveryOrb.getValue();
}

void dALBWDeathRupees_applyHalvingOnDeath() {
    if (!dALBWDeathRupees_isEnabled()) {
        return;
    }

    sLastLostRupees = 0;
    clearOrbState();
    orbDebugReset();

    const u16 wallet = dComIfGs_getRupee();
    if (wallet == 0) {
        orbDebugLog("death: wallet=0, no orb\n");
        return;
    }

    const u16 lost = (u16)((wallet + 1) / 2);
    const u16 kept = wallet - lost;
    dComIfGs_setRupee(kept);
    sLastLostRupees = lost;
    sOrbRecovery    = (u16)(lost / 2);
    if (sOrbRecovery == 0) {
        orbDebugLog("death: wallet=%u lost=%u recovery=0, no orb\n", (unsigned)wallet,
                    (unsigned)lost);
        return;
    }

    daPy_py_c* link = daPy_getLinkPlayerActorClass();
    if (!link) {
        orbDebugLog("death: no link actor\n");
        return;
    }

    sOrbPos  = link->current.pos;
    // Gameover can leave Link below the floor; prefer the last in-bounds Y.
    if (link->old.pos.abs(sOrbPos) > 1.0f && link->old.pos.y > sOrbPos.y) {
        sOrbPos.y = link->old.pos.y;
    }
    sOrbRoom = fopAcM_GetRoomNo(link);
    // Death stage: lastPlay is authoritative at gameover (see d_gameover.cpp Oocoo hook).
    // Room spawn uses getStartStageName() once the target stage is loading.
    const char* stage = dComIfGp_getLastPlayStageName();
    if (!stage || stage[0] == '\0') {
        stage = dComIfGp_getStartStageName();
    }
    if (stage) {
        strncpy(sOrbStage, stage, sizeof(sOrbStage) - 1);
        sOrbStage[sizeof(sOrbStage) - 1] = '\0';
    }

    sOrbPending = true;
    sOrbSpawned = false;

    // Kick the supplemental tear-archive load now (async) so the FX resources
    // are resident by the time the player returns to the death room.
    if (dPa_control_c* pa = g_dComIfG_gameInfo.play.getParticle()) {
        pa->ensureTearSceneRes();
    }

    orbDebugLog(
        "death: wallet=%u lost=%u kept=%u recovery=%u stage=%s startStage=%s lastStage=%s "
        "room=%d pos=(%.1f,%.1f,%.1f)\n",
        (unsigned)wallet, (unsigned)lost, (unsigned)kept, (unsigned)sOrbRecovery, sOrbStage,
        dComIfGp_getStartStageName() ? dComIfGp_getStartStageName() : "(null)",
        dComIfGp_getLastPlayStageName() ? dComIfGp_getLastPlayStageName() : "(null)", sOrbRoom,
        sOrbPos.x, sOrbPos.y, sOrbPos.z);
    OS_REPORT("ALBW death orb: lost=%u recovery=%u stage=%s room=%d pos=(%.1f,%.1f,%.1f)\n",
              (unsigned)sLastLostRupees, (unsigned)sOrbRecovery, sOrbStage, sOrbRoom, sOrbPos.x,
              sOrbPos.y, sOrbPos.z);
}

u16 dALBWDeathRupees_getLastLostAmount() {
    return sLastLostRupees;
}

u16 dALBWDeathRupees_getOrbRecoveryAmount() {
    return sOrbRecovery;
}

void dALBWDeathRupees_tickSpawn() {
    if (!dALBWDeathRupees_isEnabled()) {
        return;
    }
    if (!sOrbPending || sOrbRecovery == 0) {
        return;
    }

    // Poll the async supplemental tear-archive load; once registered, the
    // orb's per-frame body FX retry (daObjDrop_c::execute) picks it up.
    if (dPa_control_c* pa = g_dComIfG_gameInfo.play.getParticle()) {
        pa->ensureTearSceneRes();
    }

    const char* stage = dComIfGp_getStartStageName();
    if (!stage || stage[0] == '\0') {
        return;
    }

    dALBWDeathRupees_trySpawnOrbInRoom(stage, dComIfGp_roomControl_getStayNo());
}

void dALBWDeathRupees_trySpawnOrbInRoom(const char* stageName, int roomNo) {
    if (!dALBWDeathRupees_isEnabled()) {
        return;
    }
    if (!sOrbPending || sOrbRecovery == 0 || !stageName) {
        return;
    }
    if (roomNo != sOrbRoom || strcmp(stageName, sOrbStage) != 0) {
        return;
    }

    if (sOrbSpawnCooldown > 0) {
        sOrbSpawnCooldown--;
        return;
    }

    if (sOrbSpawned) {
        if (sOrbDropCreatePending) {
            return;
        }
        if (sOrbActorId != fpcM_ERROR_PROCESS_ID_e && fopAcM_SearchByID(sOrbActorId)) {
            sOrbMissingFrames = 0;
            return;
        }
        if (++sOrbMissingFrames < 30) {
            return;
        }
        orbDebugLog("spawn: actor lost, respawning (was id=%u)\n", (unsigned)sOrbActorId);
        sOrbMissingFrames = 0;
        sOrbSpawned         = false;
        sOrbActorId         = fpcM_ERROR_PROCESS_ID_e;
    }

    static const csXyz kAngle(0, 0, 0);
    static const cXyz  kScale(1.35f, 1.35f, 1.35f);

    orbDebugLog("spawn: creating at %s room=%d pos=(%.1f,%.1f,%.1f) recovery=%u\n", stageName,
                roomNo, sOrbPos.x, sOrbPos.y, sOrbPos.z, (unsigned)sOrbRecovery);
    OS_REPORT("ALBW death orb: spawning at %s room=%d pos=(%.1f,%.1f,%.1f) recovery=%u\n",
              stageName, roomNo, sOrbPos.x, sOrbPos.y, sOrbPos.z, (unsigned)sOrbRecovery);

    pushTearRenderFlags();
    sOrbDropCreatePending = true;
    sOrbCreateGate        = true;
    fopAc_ac_c* actor     = fopAcM_fastCreate(fpcNm_Obj_Drop_e, kRecoveryDropParams, &sOrbPos, roomNo,
                                              &kAngle, &kScale, -1, NULL, NULL);
    sOrbCreateGate = false;
    if (!actor) {
        sOrbDropCreatePending = false;
        sOrbSpawnCooldown     = 30;
        orbDebugLog("spawn: fopAcM_fastCreate FAILED\n");
        OS_REPORT("ALBW death orb: create FAILED\n");
        return;
    }

    sOrbActorId = fopAcM_GetID(actor);
    sOrbSpawned = true;
    orbDebugLog("spawn: ready id=%u pos=(%.1f,%.1f,%.1f)\n", (unsigned)sOrbActorId, sOrbPos.x,
                sOrbPos.y, sOrbPos.z);
    OS_REPORT("ALBW death orb: created id=%u\n", (unsigned)sOrbActorId);
}

bool dALBWDeathRupees_allowOrbDropCreate() {
    if (!dALBWDeathRupees_isEnabled()) {
        return false;
    }
    return sOrbCreateGate || sOrbDropCreatePending;
}

void dALBWDeathRupees_onOrbDropCreated(fopAc_ac_c* dropActor) {
    if (!dropActor || !sOrbDropCreatePending) {
        return;
    }
    sOrbDropCreatePending = false;
    sOrbActorId           = fopAcM_GetID(dropActor);
}

void dALBWDeathRupees_finishRecoveryDropSetup(daObjDrop_c* drop) {
    if (!drop || sOrbActorId != fopAcM_GetID(drop)) {
        return;
    }

    // Place at death height — GroundCross often hits room floor far below ledges/upper floors.
    cXyz pos = sOrbPos;
    pos.y    = sOrbPos.y + kOrbFloatAboveGround;
    if (pos.y < 80.0f) {
        pos.y = 80.0f;
    }

    // groundY logged only (not used for placement).
    dBgS_GndChk gnd;
    gnd.SetPos(&sOrbPos);
    const f32 groundY = dComIfG_Bgsp().GroundCross(&gnd);

    drop->current.pos = pos;
    drop->home.pos    = pos;
    drop->home.pos.y += 75.0f;
    drop->speedF      = 0.0f;
    drop->setMode(daObjDrop_c::MODE_WAIT_e);
    drop->mModeAction = 2;
    drop->mModeTimer  = 0;

    drop->removeBodyEffect();
    drop->createBodyEffect();
    drop->mSound.startSound(Z2SE_OBJ_LIGHTDROP_APPEAR, 0, -1);

    // Tear visuals are scene particles (Pscene###.jpc). Log which archive this
    // stage loaded and whether it contains the tear body FX — dungeons that
    // load an archive without them produce an invisible (but working) orb.
    int bodyFx = 0;
    for (int i = 0; i < 6; i++) {
        if (drop->mpBodyEffEmtrs[i] != NULL) {
            bodyFx++;
        }
    }
    dPa_control_c* pa     = g_dComIfG_gameInfo.play.getParticle();
    const int sceneNo     = pa ? pa->getScenePrtclNo() : -1;
    const bool hasTearRes = pa && pa->hasSceneParticleRes(0x838B);
    const bool suppReady  = pa && pa->ensureTearSceneRes();

    orbDebugLog("created: id=%u pos=(%.1f,%.1f,%.1f) groundY=%.1f bodyFx=%d/6 "
                "sceneJpc=Pscene%03d hasTearRes=%d suppRes=%d\n",
                (unsigned)sOrbActorId, pos.x, pos.y, pos.z, groundY, bodyFx, sceneNo,
                hasTearRes ? 1 : 0, suppReady ? 1 : 0);
}

void dALBWDeathRupees_onOrbDropCreateAborted() {
    if (!sOrbDropCreatePending) {
        return;
    }
    sOrbDropCreatePending = false;
    sOrbSpawned           = false;
    sOrbActorId           = fpcM_ERROR_PROCESS_ID_e;
    sOrbSpawnCooldown     = 60;
    orbDebugLog("create aborted (tbox gate or actor init failed)\n");
}

bool dALBWDeathRupees_isRecoveryOrbDrop(const fopAc_ac_c* dropActor) {
    if (!dropActor || sOrbRecovery == 0) {
        return false;
    }
    return fopAcM_GetID(dropActor) == sOrbActorId;
}

static bool grantOrbRecovery() {
    if (sOrbRecovery == 0) {
        return false;
    }

    const u16 wallet = dComIfGs_getRupee();
    const u16 max    = dComIfGs_getRupeeMax();
    u16       next   = wallet + sOrbRecovery;
    if (next > max) {
        next = max;
    }
    dComIfGs_setRupee(next);

    Z2GetAudioMgr()->seStart(Z2SE_SY_LIGHT_DROP_GET, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
    clearLinkTearCollectEffect();

    orbDebugLog("grant: +%u rupees (wallet %u -> %u)\n", (unsigned)sOrbRecovery, (unsigned)wallet,
                (unsigned)next);
    OS_REPORT("ALBW death orb: granted %u rupees (wallet now %u)\n", (unsigned)sOrbRecovery,
              (unsigned)next);

    // Do not delete the drop actor here — pickup runs inside daObjDrop_c::execute.
    resetOrbState(false);
    sLastLostRupees = 0;
    return true;
}

bool dALBWDeathRupees_onOrbDropGet(fopAc_ac_c* dropActor) {
    if (!dALBWDeathRupees_isRecoveryOrbDrop(dropActor)) {
        return false;
    }
    return grantOrbRecovery();
}

bool dALBWDeathRupees_onOrbItemGet(fopAc_ac_c* itemActor) {
    if (!itemActor || sOrbRecovery == 0) {
        return false;
    }
    if (fopAcM_GetID(itemActor) != sOrbActorId) {
        return false;
    }
    return grantOrbRecovery();
}

#endif  // TARGET_PC

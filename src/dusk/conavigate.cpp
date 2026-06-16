#include "dusk/conavigate.h"

#if DUSK_ENABLE_CONAVIGATE

#include "SSystem/SComponent/c_counter.h"
#include "d/actor/d_a_alink.h"
#include "d/d_albw_combat.h"
#include "d/d_com_inf_game.h"
#include "d/d_meter2_info.h"
#include "d/d_save.h"
#include "dusk/main.h"
#include "dusk/settings.h"
#include "f_ap/f_ap_game.h"
#include "m_Do/m_Do_controller_pad.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace dusk::conavigate {

namespace {

bool g_active = false;
FILE* g_log = nullptr;
char g_logPath[512] = {};
u32 g_lastHold = 0;
u32 g_lastTrig = 0;
f32 g_lastStickX = 0.0f;
f32 g_lastStickY = 0.0f;
s16 g_lastStickAngle = 0;
u32 g_lastSampleFrame = 0;

bool envEnabled() {
    const char* env = std::getenv("DUSK_CONAVIGATE");
    if (!env || env[0] == '\0') {
        return false;
    }
    return std::strcmp(env, "0") != 0 && std::strcmp(env, "false") != 0 && std::strcmp(env, "FALSE") != 0;
}

void writeLine(const char* fmt, ...) {
    if (!g_log) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    std::fprintf(g_log, "f=%06u ", g_Counter.mCounter0);
    std::vfprintf(g_log, fmt, args);
    std::fputc('\n', g_log);
    va_end(args);
}

bool stickChangedMeaningfully(f32 x, f32 y, s16 angle) {
    const f32 dx = x - g_lastStickX;
    const f32 dy = y - g_lastStickY;
    if ((dx * dx + dy * dy) >= 0.015f * 0.015f) {
        return true;
    }
    const int delta = std::abs(static_cast<int>(angle) - static_cast<int>(g_lastStickAngle));
    return delta >= 0x600;
}

}  // namespace

void tryStartFromEnv() {
    if (g_active || !envEnabled()) {
        return;
    }

    std::error_code ec;
    const auto dir = CachePath / "conavigate";
    std::filesystem::create_directories(dir, ec);
    const auto logFile = dir / "session.log";
    const std::string logString = logFile.string();
    if (logString.size() >= sizeof(g_logPath)) {
        return;
    }

    std::strncpy(g_logPath, logString.c_str(), sizeof(g_logPath) - 1);
    g_logPath[sizeof(g_logPath) - 1] = '\0';

    g_log = std::fopen(g_logPath, "w");
    if (!g_log) {
        g_logPath[0] = '\0';
        return;
    }

    g_active = true;
    g_lastHold = 0;
    g_lastTrig = 0;
    g_lastStickX = 0.0f;
    g_lastStickY = 0.0f;
    g_lastStickAngle = 0;
    g_lastSampleFrame = 0;

#if defined(_WIN32)
    const unsigned long pid = static_cast<unsigned long>(GetCurrentProcessId());
#else
    const unsigned long pid = static_cast<unsigned long>(getpid());
#endif
    std::fprintf(g_log, "=== CoNavigate session start pid=%lu ===\n", pid);
    std::fprintf(g_log, "log=%s\n", g_logPath);
#if TARGET_PC
    std::fprintf(g_log, "f=%06u [cfg] hiddenSkillRework=%d shieldParry=%d manualShield=%d\n",
                 g_Counter.mCounter0,
                 dusk::getSettings().game.hiddenSkillRework.getValue() ? 1 : 0,
                 dusk::getSettings().game.shieldParryCombat.getValue() ? 1 : 0,
                 dusk::getSettings().game.manualShielding.getValue() ? 1 : 0);
#endif
    std::fflush(g_log);
}

void shutdown() {
    if (!g_active) {
        return;
    }

    if (g_log) {
        writeLine("=== CoNavigate session end ===");
        std::fflush(g_log);
        std::fclose(g_log);
        g_log = nullptr;
    }

    g_active = false;
}

bool isActive() {
    return g_active;
}

const char* sessionLogPath() {
    return g_active && g_logPath[0] != '\0' ? g_logPath : nullptr;
}

void onPadFrame() {
    if (!g_active) {
        return;
    }

    const u32 hold = mDoCPd_c::getHold(PAD_1);
    const u32 trig = mDoCPd_c::getTrig(PAD_1);
    const f32 stickX = mDoCPd_c::getStickX(PAD_1);
    const f32 stickY = mDoCPd_c::getStickY(PAD_1);
    const s16 stickAngle = mDoCPd_c::getStickAngle(PAD_1);

    const bool buttonsChanged = hold != g_lastHold || trig != g_lastTrig;
    const bool stickChanged = stickChangedMeaningfully(stickX, stickY, stickAngle);
    if (!buttonsChanged && !stickChanged) {
        return;
    }

    // Avoid flooding when stick rests on a noisy edge.
    if (stickChanged && !buttonsChanged && (g_Counter.mCounter0 - g_lastSampleFrame) < 2) {
        return;
    }

    g_lastHold = hold;
    g_lastTrig = trig;
    g_lastStickX = stickX;
    g_lastStickY = stickY;
    g_lastStickAngle = stickAngle;
    g_lastSampleFrame = g_Counter.mCounter0;

    int linkProc = -1;
    int doStatus = -1;
    int lockOn = 0;
    int albwMeter = -1;
    daAlink_c* link = daAlink_getAlinkActorClass();
    if (link) {
        linkProc = static_cast<int>(link->mProcID);
        lockOn = link->checkAttentionLock() ? 1 : 0;
    }
    doStatus = static_cast<int>(dComIfGp_getDoStatus());
#if TARGET_PC
    albwMeter = dMeter2_getALBWMeterValue();
#endif

    writeLine(
        "[pad] hold=0x%03X trig=0x%03X stick=(%.2f,%.2f) ang=0x%04X proc=%d doSt=%d lock=%d albw=%d",
        hold & 0xFFF, trig & 0xFFF, stickX, stickY, static_cast<unsigned>(stickAngle & 0xFFFF),
        linkProc, doStatus, lockOn, albwMeter);
}

void logEvent(const char* tag, const char* fmt, ...) {
    if (!g_active || !g_log || !tag) {
        return;
    }

    std::fprintf(g_log, "f=%06u [%s] ", g_Counter.mCounter0, tag);
    if (fmt) {
        va_list args;
        va_start(args, fmt);
        std::vfprintf(g_log, fmt, args);
        va_end(args);
    }
    std::fputc('\n', g_log);
    std::fflush(g_log);
}

void logLinkCutSnapshot(::daAlink_c* link, const char* tag, const char* note) {
    if (!g_active || !g_log || !link || !tag) {
        return;
    }

    const int proc = static_cast<int>(link->mProcID);
    const f32 underFrame = link->mUnderFrameCtrl[0].getFrame();
    const int doBtn = link->doButton() ? 1 : 0;
    const int swordBtn = link->swordButton() ? 1 : 0;
    const int lock = link->checkAttentionLock() ? 1 : 0;
    const int inputR = link->checkInputOnR() ? 1 : 0;
    const int cutDir = link->getCutDirection();
#if TARGET_PC
    const int rework = dAlbw_isHiddenSkillReworkEnabled() ? 1 : 0;
    const int ready = dAlbw_peekJumpStrikeChargeReady() ? 1 : 0;
    const int jsUnlock = dComIfGs_isEventBit(dSv_event_flag_c::F_0343) ? 1 : 0;
#else
    const int rework = 0;
    const int ready = 0;
    const int jsUnlock = 0;
#endif
    const int turnMode = link->mProcVar2.field_0x300c;
    const int doSt = static_cast<int>(dComIfGp_getDoStatus());
    const int leftHand = link->mLeftHandIndex;
    const int woodVar = link->mProcVar0.field_0x3008;
    const int moveDir = link->field_0x2f98;
    f32 btkFrame = -1.0f;
    if (link->m_nSwordBtk != NULL) {
        btkFrame = link->m_nSwordBtk->getFrame();
    }

    const bool varR4 = btkFrame >= 14.0f || woodVar >= 14.0f;

    std::fprintf(g_log,
                 "f=%06u [%s] %s proc=%d uf=%.2f do=%d sw=%d lock=%d inR=%d dir=%d rw=%d "
                 "ready=%d js=%d tm=%d doSt=%d lhi=%d pv0=%d btk=%.2f varR4=%d mvDir=%d\n",
                 g_Counter.mCounter0, tag, note ? note : "-", proc, underFrame, doBtn, swordBtn, lock,
                 inputR, cutDir, rework, ready, jsUnlock, turnMode, doSt, leftHand, woodVar, btkFrame,
                 varR4 ? 1 : 0, moveDir);
    std::fflush(g_log);
}

}  // namespace dusk::conavigate

#endif  // DUSK_ENABLE_CONAVIGATE

#include "dusk/hurricane_test.h"

#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"
#include "dusk/audio/DuskDsp.hpp"
#include "dusk/settings.h"
#include "JSystem/JAudio2/JAISe.h"
#include "JSystem/JAudio2/JASTrack.h"
#include "JSystem/JAudio2/JASChannel.h"
#include "JSystem/JAudio2/JASDSPChannel.h"
#include "JSystem/JAudio2/JASOscillator.h"
#include "Z2AudioLib/Z2LinkMgr.h"
#include "Z2AudioLib/Z2SeMgr.h"
#include "Z2AudioLib/Z2SoundObject.h"

#include <algorithm>
#include <array>

namespace dusk {

#if TARGET_PC

static constexpr u32 kHurricanePlaceholderVoiceSe = Z2SE_AL_V_FALL;
static constexpr u32 kZantSpinSe = Z2SE_EN_ZAN_CTL_SPIN_ATK;
static constexpr u32 kGaleTornadoSe = Z2SE_BOOM_TORNADO;

static constexpr f32 kZantPitch = 1.05f;
static constexpr f32 kTornadoVolume = 0.25f;
static constexpr f32 kSpinnerVolume = 0.35f;
static constexpr f32 kSpinnerPitch = 1.0f;

enum HurricaneSeFlags : u8 {
    HURRICANE_SE_ONE_SHOT = 1 << 0,
    HURRICANE_SE_NO_LOOP_PRIMARY = 1 << 1,
    HURRICANE_SE_NO_LOOP_SECONDARY = 1 << 2,
    HURRICANE_SE_NO_LOOP_TERTIARY = 1 << 3,
};

struct HurricaneSePreset {
    const char* name;
    u32 primarySe;
    u32 secondarySe;
    u32 tertiarySe;
    f32 primaryPitch;
    f32 secondaryPitch;
    f32 tertiaryPitch;
    f32 secondaryVolume;
    f32 tertiaryVolume;
    u8 flags;
};

static constexpr u8 kLoopAll = 0;
static constexpr u8 kNativePrimary = HURRICANE_SE_NO_LOOP_PRIMARY;
static constexpr u8 kNativeSecondary = HURRICANE_SE_NO_LOOP_SECONDARY;
static constexpr u8 kNativeTertiary = HURRICANE_SE_NO_LOOP_TERTIARY;

static constexpr HurricaneSePreset s_hurricaneSePresets[] = {
    {"Zant + Tornado + Spinner (tornado 0.85x)", kZantSpinSe, kGaleTornadoSe, Z2SE_AL_SPINNER_RIDE,
     kZantPitch, 0.85f, kSpinnerPitch, kTornadoVolume, kSpinnerVolume, kLoopAll},
    {"Zant + Tornado + Spinner (tornado 1.0x)", kZantSpinSe, kGaleTornadoSe, Z2SE_AL_SPINNER_RIDE,
     kZantPitch, 1.0f, kSpinnerPitch, kTornadoVolume, kSpinnerVolume, kLoopAll},
    {"Zant + Tornado + Spinner (tornado 0.90x)", kZantSpinSe, kGaleTornadoSe, Z2SE_AL_SPINNER_RIDE,
     kZantPitch, 0.9f, kSpinnerPitch, kTornadoVolume, kSpinnerVolume, kLoopAll},
    {"Zant + Tornado + Spinner (tornado 0.92x)", kZantSpinSe, kGaleTornadoSe, Z2SE_AL_SPINNER_RIDE,
     kZantPitch, 0.92f, kSpinnerPitch, kTornadoVolume, kSpinnerVolume, kLoopAll},
    {"Zant + Tornado + Spinner (tornado 0.95x)", kZantSpinSe, kGaleTornadoSe, Z2SE_AL_SPINNER_RIDE,
     kZantPitch, 0.95f, kSpinnerPitch, kTornadoVolume, kSpinnerVolume, kLoopAll},
    {"Zant + Tornado + Spinner (tornado 1.05x)", kZantSpinSe, kGaleTornadoSe, Z2SE_AL_SPINNER_RIDE,
     kZantPitch, 1.05f, kSpinnerPitch, kTornadoVolume, kSpinnerVolume, kLoopAll},
    {"Zant + Tornado + Spinner (tornado 1.10x)", kZantSpinSe, kGaleTornadoSe, Z2SE_AL_SPINNER_RIDE,
     kZantPitch, 1.1f, kSpinnerPitch, kTornadoVolume, kSpinnerVolume, kLoopAll},
    {"Zant + Tornado + Spinner (tornado 1.15x)", kZantSpinSe, kGaleTornadoSe, Z2SE_AL_SPINNER_RIDE,
     kZantPitch, 1.15f, kSpinnerPitch, kTornadoVolume, kSpinnerVolume, kLoopAll},
    {"Zant + Tornado + Spinner (tornado 1.20x)", kZantSpinSe, kGaleTornadoSe, Z2SE_AL_SPINNER_RIDE,
     kZantPitch, 1.2f, kSpinnerPitch, kTornadoVolume, kSpinnerVolume, kLoopAll},
    {"Zant + Tornado + Spinner (tornado 1.25x)", kZantSpinSe, kGaleTornadoSe, Z2SE_AL_SPINNER_RIDE,
     kZantPitch, 1.25f, kSpinnerPitch, kTornadoVolume, kSpinnerVolume, kLoopAll},
    {"Zant + Tornado + Spinner (tornado 1.30x)", kZantSpinSe, kGaleTornadoSe, Z2SE_AL_SPINNER_RIDE,
     kZantPitch, 1.3f, kSpinnerPitch, kTornadoVolume, kSpinnerVolume, kLoopAll},
};

static void pinOscillatorHold(JASChannel* jasCh) {
    for (int i = 0; i < 2; i++) {
        JASOscillator* osc = &jasCh->mOscillators[i];
        if (!osc->isValid()) {
            continue;
        }

        osc->_1C = 2;
        if (osc->_08 <= 0.0f) {
            osc->_08 = 1.0f;
        }
    }
}

static void excludeVoiceWaveArams(Z2CreatureLink* link, u32 seId) {
    if (link == NULL) {
        return;
    }

    Z2SoundHandlePool* handle = link->mSoundObjSimple2.getHandleSoundID(seId);
    if (handle == NULL || !*handle) {
        handle = link->mSoundObjSimple1.getHandleSoundID(seId);
    }
    if (handle == NULL || !*handle) {
        return;
    }

    JAISound* sound = handle->getSound();
    JAISe* se = sound != NULL ? sound->asSe() : NULL;
    if (se == NULL) {
        return;
    }

    JASTrack* track = se->getTrack();
    if (track == NULL) {
        return;
    }

    for (u32 mgrIdx = 0; mgrIdx < track->mChannelMgrCount; mgrIdx++) {
        JASTrack::TChannelMgr* mgr = track->mChannelMgrs[mgrIdx];
        if (mgr == NULL) {
            continue;
        }

        for (int chIdx = 0; chIdx < JASTrack::TChannelMgr::CHANNEL_MAX; chIdx++) {
            JASChannel* jasCh = mgr->mChannels[chIdx];
            if (jasCh == NULL || jasCh->mDspCh == NULL || jasCh->mDspCh->mChannel == NULL) {
                continue;
            }

            if (jasCh->field_0xdc.mChannelType != CHANNEL_WAVE) {
                continue;
            }

            audio::excludeHurricaneSpinWaveAram(jasCh->mDspCh->mChannel->mWaveAramAddress);
        }
    }
}

static void playHurricanePlaceholderVoice(daAlink_c* link) {
    if (link == NULL) {
        return;
    }

    // Voice must not share mSoundObjSimple1 with the spinner level loop.
    link->mZ2Link.mSoundObjSimple2.startSound(kHurricanePlaceholderVoiceSe, 0, link->mVoiceReverbIntensity);
    excludeVoiceWaveArams(&link->mZ2Link, kHurricanePlaceholderVoiceSe);
}

static void forceHurricaneSpinSeWaveLoop(JAISound* sound) {
    if (sound == NULL) {
        return;
    }

    JAISe* se = sound->asSe();
    if (se == NULL) {
        return;
    }

    JASTrack* track = se->getTrack();
    if (track == NULL) {
        return;
    }

    JASChannel* loopChannel = NULL;
    u32 loopSampleCount = 0;
    bool haveLoopCandidate = false;

    for (u32 mgrIdx = 0; mgrIdx < track->mChannelMgrCount; mgrIdx++) {
        JASTrack::TChannelMgr* mgr = track->mChannelMgrs[mgrIdx];
        if (mgr == NULL) {
            continue;
        }

        for (int chIdx = 0; chIdx < JASTrack::TChannelMgr::CHANNEL_MAX; chIdx++) {
            JASChannel* jasCh = mgr->mChannels[chIdx];
            if (jasCh == NULL || jasCh->mDspCh == NULL || jasCh->mDspCh->mChannel == NULL) {
                continue;
            }

            if (jasCh->field_0xdc.mChannelType != CHANNEL_WAVE) {
                continue;
            }

            const JASWaveInfo& wave = jasCh->field_0xdc.mWaveInfo;
            const u32 sampleCount = wave.mSampleCount > 0 ? wave.mSampleCount : jasCh->mDspCh->mChannel->mSampleCount;
            const u32 aramAddress = jasCh->mDspCh->mChannel->mWaveAramAddress;
            if (sampleCount == 0 || aramAddress == 0 || audio::isHurricaneSpinWaveExcluded(aramAddress)) {
                continue;
            }

            if (!haveLoopCandidate || sampleCount < loopSampleCount) {
                loopSampleCount = sampleCount;
                loopChannel = jasCh;
                haveLoopCandidate = true;
            }
        }
    }

    if (loopChannel == NULL) {
        return;
    }

    for (u32 mgrIdx = 0; mgrIdx < track->mChannelMgrCount; mgrIdx++) {
        JASTrack::TChannelMgr* mgr = track->mChannelMgrs[mgrIdx];
        if (mgr == NULL) {
            continue;
        }

        for (int chIdx = 0; chIdx < JASTrack::TChannelMgr::CHANNEL_MAX; chIdx++) {
            JASChannel* jasCh = mgr->mChannels[chIdx];
            if (jasCh == NULL || jasCh == loopChannel || jasCh->mDspCh == NULL ||
                jasCh->mDspCh->mChannel == NULL) {
                continue;
            }

            if (jasCh->field_0xdc.mChannelType != CHANNEL_WAVE) {
                continue;
            }

            audio::excludeHurricaneSpinWaveAram(jasCh->mDspCh->mChannel->mWaveAramAddress);
        }
    }

    pinOscillatorHold(loopChannel);
    loopChannel->setUpdateTimer(0);

    JASDsp::TChannel* dspCh = loopChannel->mDspCh->mChannel;
    if (dspCh->mWaveAramAddress == 0 || loopSampleCount == 0) {
        return;
    }

    audio::registerHurricaneSpinWaveAram(dspCh->mWaveAramAddress);

    const JASWaveInfo& wave = loopChannel->field_0xdc.mWaveInfo;
    dspCh->mLoopFlag = 0xff;
    dspCh->mLoopStartSample = 0;
    dspCh->mEndSample = loopSampleCount;
    dspCh->mpLast = wave.mpLast;
    dspCh->mpPenult = wave.mpPenult;
    dspCh->mIsFinished = false;

    if (dspCh->mSamplesLeft == 0) {
        dspCh->mSamplesLeft = dspCh->mEndSample - dspCh->mLoopStartSample;
        dspCh->mSamplePosition = dspCh->mLoopStartSample;
    }
}

static const HurricaneSePreset& getCurrentHurricaneSePreset() {
    return s_hurricaneSePresets[getHurricaneTestSeIndex()];
}

static void maintainHurricaneSeLayer(daAlink_c* link, Z2SoundObjSimple& soundObj, u32 seId, f32 pitch,
                                     f32 volume, bool forceLoop) {
    if (link == NULL || seId == 0) {
        return;
    }

    Z2SoundHandlePool* handle = soundObj.startLevelSound(seId, 0, link->mVoiceReverbIntensity);
    if (handle == NULL || !*handle) {
        return;
    }

    JAISound* sound = handle->getSound();
    if (forceLoop) {
        forceHurricaneSpinSeWaveLoop(sound);
    }

    sound->getAuxiliary().movePitch(pitch, 0);
    if (volume < 1.0f) {
        sound->getAuxiliary().moveVolume(volume, 0);
    }
}

void initHurricaneSpinSe(daAlink_c* link) {
    if (link == NULL) {
        return;
    }

    const HurricaneSePreset& preset = getCurrentHurricaneSePreset();
    link->field_0x32cc = preset.primarySe;

    if ((preset.flags & HURRICANE_SE_ONE_SHOT) != 0) {
        link->mZ2Link.mSoundObjSimple2.startSound(preset.primarySe, 0, link->mVoiceReverbIntensity);
        playHurricanePlaceholderVoice(link);
        return;
    }

    maintainHurricaneSpinSe(link);
    playHurricanePlaceholderVoice(link);
}

void maintainHurricaneSpinSe(daAlink_c* link) {
    if (link == NULL) {
        return;
    }

    const HurricaneSePreset& preset = getCurrentHurricaneSePreset();
    if ((preset.flags & HURRICANE_SE_ONE_SHOT) != 0) {
        return;
    }

    audio::clearHurricaneSpinWaveRegistration();
    excludeVoiceWaveArams(&link->mZ2Link, kHurricanePlaceholderVoiceSe);

    const bool loopPrimary = (preset.flags & HURRICANE_SE_NO_LOOP_PRIMARY) == 0;
    const bool loopSecondary =
        preset.secondarySe != 0 && (preset.flags & HURRICANE_SE_NO_LOOP_SECONDARY) == 0;
    const bool loopTertiary =
        preset.tertiarySe != 0 && (preset.flags & HURRICANE_SE_NO_LOOP_TERTIARY) == 0;

    maintainHurricaneSeLayer(link, link->mZ2Link.mSoundObjSimple2, preset.primarySe, preset.primaryPitch,
                             1.0f, loopPrimary);
    maintainHurricaneSeLayer(link, link->mZ2Link.mSoundObjSimple2, preset.secondarySe,
                             preset.secondaryPitch, preset.secondaryVolume, loopSecondary);
    maintainHurricaneSeLayer(link, link->mZ2Link.mSoundObjSimple1, preset.tertiarySe, preset.tertiaryPitch,
                             preset.tertiaryVolume, loopTertiary);

    if (loopPrimary || loopSecondary || loopTertiary) {
        audio::setHurricaneSpinSeLoopActive(true);
    }
}

void stopHurricaneSpinSe(daAlink_c* link) {
    audio::setHurricaneSpinSeLoopActive(false);

    if (link == NULL) {
        return;
    }

    const HurricaneSePreset& preset = getCurrentHurricaneSePreset();
    if (preset.primarySe != 0) {
        link->mZ2Link.mSoundObjSimple2.stopSound(preset.primarySe, 0);
    }
    if (preset.secondarySe != 0) {
        link->mZ2Link.mSoundObjSimple2.stopSound(preset.secondarySe, 0);
    }
    if (preset.tertiarySe != 0) {
        link->mZ2Link.mSoundObjSimple1.stopSound(preset.tertiarySe, 0);
    }
}

bool isHurricaneTestEnabled() {
    return getSettings().game.hurricaneTest.getValue();
}

bool isLinkHurricaneProc(const daAlink_c* i_link) {
    return i_link != NULL && i_link->mProcID == daAlink_c::PROC_CUT_GS_HURRICANE &&
           isHurricaneTestEnabled();
}

bool isHurricaneWhirlwindVfx() {
    return getSettings().game.hurricaneTestVfx.getValue() == HurricaneVfxMode::WHIRLWIND;
}

bool isHurricaneBaseVfx() {
    return getSettings().game.hurricaneTestVfx.getValue() == HurricaneVfxMode::BASE;
}

u32 getHurricanePlaceholderVoiceSe() {
    return kHurricanePlaceholderVoiceSe;
}

int getHurricaneTestSeIndex() {
    const int count = static_cast<int>(std::size(s_hurricaneSePresets));
    return std::clamp(getSettings().game.hurricaneTestSe.getValue(), 0, count - 1);
}

int getHurricaneTestSeCount() {
    return static_cast<int>(std::size(s_hurricaneSePresets));
}

const char* getHurricaneTestSeName(int index) {
    if (index < 0 || index >= getHurricaneTestSeCount()) {
        return "?";
    }
    return s_hurricaneSePresets[index].name;
}

#endif

}  // namespace dusk

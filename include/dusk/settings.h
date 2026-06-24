#pragma once

#include <array>

#include "dusk/config_var.hpp"
#include "dusk/ui/controls.hpp"

namespace dusk {

using namespace config;

enum class BloomMode : int {
    Off = 0,
    Classic = 1,
    Dusk = 2,
};

enum class DepthOfFieldMode : int {
    Off = 0,
    Classic = 1,
    Dusk = 2,
};

enum class Resampler : int {
    Bilinear = 0,
    Area = 1,
};

enum class GameLanguage : u8 {
    English = OS_LANGUAGE_ENGLISH,
    German = OS_LANGUAGE_GERMAN,
    French = OS_LANGUAGE_FRENCH,
    Spanish = OS_LANGUAGE_SPANISH,
    Italian = OS_LANGUAGE_ITALIAN,
};

enum class DiscVerificationState : u8 {
    Unknown = 0,
    Success,
    HashMismatch,
};

enum class FrameInterpMode : u8 {
    Off = 0,
    Capped = 1,
    Unlimited = 2,
};

enum class MenuScaling : u8 {
    GameCube = 0,
    Wii = 1,
    Dusklight = 2,
};

enum class MagicArmorMode : u8 {
    NORMAL = 0,
    ON_DAMAGE = 1,
    DOUBLE_DEFENSE = 2,
    INVINCIBLE = 3,
    COSMETIC = 4,
    ALBW = 5,
};

// ALBW parry/bash charge HUD icon style.
enum class ParryIcons : u8 {
    SpurOnly = 0,    // original Epona-spur graphic
    SpurShield = 1,  // spur starburst + shield emblem composited
    ShieldOnly = 2,  // shield emblem only (starburst hidden)
};

// ============================================
// NEW CODE — ALBW Port (Lies of Link HUD mode)
// Lies of Link HUD layout/health style. Off keeps the vanilla corner HUD; the two
// On modes share the LoP relayout and differ only in how life is shown.
// ============================================
enum class LopHudMode : u8 {
    Off = 0,            // vanilla TP corner layout
    VanillaHearts = 1,  // LoP relayout, keep the heart containers
    HealthBar = 2,      // LoP relayout, Lies-of-P health bar instead of hearts
};
// ============================================
// NEW CODE ENDS HERE
// ============================================

// Extra Item Slot + optional D-Pad Quick Swap (Midna left, Z item, field equip/transform).
enum class ExtraItemSlotMode : int {
    Off = 0,
    ExtraOnly = 1,
    ExtraAndQuickSwap = 2,
};

// Off = vanilla reclaim shop; TrueAlbw = full rental catalog; TrueTest = enables TRUETEST new-save
// prompt and per-save world bootstrap (see docs/TrueALBWWorld.md).
enum class TrueAlbwMode : int {
    Off = 0,
    TrueAlbw = 1,
    TrueTest = 2,
};

// Focused Arts playtest cheat: tier 3 without shop; WithDebug adds an in-game HUD overlay.
enum class FocusedArtsCheatMode : int {
    Off = 0,
    On = 1,
    WithDebug = 2,
    OnMaxBank = 3,
    WithDebugMaxBank = 4,
};

namespace config {
template <>
struct ConfigEnumRange<BloomMode> {
    static constexpr auto min = BloomMode::Off;
    static constexpr auto max = BloomMode::Dusk;
};

template <>
struct ConfigEnumRange<DepthOfFieldMode> {
    static constexpr auto min = DepthOfFieldMode::Off;
    static constexpr auto max = DepthOfFieldMode::Dusk;
};

template <>
struct ConfigEnumRange<Resampler> {
    static constexpr auto min = Resampler::Bilinear;
    static constexpr auto max = Resampler::Area;
};

template <>
struct ConfigEnumRange<GameLanguage> {
    static constexpr auto min = GameLanguage::English;
    static constexpr auto max = GameLanguage::Italian;
};

template <>
struct ConfigEnumRange<DiscVerificationState> {
    static constexpr auto min = DiscVerificationState::Unknown;
    static constexpr auto max = DiscVerificationState::HashMismatch;
};

template <>
struct ConfigEnumRange<FrameInterpMode> {
    static constexpr auto min = FrameInterpMode::Off;
    static constexpr auto max = FrameInterpMode::Unlimited;
};

template <>
struct ConfigEnumRange<MenuScaling> {
    static constexpr auto min = MenuScaling::GameCube;
    static constexpr auto max = MenuScaling::Dusklight;
};

template <>
struct ConfigEnumRange<MagicArmorMode> {
    static constexpr auto min = MagicArmorMode::NORMAL;
    static constexpr auto max = MagicArmorMode::ALBW;
};

template <>
struct ConfigEnumRange<ParryIcons> {
    static constexpr auto min = ParryIcons::SpurOnly;
    static constexpr auto max = ParryIcons::ShieldOnly;
};

template <>
struct ConfigEnumRange<LopHudMode> {
    static constexpr auto min = LopHudMode::Off;
    static constexpr auto max = LopHudMode::HealthBar;
};

template <>
struct ConfigEnumRange<ExtraItemSlotMode> {
    static constexpr auto min = ExtraItemSlotMode::Off;
    static constexpr auto max = ExtraItemSlotMode::ExtraAndQuickSwap;
};

template <>
struct ConfigEnumRange<TrueAlbwMode> {
    static constexpr auto min = TrueAlbwMode::Off;
    static constexpr auto max = TrueAlbwMode::TrueTest;
};

template <>
struct ConfigEnumRange<FocusedArtsCheatMode> {
    static constexpr auto min = FocusedArtsCheatMode::Off;
    static constexpr auto max = FocusedArtsCheatMode::WithDebugMaxBank;
};

template <>
struct ConfigValueTraits<ui::ControlLayout> {
    static constexpr bool enabled = true;
};
}  // namespace config

// Persistent user settings

struct UserSettings {
    // Program settings

    struct {
        // Video
        ConfigVar<bool> enableFullscreen;
        ConfigVar<bool> enableVsync;
        ConfigVar<bool> lockAspectRatio;
        ConfigVar<bool> enableFpsOverlay;
        ConfigVar<int> fpsOverlayCorner;
        ConfigVar<int> maxFrameRate;
        ConfigVar<bool> rememberWindowSize;
        ConfigVar<int> lastWindowWidth;
        ConfigVar<int> lastWindowHeight;
    } video;

    struct {
        // Audio
        ConfigVar<int> masterVolume;
        ConfigVar<int> mainMusicVolume;
        ConfigVar<int> subMusicVolume;
        ConfigVar<int> soundEffectsVolume;
        ConfigVar<int> fanfareVolume;
        ConfigVar<bool> enableReverb;
        ConfigVar<bool> enableHrtf;
        ConfigVar<bool> menuSounds;
    } audio;

    // Game settings

    struct {
        ConfigVar<GameLanguage> language;

        // QoL
        ConfigVar<bool> enableQuickTransform;
        // Off | Extra only (Midna + Z item) | Extra + Quick Swap (d-pad equip/transform).
        ConfigVar<ExtraItemSlotMode> extraItemSlotMode;
        ConfigVar<bool> hideTvSettingsScreen;
        ConfigVar<bool> biggerWallets;
        ConfigVar<bool> noReturnRupees;
        ConfigVar<bool> disableRupeeCutscenes;
        ConfigVar<bool> noSwordRecoil;
        // ============================================
        // NEW CODE — ALBW Port (Phase 1 manual shield, Dawnlight 9ff9d35 / kamilink7)
        // ============================================
        ConfigVar<bool> manualShielding;
        // Phase 4: LoP-style parry, bash charges, ALBW rewards/penalties. Off = vanilla guard slip only.
        ConfigVar<bool> shieldParryCombat;
        // Helm punish flow, hidden-skill dispatch, ALBW meter costs, Jump Strike charge gate.
        // Always on (ALBW core); retained for save compat — not shown in settings UI.
        ConfigVar<bool> hiddenSkillRework;
        // Focused Arts charge bank + spend columns (Gameplay → ALBW Unfinished Settings).
        ConfigVar<bool> focusedArtsTest;
        // Playtest cheat: effective shop tier 3; WithDebug shows in-game FA overlay.
        ConfigVar<FocusedArtsCheatMode> focusedArtsCheat;
        ConfigVar<bool> shieldDurability;
        // Halve wallet on death and spawn a Tear-of-Light recovery orb (F_0625 gate unchanged).
        ConfigVar<bool> deathRecoveryOrb;
        // Wolf charge economy, field attack damage/stun, bite healing. Off = vanilla TP wolf.
        ConfigVar<bool> wolfLinkCombat;
        // Direct wallet credit on enemy kill and boss fight victory (additive; vanilla drops unchanged).
        ConfigVar<bool> enemyDeathRupees;
        // Master Quest: Postman heart/stamina upgrades; halved heart container/piece grants.
        ConfigVar<bool> masterQuest;
        // Boss Refinement: any-sword boss damage gates; future Zant/Ganon redesign layers.
        ConfigVar<bool> bossRefinement;
        // Shade's Refuge: Lies-of-P-style Shade Watcher rest/respawn/return system
        // (watcher spawns, death-screen "Last Shade Watcher" option, shop return
        // service). Off = whole system disabled. WIP — default off.
        ConfigVar<bool> shadeRefuge;
        // Parry/bash charge HUD icon style: spur only, spur+shield, or shield only.
        ConfigVar<ParryIcons> parryIconsMode;
        // Boss health bar HUD (name + bar) for main dungeon bosses / Ganondorf duel.
        ConfigVar<bool> bossHealthBars;
        // Lies of Link HUD: relayout vanilla HUD into a Lies-of-P spatial arrangement
        // (top-left life/meter/shield stack, top-right rupees, bottom-left items/spurs).
        // Off = vanilla TP corner layout; VanillaHearts = relayout keeping hearts;
        // HealthBar = relayout with a LoP health bar. See docs/albw-hud-lop-layout-brief.md.
        ConfigVar<LopHudMode> lopHud;
        // Epona dash-spur icons while riding. Does not affect wolf charges, shield HUD, or parry icons.
        ConfigVar<bool> showEponaSpurHud;
        // True ALBW / TRUETEST mode — see docs/TrueALBWWorld.md. Change only before loading a file.
        ConfigVar<TrueAlbwMode> trueAlbwMode;
        // ============================================
        // NEW CODE ENDS HERE
        // ============================================
        ConfigVar<int> damageMultiplier;
        // ============================================
        // NEW CODE — ALBW Port
        // Per-category true max-HP multipliers (1–16×, default 1 = off).
        // Applied once per enemy in fopAc_Execute via dAlbwHP_tryApplyTrueMaxHp().
        // ============================================
        ConfigVar<int> hpMultNormal;
        ConfigVar<int> hpMultMidBoss;
        ConfigVar<int> hpMultBoss;
        ConfigVar<int> hpMultFinalBoss;
        // Global Link attack-power divisor (1–16×, default 1 = off).
        // Applied in d_cc_uty.cpp via dAlbwHP_applyMult().
        ConfigVar<int> linkDamageDecreaseMult;
        // On-screen HP readout while Z-targeting (health / max HP / category mult).
        ConfigVar<bool> showLockonHpDebug;
        // File log of Darknut ALBW bash/guard-break state (Documents/dusklight/albw_darknut_debug.txt).
        ConfigVar<bool> showDarknutBashDebug;
        // Right stick cycles nearby battle enemies instead of manual lock-on camera.
        ConfigVar<bool> stickCycleLockon;
        // ============================================
        // NEW CODE ENDS HERE
        // ============================================
        ConfigVar<bool> noHeartDrops;
        ConfigVar<bool> noAmmoDrops;
        ConfigVar<bool> instantDeath;
        ConfigVar<bool> fastClimbing;
        ConfigVar<bool> noMissClimbing;
        ConfigVar<bool> fastTears;
        ConfigVar<bool> no2ndFishForCat;
        ConfigVar<bool> buttonFishing;
        ConfigVar<bool> instantSaves;
        ConfigVar<bool> instantText;
        ConfigVar<bool> sunsSong;
        ConfigVar<bool> autoSave;
        ConfigVar<bool> enhancedMapMenus;

        // Preferences
        ConfigVar<bool> enableMirrorMode;
        ConfigVar<bool> minimalHUD;
        ConfigVar<float> hudScale;
        ConfigVar<bool> pauseOnFocusLost;
        ConfigVar<bool> enableLinkDollRotation;
        ConfigVar<bool> enableAchievementToasts;
        ConfigVar<bool> enableControllerToasts;
        ConfigVar<bool> enableDiscordPresence;
        ConfigVar<MenuScaling> menuScalingMode;

        // Graphics
        ConfigVar<BloomMode> bloomMode;
        ConfigVar<float> bloomMultiplier;
        ConfigVar<DepthOfFieldMode> depthOfFieldMode;
        ConfigVar<bool> disableWaterRefraction;
        ConfigVar<bool> enableTextureReplacements;
        ConfigVar<FrameInterpMode> enableFrameInterpolation;
        ConfigVar<int> internalResolutionScale;
        ConfigVar<int> shadowResolutionMultiplier;
        ConfigVar<Resampler> resampler;
        ConfigVar<bool> enableMapBackground;
        ConfigVar<bool> disableCutscenePillarboxing;

        // Audio
        ConfigVar<bool> noLowHpSound;
        ConfigVar<bool> midnasLamentNonStop;

        // Input
        ConfigVar<bool> enableGyroAim;
        ConfigVar<bool> enableGyroRollgoal;
        ConfigVar<float> gyroSensitivityX;
        ConfigVar<float> gyroSensitivityY;
        ConfigVar<float> gyroSensitivityRollgoal;
        ConfigVar<float> gyroSmoothing;
        ConfigVar<float> gyroDeadband;
        ConfigVar<bool> gyroInvertPitch;
        ConfigVar<bool> gyroInvertYaw;
        ConfigVar<bool> enableMouseCamera;
        ConfigVar<bool> enableMouseAim;
        ConfigVar<float> mouseAimSensitivity;
        ConfigVar<float> mouseCameraSensitivity;
        ConfigVar<bool> invertMouseY;
        ConfigVar<bool> freeCamera;
        ConfigVar<bool> enableTouchControls;
        ConfigVar<bool> enableMenuPointer;
        ConfigVar<ui::ControlLayout> touchControlsLayout;
        ConfigVar<bool> invertCameraXAxis;
        ConfigVar<bool> invertCameraYAxis;
        ConfigVar<bool> invertFirstPersonXAxis;
        ConfigVar<bool> invertFirstPersonYAxis;
        ConfigVar<bool> invertAirSwimX;
        ConfigVar<bool> invertAirSwimY;
        ConfigVar<float> freeCameraXSensitivity;
        ConfigVar<float> freeCameraYSensitivity;
        ConfigVar<float> touchCameraXSensitivity;
        ConfigVar<float> touchCameraYSensitivity;
        ConfigVar<bool> debugFlyCam;
        ConfigVar<bool> debugFlyCamLockEvents;
        ConfigVar<bool> allowBackgroundInput;
        std::array<ConfigVar<bool>, 4> enableLED;
        ConfigVar<bool> swapDirectSelect;

        // Cheats
        ConfigVar<bool> infiniteHearts;
        ConfigVar<bool> infiniteArrows;
        ConfigVar<bool> infiniteSeeds;
        ConfigVar<bool> infiniteBombs;
        ConfigVar<bool> infiniteOil;
        ConfigVar<bool> infiniteOxygen;
        ConfigVar<bool> infiniteRupees;
        ConfigVar<bool> enableIndefiniteItemDrops;
        ConfigVar<bool> moonJump;
        ConfigVar<bool> superClawshot;
        ConfigVar<bool> alwaysGreatspin;
        ConfigVar<bool> hurricaneTest;
        ConfigVar<bool> enableFastIronBoots;
        ConfigVar<bool> canTransformAnywhere;
        ConfigVar<bool> fastRoll;
        ConfigVar<bool> fastSpinner;
        ConfigVar<MagicArmorMode> armorRupeeDrain;
        ConfigVar<bool> invincibleEnemies;

        // Technical
        ConfigVar<bool> restoreWiiGlitches;

        // Controls
        ConfigVar<bool> enableTurboKeybind;
        ConfigVar<bool> enableResetKeybind;

        // Tools
        ConfigVar<bool> speedrunMode;
        ConfigVar<bool> liveSplitEnabled;
        ConfigVar<bool> showSpeedrunRTATimer;
        ConfigVar<bool> recordingMode;
        ConfigVar<bool> removeQuestMapMarkers;
        ConfigVar<bool> showInputViewer;
        ConfigVar<bool> showInputViewerGyro;
    } game;

    struct {
        ConfigVar<std::string> isoPath;
        ConfigVar<DiscVerificationState> isoVerification;
        ConfigVar<std::string> graphicsBackend;
        ConfigVar<bool> skipPreLaunchUI;
        ConfigVar<bool> showPipelineCompilation;
        ConfigVar<bool> wasPresetChosen;
        ConfigVar<bool> checkForUpdates;
        ConfigVar<int> cardFileType;
        ConfigVar<bool> enableAdvancedSettings;
    } backend;

    // Arrays of size 4 for 4 ports
    struct {
        std::array<ActionBindConfigVar, 4> firstPersonCamera;
        std::array<ActionBindConfigVar, 4> callMidna;
        std::array<ActionBindConfigVar, 4> openMapScreen;
        std::array<ActionBindConfigVar, 4> toggleMinimap;
        std::array<ActionBindConfigVar, 4> openDusklightMenu;
        std::array<ActionBindConfigVar, 4> turboSpeedButton;
        std::array<ActionBindConfigVar, 4> cycleSword;
        std::array<ActionBindConfigVar, 4> cycleShield;
        std::array<ActionBindConfigVar, 4> quickTransform;
        std::array<ActionBindConfigVar, 4> openItemWheel;
    } actionBindings;
};

UserSettings& getSettings();

void registerSettings();

// Transient settings

struct CollisionViewSettings {
    bool enableTerrainView;
    bool enableWireframe;
    bool enableAtView;
    bool enableTgView;
    bool enableCoView;
    float terrainViewOpacity;
    float colliderViewOpacity;
    float drawRange;
};

struct TransientSettings {
    CollisionViewSettings collisionView;
    bool skipFrameRateLimit;
    bool moveLinkActive;
    bool stateShareLoadActive;
};

TransientSettings& getTransientSettings();

}  // namespace dusk

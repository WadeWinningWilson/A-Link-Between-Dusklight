#include "fmt/format.h"
#include "imgui.h"
#include "aurora/gfx.h"

#include "ImGuiConfig.hpp"
#include "dusk/hotkeys.h"
#include "dusk/settings.h"
#include "ImGuiConsole.hpp"
#include "ImGuiMenuTools.hpp"

#include "ImGuiEngine.hpp"
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_horse.h"
#include "d/d_albw_hp_mult.h"
#include "d/d_attention.h"
#include "d/d_focused_arts.h"
#include "d/d_com_inf_game.h"
#include "d/d_meter2_info.h"
#include "f_op/f_op_actor.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_name.h"
#include "dusk/data.hpp"
#include "dusk/dusk.h"
#include "dusk/main.h"
#include "m_Do/m_Do_main.h"

#include <aurora/lib/internal.hpp>
#include <SDL3/SDL_misc.h>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace aurora::gx {
extern bool enableLodBias;
}

namespace dusk {
    ImGuiMenuTools::ImGuiMenuTools() {}

    void ImGuiMenuTools::draw() {
        if (ImGui::BeginMenu("Tools")) {
            if (!dusk::IsGameLaunched) {
                ImGui::BeginDisabled();
            }

            ImGui::BeginDisabled(getSettings().game.speedrunMode);

            ImGui::MenuItem("Save Editor", hotkeys::SHOW_SAVE_EDITOR, &m_showSaveEditor);
            ImGui::MenuItem("State Share", hotkeys::SHOW_STATE_SHARE, &m_showStateShare);

            ImGui::EndDisabled();

            if (!dusk::IsGameLaunched) {
                ImGui::EndDisabled();
            }

#if DUSK_CAN_OPEN_DATA_FOLDER
            ImGui::Separator();
            if (ImGui::MenuItem("Open Data Folder")) {
                data::open_data_path();
            }
#endif

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Debug")) {
            ImGui::BeginDisabled(getSettings().game.speedrunMode);

            bool developmentMode = mDoMain::developmentMode == 1;
            if (ImGui::Checkbox("Development Mode", &developmentMode)) {
                mDoMain::developmentMode = developmentMode ? 1 : -1;
            }

            ImGui::Separator();

            auto& collisionView = getTransientSettings().collisionView;
            if (ImGui::BeginMenu("Graphics Settings")) {
                bool disableWaterRefraction = getSettings().game.disableWaterRefraction;
                if (ImGui::Checkbox("Disable Water Refraction", &disableWaterRefraction)) {
                    getSettings().game.disableWaterRefraction.setValue(disableWaterRefraction);
                    config::Save();
                }
                ImGui::Checkbox("Enable LOD Bias", &aurora::gx::enableLodBias);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Collision View")) {
                ImGui::Checkbox("Enable Terrain view", &collisionView.enableTerrainView);
                ImGui::Checkbox("Enable wireframe view", &collisionView.enableWireframe);
                ImGui::SliderFloat("Opacity##terrain", &collisionView.terrainViewOpacity, 0.0f, 100.0f);
                ImGui::SliderFloat("Draw Range", &collisionView.drawRange, 0.0f, 1000.0f);
                ImGui::Separator();
                ImGui::Checkbox("Enable Attack Collider view", &collisionView.enableAtView);
                ImGui::Checkbox("Enable Target Collider view", &collisionView.enableTgView);
                ImGui::Checkbox("Enable Push Collider view", &collisionView.enableCoView);
                ImGui::SliderFloat("Opacity##colliders", &collisionView.colliderViewOpacity, 0.0f, 100.0f);
                ImGui::EndMenu();
            }

            if (!dusk::IsGameLaunched) {
                ImGui::BeginDisabled();
            }

            ImGui::MenuItem("Process Management", hotkeys::SHOW_PROCESS_MANAGEMENT, &m_showProcessManagement);
            ImGui::MenuItem("Debug Overlay", hotkeys::SHOW_DEBUG_OVERLAY, &m_showDebugOverlay);
            ImGui::MenuItem("Heap Viewer", hotkeys::SHOW_HEAP_VIEWER, &m_showHeapOverlay);
            ImGui::MenuItem("Player Info", hotkeys::SHOW_PLAYER_INFO, &m_showPlayerInfo);
            ImGui::MenuItem("Debug Camera", hotkeys::SHOW_DEBUG_CAMERA, &m_showCameraOverlay);
            ImGui::MenuItem("Audio Debug", hotkeys::SHOW_AUDIO_DEBUG, &m_showAudioDebug);
            ImGui::MenuItem("Bloom", nullptr, &m_showBloomWindow);
            ImGui::MenuItem("Stub Log", nullptr, &m_showStubLog);
            ImGui::MenuItem("Actor Spawner", nullptr, &m_showActorSpawner);

            if (!dusk::IsGameLaunched) {
                ImGui::EndDisabled();
            }

            ImGui::MenuItem("OSReport Force", nullptr, &OSReportReallyForceEnable);

            ImGui::EndDisabled();

            ImGui::EndMenu();
        }
    }

    void ImGuiMenuTools::ShowDebugOverlay() {
        if (!getSettings().backend.enableAdvancedSettings ||
            !ImGuiConsole::CheckMenuViewToggle(ImGuiKey_F3, m_showDebugOverlay))
        {
            return;
        }

        ImGui::PushFont(ImGuiEngine::fontMono);

        ImGuiIO& io = ImGui::GetIO();
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav;
        if (m_debugOverlayCorner != -1) {
            SetOverlayWindowLocation(m_debugOverlayCorner);
            windowFlags |= ImGuiWindowFlags_NoMove;
        }

        ImGui::SetNextWindowBgAlpha(0.65f);
        if (ImGui::Begin("Debug Overlay", nullptr, windowFlags)) {
            ImGuiStringViewText(fmt::format(FMT_STRING("FPS: {:.2f}\n"), io.Framerate));
            if (frameUsagePct > 0.f) {
                ImGuiStringViewText(fmt::format(FMT_STRING("Frame usage: {:.1f}%\n"), frameUsagePct));
            }

            ImGui::Separator();

            ImGuiStringViewText(fmt::format(FMT_STRING("Backend: {}\n"), backend_name(aurora_get_backend())));

            ImGui::Separator();

            const auto& stats = lastFrameAuroraStats;

            ImGuiStringViewText(
                fmt::format(FMT_STRING("Queued pipelines:  {}\n"), stats.queuedPipelines));
            ImGuiStringViewText(
                fmt::format(FMT_STRING("Done pipelines:    {}\n"), stats.createdPipelines));
            ImGuiStringViewText(
                fmt::format(FMT_STRING("Draw call count:   {}\n"), stats.drawCallCount));
            ImGuiStringViewText(fmt::format(FMT_STRING("Merged draw calls: {}\n"),
                stats.mergedDrawCallCount));
            ImGuiStringViewText(fmt::format(FMT_STRING("Vertex size:       {}\n"),
                BytesToString(stats.lastVertSize)));
            ImGuiStringViewText(fmt::format(FMT_STRING("Uniform size:      {}\n"),
                BytesToString(stats.lastUniformSize)));
            ImGuiStringViewText(fmt::format(FMT_STRING("Index size:        {}\n"),
                BytesToString(stats.lastIndexSize)));
            ImGuiStringViewText(fmt::format(FMT_STRING("Storage size:      {}\n"),
                BytesToString(stats.lastStorageSize)));
            ImGuiStringViewText(fmt::format(FMT_STRING("Tex upload size:   {}\n"),
                BytesToString(stats.lastTextureUploadSize)));
            ImGuiStringViewText(fmt::format(
                FMT_STRING("Total:             {}\n"),
                BytesToString(stats.lastVertSize + stats.lastUniformSize +
                    stats.lastIndexSize + stats.lastStorageSize +
                    stats.lastTextureUploadSize)));

            // TODO: persist to config
            ShowCornerContextMenu(m_debugOverlayCorner, m_cameraOverlayCorner);
        }
        ImGui::End();

        ImGui::PopFont();
    }

    void ImGuiMenuTools::ShowPlayerInfo() {
        if (!getSettings().backend.enableAdvancedSettings ||
            !ImGuiConsole::CheckMenuViewToggle(ImGuiKey_F5, m_showPlayerInfo))
        {
            return;
        }

        ImGui::PushFont(ImGuiEngine::fontMono);

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav;
        if (m_playerInfoOverlayCorner != -1) {
            SetOverlayWindowLocation(m_playerInfoOverlayCorner);
            windowFlags |= ImGuiWindowFlags_NoMove;
        }

        ImGui::SetNextWindowBgAlpha(0.65f);

        if (ImGui::Begin("Player Info", nullptr, windowFlags)) {
            daAlink_c* player = (daAlink_c*)dComIfGp_getPlayer(0);
            daHorse_c* horse = dComIfGp_getHorseActor();

            double speedXzy = 0.0;
            if (player != nullptr) {
                speedXzy = sqrtf(player->speed.x * player->speed.x
                    + player->speed.z * player->speed.z
                    + player->speed.y * player->speed.y);
            }

            ImGui::Text("Global");
            ImGuiStringViewText(
                player != nullptr
                ? fmt::format("Stage: {}\n", dComIfGp_getStartStageName()) 
                : "Stage: ?\n"
            );

            ImGuiStringViewText(
                player != nullptr
                ? fmt::format("Layer: {0}\n", dComIfG_play_c::getLayerNo(0))
                : "Layer: ?\n"
            );

            ImGui::Separator();
            ImGui::Text("Link");
            ImGuiStringViewText(
                player != nullptr
                ? fmt::format("Position: {: .4f}, {: .4f}, {: .4f}\n", player->current.pos.x, player->current.pos.y, player->current.pos.z)
                : "Position: ?, ?, ?\n"
            );

            ImGuiStringViewText(
                player != nullptr
                ? fmt::format("Velocity (XYZ): {: .4f}, {: .4f}, {: .4f}\n", player->speed.x, player->speed.y, player->speed.z)
                : "Velocity (XYZ): ?, ?, ?\n"
            );

            ImGuiStringViewText(
                player != nullptr
                ? fmt::format("Speed (SpeedF): {: .4f}\n", player->speedF)
                : "Speed (SpeedF): ?\n"
            );

            ImGuiStringViewText(
                player != nullptr
                ? fmt::format("Speed (3D): {: .4f}\n", speedXzy)
                : "Speed (3D): ?\n"
            );

            ImGuiStringViewText(
                 player != nullptr
                 ? fmt::format("Angle: {0}\n", player->shape_angle.y)
                 : "Angle: ?\n"
            );

            ImGuiStringViewText(
                player != nullptr
                ? fmt::format("Room: {0}\n", fopAcM_GetRoomNo(player))
                : "Room: ?\n"
            );

            ImGuiStringViewText(
                player != nullptr
                ? fmt::format("Entry: {0}\n", dComIfGp_getStartStagePoint())
                : "Entry: ?\n"
            );

            ImGui::Separator();
            ImGui::Text("Epona");
            ImGuiStringViewText(
                horse != nullptr
                ? fmt::format("Position: {: .4f}, {: .4f}, {: .4f}\n", horse->current.pos.x, horse->current.pos.y, horse->current.pos.z)
                : "Position: ?, ?, ?\n"
            );

            ImGuiStringViewText(
                 horse != nullptr
                 ? fmt::format("Velocity (XYZ): {: .4f}, {: .4f}, {: .4f}\n", horse->speed.x, horse->speed.y, horse->speed.z)
                 : "Velocity (XYZ): ?, ?, ?\n"
            );

            ImGuiStringViewText(
                horse != nullptr
                ? fmt::format("Speed (SpeedF): {: .4f}\n", horse->speedF)
                : "Speed (SpeedF): ?\n"
            );

            ImGuiStringViewText(
                horse != nullptr
                ? fmt::format("Angle: {0}\n", horse->shape_angle.y)
                : "Angle: ?\n"
            );

            ImGuiStringViewText(
                horse != nullptr
                ? fmt::format("Room: {0}\n", fopAcM_GetRoomNo(horse))
                : "Room: ?\n"
            );

            ImGuiStringViewText(
                player != nullptr
                ? fmt::format("Saved Stage: {}\n", dComIfGs_getHorseRestartStageName())
                : "Saved Stage: ?\n"
            );

            ImGuiStringViewText(
                player != nullptr
                ? fmt::format("Saved Room: {0}\n", dComIfGs_getHorseRestartRoomNo())
                : "Saved Room: ?\n"
            );

            ShowCornerContextMenu(m_playerInfoOverlayCorner, m_debugOverlayCorner);
        }

        ImGui::End();
        ImGui::PopFont();
    }

    namespace {
    const char* lockonHpCategoryName(dAlbwHP_Category cat) {
        switch (cat) {
        case dAlbwHP_MID_BOSS:
            return "Mid-Boss";
        case dAlbwHP_BOSS:
            return "Boss";
        case dAlbwHP_FINAL:
            return "Final Boss";
        default:
            return "Common";
        }
    }

    const char* lockonDarknutPhaseName(dAlbwHP_DarknutPhase phase) {
        switch (phase) {
        case dAlbwHP_Darknut_ARMORED:
            return "Armored";
        case dAlbwHP_Darknut_TRANSITION:
            return "Transition";
        case dAlbwHP_Darknut_UNARMORED:
            return "Unarmored";
        default:
            return "Unknown";
        }
    }

    bool isLiveLockonActor(fopAc_ac_c* actor) {
        if (actor == NULL || !fopAcM_IsActor(actor)) {
            return false;
        }

        const fpc_ProcID procId = fpcM_GetID(actor);
        return procId != fpcM_ERROR_PROCESS_ID_e && fpcM_IsExecuting(procId);
    }

    bool isPlaySceneActive() {
        return fpcM_SearchByName(fpcNm_PLAY_SCENE_e) != NULL;
    }

    int countBattleLockonTargets(dAttention_c* attention) {
        int battleCount = 0;
        const int lockonCount = attention->GetLockonCount();
        for (int i = 0; i < lockonCount; i++) {
            dAttList_c* entry = attention->GetLockonList(i);
            if (entry == NULL) {
                continue;
            }

            fopAc_ac_c* actor = entry->getActor();
            if (isLiveLockonActor(actor) && fopAcM_GetGroup(actor) == fopAc_ENEMY_e &&
                (actor->attention_info.flags & fopAc_AttnFlag_BATTLE_e) != 0)
            {
                battleCount++;
            }
        }
        return battleCount;
    }
    }  // namespace

    void ImGuiMenuTools::ShowLockonHpOverlay() {
        if (!getSettings().game.showLockonHpDebug.getValue() || !dusk::IsGameLaunched ||
            !isPlaySceneActive())
        {
            return;
        }

        dAttention_c* attention = dComIfGp_getAttention();
        if (attention == NULL || !attention->LockonTruth()) {
            return;
        }

        fopAc_ac_c* target = attention->LockonTarget(0);
        if (!isLiveLockonActor(target)) {
            return;
        }

        ImGui::PushFont(ImGuiEngine::fontMono);

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav;
        if (m_lockonHpOverlayCorner != -1) {
            SetOverlayWindowLocation(m_lockonHpOverlayCorner);
            windowFlags |= ImGuiWindowFlags_NoMove;
        }

        ImGui::SetNextWindowBgAlpha(0.65f);
        if (ImGui::Begin("Lock-on HP", nullptr, windowFlags)) {
            const char* name = fopAcM_getProcNameString(target);
            if (name == NULL) {
                name = "?";
            }

            const s16 profName = fopAcM_GetName(target);
            const dAlbwHP_Category category = dAlbwHP_getCategory(profName);
            const int trueHpMult = dAlbwHP_getTrueHpMult(profName);
            const dAlbwHP_LockonDisplay hpDisplay = dAlbwHP_getLockonDisplayHp(target);
            const int battleTargets = countBattleLockonTargets(attention);

            ImGui::Text("Lock-on Target");
            ImGui::Separator();
            ImGuiStringViewText(fmt::format("Name: {}\n", name));

            if (hpDisplay.darknutPhase != dAlbwHP_Darknut_NONE) {
                ImGuiStringViewText(fmt::format("Phase: {}\n",
                                                lockonDarknutPhaseName(hpDisplay.darknutPhase)));

                if (hpDisplay.darknutPhase == dAlbwHP_Darknut_ARMORED) {
                    ImGuiStringViewText(fmt::format("Armor: {} / {} remaining\n",
                                                    hpDisplay.current, hpDisplay.max));
                } else if (hpDisplay.darknutPhase == dAlbwHP_Darknut_TRANSITION) {
                    ImGuiStringViewText("Armor: complete\n");
                } else if (hpDisplay.darknutPhase == dAlbwHP_Darknut_UNARMORED) {
                    ImGuiStringViewText(fmt::format("Internal: {} / {}\n", hpDisplay.current,
                                                    hpDisplay.max));
                    ImGuiStringViewText("(internal damage meter)\n");
                }

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.65f, 0.65f, 1.0f));
                ImGuiStringViewText(fmt::format("HP (actor): {} / {}\n", hpDisplay.actorHealth,
                                                hpDisplay.actorHealthMax));
                ImGui::PopStyleColor();
            } else {
                ImGuiStringViewText(
                    fmt::format("HP: {} / {}\n", hpDisplay.current, hpDisplay.max));
                if (hpDisplay.customMeter) {
                    ImGuiStringViewText("(internal damage meter)\n");
                }
            }
            ImGuiStringViewText(
                fmt::format("Category: {}\n", lockonHpCategoryName(category)));
            ImGuiStringViewText(fmt::format("True HP mult: {}x\n", trueHpMult));
            ImGuiStringViewText(
                fmt::format("Battle targets nearby: {}\n", battleTargets));

            ShowCornerContextMenu(m_lockonHpOverlayCorner, m_playerInfoOverlayCorner);
        }
        ImGui::End();
        ImGui::PopFont();
    }

    void ImGuiMenuTools::ShowFocusedArtsDebugOverlay() {
        if (!dFocusedArts_isDebugOverlayEnabled() || !dusk::IsGameLaunched ||
            !isPlaySceneActive())
        {
            return;
        }

        ImGui::PushFont(ImGuiEngine::fontMono);

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav;
        SetOverlayWindowLocation(1);
        windowFlags |= ImGuiWindowFlags_NoMove;

        ImGui::SetNextWindowBgAlpha(0.65f);
        if (ImGui::Begin("Focused Arts", nullptr, windowFlags)) {
            const int maxBank = dFocusedArts_getMaxBank();
            const int bank = dFocusedArts_getBankCount();
            const int fill = dFocusedArts_getFillNumerator();
            const int fillDen = dFocusedArts_getFillDenominator();
            const int spendCol = dFocusedArts_getSpendColumn();

            ImGui::Text("Focused Arts (test)");
            ImGui::Separator();
            ImGuiStringViewText(fmt::format("Tier: {} (purchased {})\n",
                                            dFocusedArts_getEffectiveTier(),
                                            dFocusedArts_getPurchasedTier()));
            ImGuiStringViewText(fmt::format("Bank: {} / {}\n", bank, maxBank));
            ImGuiStringViewText(fmt::format("Fill: {} / {}\n", fill, fillDen));
            ImGuiStringViewText(fmt::format("Spend ready: {}\n",
                                            dFocusedArts_isSpendReady() ? "yes" : "no"));
            ImGuiStringViewText(fmt::format("In spend seq: {}\n",
                                            dFocusedArts_isInSpendSequence() ? "yes" : "no"));
            if (spendCol > 0) {
                ImGuiStringViewText(fmt::format("Last spend column: T{}\n", spendCol));
            }
            ImGuiStringViewText(fmt::format("Charged dmg tier: T{}\n",
                                            dFocusedArts_getChargedDamageTier()));
            ImGui::Separator();
            ImGuiStringViewText(
                fmt::format("HS attack values ({}, FA formula):\n",
                            dFocusedArts_getHiddenSkillAttackDebugSwordLabel()));
            ImGuiStringViewText("  [x]=save bit  V=vanilla  now=live  T1/2/3=charged tiers\n");
            for (int i = 0; i < dFocusedArts_getHiddenSkillAttackDebugLineCount(); ++i) {
                char line[96];
                dFocusedArts_formatHiddenSkillAttackDebugLine(i, line, sizeof(line));
                if (line[0] != '\0') {
                    ImGuiStringViewText(fmt::format("  {}\n", line));
                }
            }
            ImGuiStringViewText(fmt::format("ALBW: {} / {} locked={} suppress={}\n",
                                            dMeter2_getALBWMeterValue(), dMeter2_getALBWMaxValue(),
                                            dMeter2_isALBWLocked() ? "yes" : "no",
                                            dFocusedArts_shouldSuppressAlbwMeterDrain() ? "yes"
                                                                                        : "no"));
            ImGui::Separator();
            ImGuiStringViewText("Recent FA events:\n");
            for (int i = 0; i < 4; i++) {
                const char* line = dFocusedArts_getRecentEventLine(i);
                if (line[0] != '\0') {
                    ImGuiStringViewText(fmt::format("  {}\n", line));
                }
            }
            ImGui::Separator();
            ImGuiStringViewText(
                "ALBW on HS while banking (not spending):\n"
                "  1/2 base (5450): EB, MD, JS charge, Helm, GS\n"
                "  1/6 base (1817): Back Slice (+ spin charge)\n"
                "Spend sequence: ALBW suppressed ~5s after each spend charge.\n");
            if (dFocusedArts_hasSpecialFinisherAvailable()) {
                ImGuiStringViewText("Finisher: available on T1 spend\n");
            }
            if (maxBank <= 0) {
                ImGuiStringViewText(
                    "Enable Focused Arts Cheat or buy tiers (shop API).\n");
            }
            if (ImGui::Button("Reset FA meter")) {
                dFocusedArts_resetRuntimeState();
            }
        }
        ImGui::End();
        ImGui::PopFont();
    }
}

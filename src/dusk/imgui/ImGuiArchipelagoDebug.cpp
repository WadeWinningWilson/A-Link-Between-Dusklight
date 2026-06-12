#include <dusk/imgui/ImGuiArchipelagoDebug.h>

#include "imgui.h"

#include "Archipelago.h"
#include "aurora/lib/window.hpp"
#include "dusk/file_select.hpp"
#include "dusk/archipelago/archipelago_context.hpp"

namespace dusk {

constexpr std::array<SDL_DialogFileFilter, 2> kFileFilters{{
    {"Archipelago Configuration File", "yaml"},
    {"All Files", "*"},
}};

void FileDialogCallback(void*, const char* path, const char* error) {
    if (path == nullptr || error != nullptr) {
        return;
    }

    archi::ArchipelagoContext::SetAPConfigYamlPath(path);
}

void OpenApFilePicker() noexcept {
    ShowFileSelect(&FileDialogCallback, nullptr, aurora::window::get_sdl_window(),
        kFileFilters.data(), kFileFilters.size(), nullptr, false);
}

ImGuiArchipelagoDebug::ImGuiArchipelagoDebug() {

}

void ImGuiArchipelagoDebug::drawMenuItem() {
    if (ImGui::BeginMenu("Randomizer")) {
        ImGui::Checkbox("Archipelago Window", &m_drawWindow);
        ImGui::EndMenu();
    }
}

void ImGuiArchipelagoDebug::drawWindow() {
    if (!m_drawWindow)
        return;

    if (!m_hasInitValues) {
        m_hasInitValues = true;
        SAFE_STRCPY(m_serverIpInputBuffer, archi::ArchipelagoContext::GetServerIp().c_str());
        SAFE_STRCPY(m_serverPassInputBuffer, archi::ArchipelagoContext::GetPassword().c_str());
        SAFE_STRCPY(m_slotNameInputBuffer, archi::ArchipelagoContext::GetSlotName().c_str());
    }

    ImGui::Begin("Archipelago Debug", &m_drawWindow);

    if (ImGui::InputText("Server IP", m_serverIpInputBuffer, sizeof(m_serverIpInputBuffer))) {
        archi::ArchipelagoContext::SetServerIp(m_serverIpInputBuffer);
    }

    ImGuiInputTextFlags flags = m_showPassword ?  ImGuiInputTextFlags_None : ImGuiInputTextFlags_Password;
    if (ImGui::InputText("Server Pass", m_serverPassInputBuffer, sizeof(m_serverPassInputBuffer), flags)) {
        archi::ArchipelagoContext::SetPassword(m_serverPassInputBuffer);
    }
    ImGui::Checkbox("Show Password", &m_showPassword);

    if (ImGui::InputText("Slot Name", m_slotNameInputBuffer, sizeof(m_slotNameInputBuffer))) {
        archi::ArchipelagoContext::SetSlotName(m_slotNameInputBuffer);
    }

    if (ImGui::Button("Set Archipelago Config Path")) {
        OpenApFilePicker();
    }

    if (ImGui::Button("Test Config Convert")) {
        randomizer::seedgen::config::Config config;
        archi::ArchipelagoContext::GenerateConfigFromAP(config);
    }

    if (archi::ArchipelagoContext::IsConnected()) {
        if (ImGui::Button("Disconnect")) {
            archi::ArchipelagoContext::DisconnectFromServer();
        }
    }else {
        if (ImGui::Button("Connect")) {
            archi::ArchipelagoContext::ConnectToServer();
        }
    }

    ImGui::End();
}
}
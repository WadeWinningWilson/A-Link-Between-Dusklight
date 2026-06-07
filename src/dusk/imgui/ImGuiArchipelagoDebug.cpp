#include <dusk/imgui/ImGuiArchipelagoDebug.h>

#include "imgui.h"

#include "Archipelago.h"

namespace dusk {
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

    ImGui::Begin("Archipelago Debug");

    ImGui::End();
}
}
#pragma once

namespace dusk {
class ImGuiArchipelagoDebug {
private:
    bool m_drawWindow = false;
    bool m_showPassword = false;
    bool m_hasInitValues = false;

    char m_serverIpInputBuffer[0x40] = {};
    char m_serverPassInputBuffer[0x40] = {};
    char m_slotNameInputBuffer[0x40] = {};
public:
    ImGuiArchipelagoDebug();

    void drawMenuItem();
    void drawWindow();
};
}
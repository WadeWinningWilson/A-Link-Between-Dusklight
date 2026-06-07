#pragma once

namespace dusk {
class ImGuiArchipelagoDebug {
private:
    bool m_drawWindow = false;
public:
    ImGuiArchipelagoDebug();

    void drawMenuItem();
    void drawWindow();
};
}
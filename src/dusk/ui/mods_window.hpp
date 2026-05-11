#pragma once

#include "window.hpp"

#include <vector>

namespace dusk::ui {

class ModsWindow : public Window {
public:
    ModsWindow();
    void update() override;

private:
    struct ModSnapshot {
        bool active;
        bool load_failed;
    };
    std::vector<ModSnapshot> mSnapshot;
    int mActiveModIndex = 0;
};

}  // namespace dusk::ui

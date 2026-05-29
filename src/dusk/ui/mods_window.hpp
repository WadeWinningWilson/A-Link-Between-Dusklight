#pragma once

#include "window.hpp"

#include <vector>

#include "dusk/mod_loader.hpp"

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
    ModIndex mActiveModIndex = 0;
};

}  // namespace dusk::ui

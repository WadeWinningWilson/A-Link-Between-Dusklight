#include "dusk/action_bindings.h"

#include "aurora/lib/input.hpp"
#include "dusk/dpad_quick_swap.h"
#include "dusk/settings.h"
#include "dusk/ui/ui.hpp"

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_scancode.h>
#include <dolphin/pad.h>

namespace dusk {

static std::array<std::array<ActionBindPressData, static_cast<int>(ActionBinds::COUNT)>, PAD_CHANMAX> actionPressData{};

struct VirtualActionBindData {
    bool pressed = false;
    bool available = false;
};

static std::array<std::array<VirtualActionBindData, static_cast<int>(ActionBinds::COUNT)>, PAD_CHANMAX> virtualActionData{};

ActionBindsMap& getActionBinds() {
    static ActionBindsMap actionBinds = {
        {ActionBinds::FIRST_PERSON_CAMERA, {&getSettings().actionBindings.firstPersonCamera, "First Person Camera"}},
        {ActionBinds::CALL_MIDNA,          {&getSettings().actionBindings.callMidna,         "Call Midna"}},
        {ActionBinds::OPEN_MAP_SCREEN,     {&getSettings().actionBindings.openMapScreen,     "Open Map Screen"}},
        {ActionBinds::TOGGLE_MINIMAP,      {&getSettings().actionBindings.toggleMinimap,     "Toggle Minimap"}},
        {ActionBinds::OPEN_DUSKLIGHT_MENU, {&getSettings().actionBindings.openDusklightMenu, "Open Dusklight Menu"}},
        {ActionBinds::TURBO_SPEED_BUTTON,  {&getSettings().actionBindings.turboSpeedButton,  "Turbo Speed Button"}},
        {ActionBinds::CYCLE_SWORD,         {&getSettings().actionBindings.cycleSword,        "Cycle Sword"}},
        {ActionBinds::CYCLE_SHIELD,        {&getSettings().actionBindings.cycleShield,       "Cycle Shield"}},
        {ActionBinds::QUICK_TRANSFORM,     {&getSettings().actionBindings.quickTransform,    "Quick Transform"}},
    };
    return actionBinds;
}

bool isActionBound(ActionBinds action, u32 port) {
    auto& actionBinds = getActionBinds();
    // Check to make sure action is properly bound
    if (!actionBinds.contains(action)) {
        return false;
    }

    if (port < PAD_CHANMAX && virtualActionData[port][static_cast<int>(action)].available) {
        return true;
    }

    return getActionBindButton(action, port) != PAD_NATIVE_BUTTON_INVALID;
}

void updateActionBindings() {
    for (u32 port = 0; port < PAD_CHANMAX; ++port) {
        // Move the current press to the previous frame
        for (auto& pressData : actionPressData[port]) {
            pressData.pressedPrevFrame = pressData.pressedCurFrame;
            pressData.pressedCurFrame = false;
        }

        // Update current frame with whether action button is pressed
        for (auto& [action, boundAction] : getActionBinds()) {
            // If the action isn't bound, or if documents are visible and the action isn't
            // opening the dusklight menu, don't update. Otherwise, we may accidentally
            // perform actions while the dusklight menu is open.
            const int button = boundAction.configVars->at(port);
            const bool virtualAvailable = virtualActionData[port][static_cast<int>(action)].available;
            if ((button == PAD_NATIVE_BUTTON_INVALID && !virtualAvailable) ||
                (ui::any_document_visible() && action != ActionBinds::OPEN_DUSKLIGHT_MENU)) {
                continue;
            }

            if (button != PAD_NATIVE_BUTTON_INVALID) {
                // If keyboard is active for this port
                u32 count = 0;
                if (PADGetKeyButtonBindings(port, &count) != nullptr) {
                    int numKeys = 0;
                    const bool* kbState = SDL_GetKeyboardState(&numKeys);
                    if (kbState[button]) {
                        actionPressData[port][static_cast<int>(action)].pressedCurFrame = true;
                    }
                } else {
                    // If controller is active
                    auto controller = aurora::input::get_controller_for_player(port);
                    if (controller) {
                        if (SDL_GetGamepadButton(controller->m_controller, static_cast<SDL_GamepadButton>(button))) {
                            actionPressData[port][static_cast<int>(action)].pressedCurFrame = true;
                        }
                    }
                }
            }
        }

        for (auto& [action, _] : getActionBinds()) {
            const auto& virtualAction = virtualActionData[port][static_cast<int>(action)];
            if (virtualAction.available && virtualAction.pressed && !ui::any_document_visible()) {
                actionPressData[port][static_cast<int>(action)].pressedCurFrame = true;
            }
        }
    }
}

void setVirtualActionBind(ActionBinds action, u32 port, bool pressed, bool available) {
    if (port >= PAD_CHANMAX) {
        return;
    }
    virtualActionData[port][static_cast<int>(action)] = {
        .pressed = pressed,
        .available = available,
    };
}

void clearVirtualActionBind(ActionBinds action, u32 port) {
    if (port >= PAD_CHANMAX) {
        return;
    }
    virtualActionData[port][static_cast<int>(action)] = {};
}

void clearAllVirtualActionBinds() {
    virtualActionData = {};
}

bool getActionBindTrig(ActionBinds action, u32 port) {
    return actionPressData[port][static_cast<int>(action)].pressedCurFrame &&
          !actionPressData[port][static_cast<int>(action)].pressedPrevFrame;
}

bool getActionBindHold(ActionBinds action, u32 port) {
    return actionPressData[port][static_cast<int>(action)].pressedCurFrame &&
           actionPressData[port][static_cast<int>(action)].pressedPrevFrame;
}

bool getActionBindHoldAnyPort(ActionBinds action) {
    for (u32 port = 0; port < PAD_CHANMAX; ++port) {
        if (getActionBindHold(action, port)) {
            return true;
        }
    }
    return false;
}

int getActionBindButton(ActionBinds action, u32 port) {
    return (*getActionBinds()[action].configVars)[port];
}

#if TARGET_PC
static bool bindUsesPadButton(ActionBinds action, u32 port, PADButton padButton) {
    if (!isActionBound(action, port)) {
        return false;
    }

    const int bindButton = getActionBindButton(action, port);
    if (bindButton == PAD_NATIVE_BUTTON_INVALID) {
        return false;
    }

    switch (padButton) {
    case PAD_BUTTON_UP:
        if (bindButton == SDL_GAMEPAD_BUTTON_DPAD_UP) {
            return true;
        }
        break;
    case PAD_BUTTON_DOWN:
        if (bindButton == SDL_GAMEPAD_BUTTON_DPAD_DOWN) {
            return true;
        }
        break;
    case PAD_BUTTON_LEFT:
        if (bindButton == SDL_GAMEPAD_BUTTON_DPAD_LEFT) {
            return true;
        }
        break;
    case PAD_BUTTON_RIGHT:
        if (bindButton == SDL_GAMEPAD_BUTTON_DPAD_RIGHT) {
            return true;
        }
        break;
    default:
        break;
    }

    u32 count = 0;
    const PADKeyButtonBinding* bindings = PADGetKeyButtonBindings(port, &count);
    if (bindings == nullptr) {
        return false;
    }

    for (u32 i = 0; i < count; ++i) {
        if (bindings[i].padButton == padButton && bindings[i].scancode == bindButton) {
            return true;
        }
    }

    return false;
}

bool isExtraItemSlotEnabled() {
    return getSettings().game.extraItemSlotMode.getValue() != ExtraItemSlotMode::Off;
}

bool isDpadQuickSwapEnabled() {
    return getSettings().game.extraItemSlotMode.getValue() == ExtraItemSlotMode::ExtraAndQuickSwap;
}

bool callMidnaReservesDpadLeft(u32 port) {
    if (!isExtraItemSlotEnabled() || port != 0) {
        return false;
    }

    if (isActionBound(ActionBinds::CALL_MIDNA, port)) {
        return getActionBindButton(ActionBinds::CALL_MIDNA, port) == SDL_GAMEPAD_BUTTON_DPAD_LEFT ||
               bindUsesPadButton(ActionBinds::CALL_MIDNA, port, PAD_BUTTON_LEFT);
    }

    // Built-in PC default when Call Midna is not customized in config.
    return true;
}

bool dpadUpReservedForQuickSwap(u32 port) {
    return isDpadQuickSwapEnabled() && bindUsesPadButton(ActionBinds::CYCLE_SWORD, port, PAD_BUTTON_UP);
}

bool dpadDownReservedForQuickSwap(u32 port) {
    return isDpadQuickSwapEnabled() &&
           bindUsesPadButton(ActionBinds::QUICK_TRANSFORM, port, PAD_BUTTON_DOWN);
}

bool dpadRightReservedForQuickSwap(u32 port) {
    return isDpadQuickSwapEnabled() &&
           bindUsesPadButton(ActionBinds::CYCLE_SHIELD, port, PAD_BUTTON_RIGHT);
}
#endif

}

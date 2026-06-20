#pragma once

#include <unordered_map>

#include "dusk/config_var.hpp"

namespace dusk {

enum class ActionBinds {
    FIRST_PERSON_CAMERA,
    CALL_MIDNA,
    OPEN_MAP_SCREEN,
    TOGGLE_MINIMAP,
    OPEN_DUSKLIGHT_MENU,
    TURBO_SPEED_BUTTON,
    CYCLE_SWORD,
    CYCLE_SHIELD,
    QUICK_TRANSFORM,
    COUNT,
};

struct ActionBindData {
    std::array<config::ActionBindConfigVar, 4>* configVars{};
    std::string actionName{};
};

struct ActionBindPressData {
    bool pressedCurFrame{false};
    bool pressedPrevFrame{false};
};

using ActionBindsMap = std::unordered_map<ActionBinds, ActionBindData>;

ActionBindsMap& getActionBinds();

bool isActionBound(ActionBinds action, u32 port);

void updateActionBindings();

void setVirtualActionBind(ActionBinds action, u32 port, bool pressed, bool available = true);

void clearVirtualActionBind(ActionBinds action, u32 port);

void clearAllVirtualActionBinds();

bool getActionBindTrig(ActionBinds action, u32 port);

bool getActionBindHold(ActionBinds action, u32 port);

bool getActionBindHoldAnyPort(ActionBinds action);

int getActionBindButton(ActionBinds action, u32 port);

#if TARGET_PC
/** True when Extra Item Slot is enabled (Midna on d-pad, Z free for an item). */
bool isExtraItemSlotEnabled();

/** True when D-Pad Quick Swap mode is active. */
bool isDpadQuickSwapEnabled();

/** True when Call Midna uses left d-pad (default or custom bind). Map open must yield. */
bool callMidnaReservesDpadLeft(u32 port = 0);

bool dpadUpReservedForQuickSwap(u32 port = 0);
bool dpadDownReservedForQuickSwap(u32 port = 0);
bool dpadRightReservedForQuickSwap(u32 port = 0);
#endif

}

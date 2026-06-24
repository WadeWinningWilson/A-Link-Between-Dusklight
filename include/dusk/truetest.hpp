#pragma once

#include "types.h"

namespace dusk::truetest {

// Rental shop unlock: global True ALBW / TRUETEST modes, or a TRUETEST-stamped save.
bool isTrueAlbwShopEnabled();

// Per-save TRUETEST stamp (world bootstrap applies only when true).
bool isTrueTestSave();

// Field play is active (lock global mode changes mid-save).
bool isFieldGameplayActive();

// Global True ALBW mode may be edited (file select, title, prelaunch — not field play).
bool canChangeGlobalTrueAlbwMode();

// Rental Postman, death item recovery, and warp choice: vanilla F_0625 or TRUETEST stamp.
bool isAlbwPostmanUnlocked();

// File select: stamp save and clear bootstrap bit after name + horse name.
void applyNewSavePreset(bool i_trueTestSelected);

// First field entry after load: run world bootstrap once per TRUETEST save.
void onStageLoad();

// Minimum Faron layer index for TRUETEST (post–Forest Temple); used by layer hook.
int getFaronLayerFloor();

// All mirror shards (0–3) and fused shadows (crystal 0–2); opens the Hyrule Castle barrier.
bool isGanonBarrierCleared();

}  // namespace dusk::truetest

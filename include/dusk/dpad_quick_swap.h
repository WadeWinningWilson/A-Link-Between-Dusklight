#pragma once

#include "dolphin/types.h"

namespace dusk {

#if TARGET_PC
void applyDpadQuickSwapPresetBinds();

bool canUseDpadQuickSwap(u32 port = 0);

void cycleNextSword();
void cycleNextShield();
#endif

}

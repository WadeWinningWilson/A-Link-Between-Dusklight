#pragma once

#include <cstdint>

namespace dusk {

void hookInstallByAddr(void* fn_addr, void* tramp_fn, void** orig_store);

void hookRegisterPre (void* fn_addr, void* mod, int32_t (*fn)(void* args));
void hookRegisterPost(void* fn_addr, void* mod, const char* mod_name, void (*fn)(void* args, void* retval));
bool hookSetReplace  (void* fn_addr, void* mod, const char* mod_name, void (*fn)(void* args, void* retval));

bool hookDispatchPre (void* fn_addr, void* args, void* retval);
void hookDispatchPost(void* fn_addr, void* args, void* retval);

void hookClearMod(void* mod);

} // namespace dusk

#include "dusk/gx_helper.h"

GXTexObjRAII::~GXTexObjRAII() { GXDestroyTexObj(this); }
void GXTexObjRAII::reset() { GXDestroyTexObj(this); }

GXScopedDebugGroup::GXScopedDebugGroup(const char* text) {
#if DUSK_GFX_DEBUG_GROUPS
    GXPushDebugGroup(text);
#else
    (void)text;
#endif
}
GXScopedDebugGroup::~GXScopedDebugGroup() {
#if DUSK_GFX_DEBUG_GROUPS
    GXPopDebugGroup();
#endif
}

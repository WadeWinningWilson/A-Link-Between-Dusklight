#include "dusk/gx_helper.h"

GXTexObjRAII::~GXTexObjRAII() { GXDestroyTexObj(this); }
void GXTexObjRAII::reset() { GXDestroyTexObj(this); }

GXScopedDebugGroup::GXScopedDebugGroup(const char* text) {
    GXPushDebugGroup(text);
}
GXScopedDebugGroup::~GXScopedDebugGroup() {
    GXPopDebugGroup();
}

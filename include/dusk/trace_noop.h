#pragma once

// No-op placeholders for optional dev trace macros (CONAV_LOG / CONAV_CUT_SNAPSHOT).
#define CONAV_LOG(tag, ...) ((void)0)
#define CONAV_CUT_SNAPSHOT(link, tag, note) ((void)0)

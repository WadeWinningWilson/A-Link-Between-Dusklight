#ifndef DUSK_CONAVIGATE_H
#define DUSK_CONAVIGATE_H

#include <cstdarg>

class daAlink_c;

namespace dusk::conavigate {

#if DUSK_ENABLE_CONAVIGATE
void tryStartFromEnv();
void shutdown();
bool isActive();
const char* sessionLogPath();

// Sample pad 0 when buttons/stick change (called from mDoCPd_c::read).
void onPadFrame();

// Combat / proc events (tag groups lines in the log).
void logEvent(const char* tag, const char* fmt, ...);

// Full Link cut-state snapshot (Jump Strike / charge debug).
void logLinkCutSnapshot(::daAlink_c* link, const char* tag, const char* note);
#else
inline void tryStartFromEnv() {}
inline void shutdown() {}
inline bool isActive() { return false; }
inline const char* sessionLogPath() { return nullptr; }
inline void onPadFrame() {}
inline void logEvent(const char*, const char*, ...) {}
inline void logLinkCutSnapshot(::daAlink_c*, const char*, const char*) {}
#endif

}  // namespace dusk::conavigate

#if DUSK_ENABLE_CONAVIGATE
#define CONAV_LOG(tag, ...)                                                                        \
    do {                                                                                           \
        if (dusk::conavigate::isActive()) {                                                        \
            dusk::conavigate::logEvent(tag, __VA_ARGS__);                                          \
        }                                                                                          \
    } while (0)
#define CONAV_CUT_SNAPSHOT(link, tag, note)                                                        \
    do {                                                                                           \
        if (dusk::conavigate::isActive()) {                                                        \
            dusk::conavigate::logLinkCutSnapshot(link, tag, note);                                   \
        }                                                                                          \
    } while (0)
#else
#define CONAV_LOG(tag, ...) ((void)0)
#define CONAV_CUT_SNAPSHOT(link, tag, note) ((void)0)
#endif

#endif  // DUSK_CONAVIGATE_H

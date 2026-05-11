#pragma once
#include <cstring>
#include <memory>
#include <type_traits>
#include "dusk/mod_api.h"

namespace dusk {

inline DuskModAPI* g_api = nullptr;
inline void init(DuskModAPI* api) { g_api = api; }

template <class T>
T arg(void* args_raw, int n) noexcept {
    void** a = static_cast<void**>(args_raw);
    return *static_cast<std::add_pointer_t<std::remove_reference_t<T>>>(a[n]);
}

template <class T>
std::remove_reference_t<T>& argRef(void* args_raw, int n) noexcept {
    void** a = static_cast<void**>(args_raw);
    return *static_cast<std::add_pointer_t<std::remove_reference_t<T>>>(a[n]);
}

template <class F>
void* mfpAddr(F fn) noexcept {
    void* p = nullptr;
    static_assert(sizeof(fn) >= sizeof(void*), "unexpected MFP size");
    std::memcpy(&p, &fn, sizeof(void*));
    return p;
}

template <auto MFP, class R, class Self, class Orig, class... A>
struct HookEntryBase {
    static inline Orig g_orig = nullptr;

    static R trampoline(Self self, A... args) {
        void* ptrs[] = {static_cast<void*>(std::addressof(self)), static_cast<void*>(std::addressof(args))...};
        if constexpr (std::is_void_v<R>) {
            const bool cancel = g_api->hook_dispatch_pre(mfpAddr(MFP), static_cast<void*>(ptrs), nullptr);
            if (!cancel) g_orig(self, args...);
            g_api->hook_dispatch_post(mfpAddr(MFP), static_cast<void*>(ptrs), nullptr);
        } else {
            R result{};
            const bool cancel = g_api->hook_dispatch_pre(mfpAddr(MFP), static_cast<void*>(ptrs), static_cast<void*>(std::addressof(result)));
            if (!cancel) result = g_orig(self, args...);
            g_api->hook_dispatch_post(mfpAddr(MFP), static_cast<void*>(ptrs), static_cast<void*>(std::addressof(result)));
            return result;
        }
    }
};

template <auto FP, class R, class Orig, class... A>
struct HookEntryFreeBase {
    static inline Orig g_orig = nullptr;

    static R trampoline(A... args) {
        if constexpr (sizeof...(A) == 0) {
            if constexpr (std::is_void_v<R>) {
                const bool cancel = g_api->hook_dispatch_pre(mfpAddr(FP), nullptr, nullptr);
                if (!cancel) g_orig(args...);
                g_api->hook_dispatch_post(mfpAddr(FP), nullptr, nullptr);
            } else {
                R result{};
                const bool cancel = g_api->hook_dispatch_pre(mfpAddr(FP), nullptr, static_cast<void*>(std::addressof(result)));
                if (!cancel) result = g_orig(args...);
                g_api->hook_dispatch_post(mfpAddr(FP), nullptr, static_cast<void*>(std::addressof(result)));
                return result;
            }
        } else {
            void* ptrs[] = {static_cast<void*>(std::addressof(args))...};
            if constexpr (std::is_void_v<R>) {
                const bool cancel = g_api->hook_dispatch_pre(mfpAddr(FP), static_cast<void*>(ptrs), nullptr);
                if (!cancel) g_orig(args...);
                g_api->hook_dispatch_post(mfpAddr(FP), static_cast<void*>(ptrs), nullptr);
            } else {
                R result{};
                const bool cancel = g_api->hook_dispatch_pre(mfpAddr(FP), static_cast<void*>(ptrs), static_cast<void*>(std::addressof(result)));
                if (!cancel) result = g_orig(args...);
                g_api->hook_dispatch_post(mfpAddr(FP), static_cast<void*>(ptrs), static_cast<void*>(std::addressof(result)));
                return result;
            }
        }
    }
};

template <auto MFP>
struct HookEntry;

template <class C, class R, class... A, R (C::*MFP)(A...)>
struct HookEntry<MFP> : HookEntryBase<MFP, R, C*, R(*)(C*, A...), A...> {};

template <class C, class R, class... A, R (C::*MFP)(A...) const>
struct HookEntry<MFP> : HookEntryBase<MFP, R, const C*, R(*)(const C*, A...), A...> {};

template <class R, class... A, R (*FP)(A...)>
struct HookEntry<FP> : HookEntryFreeBase<FP, R, R(*)(A...), A...> {};

template <auto MFP>
void hookAddPre(int32_t (*fn)(void* args)) {
    using E = HookEntry<MFP>;
    g_api->hook_install(mfpAddr(MFP), reinterpret_cast<void*>(E::trampoline),
                        reinterpret_cast<void**>(&E::g_orig));
    g_api->hook_pre(mfpAddr(MFP), fn);
}

template <auto MFP>
void hookAddPost(void (*fn)(void* args, void* retval)) {
    using E = HookEntry<MFP>;
    g_api->hook_install(mfpAddr(MFP), reinterpret_cast<void*>(E::trampoline),
                        reinterpret_cast<void**>(&E::g_orig));
    g_api->hook_post(mfpAddr(MFP), fn);
}

template <auto MFP>
void hookSetReplace(void (*fn)(void* args, void* retval)) {
    using E = HookEntry<MFP>;
    g_api->hook_install(mfpAddr(MFP), reinterpret_cast<void*>(E::trampoline),
                        reinterpret_cast<void**>(&E::g_orig));
    g_api->hook_replace(mfpAddr(MFP), fn);
}

}  // namespace dusk

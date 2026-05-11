// Tests every feature of the Dusk mod API. Results shown in the mod tab.

#include "d/actor/d_a_alink.h"
#include "dusk/hook.hpp"
#include "dusk/mod_api.h"
#include "m_Do/m_Do_controller_pad.h"

#include <cstdio>
#include <cstring>

static int g_ticks = 0;
static bool g_pre_fired = false;
static bool g_post_fired = false;
static bool g_replace_fired = false;
static bool g_arg_write_ok = false;
static int g_pre_cancel_count = 0;
static int g_post_count = 0;
static bool g_resource_ok = false;
static char g_resource_text[256] = {};
static char g_mod_dir_snippet[64] = {};

static DuskElemHandle g_el_pre_badge = nullptr;
static DuskElemHandle g_el_post_badge = nullptr;
static DuskElemHandle g_el_replace_badge = nullptr;
static DuskElemHandle g_el_argwrite_badge = nullptr;
static DuskElemHandle g_el_cancel_count = nullptr;
static DuskElemHandle g_el_post_count = nullptr;
static DuskElemHandle g_el_link_angle = nullptr;

// Pre-hook on posMove. Hold L to test argRef write and cancellation.
static int32_t on_posMove_pre(void* args) {
    g_pre_fired = true;
    if (mDoCPd_c::getHoldL(PAD_1)) {
        dusk::argRef<daAlink_c*>(args, 0)->shape_angle.y = 0;
        g_arg_write_ok = true;
        ++g_pre_cancel_count;
        return 1; // cancel
    }
    return 0;
}

// Post-hook on posMove. Fires even when the pre-hook cancelled.
static void on_posMove_post(void* args, void* retval) {
    g_post_fired = true;
    ++g_post_count;
    (void)args;
    (void)retval;
}

// Replace-hook on execute. Calls through to the original so gameplay is unaffected.
using ExecuteEntry = dusk::HookEntry<&daAlink_c::execute>;
static void on_execute_replace(void* args, void* retval) {
    g_replace_fired = true;
    int result = ExecuteEntry::g_orig(dusk::arg<daAlink_c*>(args, 0));
    if (retval) {
        *static_cast<int*>(retval) = result;
    }
}

static void on_reset(void*) {
    g_pre_fired = false;
    g_post_fired = false;
    g_replace_fired = false;
    g_arg_write_ok = false;
    g_pre_cancel_count = 0;
    g_post_count = 0;
}

static void BuildPanel(DuskPanelHandle panel, void*) {
    DuskModAPI* api = dusk::g_api;

    api->panel_add_section(panel, "Hooks");
    g_el_pre_badge = api->panel_add_badge_row(panel, "pre-hook fired (posMove)", g_pre_fired);
    g_el_post_badge = api->panel_add_badge_row(panel, "post-hook fired (posMove)", g_post_fired);
    g_el_replace_badge =
        api->panel_add_badge_row(panel, "replace-hook fired (execute)", g_replace_fired);
    g_el_argwrite_badge =
        api->panel_add_badge_row(panel, "argRef write + pre cancel (hold L)", g_arg_write_ok);

    char countBuf[64];
    snprintf(countBuf, sizeof(countBuf), "pre cancels: %d", g_pre_cancel_count);
    g_el_cancel_count = api->panel_add_dyn_text(panel, countBuf);
    snprintf(countBuf, sizeof(countBuf), "post calls: %d", g_post_count);
    g_el_post_count = api->panel_add_dyn_text(panel, countBuf);

    api->panel_add_section(panel, "Resources");
    api->panel_add_badge_row(panel, "load_resource (hello.txt)", g_resource_ok);
    if (g_resource_text[0] != '\0') {
        api->panel_add_dyn_text(panel, g_resource_text);
    }

    api->panel_add_section(panel, "API Fields");
    api->panel_add_badge_row(panel, "mod_dir non-empty", g_mod_dir_snippet[0] != '\0');
    api->panel_add_dyn_text(panel, g_mod_dir_snippet);

    api->panel_add_section(panel, "Actions");
    api->panel_add_button(panel, "Reset Results", on_reset, nullptr);

    g_el_link_angle = api->panel_add_dyn_text(panel, "");
}

static void UpdatePanel(void*) {
    DuskModAPI* api = dusk::g_api;

    api->elem_set_badge(g_el_pre_badge, g_pre_fired);
    api->elem_set_badge(g_el_post_badge, g_post_fired);
    api->elem_set_badge(g_el_replace_badge, g_replace_fired);
    api->elem_set_badge(g_el_argwrite_badge, g_arg_write_ok);

    char buf[64];
    snprintf(buf, sizeof(buf), "pre cancels: %d", g_pre_cancel_count);
    api->elem_set_text(g_el_cancel_count, buf);

    snprintf(buf, sizeof(buf), "post calls: %d", g_post_count);
    api->elem_set_text(g_el_post_count, buf);

    daAlink_c* link = daAlink_getAlinkActorClass();
    snprintf(buf, sizeof(buf), "Link y angle: %d", link ? (int)link->shape_angle.y : 0);
    api->elem_set_text(g_el_link_angle, buf);
}

extern "C" {

void mod_init(DuskModAPI* api) {
    dusk::init(api);

    api->log_info("mod_test initializing");
    api->log_warn("log_warn smoke test");
    api->log_error("log_error smoke test");

    snprintf(g_mod_dir_snippet, sizeof(g_mod_dir_snippet), "%.60s", api->mod_dir);

    size_t size = 0;
    void* data = api->load_resource("hello.txt", &size);
    if (data) {
        size_t copy = size < sizeof(g_resource_text) - 1 ? size : sizeof(g_resource_text) - 1;
        memcpy(g_resource_text, data, copy);
        g_resource_text[copy] = '\0';
        while (copy > 0 && g_resource_text[copy - 1] == '\n') {
            g_resource_text[--copy] = '\0';
        }
        api->free_resource(data);
        g_resource_ok = true;
        api->log_info("load_resource OK: \"%s\"", g_resource_text);
    } else {
        api->log_error("load_resource FAILED for hello.txt");
    }

    void* missing = api->load_resource("does_not_exist.bin", nullptr);
    if (!missing) {
        api->log_info("load_resource missing-file: correctly returned nullptr");
    } else {
        api->log_error("load_resource missing-file: unexpectedly returned data");
        api->free_resource(missing);
    }

    dusk::hookAddPre<&daAlink_c::posMove>(on_posMove_pre);
    dusk::hookAddPost<&daAlink_c::posMove>(on_posMove_post);
    dusk::hookSetReplace<&daAlink_c::execute>(on_execute_replace);

    api->register_tab_content(BuildPanel, nullptr);
    api->register_tab_update(UpdatePanel, nullptr);

    api->log_info("mod_test ready");
}

void mod_tick(DuskModAPI* api) {
    ++g_ticks;
    (void)api;
}

void mod_cleanup(DuskModAPI* api) {
    api->log_info("mod_test unloaded after %d ticks", g_ticks);
    g_el_pre_badge = g_el_post_badge = g_el_replace_badge = nullptr;
    g_el_argwrite_badge = g_el_cancel_count = g_el_post_count = nullptr;
    g_el_link_angle = nullptr;
}
}

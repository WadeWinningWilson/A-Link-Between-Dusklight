#include "commands.hpp"

#include "JSystem/JUtility/JUTGamePad.h"
#include "SSystem/SComponent/c_sxyz.h"
#include "SSystem/SComponent/c_xyz.h"
#include "c/c_damagereaction.h"
#include "d/actor/d_a_alink.h"
#include "d/d_com_inf_game.h"
#include "d/d_kankyo.h"
#include "d/d_stage.h"
#include "dusk/game_clock.h"
#include "f_op/f_op_actor_mng.h"
#include "f_pc/f_pc_layer.h"
#include "f_pc/f_pc_layer_iter.h"
#include "f_pc/f_pc_manager.h"
#include "f_pc/f_pc_node.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "fmt/format.h"

namespace dusk {
namespace {

static constexpr int kMaxHistory = 64;

static std::vector<std::string> SplitArgs(std::string_view input) {
    std::vector<std::string> args;
    std::string cur;
    for (char c : input) {
        if (c == ' ' || c == '\t') {
            if (!cur.empty()) {
                args.push_back(std::move(cur));
                cur.clear();
            }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) {
        args.push_back(std::move(cur));
    }
    return args;
}

static std::optional<long> ParseLong(const std::string& s) {
    if (s.empty()) {
        return std::nullopt;
    }
    char* end = nullptr;
    const long v = std::strtol(s.c_str(), &end, 0);
    return (end != s.c_str() && *end == '\0') ? std::optional<long>{v} : std::nullopt;
}

static std::optional<float> ParseFloat(const std::string& s) {
    if (s.empty()) {
        return std::nullopt;
    }
    char* end = nullptr;
    const float v = std::strtof(s.c_str(), &end);
    return (end != s.c_str() && *end == '\0') ? std::optional<float>{v} : std::nullopt;
}

static const char* ActorShortName(s16 profname) {
    const char* n = dStage_getName(profname, -1);
    return n ? n : "?";
}

// Resolves @found / @link / @<id> to a process pointer, outputting an error on failure
static base_process_class* ParseProcArg(
    const std::string& s, unsigned int foundProcId, const CommandOutput& output) {
    if (s.empty() || s[0] != '@') {
        output("Error: proc reference must start with @");
        return nullptr;
    }
    const std::string inner = s.substr(1);
    unsigned int id;
    if (inner == "found") {
        if (foundProcId == 0) {
            output("Error: @found is not set");
            return nullptr;
        }
        id = foundProcId;
    } else if (inner == "link") {
        auto* player = dComIfGp_getPlayer(0);
        if (player == nullptr) {
            output("Error: player not available");
            return nullptr;
        }
        id = (unsigned int)fpcM_GetID(player);
    } else {
        const auto v = ParseLong(inner);
        if (!v) {
            output("Error: invalid proc ID");
            return nullptr;
        }
        id = (unsigned int)*v;
    }
    auto* proc = fpcM_SearchByID(id);
    if (proc == nullptr) {
        output(fmt::format(FMT_STRING("Error: proc {} not found"), id));
        return nullptr;
    }
    return proc;
}

// Like ParseProcArg but ensures the proc is an actor
static fopAc_ac_c* ParseActorArg(
    const std::string& s, unsigned int foundProcId, const CommandOutput& output) {
    auto* proc = ParseProcArg(s, foundProcId, output);
    if (proc == nullptr) {
        return nullptr;
    }
    if (!fopAcM_IsActor(proc)) {
        output("Error: proc is not an actor");
        return nullptr;
    }
    return static_cast<fopAc_ac_c*>(proc);
}

static std::optional<s16> ParseActorId(const std::string& s) {
    if (const auto v = ParseLong(s)) {
        return (s16)*v;
    }
    const auto* entry = dStage_searchNameCI(s.c_str());
    return entry ? std::optional<s16>{entry->procname} : std::nullopt;
}

static std::optional<cXyz> ParseXYZ(const std::vector<std::string>& args, size_t i) {
    const auto x = ParseFloat(args[i]), y = ParseFloat(args[i + 1]), z = ParseFloat(args[i + 2]);
    return (x && y && z) ? std::optional<cXyz>{cXyz(*x, *y, *z)} : std::nullopt;
}

static bool TryAngle(
    s16& out, const std::vector<std::string>& args, size_t i, const CommandOutput& output) {
    if (args.size() <= i) {
        return true;
    }
    const auto a = ParseLong(args[i]);
    if (!a) {
        output("Error: invalid angle");
        return false;
    }
    out = (s16)*a;
    return true;
}

static std::string actorLine(const base_process_class* proc) {
    const auto* ac = static_cast<const fopAc_ac_c*>(proc);
    return fmt::format(FMT_STRING("procId={} 0x{:04X} ({}) @ ({:.2f}, {:.2f}, {:.2f}) room={}"),
        (unsigned int)proc->id, (unsigned int)(u16)proc->profname, ActorShortName(proc->profname),
        ac->current.pos.x, ac->current.pos.y, ac->current.pos.z, (int)ac->current.roomNo);
}

static void recurseLayer(void* p, int (*callback)(void*, void*), void* ctx) {
    auto* proc = static_cast<base_process_class*>(p);
    if (fpcBs_Is_JustOfType(g_fpcNd_type, proc->subtype)) {
        fpcLyIt_OnlyHere(&static_cast<process_node_class*>(p)->layer, callback, ctx);
    }
}

struct ListContext {
    s16 targetId;
    std::vector<std::string>* output;
};
struct FindContext {
    s16 targetId;
    std::vector<base_process_class*> matches;
};

static int ListActorCallback(void* p, void* ctx) {
    auto* proc = static_cast<base_process_class*>(p);
    auto* context = static_cast<ListContext*>(ctx);
    if (fopAcM_IsActor(proc) && (context->targetId < 0 || proc->profname == context->targetId)) {
        context->output->push_back("  " + actorLine(proc));
    }
    recurseLayer(p, ListActorCallback, ctx);
    return 1;
}

static int FindActorCallback(void* p, void* ctx) {
    auto* proc = static_cast<base_process_class*>(p);
    auto* context = static_cast<FindContext*>(ctx);
    if (fopAcM_IsActor(proc) && proc->profname == context->targetId) {
        context->matches.push_back(proc);
    }
    recurseLayer(p, FindActorCallback, ctx);
    return 1;
}

}  // namespace

void runCommand(std::string_view cmdLine, CommandState& state, const CommandOutput& output) {
    output(fmt::format(FMT_STRING("> {}"), cmdLine));

    if (!cmdLine.empty()) {
        if (state.history.empty() || state.history.back() != cmdLine) {
            state.history.push_back(std::string(cmdLine));
            if ((int)state.history.size() > kMaxHistory) {
                state.history.erase(state.history.begin());
            }
        }
    }

    auto args = SplitArgs(cmdLine);
    if (args.empty()) {
        return;
    }
    const auto& cmd = args[0];

    auto requirePlayer = [&]() -> daAlink_c* {
        auto* p = (daAlink_c*)dComIfGp_getPlayer(0);
        if (p == nullptr) {
            output("Error: player not available");
        }
        return p;
    };

    if (cmd == "tp") {
        auto* player = requirePlayer();
        if (player == nullptr) {
            return;
        }

        if (args.size() >= 2 && args[1].starts_with('@')) {
            auto* ac = ParseActorArg(args[1], state.foundProcId, output);
            if (ac == nullptr) {
                return;
            }

            if (args.size() >= 3 && args[2].starts_with('@')) {
                auto* destAc = ParseActorArg(args[2], state.foundProcId, output);
                if (destAc == nullptr) {
                    return;
                }
                const cXyz destPos = destAc->current.pos;
                ac->current.pos = destPos;
                output(fmt::format(FMT_STRING("Moved actor {} to ({:.2f}, {:.2f}, {:.2f})"), ac->id,
                    destPos.x, destPos.y, destPos.z));
                return;
            }

            if (args.size() >= 5) {
                const auto pos = ParseXYZ(args, 2);
                if (!pos) {
                    output("Error: invalid coordinates");
                    return;
                }
                ac->current.pos = *pos;
                if (!TryAngle(ac->shape_angle.y, args, 5, output)) {
                    return;
                }
                output(fmt::format(FMT_STRING("Moved actor {} to ({:.2f}, {:.2f}, {:.2f})"), ac->id,
                    pos->x, pos->y, pos->z));
                return;
            }

            player->current.pos = ac->current.pos;
            output(fmt::format(FMT_STRING("Teleported to actor {} ({:.2f}, {:.2f}, {:.2f})"),
                ac->id, ac->current.pos.x, ac->current.pos.y, ac->current.pos.z));
            return;
        }

        if (args.size() < 4) {
            output("Usage: tp <x> <y> <z> [angle]  |  tp @<procId> [<x> <y> <z> [angle] | @link]");
            return;
        }
        const auto pos = ParseXYZ(args, 1);
        if (!pos) {
            output("Error: invalid coordinates");
            return;
        }
        player->current.pos = *pos;
        if (!TryAngle(player->shape_angle.y, args, 4, output)) {
            return;
        }
        output(fmt::format(
            FMT_STRING("Teleported to ({:.2f}, {:.2f}, {:.2f})"), pos->x, pos->y, pos->z));
        return;
    }

    if (cmd == "spawn") {
        if (args.size() < 2) {
            output("Usage: spawn <actorId> [params] [x y z] [angle]");
            return;
        }
        auto* player = requirePlayer();
        if (player == nullptr) {
            return;
        }

        const auto actorId = ParseActorId(args[1]);
        if (!actorId) {
            output("Error: unknown actor ID or name");
            return;
        }

        long paramsL = -1;
        if (args.size() >= 3) {
            const auto p = ParseLong(args[2]);
            if (!p) {
                output("Error: invalid params");
                return;
            }
            paramsL = *p;
        }

        cXyz pos = player->current.pos;
        if (args.size() >= 6) {
            const auto spawnPos = ParseXYZ(args, 3);
            if (!spawnPos) {
                output("Error: invalid spawn coordinates");
                return;
            }
            pos = *spawnPos;
        }

        s16 angleY = 0;
        if (!TryAngle(angleY, args, 6, output)) {
            return;
        }
        csXyz angle;
        angle.set(0, angleY, 0);
        cXyz scale(1.0f, 1.0f, 1.0f);

        layer_class* savedLayer = fpcLy_CurrentLayer();
        base_process_class* playScene = fpcM_SearchByName(fpcNm_PLAY_SCENE_e);
        if (playScene != nullptr) {
            fpcLy_SetCurrentLayer(&((process_node_class*)playScene)->layer);
        }
        unsigned int result = fopAcM_create(
            *actorId, (u32)paramsL, &pos, player->current.roomNo, &angle, &scale, (s8)-1);
        fpcLy_SetCurrentLayer(savedLayer);

        output(result != 0 ? fmt::format(FMT_STRING("Spawned actorId=0x{:04X} procId={}"),
                                 (unsigned int)(u16)*actorId, result) :
                             fmt::format(FMT_STRING("Failed to spawn actorId=0x{:04X}"),
                                 (unsigned int)(u16)*actorId));
        return;
    }

    if (cmd == "reset") {
        JUTGamePad::C3ButtonReset::sResetSwitchPushing = true;
        output("Soft reset triggered");
        return;
    }

    if (cmd == "warp") {
        if (args.size() < 4) {
            output("Usage: warp <stageName> <point> <roomNo> [layer=-1]");
            output("  e.g.  warp F_SP121 0 0");
            return;
        }
        const auto pointL = ParseLong(args[2]), roomL = ParseLong(args[3]);
        if (!pointL || !roomL) {
            output("Error: invalid point or room number");
            return;
        }
        long layerL = -1;
        if (args.size() >= 5) {
            const auto l = ParseLong(args[4]);
            if (!l) {
                output("Error: invalid layer");
                return;
            }
            layerL = *l;
        }
        state.lastWarpStage = args[1];
        dComIfGp_setNextStage(state.lastWarpStage.c_str(), (s16)*pointL, (s8)*roomL, (s8)layerL);
        output(fmt::format(FMT_STRING("Warping to {} point={} room={} layer={}"), args[1],
            (int)(s16)*pointL, (int)(s8)*roomL, (int)(s8)layerL));
        return;
    }

    if (cmd == "list") {
        s16 targetId = -1;
        if (args.size() >= 2) {
            const auto id = ParseActorId(args[1]);
            if (!id) {
                output("Error: unknown actor ID or name");
                return;
            }
            targetId = *id;
        }
        std::vector<std::string> results;
        ListContext ctx{targetId, &results};
        fpcLyIt_OnlyHere(fpcLy_RootLayer(), ListActorCallback, &ctx);
        output(results.empty() ? "No matching actors found" :
                                 fmt::format(FMT_STRING("Found {} actor(s):"), results.size()));
        for (const auto& r : results) {
            output(r);
        }
        return;
    }

    if (cmd == "killall") {
        if (args.size() < 2) {
            output("Usage: killall <actorId|name>");
            return;
        }
        const auto targetId = ParseActorId(args[1]);
        if (!targetId) {
            output("Error: unknown actor ID or name");
            return;
        }
        FindContext ctx{*targetId, {}};
        fpcLyIt_OnlyHere(fpcLy_RootLayer(), FindActorCallback, &ctx);
        for (auto* proc : ctx.matches) {
            fpcM_Delete(proc);
        }
        output(fmt::format(FMT_STRING("Deleted {} actor(s) of type {} ({})"),
            (int)ctx.matches.size(), (unsigned int)(u16)*targetId, ActorShortName(*targetId)));
        return;
    }

    if (cmd == "heal") {
        auto* player = requirePlayer();
        if (player == nullptr) {
            return;
        }
        const u16 maxLife = dComIfGs_getMaxLife() / 5 * 4;
        u16 newLife = maxLife;
        if (args.size() >= 2) {
            const auto amount = ParseLong(args[1]);
            if (!amount) {
                output("Error: invalid amount");
                return;
            }
            newLife =
                (u16)std::max(0L, std::min((long)maxLife, (long)dComIfGs_getLife() + *amount));
        }
        dComIfGs_setLife(newLife);
        output(fmt::format(FMT_STRING("Health: {}/{}"), (int)newLife, (int)maxLife));
        return;
    }

    if (cmd == "kill") {
        if (args.size() >= 2) {
            auto* proc = ParseProcArg(args[1], state.foundProcId, output);
            if (proc == nullptr) {
                return;
            }
            fpcM_Delete(proc);
            output(fmt::format(FMT_STRING("Deleted proc {}"), proc->id));
        } else {
            auto* player = requirePlayer();
            if (player == nullptr) {
                return;
            }
            dComIfGs_setLife(0);
            output("Set Link's health to 0");
        }
        return;
    }

    if (cmd == "rate") {
        if (args.size() >= 2) {
            const auto hz = ParseLong(args[1]);
            if (!hz || *hz <= 0) {
                output("Error: rate must be a positive integer");
                return;
            }
            dusk::game_clock::set_sim_rate((float)*hz);
        }
        output(fmt::format(FMT_STRING("Sim rate: {} hz"), (int)dusk::game_clock::get_sim_rate()));
        return;
    }

    if (cmd == "ebf") {
        if (args.size() >= 2) {
            const auto val = ParseLong(args[1]);
            if (!val || *val < 0 || *val > 255) {
                output("Error: value must be 0-255");
                return;
            }
            cDmr_SkipInfo = (u8)*val;
        }
        output(fmt::format(FMT_STRING("EBF = {}"), (int)cDmr_SkipInfo));
        return;
    }

    if (cmd == "time") {
        if (args.size() >= 2) {
            const auto t = ParseFloat(args[1]);
            if (!t || *t < 0.0f || *t > 360.0f) {
                output("Error: time must be a float between 0 and 360");
                return;
            }
            dKy_instant_timechg(*t);
        }
        output(fmt::format(FMT_STRING("Time: {:.2f} ({}:{:02d})"), dComIfGs_getTime(),
            dKy_getdaytime_hour(), dKy_getdaytime_minute()));
        return;
    }

    if (cmd == "rupees") {
        const u16 maxRupees = dComIfGs_getRupeeMax();
        if (args.size() >= 2) {
            const auto amount = ParseLong(args[1]);
            if (!amount) {
                output("Error: invalid amount");
                return;
            }
            dComIfGs_setRupee((u16)std::max(0L, std::min((long)maxRupees, *amount)));
        }
        output(fmt::format(FMT_STRING("Rupees: {}/{}"), (int)dComIfGs_getRupee(), (int)maxRupees));
        return;
    }

    if (cmd == "find") {
        if (args.size() < 2) {
            if (state.foundProcId == 0) {
                output("@found is not set");
                return;
            }
            auto* proc = fpcM_SearchByID(state.foundProcId);
            output(proc != nullptr && fopAcM_IsActor(proc) ?
                       "@found = " + actorLine(proc) :
                       fmt::format(FMT_STRING("@found = {} (no longer exists)"),
                           (unsigned int)state.foundProcId));
            return;
        }

        const auto targetId = ParseActorId(args[1]);
        if (!targetId) {
            output("Error: unknown actor ID or name");
            return;
        }

        int targetN = 1;
        if (args.size() >= 3) {
            const auto n = ParseLong(args[2]);
            if (!n || *n < 1) {
                output("Error: index must be >= 1");
                return;
            }
            targetN = (int)*n;
        }

        FindContext ctx{*targetId, {}};
        fpcLyIt_OnlyHere(fpcLy_RootLayer(), FindActorCallback, &ctx);

        if (ctx.matches.empty()) {
            output(fmt::format(FMT_STRING("No actors found for '{}'"), args[1]));
            return;
        }

        std::sort(ctx.matches.begin(), ctx.matches.end(),
            [](const base_process_class* a, const base_process_class* b) { return a->id < b->id; });

        if (targetN > (int)ctx.matches.size()) {
            output(fmt::format(
                FMT_STRING("Error: only {} actor(s) of that type exist"), (int)ctx.matches.size()));
            return;
        }

        auto* picked = ctx.matches[(size_t)(targetN - 1)];
        state.foundProcId = picked->id;
        output(fmt::format(
            FMT_STRING("@found [{}/{}] {}"), targetN, (int)ctx.matches.size(), actorLine(picked)));
        return;
    }

    if (cmd == "transform") {
        auto* player = requirePlayer();
        if (player == nullptr) { return; }
        player->procCoMetamorphoseInit();
        output("Transforming");
        return;
    }

    if (cmd == "pos") {
        auto* player = requirePlayer();
        if (player == nullptr) {
            return;
        }
        output(fmt::format(FMT_STRING("pos: {:.4f} {:.4f} {:.4f}"), player->current.pos.x,
            player->current.pos.y, player->current.pos.z));
        output(
            fmt::format(FMT_STRING("stage: {}  room: {}  entry: {}"), dComIfGp_getStartStageName(),
                (int)player->current.roomNo, (int)dComIfGp_getStartStagePoint()));
        return;
    }

    if (cmd == "help") {
        output("@<ref>  =  @<procId> | @found | @link");
        output("");
        output("ebf [0-255]                                 Get or set cDmr_SkipInfo");
        output("find <id|name> [n=1]                        Store nth actor as @found");
        output("heal [amount]                               Heal to max, or by relative amount");
        output("kill                                        Set Link health to 0");
        output("kill @<ref>                                 Delete proc");
        output("killall <id|name>                           Delete all actors of a type");
        output("list [id|name]                              List actors in scene");
        output("pos                                         Print player position and stage");
        output("rate [hz]                                   Get or set sim rate (1-1000, default 30)");
        output("reset                                       Soft reset");
        output("rupees [amount]                             Get or set rupee count");
        output("spawn <id|name> [params] [x y z] [angle]    Spawn actor");
        output("time [0-360]                                Get or set time of day");
        output("tp <x> <y> <z> [angle]                      Teleport Link to coords");
        output("tp @<ref>                                   Teleport Link to actor");
        output("tp @<ref> <x> <y> <z> [angle]               Move actor to coords");
        output("tp @<ref> @<ref>                            Move actor to actor");
        output("transform                                   Force transform");
        output("warp <stage> <point> <room> [layer]         Warp to stage");
        return;
    }

    output(fmt::format(FMT_STRING("Unknown command '{}' (try 'help')"), cmd));
}

}  // namespace dusk

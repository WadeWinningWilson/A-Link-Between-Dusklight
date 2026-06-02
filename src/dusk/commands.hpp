#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace dusk {

using CommandOutput = std::function<void(std::string)>;

struct CommandState {
    std::vector<std::string> history;
    unsigned int foundProcId = 0;
    std::string lastWarpStage;
};

// Execute a single command line. Calls output() for every line of response.
// Manages history internally; callers need only hold a CommandState.
void runCommand(std::string_view cmdLine, CommandState& state, const CommandOutput& output);

}  // namespace dusk

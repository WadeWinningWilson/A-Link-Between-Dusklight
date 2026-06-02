#pragma once

#include "document.hpp"
#include "dusk/commands.hpp"

#include <string>
#include <vector>

namespace Rml {
class ElementFormControlInput;
}

namespace dusk::ui {

class CommandConsole : public Document {
public:
    CommandConsole();
    void show() override;
    void update() override;

private:
    struct OutputLine {
        std::string text;
        float remain;
    };

    static constexpr float kDurationNormal = 6.0f;
    static constexpr float kFadeSeconds = 0.8f;
    static constexpr int kMaxStoredLines = 64;
    static constexpr int kMaxVisibleLines = 24;
    static constexpr int kMaxMsgHistory = 500;

    Rml::Element* mConsole = nullptr;
    Rml::Element* mOutput = nullptr;
    Rml::ElementFormControlInput* mInput = nullptr;

    std::vector<OutputLine> mOutputLines;
    std::vector<std::string> mMsgHistory;
    int mHistoryPos = -1;
    bool mInputActive = false;
    bool mScrollToBottom = false;
    bool mPrevSlashState = false;

    CommandState mState;

    bool handle_nav_command(Rml::Event& event, NavCommand cmd) override;

    void ConsolePrint(std::string text);
    void openInput();
    void closeInput();
    void executeFromInput();
    void navigateHistory(int dir);
};

}  // namespace dusk::ui

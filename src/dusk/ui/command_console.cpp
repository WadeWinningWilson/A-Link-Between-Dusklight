#include "command_console.hpp"

#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <SDL3/SDL_keyboard.h>
#include <aurora/rmlui.hpp>
#include <imgui.h>

#include "dusk/settings.h"

#include <algorithm>
#include <string>
#include <string_view>

#include "fmt/format.h"
#include "ui.hpp"

namespace dusk::ui {
namespace {

static const Rml::String kDocumentSource = R"RML(
<rml>
<head>
    <link type="text/rcss" href="res/rml/command_console.rcss" />
</head>
<body>
    <console id="console">
        <output id="console-output"></output>
        <input id="console-input" type="text" maxlength="255" />
    </console>
</body>
</rml>
)RML";

static bool isCommand(std::string_view text) {
    return text.size() >= 2 && text[0] == '>' && text[1] == ' ';
}

}  // namespace

CommandConsole::CommandConsole() : Document(kDocumentSource) {
    mConsole = mDocument ? mDocument->GetElementById("console") : nullptr;
    mOutput = mDocument ? mDocument->GetElementById("console-output") : nullptr;
    auto* rawInput = mDocument ? mDocument->GetElementById("console-input") : nullptr;
    mInput = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(rawInput);

    if (mConsole != nullptr) {
        mConsole->SetAttribute("hidden", "");
    }

    listen(
        Rml::EventId::Keydown,
        [this](Rml::Event& event) {
            if (!mInputActive) {
                return;
            }
            const auto key = static_cast<Rml::Input::KeyIdentifier>(event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN));
            if (key == Rml::Input::KI_RETURN) {
                executeFromInput();
                event.StopImmediatePropagation();
            } else if (key == Rml::Input::KI_ESCAPE) {
                closeInput();
                event.StopImmediatePropagation();
            } else if (key == Rml::Input::KI_UP) {
                navigateHistory(-1);
                event.StopImmediatePropagation();
            } else if (key == Rml::Input::KI_DOWN) {
                navigateHistory(+1);
                event.StopImmediatePropagation();
            }
        },
        true);
}

void CommandConsole::show() {
    if (mDocument != nullptr) {
        mDocument->Show(Rml::ModalFlag::None, Rml::FocusFlag::None, Rml::ScrollFlag::None);
    }
}

bool CommandConsole::handle_nav_command(Rml::Event&, NavCommand) {
    return false;
}

void CommandConsole::update() {
    if (!getSettings().backend.enableAdvancedSettings) {
        return;
    }

    int numKeys = 0;
    const bool* keyState = SDL_GetKeyboardState(&numKeys);
    const bool slashNow = SDL_SCANCODE_SLASH < numKeys && keyState[SDL_SCANCODE_SLASH];
    if (!mInputActive && slashNow && !mPrevSlashState) {
        openInput();
    }
    mPrevSlashState = slashNow;

    const float dt = std::max(ImGui::GetIO().DeltaTime, 0.0f);
    for (auto& line : mOutputLines) {
        line.remain -= dt;
    }
    const auto [first, last] =
        std::ranges::remove_if(mOutputLines, [](const OutputLine& l) { return l.remain <= 0.0f; });
    mOutputLines.erase(first, last);

    if (mConsole != nullptr) {
        if (!mOutputLines.empty() || mInputActive) {
            mConsole->RemoveAttribute("hidden");
        } else {
            mConsole->SetAttribute("hidden", "");
            return;
        }
    }

    if (mOutput == nullptr) {
        return;
    }

    Rml::String html;
    if (mInputActive) {
        for (const auto& msg : mMsgHistory) {
            html += isCommand(msg) ? "<line class=\"cmd\">" + escape(msg) + "</line>" : "<line>" + escape(msg) + "</line>";
        }
        mOutput->SetAttribute("open", "");
    } else {
        const int total = (int)mOutputLines.size();
        const int startIdx = std::max(0, total - kMaxVisibleLines);
        for (int i = startIdx; i < total; ++i) {
            const auto& line = mOutputLines[i];
            const float alpha = line.remain < kFadeSeconds ? line.remain / kFadeSeconds : 1.0f;
            const Rml::String cls = isCommand(line.text) ? " class=\"cmd\"" : "";
            html += fmt::format(FMT_STRING("<line{} style=\"opacity: {:.3f}\">{}</line>"), cls,
                alpha, escape(line.text));
        }
        mOutput->RemoveAttribute("open");
    }

    mOutput->SetInnerRML(html);

    if (mScrollToBottom && mInputActive) {
        mOutput->SetScrollTop(1e9f);
        mScrollToBottom = false;
    }
}

void CommandConsole::openInput() {
    mInputActive = true;
    mScrollToBottom = true;
    if (mConsole != nullptr) {
        mConsole->SetAttribute("open", "");
        mConsole->RemoveAttribute("hidden");
    }
    if (mInput != nullptr) {
        mInput->SetValue("");
        aurora::rmlui::set_input_type(aurora::rmlui::InputType::Text);
        mInput->Focus(true);
    }
}

void CommandConsole::closeInput() {
    mInputActive = false;
    mHistoryPos = -1;
    if (mConsole != nullptr) {
        mConsole->RemoveAttribute("open");
    }
    if (mInput != nullptr) {
        mInput->SetValue("");
    }
    if (mDocument != nullptr) {
        mDocument->Focus(false);
    }
}

void CommandConsole::executeFromInput() {
    if (mInput == nullptr) {
        closeInput();
        return;
    }
    const Rml::String value = mInput->GetValue();
    closeInput();
    if (!value.empty()) {
        runCommand(value, mState, [this](std::string text) { ConsolePrint(std::move(text)); });
    }
    mScrollToBottom = true;
}

void CommandConsole::navigateHistory(int dir) {
    if (mState.history.empty() || mInput == nullptr) {
        return;
    }
    const int prev = mHistoryPos;
    if (dir < 0) {
        if (mHistoryPos == -1) {
            mHistoryPos = (int)mState.history.size() - 1;
        } else if (mHistoryPos > 0) {
            --mHistoryPos;
        }
    } else {
        if (mHistoryPos != -1 && ++mHistoryPos >= (int)mState.history.size()) {
            mHistoryPos = -1;
        }
    }
    if (prev != mHistoryPos) {
        const char* str = mHistoryPos >= 0 ? mState.history[mHistoryPos].c_str() : "";
        mInput->SetValue(str);
        const int end = static_cast<int>(Rml::StringUtilities::LengthUTF8(mInput->GetValue()));
        mInput->SetSelectionRange(end, end);
    }
}

void CommandConsole::ConsolePrint(std::string text) {
    mMsgHistory.push_back(text);
    if ((int)mMsgHistory.size() > kMaxMsgHistory) {
        mMsgHistory.erase(mMsgHistory.begin());
    }
    mOutputLines.push_back({std::move(text), kDurationNormal});
    mScrollToBottom = true;
    if ((int)mOutputLines.size() > kMaxStoredLines) {
        mOutputLines.erase(mOutputLines.begin());
    }
}

}  // namespace dusk::ui

#include "pane.hpp"

#include "Z2AudioLib/Z2SeMgr.h"
#include "m_Do/m_Do_audio.h"
#include "ui.hpp"

namespace dusk::ui {
namespace {

Rml::Element* createRoot(Rml::Element* parent) {
    auto* doc = parent->GetOwnerDocument();
    auto elem = doc->CreateElement("pane");
    return parent->AppendChild(std::move(elem));
}

}  // namespace

Pane::Pane(Rml::Element* parent, Type type, bool bottomSpacer) : FluentComponent(createRoot(parent)), mType(type), mBottomSpacer(bottomSpacer) {
    listen(Rml::EventId::Keydown, [this](Rml::Event& event) {
        const auto cmd = map_nav_event(event);

        // If navigating to the next pane, select the focused item
        if (mType == Type::Controlled && cmd == NavCommand::Right) {
            auto* target = event.GetTargetElement();
            int focusedChild = -1;
            for (size_t i = 0; i < mChildren.size(); ++i) {
                if (mChildren[i]->contains(target)) {
                    focusedChild = i;
                    break;
                }
            }
            if (focusedChild == -1) {
                return;
            }
            set_selected_item(focusedChild);
            return;
        }

        int direction = 0;
        if (cmd == NavCommand::Down) {
            direction = 1;
        } else if (cmd == NavCommand::Up) {
            direction = -1;
        } else {
            return;
        }
        auto* target = event.GetTargetElement();
        int focusedChild = -1;
        for (size_t i = 0; i < mChildren.size(); ++i) {
            if (mChildren[i]->contains(target)) {
                focusedChild = i;
                break;
            }
        }
        if (focusedChild == -1) {
            return;
        }
        int i = focusedChild + direction;
        while (i >= 0 && i < mChildren.size()) {
            if (mChildren[i]->focus()) {
                mDoAud_seStartMenu(kSoundItemFocus);
                event.StopPropagation();
                break;
            }
            i += direction;
        }
    });

    if (type == Type::Controlled) {
        // For controlled panes, handle SelectButton Submit events for item selection
        listen(Rml::EventId::Submit, [this](Rml::Event& event) {
            int childIndex = -1;
            for (int i = 0; i < mChildren.size(); ++i) {
                if (event.GetTargetElement() == mChildren[i]->root()) {
                    childIndex = i;
                }
            }
            // If item already selected, deselect
            if (childIndex >= 0 && childIndex < mChildren.size() &&
                mChildren[childIndex]->selected())
            {
                childIndex = -1;
            }
            set_selected_item(childIndex);
            // If the selection was handled locally, don't allow it to bubble up to window
            if (event.GetParameter("handled", false)) {
                event.StopPropagation();
            }
        });
    }
}

void Pane::update() {
    finalize();
    Component::update();
}

void Pane::set_selected_item(int index) {
    if (mType == Type::Uncontrolled) {
        return;
    }
    for (int i = 0; i < mChildren.size(); ++i) {
        mChildren[i]->set_selected(i == index);
    }
}

Component& Pane::register_control(
    Component& component, Pane& nextPane, std::function<void(Pane&)> callback) {
    component.listen(component.root(), Rml::EventId::Mouseover,
        [this, &component, &nextPane, callback](Rml::Event&) {
            if (component.disabled()) {
                return;
            }
            bool anySelected = false;
            for (const auto& child : mChildren) {
                if (child->selected()) {
                    anySelected = true;
                    break;
                }
            }
            if (!anySelected) {
                nextPane.clear();
                if (callback) {
                    callback(nextPane);
                }
            }
        });
    component.listen(component.root(), Rml::EventId::Focus,
        [this, &component, &nextPane, callback = std::move(callback)](Rml::Event&) {
            if (component.disabled()) {
                return;
            }
            nextPane.clear();

            // If an item is already selected, deselect
            for (const auto& child : mChildren) {
                if (child->selected()) {
                    set_selected_item(-1);
                    break;
                }
            }

            if (callback) {
                callback(nextPane);
            }
        });
    return component;
}

bool Pane::focus() {
    // Focus the first selected child
    for (const auto& child : mChildren) {
        if (child->selected() && child->focus()) {
            return true;
        }
    }
    // Otherwise, focus the first focusable child
    for (const auto& child : mChildren) {
        if (child->focus()) {
            return true;
        }
    }
    return false;
}

Rml::Element* Pane::add_section(const Rml::String& text) {
    auto* elem = append(mRoot, "div");
    elem->SetClass("section-heading", true);
    elem->SetInnerRML(escape(text));
    return elem;
}

Rml::Element* Pane::add_text(const Rml::String& text) {
    auto* elem = append(mRoot, "div");
    elem->SetInnerRML(escape(text));
    return elem;
}

Rml::Element* Pane::add_rml(const Rml::String& rml) {
    auto* elem = append(mRoot, "div");
    elem->SetInnerRML(rml);
    return elem;
}

void Pane::finalize() {
    if (finalized) {
        return;
    }
    finalized = true;

    // Append spacer element to the bottom. RmlUi does not properly handle
    // padding-bottom or margin-bottom on a scrollable flex container, so
    // we need to create a fake spacer with an actual layout height to get
    // padding at the bottom of a scrollable container.
    if (mBottomSpacer) {
        append(mRoot, "spacer");
    }
}

void Pane::clear() {
    clear_children();
    finalized = false;
}

float Pane::get_focused_child_y() {
    float childToFocusY = -1.f;
    for (const auto& child : children()) {
        if (child->root()->IsPseudoClassSet("focus")) {
            childToFocusY = child->root()->GetAbsoluteTop();
        }
    }
    return childToFocusY;
}

// Focuses the child closest to the given y position
bool Pane::focus_closest_child(float posY) {
    Rml::Element* closestchild = nullptr;
    // If the y-pos is less than 0, focus the middle child
    if (posY < 0.f && !children().empty()) {
        closestchild = children().at(children().size() / 2)->root();
    // Otherwise, choose the closest one
    } else if (posY >= 0.f) {
        float closestRightChildDistance = std::numeric_limits<float>::max();
        for (const auto& child : children()) {
            float distance = std::abs(posY - child->root()->GetAbsoluteTop());
            if (distance < closestRightChildDistance) {
                closestchild = child->root();
                closestRightChildDistance = distance;
            }
        }
    }

    // If we found a child, focus it
    if (closestchild) {
        closestchild->SetPseudoClass("focus-visible", true);
        closestchild->Focus();
        return true;
    }

    return false;
}

}  // namespace dusk::ui

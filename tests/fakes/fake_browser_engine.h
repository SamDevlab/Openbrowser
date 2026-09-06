#pragma once

#include "engine/browser_engine.h"

#include <cstddef>
#include <string>
#include <vector>

namespace openbrowser::tests {

enum class EngineCommandType {
    Create,
    Close,
    Activate,
    Navigate,
    Suspend,
    Resume,
};

struct EngineCommand {
    EngineCommandType type;
    core::TabId tab_id;
    std::string url;
};

class FakeBrowserEngine final : public engine::BrowserEngine {
public:
    void CreateTab(const core::Tab& tab) override {
        commands.push_back({.type = EngineCommandType::Create, .tab_id = tab.id, .url = tab.url});
    }

    void CloseTab(const core::TabId& tab_id) override {
        commands.push_back({.type = EngineCommandType::Close, .tab_id = tab_id, .url = {}});
    }

    void ActivateTab(const core::TabId& tab_id) override {
        commands.push_back({.type = EngineCommandType::Activate, .tab_id = tab_id, .url = {}});
    }

    void Navigate(const engine::NavigationRequest& request) override {
        commands.push_back({.type = EngineCommandType::Navigate, .tab_id = request.tab_id, .url = request.url});
    }

    void Suspend(const core::TabId& tab_id) override {
        commands.push_back({.type = EngineCommandType::Suspend, .tab_id = tab_id, .url = {}});
    }

    void Resume(const core::TabId& tab_id) override {
        commands.push_back({.type = EngineCommandType::Resume, .tab_id = tab_id, .url = {}});
    }

    [[nodiscard]] std::size_t Count(const EngineCommandType type) const {
        std::size_t count = 0;
        for (const auto& command : commands) {
            if (command.type == type) {
                ++count;
            }
        }
        return count;
    }

    std::vector<EngineCommand> commands;
};

}  // namespace openbrowser::tests

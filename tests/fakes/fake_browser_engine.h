#pragma once

#include "engine/browser_engine.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace openbrowser::tests {

using core::TabId;
using core::WorkspaceId;
using core::Tab;
using engine::BrowserEngine;
using engine::BrowserEngineEventSink;
using engine::NavigationRequest;

enum class EngineCommandType {
    Create,
    Close,
    Activate,
    Navigate,
    GoBack,
    GoForward,
    Reload,
    Suspend,
    Resume,
};

struct EngineCommand {
    EngineCommandType type;
    core::TabId tab_id;
    std::string url;
    std::optional<core::WorkspaceId> workspace_id{std::nullopt};
};

class FakeBrowserEngine final : public engine::BrowserEngine {
public:
    void SetEventSink(engine::BrowserEngineEventSink* sink) noexcept override {
        event_sink = sink;
    }

    void CreateTab(const core::Tab& tab) override {
        commands.push_back({
            .type = EngineCommandType::Create,
            .tab_id = tab.id,
            .url = tab.url,
            .workspace_id = tab.workspace_id,
        });

        if (emit_initial_navigation_during_create && event_sink != nullptr) {
            event_sink->OnNavigationStarted({.tab_id = tab.id, .url = tab.url});
            event_sink->OnNavigationCommitted({.tab_id = tab.id, .url = tab.url});
        }
    }

    void CloseTab(const core::TabId& tab_id) override {
        commands.push_back({.type = EngineCommandType::Close, .tab_id = tab_id, .url = {}, .workspace_id = std::nullopt});
    }

    void ActivateTab(const core::TabId& tab_id) override {
        commands.push_back({.type = EngineCommandType::Activate, .tab_id = tab_id, .url = {}, .workspace_id = std::nullopt});
    }

    void Navigate(const engine::NavigationRequest& request) override {
        commands.push_back({.type = EngineCommandType::Navigate, .tab_id = request.tab_id, .url = request.url, .workspace_id = std::nullopt});
    }

    void GoBack(const core::TabId& tab_id) override {
        commands.push_back({.type = EngineCommandType::GoBack, .tab_id = tab_id, .url = {}, .workspace_id = std::nullopt});
    }

    void GoForward(const core::TabId& tab_id) override {
        commands.push_back({.type = EngineCommandType::GoForward, .tab_id = tab_id, .url = {}, .workspace_id = std::nullopt});
    }

    void Reload(const core::TabId& tab_id) override {
        commands.push_back({.type = EngineCommandType::Reload, .tab_id = tab_id, .url = {}, .workspace_id = std::nullopt});
    }

    void Suspend(const core::TabId& tab_id) override {
        commands.push_back({.type = EngineCommandType::Suspend, .tab_id = tab_id, .url = {}, .workspace_id = std::nullopt});
    }

    void Resume(const core::TabId& tab_id) override {
        commands.push_back({.type = EngineCommandType::Resume, .tab_id = tab_id, .url = {}, .workspace_id = std::nullopt});
    }

    void EmitNavigationStarted(const core::TabId& tab_id, std::string url) const {
        if (event_sink != nullptr) {
            event_sink->OnNavigationStarted({.tab_id = tab_id, .url = std::move(url)});
        }
    }

    void EmitNavigationCommitted(const core::TabId& tab_id, std::string url) const {
        if (event_sink != nullptr) {
            event_sink->OnNavigationCommitted({.tab_id = tab_id, .url = std::move(url)});
        }
    }

    void EmitNavigationFailed(
        const core::TabId& tab_id,
        std::string url,
        const int error_code,
        std::string error_text) const {
        if (event_sink != nullptr) {
            event_sink->OnNavigationFailed({
                .tab_id = tab_id,
                .url = std::move(url),
                .error_code = error_code,
                .error_text = std::move(error_text),
            });
        }
    }

    void EmitTitleChanged(const core::TabId& tab_id, std::string title) const {
        if (event_sink != nullptr) {
            event_sink->OnTitleChanged({.tab_id = tab_id, .title = std::move(title)});
        }
    }

    void EmitRendererCrashed(const core::TabId& tab_id, std::string reason) const {
        if (event_sink != nullptr) {
            event_sink->OnRendererCrashed({.tab_id = tab_id, .reason = std::move(reason)});
        }
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

    engine::BrowserEngineEventSink* event_sink{nullptr};
    bool emit_initial_navigation_during_create{false};
    std::vector<EngineCommand> commands;
};

}  // namespace openbrowser::tests

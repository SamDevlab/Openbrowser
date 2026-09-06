#pragma once

#include "core/tabs/tab.h"
#include "engine/browser_engine.h"

#include <optional>
#include <string>
#include <vector>

namespace openbrowser::core {

class BrowserSession {
public:
    explicit BrowserSession(engine::BrowserEngine& engine) noexcept;

    [[nodiscard]] bool OpenTab(Tab tab, bool activate = true);
    [[nodiscard]] bool CloseTab(const TabId& tab_id);
    [[nodiscard]] bool ActivateTab(const TabId& tab_id);
    [[nodiscard]] bool Navigate(const TabId& tab_id, std::string url);
    [[nodiscard]] bool SuspendTab(const TabId& tab_id);
    [[nodiscard]] bool ResumeTab(const TabId& tab_id);

    [[nodiscard]] const Tab* FindTab(const TabId& tab_id) const;
    [[nodiscard]] const std::vector<Tab>& Tabs() const noexcept;
    [[nodiscard]] const std::optional<TabId>& ActiveTabId() const noexcept;

private:
    using TabIterator = std::vector<Tab>::iterator;

    [[nodiscard]] TabIterator FindMutable(const TabId& tab_id);
    void DemoteActiveTab();
    void ActivateAfterClose(std::size_t preferred_index);

    engine::BrowserEngine& engine_;
    std::vector<Tab> tabs_;
    std::optional<TabId> active_tab_id_;
};

}  // namespace openbrowser::core

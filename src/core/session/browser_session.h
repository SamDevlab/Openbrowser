#pragma once

#include "core/session/browser_session_observer.h"
#include "core/tabs/tab.h"
#include "engine/browser_engine.h"
#include "engine/browser_engine_events.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace openbrowser::core {

enum class CloseActivationPolicy {
    ActivateFallback,
    DoNotActivateFallback,
};

struct ClosedTabRecord {
    std::string url;
    std::string title;
    std::optional<WorkspaceId> workspace_id;
    bool is_ephemeral{false};
};

enum class ClosedTabMode {
    PersistentOnly,
    EphemeralOnly,
    Any,
};

class BrowserSession final : public engine::BrowserEngineEventSink {
public:
    explicit BrowserSession(engine::BrowserEngine& engine) noexcept;
    ~BrowserSession() override;

    BrowserSession(const BrowserSession&) = delete;
    BrowserSession& operator=(const BrowserSession&) = delete;
    BrowserSession(BrowserSession&&) = delete;
    BrowserSession& operator=(BrowserSession&&) = delete;

    void AddObserver(BrowserSessionObserver* observer);
    void RemoveObserver(BrowserSessionObserver* observer) noexcept;

    [[nodiscard]] bool OpenTab(Tab tab, bool activate = true);
    [[nodiscard]] bool CloseTab(
        const TabId& tab_id,
        CloseActivationPolicy activation_policy = CloseActivationPolicy::ActivateFallback);
    [[nodiscard]] bool ActivateTab(const TabId& tab_id);
    [[nodiscard]] bool Navigate(const TabId& tab_id, std::string url);
    [[nodiscard]] bool GoBack(const TabId& tab_id);
    [[nodiscard]] bool GoForward(const TabId& tab_id);
    [[nodiscard]] bool Reload(const TabId& tab_id);
    [[nodiscard]] bool SuspendTab(const TabId& tab_id);
    [[nodiscard]] bool ResumeTab(const TabId& tab_id);
    [[nodiscard]] bool DiscardTab(const TabId& tab_id);

    [[nodiscard]] std::optional<TabId> ReopenLastClosedTab(
        ClosedTabMode mode = ClosedTabMode::PersistentOnly);
    [[nodiscard]] const std::vector<ClosedTabRecord>& ClosedTabs() const noexcept;
    void PurgeEphemeralClosedTabs();
    [[nodiscard]] bool CycleTab(
        bool forward,
        const std::function<bool(const Tab&)>& filter = nullptr);

    // These are non-owning views into the session's internal storage. Any
    // operation that mutates the tab collection (for example OpenTab,
    // CloseTab, ReopenLastClosedTab, or privacy-mode transitions that perform
    // those operations) may invalidate previously returned Tab pointers or
    // references. Keep stable TabId values across mutations and reacquire the
    // view afterwards instead of retaining a Tab* or Tab&.
    [[nodiscard]] const Tab* FindTab(const TabId& tab_id) const;
    [[nodiscard]] const std::vector<Tab>& Tabs() const noexcept;
    [[nodiscard]] const std::optional<TabId>& ActiveTabId() const noexcept;

    void OnNavigationStarted(const engine::NavigationStartedEvent& event) override;
    void OnNavigationCommitted(const engine::NavigationCommittedEvent& event) override;
    void OnNavigationFailed(const engine::NavigationFailedEvent& event) override;
    void OnTitleChanged(const engine::TitleChangedEvent& event) override;
    void OnRendererCrashed(const engine::RendererCrashedEvent& event) override;

private:
    using TabIterator = std::vector<Tab>::iterator;

    [[nodiscard]] TabIterator FindMutable(const TabId& tab_id);
    [[nodiscard]] bool PrepareTabForNavigationCommand(const TabId& tab_id);
    void NotifyObservers();
    void DemoteActiveTab();
    void ActivateAfterClose(std::size_t preferred_index);

    engine::BrowserEngine& engine_;
    std::vector<Tab> tabs_;
    std::vector<ClosedTabRecord> closed_tabs_;
    std::optional<TabId> active_tab_id_;
    std::vector<BrowserSessionObserver*> observers_;
    std::uint64_t last_activated_counter_{0};
    std::uint64_t restored_counter_{0};
};

}  // namespace openbrowser::core

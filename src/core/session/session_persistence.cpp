#include "core/session/session_persistence.h"

#include "core/session/focus_session_controller.h"
#include "core/storage/atomic_file_store.h"
#include "core/storage/json_helper.h"

#include <utility>

namespace openbrowser::core {
namespace {

std::string LifecycleToString(const TabLifecycle lifecycle) {
    switch (lifecycle) {
        case TabLifecycle::Active: return "Active";
        case TabLifecycle::Background: return "Background";
        case TabLifecycle::Suspended: return "Suspended";
        case TabLifecycle::Discarded: return "Discarded";
    }
    return "Background";
}

TabLifecycle StringToLifecycle(const std::string_view str) {
    if (str == "Active") return TabLifecycle::Active;
    if (str == "Suspended") return TabLifecycle::Suspended;
    if (str == "Discarded") return TabLifecycle::Discarded;
    return TabLifecycle::Background;
}

std::string FocusStateToString(const FocusState state) {
    switch (state) {
        case FocusState::Now: return "Now";
        case FocusState::Next: return "Next";
        case FocusState::Later: return "Later";
        case FocusState::Paused: return "Paused";
    }
    return "Next";
}

FocusState StringToFocusState(const std::string_view str) {
    if (str == "Now") return FocusState::Now;
    if (str == "Later") return FocusState::Later;
    if (str == "Paused") return FocusState::Paused;
    return FocusState::Next;
}

}  // namespace

SessionSnapshot SessionPersistence::CaptureSnapshot(
    const BrowserSession& session,
    const FocusQueue& queue,
    const bool clean_shutdown) {
    SessionSnapshot snapshot;
    snapshot.schema_version = 1;
    snapshot.clean_shutdown = clean_shutdown;

    for (const auto& tab : session.Tabs()) {
        if (tab.is_ephemeral) {
            continue;
        }
        snapshot.tabs.push_back({
            .id = tab.id,
            .url = tab.url,
            .title = tab.title,
            .workspace_id = tab.workspace_id,
            .lifecycle = tab.lifecycle,
        });
    }

    if (session.ActiveTabId().has_value()) {
        const auto* active_tab = session.FindTab(*session.ActiveTabId());
        if (active_tab != nullptr && !active_tab->is_ephemeral) {
            snapshot.active_tab_id = session.ActiveTabId();
        } else if (!snapshot.tabs.empty()) {
            snapshot.active_tab_id = snapshot.tabs.front().id;
        } else {
            snapshot.active_tab_id = std::nullopt;
        }
    }

    for (const auto& item : queue.Items()) {
        if (item.tab_id.has_value()) {
            const auto* tab = session.FindTab(*item.tab_id);
            if (tab != nullptr && tab->is_ephemeral) {
                continue;
            }
        }
        snapshot.focus_items.push_back({
            .id = item.id,
            .url = item.url,
            .tab_id = item.tab_id,
            .workspace_id = item.workspace_id,
            .state = item.state,
        });
    }

    return snapshot;
}

bool SessionPersistence::RestoreSession(
    BrowserSession& session,
    FocusQueue& queue,
    const SessionSnapshot& snapshot) {
    // 1. Restore focus items
    for (const auto& item : snapshot.focus_items) {
        if (!queue.Contains(item.id)) {
            static_cast<void>(queue.Enqueue({
                .id = item.id,
                .url = item.url,
                .tab_id = item.tab_id,
                .workspace_id = item.workspace_id,
                .state = item.state,
            }));
        }
    }

    // 2. Restore tabs
    if (snapshot.tabs.empty()) {
        return true;
    }

    const std::string* target_active_id = nullptr;
    if (snapshot.active_tab_id.has_value()) {
        for (const auto& tab : snapshot.tabs) {
            if (tab.id == *snapshot.active_tab_id) {
                target_active_id = &tab.id;
                break;
            }
        }
    }
    if (target_active_id == nullptr && !snapshot.tabs.empty()) {
        target_active_id = &snapshot.tabs.front().id;
    }

    for (const auto& tab : snapshot.tabs) {
        if (session.FindTab(tab.id) != nullptr) {
            continue;
        }

        const bool is_active = (target_active_id != nullptr && tab.id == *target_active_id);
        Tab t;
        t.id = tab.id;
        t.url = tab.url;
        t.title = tab.title.empty() ? tab.url : tab.title;
        t.lifecycle = is_active ? TabLifecycle::Active : TabLifecycle::Background;
        t.workspace_id = tab.workspace_id;

        static_cast<void>(session.OpenTab(std::move(t), is_active));
    }

    if (target_active_id != nullptr) {
        static_cast<void>(session.ActivateTab(*target_active_id));
    }

    // 3. Synchronize tab closures with focus queue to ensure tab_id consistency
    FocusSessionController::SynchronizeTabClosures(session, queue);

    return true;
}

std::string SessionPersistence::Serialize(const SessionSnapshot& snapshot) {
    std::string out;
    out.reserve(1024);

    out += "{\n";
    out += "  \"schema_version\": " + std::to_string(snapshot.schema_version) + ",\n";
    out += "  \"clean_shutdown\": " + std::string(snapshot.clean_shutdown ? "true" : "false") + ",\n";

    out += "  \"active_tab_id\": ";
    if (snapshot.active_tab_id.has_value()) {
        storage::EscapeJsonString(*snapshot.active_tab_id, out);
    } else {
        out += "null";
    }
    out += ",\n";

    // Tabs
    out += "  \"tabs\": [\n";
    for (std::size_t i = 0; i < snapshot.tabs.size(); ++i) {
        const auto& tab = snapshot.tabs[i];
        out += "    {\n";
        out += "      \"id\": "; storage::EscapeJsonString(tab.id, out); out += ",\n";
        out += "      \"url\": "; storage::EscapeJsonString(tab.url, out); out += ",\n";
        out += "      \"title\": "; storage::EscapeJsonString(tab.title, out); out += ",\n";
        out += "      \"workspace_id\": ";
        if (tab.workspace_id.has_value()) {
            storage::EscapeJsonString(*tab.workspace_id, out);
        } else {
            out += "null";
        }
        out += ",\n";
        out += "      \"lifecycle\": "; storage::EscapeJsonString(LifecycleToString(tab.lifecycle), out); out += "\n";
        out += "    }";
        if (i + 1 < snapshot.tabs.size()) {
            out += ",";
        }
        out += "\n";
    }
    out += "  ],\n";

    // Focus items
    out += "  \"focus_items\": [\n";
    for (std::size_t i = 0; i < snapshot.focus_items.size(); ++i) {
        const auto& item = snapshot.focus_items[i];
        out += "    {\n";
        out += "      \"id\": "; storage::EscapeJsonString(item.id, out); out += ",\n";
        out += "      \"url\": "; storage::EscapeJsonString(item.url, out); out += ",\n";
        out += "      \"tab_id\": ";
        if (item.tab_id.has_value()) {
            storage::EscapeJsonString(*item.tab_id, out);
        } else {
            out += "null";
        }
        out += ",\n";
        out += "      \"workspace_id\": ";
        if (item.workspace_id.has_value()) {
            storage::EscapeJsonString(*item.workspace_id, out);
        } else {
            out += "null";
        }
        out += ",\n";
        out += "      \"state\": "; storage::EscapeJsonString(FocusStateToString(item.state), out); out += "\n";
        out += "    }";
        if (i + 1 < snapshot.focus_items.size()) {
            out += ",";
        }
        out += "\n";
    }
    out += "  ]\n";
    out += "}\n";

    return out;
}

std::optional<SessionSnapshot> SessionPersistence::Deserialize(const std::string_view json) {
    const auto root = storage::ParseJson(json);
    if (!root.has_value() || root->type != storage::JsonValue::Type::Object) {
        return std::nullopt;
    }

    SessionSnapshot snapshot;
    snapshot.schema_version = root->GetSizeT("schema_version", 1);
    snapshot.clean_shutdown = root->GetBool("clean_shutdown", false);
    snapshot.active_tab_id = root->GetOptionalString("active_tab_id");

    if (const auto* tabs_val = root->Find("tabs"); tabs_val && tabs_val->type == storage::JsonValue::Type::Array) {
        for (const auto& item : tabs_val->arr_val) {
            if (item.type != storage::JsonValue::Type::Object) continue;
            SessionTabRecord record;
            record.id = item.GetString("id");
            record.url = item.GetString("url");
            record.title = item.GetString("title");
            record.workspace_id = item.GetOptionalString("workspace_id");
            record.lifecycle = StringToLifecycle(item.GetString("lifecycle"));
            if (!record.id.empty()) {
                snapshot.tabs.push_back(std::move(record));
            }
        }
    }

    if (const auto* focus_val = root->Find("focus_items"); focus_val && focus_val->type == storage::JsonValue::Type::Array) {
        for (const auto& item : focus_val->arr_val) {
            if (item.type != storage::JsonValue::Type::Object) continue;
            FocusItemRecord record;
            record.id = item.GetString("id");
            record.url = item.GetString("url");
            record.tab_id = item.GetOptionalString("tab_id");
            record.workspace_id = item.GetOptionalString("workspace_id");
            record.state = StringToFocusState(item.GetString("state"));
            if (!record.id.empty()) {
                snapshot.focus_items.push_back(std::move(record));
            }
        }
    }

    return snapshot;
}

bool SessionPersistence::SaveToFile(
    const std::filesystem::path& file_path,
    const SessionSnapshot& snapshot) {
    if (file_path.empty()) return false;
    const auto json = Serialize(snapshot);
    const auto result = storage::AtomicWriteFile(file_path, json, /*keep_backup=*/true);
    return result.success;
}

std::optional<SessionSnapshot> SessionPersistence::LoadFromFile(
    const std::filesystem::path& file_path) {
    if (file_path.empty()) return std::nullopt;
    const auto result = storage::ReadFileWithBackupRecovery(file_path, [](std::string_view content) {
        const auto root = storage::ParseJson(content);
        return root.has_value() && root->type == storage::JsonValue::Type::Object;
    });

    if (!result.success) {
        return std::nullopt;
    }
    return Deserialize(result.content);
}

bool SessionPersistence::WasLastShutdownClean(
    const std::filesystem::path& file_path) {
    const auto snapshot = LoadFromFile(file_path);
    if (!snapshot.has_value()) {
        return true;
    }
    return snapshot->clean_shutdown;
}

}  // namespace openbrowser::core

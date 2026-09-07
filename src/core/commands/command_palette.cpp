#include "core/commands/command_palette.h"

#include <algorithm>
#include <cctype>

namespace openbrowser::core {
namespace {

std::string LowerAscii(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (const char c : value) {
        out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

}  // namespace

CommandPalette::CommandPalette(const ActionRegistry& registry)
    : registry_(registry) {}

int CommandPalette::CalculateScore(
    const std::string_view query,
    const ActionDefinition& action) {
    if (query.empty()) {
        return 1;
    }

    const std::string q = LowerAscii(query);
    const std::string title = LowerAscii(action.title);
    const std::string id = LowerAscii(action.id);
    const std::string desc = LowerAscii(action.description);
    const std::string cat = LowerAscii(ActionCategoryToString(action.category));

    int score = 0;

    if (title == q) {
        score += 100;
    } else if (title.rfind(q, 0) == 0) {  // starts with
        score += 60;
    } else if (title.find(q) != std::string::npos) {
        score += 30;
    }

    if (id.find(q) != std::string::npos) {
        score += 20;
    }

    if (desc.find(q) != std::string::npos) {
        score += 10;
    }

    if (cat.find(q) != std::string::npos) {
        score += 15;
    }

    return score;
}

std::vector<CommandPaletteMatch> CommandPalette::Search(
    const std::string_view query,
    const std::optional<ActionCategory> category_filter) const {
    std::vector<CommandPaletteMatch> results;

    for (const auto& action : registry_.ListActions()) {
        if (category_filter.has_value() && action.category != *category_filter) {
            continue;
        }

        const int score = CalculateScore(query, action);
        if (score > 0) {
            results.push_back({.action = action, .score = score});
        }
    }

    std::sort(results.begin(), results.end(), [](const CommandPaletteMatch& a, const CommandPaletteMatch& b) {
        if (a.score != b.score) {
            return a.score > b.score;  // higher score first
        }
        return a.action.title < b.action.title;
    });

    return results;
}

}  // namespace openbrowser::core

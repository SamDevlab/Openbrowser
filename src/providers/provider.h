#pragma once

#include <string>

namespace openbrowser::providers {

enum class ProviderKind {
    Sync,
    Filters,
    Updates,
    Dns,
    PackRegistry,
};

struct ProviderDescriptor {
    std::string id;
    ProviderKind kind;
    bool local{false};
    bool self_hosted{false};
    bool requires_network{false};
};

class Provider {
public:
    virtual ~Provider() = default;
    [[nodiscard]] virtual ProviderDescriptor Describe() const = 0;
};

}  // namespace openbrowser::providers

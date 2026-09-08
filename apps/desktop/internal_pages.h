#pragma once

#include "include/cef_base.h"
#include "include/cef_request_context.h"

#include <memory>
#include <string>

namespace openbrowser::desktop {

struct NewTabAppearance {
    std::string theme{"dark"};
    std::string accent{"violet"};
    std::string wallpaper{"aura"};
    bool show_shortcuts{true};
    bool show_context{true};
};

struct NewTabAppearanceState;

class InternalPageRegistry final {
public:
    InternalPageRegistry();
    ~InternalPageRegistry();

    InternalPageRegistry(const InternalPageRegistry&) = delete;
    InternalPageRegistry& operator=(const InternalPageRegistry&) = delete;

    [[nodiscard]] bool RegisterForContext(CefRefPtr<CefRequestContext> context) const;
    void SetNewTabAppearance(NewTabAppearance appearance);

private:
    std::shared_ptr<NewTabAppearanceState> appearance_state_;
    bool registered_{false};
};

}  // namespace openbrowser::desktop
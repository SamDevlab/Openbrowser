#pragma once

#include "include/cef_base.h"
#include "include/cef_request_context.h"

namespace openbrowser::desktop {

class InternalPageRegistry final {
public:
    InternalPageRegistry();
    ~InternalPageRegistry();

    InternalPageRegistry(const InternalPageRegistry&) = delete;
    InternalPageRegistry& operator=(const InternalPageRegistry&) = delete;

    [[nodiscard]] bool RegisterForContext(CefRefPtr<CefRequestContext> context) const;

private:
    bool registered_{false};
};

}  // namespace openbrowser::desktop

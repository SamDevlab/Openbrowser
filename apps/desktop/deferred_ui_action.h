#pragma once

#include "include/cef_task.h"
#include "include/wrapper/cef_helpers.h"

#include <functional>
#include <memory>
#include <utility>

namespace openbrowser::desktop {

class DeferredUiActionTask final : public CefTask {
public:
    DeferredUiActionTask(
        std::weak_ptr<bool> alive_token,
        std::function<void()> callback)
        : alive_token_(std::move(alive_token)),
          callback_(std::move(callback)) {}

    void Execute() override {
        CEF_REQUIRE_UI_THREAD();
        const auto alive = alive_token_.lock();
        if (alive && *alive && callback_) {
            callback_();
        }
        callback_ = nullptr;
    }

private:
    std::weak_ptr<bool> alive_token_;
    std::function<void()> callback_;

    IMPLEMENT_REFCOUNTING(DeferredUiActionTask);
};

inline void PostDeferredUiAction(
    std::weak_ptr<bool> alive_token,
    std::function<void()> callback) {
    CEF_REQUIRE_UI_THREAD();
    CefPostTask(
        TID_UI,
        new DeferredUiActionTask(std::move(alive_token), std::move(callback)));
}

}  // namespace openbrowser::desktop

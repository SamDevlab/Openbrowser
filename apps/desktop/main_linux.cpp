#include "desktop_app.h"

#if defined(CEF_X11)
#include <X11/Xlib.h>
#endif

#include "include/base/cef_logging.h"
#include "include/cef_app.h"

namespace {

#if defined(CEF_X11)
int XErrorHandlerImpl(Display* /*display*/, XErrorEvent* event) {
    LOG(WARNING) << "X error: type=" << event->type
                 << " serial=" << event->serial
                 << " error_code=" << static_cast<int>(event->error_code)
                 << " request_code=" << static_cast<int>(event->request_code)
                 << " minor_code=" << static_cast<int>(event->minor_code);
    return 0;
}

int XIOErrorHandlerImpl(Display* /*display*/) {
    return 0;
}
#endif

}  // namespace

NO_STACK_PROTECTOR
int main(int argc, char* argv[]) {
    CefMainArgs main_args(argc, argv);

    const int subprocess_exit_code = CefExecuteProcess(main_args, nullptr, nullptr);
    if (subprocess_exit_code >= 0) {
        return subprocess_exit_code;
    }

#if defined(CEF_X11)
    XSetErrorHandler(XErrorHandlerImpl);
    XSetIOErrorHandler(XIOErrorHandlerImpl);
#endif

    CefSettings settings;
#if !defined(CEF_USE_SANDBOX)
    settings.no_sandbox = true;
#endif

    CefRefPtr<openbrowser::desktop::DesktopApp> app(new openbrowser::desktop::DesktopApp());
    if (!CefInitialize(main_args, settings, app.get(), nullptr)) {
        return CefGetExitCode();
    }

    CefRunMessageLoop();
    app->ShutdownRuntime();
    CefShutdown();
    app = nullptr;
    return 0;
}

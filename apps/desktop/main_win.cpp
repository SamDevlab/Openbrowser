#include "desktop_app.h"

#include <windows.h>

#include "include/cef_app.h"
#include "include/cef_sandbox_win.h"
#include "include/cef_version_info.h"

namespace {

int RunMain(HINSTANCE instance, void* sandbox_info) {
    CefMainArgs main_args(instance);

    const int subprocess_exit_code = CefExecuteProcess(main_args, nullptr, sandbox_info);
    if (subprocess_exit_code >= 0) {
        return subprocess_exit_code;
    }

    CefSettings settings;
    if (sandbox_info == nullptr) {
        settings.no_sandbox = true;
    }

    CefRefPtr<openbrowser::desktop::DesktopApp> app(new openbrowser::desktop::DesktopApp());
    if (!CefInitialize(main_args, settings, app.get(), sandbox_info)) {
        return CefGetExitCode();
    }

    CefRunMessageLoop();
    app->ShutdownRuntime();
    CefShutdown();
    app = nullptr;
    return 0;
}

}  // namespace

#if defined(CEF_USE_BOOTSTRAP)

CEF_BOOTSTRAP_EXPORT int RunWinMain(
    HINSTANCE instance,
    LPWSTR command_line,
    int show_command,
    void* sandbox_info,
    cef_version_info_t* version_info) {
    (void)command_line;
    (void)show_command;
    (void)version_info;
    return RunMain(instance, sandbox_info);
}

#else

int APIENTRY wWinMain(
    HINSTANCE instance,
    HINSTANCE previous_instance,
    LPWSTR command_line,
    int show_command) {
    (void)previous_instance;

#if defined(ARCH_CPU_32_BITS)
    const int preferred_stack_exit_code =
        CefRunWinMainWithPreferredStackSize(wWinMain, instance, command_line, show_command);
    if (preferred_stack_exit_code >= 0) {
        return preferred_stack_exit_code;
    }
#endif

    void* sandbox_info = nullptr;
#if defined(CEF_USE_SANDBOX)
    CefScopedSandboxInfo scoped_sandbox;
    sandbox_info = scoped_sandbox.sandbox_info();
#endif

    return RunMain(instance, sandbox_info);
}

#endif

#include "desktop_app.h"

#include <aclapi.h>
#include <sddl.h>
#include <windows.h>

#include <string>
#include <vector>

#include "include/cef_app.h"
#include "include/cef_sandbox_win.h"
#include "include/cef_version_info.h"

namespace {

constexpr wchar_t kLpacSid[] = L"S-1-15-2-2";
constexpr DWORD kLpacAccess = FILE_GENERIC_READ | FILE_GENERIC_EXECUTE;
constexpr BYTE kLpacInheritance = OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE;

std::wstring RuntimeDirectory() {
    std::vector<wchar_t> buffer(32768);
    const DWORD length = GetModuleFileNameW(
        nullptr,
        buffer.data(),
        static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        return {};
    }

    std::wstring path(buffer.data(), length);
    const std::size_t separator = path.find_last_of(L"\\/");
    if (separator == std::wstring::npos) {
        return {};
    }
    path.resize(separator);
    return path;
}

bool HasLpacRuntimeAcl(const std::wstring& runtime_directory) {
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    PACL dacl = nullptr;
    const DWORD security_result = GetNamedSecurityInfoW(
        runtime_directory.c_str(),
        SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION,
        nullptr,
        nullptr,
        &dacl,
        nullptr,
        &descriptor);
    if (security_result != ERROR_SUCCESS || dacl == nullptr) {
        if (descriptor != nullptr) {
            LocalFree(descriptor);
        }
        return false;
    }

    PSID lpac_sid = nullptr;
    if (!ConvertStringSidToSidW(kLpacSid, &lpac_sid)) {
        LocalFree(descriptor);
        return false;
    }

    bool found = false;
    for (DWORD index = 0; index < dacl->AceCount; ++index) {
        void* raw_ace = nullptr;
        if (!GetAce(dacl, index, &raw_ace) || raw_ace == nullptr) {
            continue;
        }

        const auto* header = static_cast<ACE_HEADER*>(raw_ace);
        if (header->AceType != ACCESS_ALLOWED_ACE_TYPE) {
            continue;
        }

        const auto* ace = static_cast<ACCESS_ALLOWED_ACE*>(raw_ace);
        auto* ace_sid = const_cast<DWORD*>(&ace->SidStart);
        if (!EqualSid(ace_sid, lpac_sid)) {
            continue;
        }

        const bool has_access = (ace->Mask & kLpacAccess) == kLpacAccess;
        const bool has_inheritance =
            (header->AceFlags & kLpacInheritance) == kLpacInheritance;
        if (has_access && has_inheritance) {
            found = true;
            break;
        }
    }

    LocalFree(lpac_sid);
    LocalFree(descriptor);
    return found;
}

bool ApplyLpacRuntimeAcl(const std::wstring& runtime_directory) {
    if (HasLpacRuntimeAcl(runtime_directory)) {
        return true;
    }

    // Match the mechanism used by CEF's SET_LPAC_ACLS macro. icacls applies
    // the inheritable ACE to the directory and propagates it to the already
    // extracted runtime files, which is required for the Network Service LPAC
    // sandbox. Launch the system binary directly rather than through cmd.exe.
    std::vector<wchar_t> system_directory(MAX_PATH + 1);
    const UINT system_length = GetSystemDirectoryW(
        system_directory.data(),
        static_cast<UINT>(system_directory.size()));
    if (system_length == 0 || system_length >= system_directory.size()) {
        return false;
    }

    std::wstring icacls(system_directory.data(), system_length);
    icacls += L"\\icacls.exe";
    std::wstring command_line =
        L"\"" + icacls + L"\" \"" + runtime_directory +
        L"\" /grant *S-1-15-2-2:(OI)(CI)(RX) /Q";
    std::vector<wchar_t> mutable_command(command_line.begin(), command_line.end());
    mutable_command.push_back(L'\0');

    STARTUPINFOW startup_info{};
    startup_info.cb = sizeof(startup_info);
    PROCESS_INFORMATION process_info{};
    if (!CreateProcessW(
            icacls.c_str(),
            mutable_command.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            runtime_directory.c_str(),
            &startup_info,
            &process_info)) {
        return false;
    }

    const DWORD wait_result = WaitForSingleObject(process_info.hProcess, 15000);
    DWORD exit_code = ERROR_GEN_FAILURE;
    if (wait_result == WAIT_TIMEOUT) {
        static_cast<void>(TerminateProcess(process_info.hProcess, ERROR_TIMEOUT));
    } else if (wait_result == WAIT_OBJECT_0) {
        static_cast<void>(GetExitCodeProcess(process_info.hProcess, &exit_code));
    }

    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);
    return wait_result == WAIT_OBJECT_0 && exit_code == ERROR_SUCCESS &&
           HasLpacRuntimeAcl(runtime_directory);
}

bool IsPrimaryBrowserProcess() {
    const wchar_t* command_line = GetCommandLineW();
    return command_line == nullptr || wcsstr(command_line, L"--type=") == nullptr;
}

int FailSandboxInitialization() {
    MessageBoxW(
        nullptr,
        L"Openbrowser could not establish the Windows permissions required "
        L"for the Chromium sandbox. Move the extracted Openbrowser folder "
        L"to a normal NTFS location you own and try again.",
        L"Openbrowser sandbox initialization failed",
        MB_OK | MB_ICONERROR);
    return static_cast<int>(ERROR_ACCESS_DENIED);
}

int RunMain(HINSTANCE instance, void* sandbox_info) {
#if defined(OPENBROWSER_WINDOWS_SANDBOX)
    if (IsPrimaryBrowserProcess()) {
        // A build configured as sandboxed must never silently fall back to
        // CefSettings::no_sandbox. The bootstrap is expected to provide the
        // sandbox information object for the primary browser process.
        if (sandbox_info == nullptr) {
            return FailSandboxInitialization();
        }

        const std::wstring runtime_directory = RuntimeDirectory();
        if (runtime_directory.empty() || !ApplyLpacRuntimeAcl(runtime_directory)) {
            return FailSandboxInitialization();
        }
    }
#endif

    CefMainArgs main_args(instance);

    const int subprocess_exit_code = CefExecuteProcess(main_args, nullptr, sandbox_info);
    if (subprocess_exit_code >= 0) {
        return subprocess_exit_code;
    }

    CefSettings settings;
#if defined(OPENBROWSER_WINDOWS_SANDBOX)
    if (sandbox_info == nullptr) {
        return FailSandboxInitialization();
    }
#else
    if (sandbox_info == nullptr) {
        settings.no_sandbox = true;
    }
#endif

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

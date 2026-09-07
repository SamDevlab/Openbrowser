#include <windows.h>
#include <dbghelp.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>

#pragma comment(lib, "Dbghelp.lib")

namespace {

bool DiagnosticsEnabled() {
    wchar_t value[2]{};
    return GetEnvironmentVariableW(
               L"OPENBROWSER_NONINTERACTIVE",
               value,
               static_cast<DWORD>(std::size(value))) > 0;
}

std::filesystem::path RuntimeDirectory() {
    std::vector<wchar_t> buffer(32768);
    const DWORD length = GetModuleFileNameW(
        nullptr,
        buffer.data(),
        static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        return {};
    }

    std::filesystem::path executable(std::wstring(buffer.data(), length));
    return executable.parent_path();
}

void LogAccessViolation(EXCEPTION_POINTERS* exception_info) {
    if (!DiagnosticsEnabled() || exception_info == nullptr ||
        exception_info->ExceptionRecord == nullptr ||
        exception_info->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION) {
        return;
    }

    const auto runtime_directory = RuntimeDirectory();
    if (runtime_directory.empty()) {
        return;
    }

    std::ofstream stream(runtime_directory / L"debug.log", std::ios::app);
    if (!stream.is_open()) {
        return;
    }

    const auto address = reinterpret_cast<std::uintptr_t>(
        exception_info->ExceptionRecord->ExceptionAddress);

    stream << "[openbrowser-crash] exception=0x"
           << std::hex << std::uppercase
           << exception_info->ExceptionRecord->ExceptionCode
           << " address=0x" << address;

    MEMORY_BASIC_INFORMATION memory_info{};
    if (VirtualQuery(
            exception_info->ExceptionRecord->ExceptionAddress,
            &memory_info,
            sizeof(memory_info)) != 0 &&
        memory_info.AllocationBase != nullptr) {
        const auto module_base = reinterpret_cast<std::uintptr_t>(memory_info.AllocationBase);
        std::vector<wchar_t> module_path(32768);
        const DWORD module_length = GetModuleFileNameW(
            static_cast<HMODULE>(memory_info.AllocationBase),
            module_path.data(),
            static_cast<DWORD>(module_path.size()));
        if (module_length > 0 && module_length < module_path.size()) {
            stream << " module="
                   << std::filesystem::path(
                          std::wstring(module_path.data(), module_length))
                          .filename()
                          .string();
        }
        stream << " rva=0x" << (address - module_base);
    }

    HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME | SYMOPT_LOAD_LINES);
    if (SymInitialize(process, nullptr, TRUE)) {
        alignas(SYMBOL_INFO) unsigned char symbol_buffer[
            sizeof(SYMBOL_INFO) + MAX_SYM_NAME]{};
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbol_buffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;

        DWORD64 displacement = 0;
        if (SymFromAddr(
                process,
                static_cast<DWORD64>(address),
                &displacement,
                symbol)) {
            stream << " symbol=" << symbol->Name
                   << "+0x" << displacement;

            IMAGEHLP_LINE64 line{};
            line.SizeOfStruct = sizeof(line);
            DWORD line_displacement = 0;
            if (SymGetLineFromAddr64(
                    process,
                    static_cast<DWORD64>(address),
                    &line_displacement,
                    &line) &&
                line.FileName != nullptr) {
                stream << " source=" << line.FileName << ':' << std::dec << line.LineNumber;
            }
        }
        SymCleanup(process);
    }

    stream << '\n';
    stream.flush();
}

LONG CALLBACK StartupExceptionHandler(EXCEPTION_POINTERS* exception_info) {
    LogAccessViolation(exception_info);
    return EXCEPTION_CONTINUE_SEARCH;
}

class ExceptionHandlerRegistration final {
public:
    ExceptionHandlerRegistration()
        : handle_(AddVectoredExceptionHandler(1, StartupExceptionHandler)) {}

    ~ExceptionHandlerRegistration() {
        if (handle_ != nullptr) {
            RemoveVectoredExceptionHandler(handle_);
        }
    }

    ExceptionHandlerRegistration(const ExceptionHandlerRegistration&) = delete;
    ExceptionHandlerRegistration& operator=(const ExceptionHandlerRegistration&) = delete;

private:
    void* handle_{nullptr};
};

ExceptionHandlerRegistration g_exception_handler_registration;

}  // namespace

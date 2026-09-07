#include <windows.h>
#include <dbghelp.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
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

std::wstring DiagnosticSymbolPath() {
    std::vector<wchar_t> buffer(32768);
    const DWORD length = GetEnvironmentVariableW(
        L"OPENBROWSER_SYMBOL_PATH",
        buffer.data(),
        static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        return {};
    }
    return std::wstring(buffer.data(), length);
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

    const auto* record = exception_info->ExceptionRecord;
    const auto address = reinterpret_cast<std::uintptr_t>(record->ExceptionAddress);

    stream << "[openbrowser-crash] exception=0x"
           << std::hex << std::uppercase
           << record->ExceptionCode
           << " address=0x" << address;

    if (record->NumberParameters >= 2) {
        const ULONG_PTR operation = record->ExceptionInformation[0];
        const ULONG_PTR target = record->ExceptionInformation[1];
        const char* operation_name = "unknown";
        if (operation == 0) {
            operation_name = "read";
        } else if (operation == 1) {
            operation_name = "write";
        } else if (operation == 8) {
            operation_name = "execute";
        }
        stream << " operation=" << operation_name
               << " target=0x" << static_cast<std::uintptr_t>(target);
    }

    MEMORY_BASIC_INFORMATION memory_info{};
    if (VirtualQuery(
            record->ExceptionAddress,
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
    const std::wstring symbol_path = DiagnosticSymbolPath();
    const wchar_t* symbol_search_path = symbol_path.empty() ? nullptr : symbol_path.c_str();
    if (SymInitializeW(process, symbol_search_path, TRUE)) {
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

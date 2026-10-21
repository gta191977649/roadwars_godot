#pragma once
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace roadwars {
using DiagnosticHandler = void (*)(const char*) noexcept;
inline std::atomic<DiagnosticHandler> diagnostic_handler{};
// Always terminates, including builds with NDEBUG. Break here in a debugger.
[[noreturn]] inline void
not_implemented(const char* function_id, const char* original_symbol,
                std::source_location location = std::source_location::current()) {
    char message[8192];
    std::snprintf(message, sizeof(message),
                  "[RoadWars] NOT IMPLEMENTED: %s\n  RE: %s\n  at %s:%u (%s)\n", original_symbol,
                  function_id, location.file_name(), location.line(), location.function_name());
    if (const auto handler = diagnostic_handler.load())
        handler(message);
    std::fputs(message, stderr);
    std::fflush(stderr);
    assert(false && "RoadWars: not implemented (see console for original address)");
    std::abort();
}
} // namespace roadwars

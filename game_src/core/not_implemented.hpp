#pragma once
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace roadwars {
// Always terminates, including builds with NDEBUG. Break here in a debugger.
[[noreturn]] inline void not_implemented(
    const char* function_id, const char* original_symbol,
    std::source_location location = std::source_location::current()) {
    std::fprintf(stderr, "[RoadWars] NOT IMPLEMENTED: %s\n  RE: %s\n  at %s:%u (%s)\n",
                 original_symbol, function_id, location.file_name(), location.line(),
                 location.function_name());
    std::fflush(stderr);
    assert(false && "RoadWars: not implemented (see console for original address)");
    std::abort();
}
} // namespace roadwars

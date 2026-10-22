#include "platform/godot/extension/frame_trace.hpp"
#include "platform/godot/extension/game_files.hpp"
#include <cstring>
#include <godot_cpp/classes/file_access.hpp>

namespace roadwars {
Result GameFiles::read(std::string_view path, std::vector<std::byte>& output) {
    trace_resource(FrameResource::file_read, path);
    output.clear();
    if (path.empty() || path.find("..") != std::string_view::npos ||
        path.find(':') != std::string_view::npos || path.front() == '/' || path.front() == '\\')
        return Result::invalid_argument;
    auto relative =
        godot::String::utf8(path.data(), static_cast<std::int64_t>(path.size())).replace("\\", "/");
    const auto file = godot::FileAccess::open(root_.path_join(relative), godot::FileAccess::READ);
    if (file.is_null())
        return Result::not_found;
    const auto length = file->get_length();
    if (length > 1024ULL * 1024 * 1024)
        return Result::invalid_argument;
    const auto buffer = file->get_buffer(length);
    if (static_cast<std::uint64_t>(buffer.size()) != length)
        return Result::invalid_argument;
    output.resize(length);
    if (length)
        std::memcpy(output.data(), buffer.ptr(), static_cast<std::size_t>(length));
    return Result::ok;
}
} // namespace roadwars

#include "platform/godot/extension/game_files.hpp"
#include "game_src/unresolved/resource_store.hpp"
#include "platform/godot/extension/frame_trace.hpp"
#include <cstring>
#include <filesystem>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace roadwars {
godot::String resolve_game_path(const godot::String& root, const godot::String& relative,
                                bool directory, godot::String& error) {
    auto current = root;
    error = "";
    const auto components = relative.replace("\\", "/").split("/", false);
    for (int i = 0; i < components.size(); ++i) {
        const auto part = components[i];
        if (part == "." || part == ".." || part.contains(":")) {
            error = "Invalid resource path: " + relative;
            return {};
        }
        const auto dir = godot::DirAccess::open(current);
        if (dir.is_null()) {
            error = "Unreadable directory: " + current;
            return {};
        }
        // Enumerate even when the exact spelling exists: ambiguous case variants
        // must behave consistently on case-sensitive and insensitive filesystems.
        godot::String match;
        int matches = 0;
        dir->list_dir_begin();
        for (auto name = dir->get_next(); !name.is_empty(); name = dir->get_next()) {
            if (name.nocasecmp_to(part) == 0) {
                match = name;
                ++matches;
            }
        }
        dir->list_dir_end();
        if (matches != 1) {
            error = (matches ? "Ambiguous case variants: " : "Missing resource: ") +
                    current.path_join(part);
            return {};
        }
        current = current.path_join(match);
        if ((i + 1 < components.size() || directory) &&
            !godot::DirAccess::dir_exists_absolute(current)) {
            error = "Not a directory: " + current;
            return {};
        }
    }
    if (!directory && godot::FileAccess::open(current, godot::FileAccess::READ).is_null()) {
        error = "Unreadable file: " + current;
        return {};
    }
    return current;
}
bool validate_game_directory(const godot::String& root, godot::String& error) {
    error = "";
    for (const auto* name : {"Data.pkh", "Data.pak"}) {
        godot::String issue;
        if (resolve_game_path(root, name, false, issue).is_empty())
            error += issue + "\n";
    }
    for (const auto* name : {"Cars", "Tracks", "Sfx"}) {
        godot::String issue;
        if (resolve_game_path(root, name, true, issue).is_empty())
            error += issue + "\n";
    }
    if (!error.is_empty())
        return false;
    GameFiles files(root);
    ResourceStore resources(files);
    if (resources.open() != Result::ok) {
        error = "Invalid or unreadable Data.pkh / Data.pak archive.";
        return false;
    }
    for (const auto* name : {"Ini/RoadWars.ini", "Screens/SelectGameOption/Data.ini",
                             "Fonts/16Font1.bmp", "Fonts/16ScreenTitleFont.bmp"}) {
        std::vector<std::byte> bytes;
        auto result = resources.read(name, bytes);
        std::string encoded(name);
        if (result == Result::not_found && encoded.ends_with(".ini")) {
            encoded.replace(encoded.size() - 4, 4, ".pse");
            result = resources.read(encoded, bytes);
        }
        if (result != Result::ok || bytes.empty())
            error += "Missing or unreadable resource: " + godot::String(name) + "\n";
    }
    return error.is_empty();
}
godot::String current_game_directory() {
    std::error_code error;
    const auto path = std::filesystem::current_path(error).generic_u8string();
    if (error)
        return {};
    return godot::String::utf8(reinterpret_cast<const char*>(path.data()), path.size());
}
godot::String detect_game_directory() {
    const godot::String starts[] = {
        current_game_directory(),
        godot::OS::get_singleton()->get_executable_path().get_base_dir()};
    for (auto path : starts) {
        while (!path.is_empty()) {
            godot::String error;
            if (validate_game_directory(path, error))
                return path;
            const auto parent = path.get_base_dir();
            if (parent == path)
                break;
            path = parent;
        }
    }
    return {};
}
Result GameFiles::read(std::string_view path, std::vector<std::byte>& output) {
    trace_resource(FrameResource::file_read, path);
    output.clear();
    if (path.empty() || path.find("..") != std::string_view::npos ||
        path.find(':') != std::string_view::npos || path.front() == '/' || path.front() == '\\')
        return Result::invalid_argument;
    auto relative =
        godot::String::utf8(path.data(), static_cast<std::int64_t>(path.size())).replace("\\", "/");
    auto found = resolved_.find(std::string(path));
    if (found == resolved_.end()) {
        godot::String error;
        auto resolved = resolve_game_path(root_, relative, false, error);
        if (resolved.is_empty()) {
            if (error.begins_with("Ambiguous case variants:")) {
                godot::UtilityFunctions::push_error(error);
                return Result::invalid_argument;
            }
            return Result::not_found;
        }
        found = resolved_.emplace(std::string(path), std::move(resolved)).first;
    }
    const auto file = godot::FileAccess::open(found->second, godot::FileAccess::READ);
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

#include "game_src/unresolved/resource_store.hpp"
#include "game_src/unresolved/binary_reader.hpp"
#include <algorithm>
#include <charconv>
#include <sstream>

namespace roadwars {
std::string ResourceStore::canonical(std::string_view path) {
    std::string result;
    for (unsigned char value : path) {
        if (value == '\\')
            value = '/';
        if (value >= 'a' && value <= 'z')
            value -= 'a' - 'A';
        result += static_cast<char>(value);
    }
    return result;
}
Result ResourceStore::open() {
    std::vector<std::byte> index, archive;
    if (reader_.read("Data.pkh", index) != Result::ok ||
        reader_.read("Data.pak", archive) != Result::ok)
        return Result::not_found;
    std::map<std::string, ArchiveEntry> entries;
    try {
        BinaryReader stream(index);
        while (stream.remaining()) {
            const auto count = stream.u32();
            if (count < 2 || count > 4096)
                return Result::invalid_argument;
            auto data = stream.take(count);
            if (data.back() != std::byte{})
                return Result::invalid_argument;
            std::string name;
            for (std::size_t i = 0; i + 1 < count; ++i)
                name += static_cast<char>((std::to_integer<unsigned>(data[i]) - 1) & 255);
            const auto low = stream.u32();
            const auto high = stream.u32();
            ArchiveEntry entry{low | (static_cast<std::uint64_t>(high) << 32), stream.u32()};
            if (entry.offset > archive.size() || entry.size > archive.size() - entry.offset)
                return Result::invalid_argument;
            if (!entries.emplace(canonical(name), entry).second)
                return Result::invalid_argument;
        }
    } catch (const std::exception&) {
        return Result::invalid_argument;
    }
    archive_ = std::move(archive);
    entries_ = std::move(entries);
    return Result::ok;
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004f0c38
// Portable archive-first lookup. Legacy handle/cache mechanics are host-independent replacements.
Result ResourceStore::read(std::string_view path, std::vector<std::byte>& output) {
    output.clear();
    const auto key = canonical(path);
    if (key.empty() || key.front() == '/' || key.find(':') != std::string::npos ||
        key.find("..") != std::string::npos)
        return Result::invalid_argument;
    const auto found = entries_.find(key);
    if (found == entries_.end())
        return reader_.read(path, output);
    const auto& entry = found->second;
    const auto start = archive_.begin() + static_cast<std::ptrdiff_t>(entry.offset);
    output.assign(start, start + entry.size);
    return Result::ok;
}
namespace {
std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};
    return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}
} // namespace
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004d5ad0
// Test-path parser only; original token/include grammar is not completely recovered.
Result Configuration::load(ResourceStore& resources, std::string_view path) {
    values_.clear();
    std::vector<std::byte> bytes;
    std::string encoded(path);
    const auto dot = encoded.find_last_of('.');
    if (dot == std::string::npos)
        return Result::invalid_argument;
    encoded.replace(dot, std::string::npos, ".pse");
    auto result = resources.read(encoded, bytes);
    if (result == Result::ok) {
        for (auto& byte : bytes)
            byte = static_cast<std::byte>((std::to_integer<unsigned>(byte) - 1) & 255);
    } else if (result == Result::not_found) {
        result = resources.read(path, bytes);
        if (result != Result::ok)
            return result;
    } else {
        return result;
    }
    std::string text;
    for (auto byte : bytes)
        text += static_cast<char>(byte);
    std::istringstream stream(text);
    std::string section, line;
    while (std::getline(stream, line)) {
        line = trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#')
            continue;
        if (line.front() == '[' && line.back() == ']') {
            section = trim(line.substr(1, line.size() - 2));
            continue;
        }
        const auto separator = line.find('=');
        if (separator == std::string::npos)
            return Result::invalid_argument;
        values_[ResourceStore::canonical(section + "/" + trim(line.substr(0, separator)))] =
            trim(line.substr(separator + 1));
    }
    return Result::ok;
}
std::string Configuration::value(std::string_view section, std::string_view key) const {
    const auto found =
        values_.find(ResourceStore::canonical(std::string(section) + "/" + std::string(key)));
    return found == values_.end() ? std::string{} : found->second;
}
Result Configuration::number(std::string_view section, std::string_view key, float& output) const {
    const auto text = value(section, key);
    float number{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), number);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
        !std::isfinite(number))
        return Result::invalid_argument;
    output = number;
    return Result::ok;
}
} // namespace roadwars

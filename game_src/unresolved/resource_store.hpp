#pragma once
#include "game_src/interfaces/host.hpp"
#include <map>
#include <string>

namespace roadwars {
struct ArchiveEntry {
    std::uint64_t offset{};
    std::uint32_t size{};
};
class ResourceStore {
  public:
    explicit ResourceStore(ResourceReader& reader) : reader_(reader) {}
    Result open();
    Result read(std::string_view path, std::vector<std::byte>& output);
    static std::string canonical(std::string_view path);
    const std::map<std::string, ArchiveEntry>& entries() const { return entries_; }

  private:
    ResourceReader& reader_;
    std::map<std::string, ArchiveEntry> entries_;
    std::vector<std::byte> archive_;
};
class Configuration {
  public:
    Result load(ResourceStore& resources, std::string_view path);
    std::string value(std::string_view section, std::string_view key) const;
    Result number(std::string_view section, std::string_view key, float& output) const;

  private:
    std::map<std::string, std::string> values_;
};
} // namespace roadwars

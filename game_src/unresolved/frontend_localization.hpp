#pragma once
#include "game_src/unresolved/resource_store.hpp"
#include <map>
#include <string>
#include <vector>

namespace roadwars {
// Portable representation of the original localization table, not an original class identity.
class FrontendLocalization {
  public:
    Result load(ResourceStore&, std::string_view table, std::string& error);
    Result decode(std::span<const std::byte> encoded, std::string& error);
    // Language zero is the source key. Missing translations return not_found and the key.
    // Output remains in the original single-byte glyph encoding, not UTF-8.
    Result lookup(std::string_view key, int language, std::string& output) const;
    std::size_t size() const { return records_.size(); }

  private:
    std::map<std::string, std::vector<std::string>, std::less<>> records_;
};

std::span<const unsigned char> frontend_title_alphabet();
std::span<const unsigned char> frontend_text_alphabet();
std::span<const unsigned char> frontend_option_alphabet();
std::span<const unsigned char> frontend_menu_alphabet();
std::span<const unsigned char> frontend_cyber_alphabet(bool small);
std::span<const unsigned char> frontend_garage_alphabet();
} // namespace roadwars

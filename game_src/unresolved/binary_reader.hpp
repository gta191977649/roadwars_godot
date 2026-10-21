#pragma once
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>

namespace roadwars {
class BinaryReader {
  public:
    explicit BinaryReader(std::span<const std::byte> bytes) : bytes_(bytes) {}
    std::span<const std::byte> take(std::size_t count) {
        if (count > bytes_.size() - offset_)
            throw std::runtime_error("Truncated resource");
        const auto result = bytes_.subspan(offset_, count);
        offset_ += count;
        return result;
    }
    std::uint32_t u32() {
        auto data = take(4);
        std::uint32_t value{};
        for (int i = 0; i < 4; ++i)
            value |= std::to_integer<std::uint32_t>(data[i]) << (i * 8);
        return value;
    }
    std::uint16_t u16() {
        auto data = take(2);
        return std::to_integer<std::uint16_t>(data[0]) |
               (std::to_integer<std::uint16_t>(data[1]) << 8);
    }
    float f32() {
        const auto value = std::bit_cast<float>(u32());
        if (!std::isfinite(value))
            throw std::runtime_error("Nonfinite resource coordinate at byte " +
                                     std::to_string(offset_ - 4));
        return value;
    }
    std::string text(std::size_t count) {
        auto data = take(count);
        std::string result;
        for (auto byte : data) {
            if (byte == std::byte{})
                break;
            result += static_cast<char>(byte);
        }
        return result;
    }
    std::size_t remaining() const { return bytes_.size() - offset_; }
    std::size_t offset() const { return offset_; }

  private:
    std::span<const std::byte> bytes_;
    std::size_t offset_{};
};
} // namespace roadwars

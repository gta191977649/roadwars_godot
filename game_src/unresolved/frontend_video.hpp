#pragma once
#include "game_src/unresolved/bitmap.hpp"
#include <array>
#include <string>

namespace roadwars {
// New portable container/codec adaptation. The original delegates AVI playback to VFW.
struct FrontendVideoAudio {
    std::uint16_t format_tag{}, channels{}, block_alignment{}, bits_per_sample{};
    std::uint32_t sample_rate{};
    std::vector<std::byte> encoded;
};
class FrontendVideo {
  public:
    Result open(std::span<const std::byte> bytes, std::string& error);
    Result frame_at(double seconds, Bitmap& output);
    void close();
    double duration() const;
    std::size_t frame_count() const { return frames_.size(); }
    std::uint32_t width() const { return width_; }
    std::uint32_t height() const { return height_; }
    const FrontendVideoAudio& audio() const { return audio_; }

  private:
    using Color = std::array<std::byte, 4>;
    using Vector = std::array<Color, 4>;
    struct Strip {
        std::array<Vector, 256> small{}, large{};
    };
    Result decode_frame(std::span<const std::byte>);
    void reset_decoder();
    std::uint32_t width_{}, height_{}, rate_{}, scale_{};
    std::vector<std::vector<std::byte>> frames_;
    FrontendVideoAudio audio_;
    std::array<Strip, 32> strips_{};
    Bitmap decoded_;
    std::size_t next_frame_{};
};
} // namespace roadwars

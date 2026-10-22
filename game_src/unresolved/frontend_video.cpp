#include "game_src/unresolved/frontend_video.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace roadwars {
namespace {
class VideoReader {
  public:
    explicit VideoReader(std::span<const std::byte> bytes) : bytes_(bytes) {}
    std::size_t remaining() const { return bytes_.size() - position_; }
    std::span<const std::byte> take(std::size_t count) {
        if (count > remaining())
            throw std::runtime_error("truncated AVI/Cinepak data");
        const auto result = bytes_.subspan(position_, count);
        position_ += count;
        return result;
    }
    unsigned byte() { return std::to_integer<unsigned>(take(1)[0]); }
    std::uint32_t little(unsigned count) {
        std::uint32_t value{};
        for (unsigned i = 0; i < count; ++i)
            value |= byte() << (8 * i);
        return value;
    }
    std::uint32_t big(unsigned count) {
        std::uint32_t value{};
        for (unsigned i = 0; i < count; ++i)
            value = (value << 8) | byte();
        return value;
    }

  private:
    std::span<const std::byte> bytes_;
    std::size_t position_{};
};
bool fourcc(std::span<const std::byte> bytes, std::string_view name) {
    return bytes.size() == 4 && name.size() == 4 &&
           std::equal(bytes.begin(), bytes.end(), name.begin(), [](std::byte a, char b) {
               return std::to_integer<unsigned>(a) == static_cast<unsigned char>(b);
           });
}
struct RiffChunk {
    std::span<const std::byte> id, bytes;
};
RiffChunk chunk(VideoReader& reader) {
    const auto id = reader.take(4);
    const auto length = reader.little(4);
    const auto bytes = reader.take(length);
    if ((length & 1) != 0)
        reader.take(1);
    return {id, bytes};
}
class CinepakBits {
  public:
    explicit CinepakBits(VideoReader& reader) : reader_(reader) {}
    bool bit() {
        if (!left_) {
            flags_ = reader_.big(4);
            left_ = 32;
        }
        --left_;
        return (flags_ & (std::uint32_t{1} << left_)) != 0;
    }

  private:
    VideoReader& reader_;
    std::uint32_t flags_{};
    unsigned left_{};
};
} // namespace

void FrontendVideo::close() {
    frames_.clear();
    audio_ = {};
    width_ = height_ = rate_ = scale_ = 0;
    decoded_ = {};
    next_frame_ = 0;
}
double FrontendVideo::duration() const {
    return rate_ ? static_cast<double>(frames_.size()) * scale_ / rate_ : 0;
}
void FrontendVideo::reset_decoder() {
    strips_ = {};
    decoded_ = {};
    decoded_.width = width_;
    decoded_.height = height_;
    decoded_.source_bits = 24;
    decoded_.rgba.resize(static_cast<std::size_t>(width_) * height_ * 4);
    for (std::size_t i = 3; i < decoded_.rgba.size(); i += 4)
        decoded_.rgba[i] = std::byte{255};
    next_frame_ = 0;
}
Result FrontendVideo::open(std::span<const std::byte> bytes, std::string& error) {
    close();
    error.clear();
    try {
        if (bytes.size() > 128 * 1024 * 1024)
            throw std::runtime_error("AVI exceeds frontend size limit");
        VideoReader root(bytes);
        if (!fourcc(root.take(4), "RIFF"))
            throw std::runtime_error("expected RIFF AVI");
        const auto size = root.little(4);
        if (size < 4 || size != root.remaining())
            throw std::runtime_error("invalid RIFF length");
        if (!fourcc(root.take(4), "AVI "))
            throw std::runtime_error("expected AVI form");
        int video_stream = -1, audio_stream = -1, stream_count = 0;
        std::vector<std::span<const std::byte>> movies;
        while (root.remaining()) {
            const auto top = chunk(root);
            if (!fourcc(top.id, "LIST"))
                continue;
            VideoReader list(top.bytes);
            const auto type = list.take(4);
            if (fourcc(type, "movi")) {
                movies.push_back(list.take(list.remaining()));
                continue;
            }
            if (!fourcc(type, "hdrl"))
                continue;
            while (list.remaining()) {
                const auto header = chunk(list);
                if (!fourcc(header.id, "LIST"))
                    continue;
                VideoReader stream(header.bytes);
                if (!fourcc(stream.take(4), "strl"))
                    continue;
                const int stream_index = stream_count++;
                std::span<const std::byte> description, format;
                while (stream.remaining()) {
                    const auto item = chunk(stream);
                    if (fourcc(item.id, "strh"))
                        description = item.bytes;
                    if (fourcc(item.id, "strf"))
                        format = item.bytes;
                }
                VideoReader desc(description);
                const auto kind = desc.take(4);
                if (fourcc(kind, "vids")) {
                    if (video_stream >= 0 || !fourcc(desc.take(4), "cvid"))
                        throw std::runtime_error("frontend AVI requires one Cinepak stream");
                    desc.take(12);
                    scale_ = desc.little(4);
                    rate_ = desc.little(4);
                    VideoReader fmt(format);
                    if (fmt.little(4) < 40)
                        throw std::runtime_error("invalid AVI bitmap header");
                    width_ = fmt.little(4);
                    height_ = fmt.little(4);
                    if (fmt.little(2) != 1 || fmt.little(2) != 24 || !fourcc(fmt.take(4), "cvid"))
                        throw std::runtime_error("unsupported Cinepak pixel format");
                    if (!width_ || !height_ || width_ > 2048 || height_ > 2048 || !scale_ ||
                        !rate_ || rate_ > 1000000 || scale_ > 1000000)
                        throw std::runtime_error("invalid AVI dimensions or time base");
                    video_stream = stream_index;
                } else if (fourcc(kind, "auds")) {
                    if (audio_stream >= 0)
                        throw std::runtime_error("multiple AVI audio streams unsupported");
                    VideoReader fmt(format);
                    audio_.format_tag = static_cast<std::uint16_t>(fmt.little(2));
                    audio_.channels = static_cast<std::uint16_t>(fmt.little(2));
                    audio_.sample_rate = fmt.little(4);
                    fmt.take(4);
                    audio_.block_alignment = static_cast<std::uint16_t>(fmt.little(2));
                    audio_.bits_per_sample = static_cast<std::uint16_t>(fmt.little(2));
                    audio_stream = stream_index;
                }
            }
        }
        if (video_stream < 0 || video_stream > 99)
            throw std::runtime_error("missing AVI video stream");
        const auto read_movie = [&](auto&& self, std::span<const std::byte> payload,
                                    unsigned depth) -> void {
            if (depth > 4)
                throw std::runtime_error("AVI record nesting exceeds limit");
            VideoReader movie(payload);
            while (movie.remaining()) {
                const auto item = chunk(movie);
                if (fourcc(item.id, "LIST")) {
                    VideoReader record(item.bytes);
                    if (!fourcc(record.take(4), "rec "))
                        throw std::runtime_error("unexpected AVI movie list");
                    self(self, record.take(record.remaining()), depth + 1);
                    continue;
                }
                const auto first = std::to_integer<unsigned>(item.id[0]);
                const auto second = std::to_integer<unsigned>(item.id[1]);
                if (first < '0' || first > '9' || second < '0' || second > '9')
                    continue;
                const int index = static_cast<int>((first - '0') * 10 + second - '0');
                if (index == video_stream && std::to_integer<unsigned>(item.id[2]) == 'd' &&
                    (std::to_integer<unsigned>(item.id[3]) == 'c' ||
                     std::to_integer<unsigned>(item.id[3]) == 'b')) {
                    if (frames_.size() >= 10000)
                        throw std::runtime_error("AVI frame count exceeds frontend limit");
                    frames_.emplace_back(item.bytes.begin(), item.bytes.end());
                } else if (index == audio_stream && std::to_integer<unsigned>(item.id[2]) == 'w' &&
                           std::to_integer<unsigned>(item.id[3]) == 'b') {
                    audio_.encoded.insert(audio_.encoded.end(), item.bytes.begin(),
                                          item.bytes.end());
                }
            }
        };
        for (auto movie : movies)
            read_movie(read_movie, movie, 0);
        if (frames_.empty())
            throw std::runtime_error("AVI contains no video frames");
        reset_decoder();
        return Result::ok;
    } catch (const std::exception& exception) {
        close();
        error = exception.what();
        return Result::invalid_argument;
    }
}

Result FrontendVideo::decode_frame(std::span<const std::byte> bytes) {
    if (bytes.empty())
        return Result::ok; // An empty AVI video packet repeats the previous picture.
    try {
        VideoReader frame(bytes);
        const auto flags = frame.byte();
        if (frame.big(3) != bytes.size() || frame.big(2) != width_ || frame.big(2) != height_)
            return Result::invalid_argument;
        const auto count = frame.big(2);
        if (count > strips_.size())
            return Result::invalid_argument;
        unsigned previous_bottom = 0;
        for (unsigned i = 0; i < count; ++i) {
            const auto strip_type = frame.byte();
            const auto strip_size = frame.big(3);
            if ((strip_type != 0x10 && strip_type != 0x11) || strip_size < 12)
                return Result::invalid_argument;
            unsigned top = frame.big(2);
            const unsigned left = frame.big(2);
            unsigned bottom = frame.big(2);
            const unsigned right = frame.big(2);
            if (top == 0) {
                top = previous_bottom;
                bottom += top;
            }
            if (left >= right || top >= bottom || right > width_ || bottom > height_ || left % 4 ||
                top % 4)
                return Result::invalid_argument;
            previous_bottom = bottom;
            auto& strip = strips_[i];
            if (i && !(flags & 1))
                strip = strips_[i - 1];
            VideoReader chunks(frame.take(strip_size - 12));
            bool vectors = false;
            while (chunks.remaining()) {
                const auto type = chunks.byte();
                const auto length = chunks.big(3);
                if (length < 4)
                    return Result::invalid_argument;
                VideoReader data(chunks.take(length - 4));
                if (type >= 0x20 && type <= 0x27) {
                    auto& book = (type & 2) ? strip.large : strip.small;
                    const unsigned color_bytes = (type & 4) ? 4 : 6;
                    CinepakBits bits(data);
                    for (unsigned entry = 0; entry < book.size(); ++entry) {
                        if (!data.remaining() && (!(type & 1) || entry % 32 == 0))
                            break;
                        if ((type & 1) && !bits.bit())
                            continue;
                        std::array<int, 4> luma{};
                        for (auto& value : luma)
                            value = static_cast<int>(data.byte());
                        int u = 0, v = 0;
                        if (color_bytes == 6) {
                            const auto encoded_u = data.byte(), encoded_v = data.byte();
                            u = encoded_u < 128 ? static_cast<int>(encoded_u)
                                                : static_cast<int>(encoded_u) - 256;
                            v = encoded_v < 128 ? static_cast<int>(encoded_v)
                                                : static_cast<int>(encoded_v) - 256;
                        }
                        for (unsigned pixel = 0; pixel < 4; ++pixel) {
                            const auto clamp = [](int value) {
                                return static_cast<std::byte>(std::clamp(value, 0, 255));
                            };
                            book[entry][pixel] = {clamp(luma[pixel] + 2 * v),
                                                  clamp(luma[pixel] - u / 2 - v),
                                                  clamp(luma[pixel] + 2 * u), std::byte{255}};
                        }
                    }
                    if (data.remaining()) {
                        return Result::invalid_argument;
                    }
                } else if (type >= 0x30 && type <= 0x32) {
                    if (vectors)
                        return Result::invalid_argument;
                    vectors = true;
                    CinepakBits bits(data);
                    for (unsigned y = top; y < bottom; y += 4) {
                        for (unsigned x = left; x < right; x += 4) {
                            if (type == 0x31 && !bits.bit())
                                continue;
                            const bool small_vectors = type != 0x32 && bits.bit();
                            std::array<unsigned, 4> indices{};
                            const unsigned vector_count = small_vectors ? 4 : 1;
                            for (unsigned n = 0; n < vector_count; ++n)
                                indices[n] = data.byte();
                            for (unsigned dy = 0; dy < 4 && y + dy < bottom; ++dy) {
                                for (unsigned dx = 0; dx < 4 && x + dx < right; ++dx) {
                                    const auto& color =
                                        small_vectors
                                            ? strip.small[indices[(dy / 2) * 2 + dx / 2]]
                                                         [(dy % 2) * 2 + dx % 2]
                                            : strip.large[indices[0]][(dy / 2) * 2 + dx / 2];
                                    const auto destination =
                                        (static_cast<std::size_t>(y + dy) * width_ + x + dx) * 4;
                                    std::copy(color.begin(), color.end(),
                                              decoded_.rgba.begin() + destination);
                                }
                            }
                        }
                    }
                    // Original AVI packets pad vector chunks to an even byte count;
                    // this alignment byte is not constrained to zero.
                    if (data.remaining() > 1) {
                        return Result::invalid_argument;
                    }
                } else {
                    return Result::unavailable;
                }
            }
            if (!vectors)
                return Result::invalid_argument;
        }
        return frame.remaining() ? Result::invalid_argument : Result::ok;
    } catch (const std::exception&) {
        return Result::invalid_argument;
    }
}
Result FrontendVideo::frame_at(double seconds, Bitmap& output) {
    if (frames_.empty())
        return Result::unavailable;
    if (!std::isfinite(seconds) || seconds < 0)
        return Result::invalid_argument;
    const double frame_number = seconds * rate_ / scale_;
    const auto target = frame_number >= static_cast<double>(frames_.size())
                            ? frames_.size() - 1
                            : static_cast<std::size_t>(frame_number);
    if (target + 1 < next_frame_)
        reset_decoder();
    while (next_frame_ <= target) {
        const auto result = decode_frame(frames_[next_frame_]);
        if (result != Result::ok) {
            reset_decoder();
            return result;
        }
        ++next_frame_;
    }
    output = decoded_;
    return Result::ok;
}
} // namespace roadwars

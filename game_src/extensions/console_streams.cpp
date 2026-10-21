#include "game_src/extensions/console_streams.hpp"
#include <iostream>

namespace roadwars::extensions {
ConsoleStreams::Buffer::Buffer(std::ostream& stream, std::shared_ptr<DebugMessages> messages,
                               bool error)
    : stream_(stream), previous_(stream.rdbuf()), messages_(std::move(messages)), error_(error) {
    stream_.rdbuf(this);
}
ConsoleStreams::Buffer::~Buffer() {
    sync();
    stream_.rdbuf(previous_);
}
void ConsoleStreams::Buffer::append(const char* data, std::size_t size) {
    pending_.append(data, size);
    std::size_t newline;
    while ((newline = pending_.find('\n')) != std::string::npos) {
        messages_->append(pending_.substr(0, newline), error_);
        pending_.erase(0, newline + 1);
    }
    if (pending_.size() > 65536) {
        messages_->append(pending_, error_);
        pending_.clear();
    }
}
std::streamsize ConsoleStreams::Buffer::xsputn(const char* data, std::streamsize size) {
    std::lock_guard lock(mutex_);
    const auto result = previous_->sputn(data, size);
    append(data, static_cast<std::size_t>(size));
    return result;
}
ConsoleStreams::Buffer::int_type ConsoleStreams::Buffer::overflow(int_type value) {
    if (traits_type::eq_int_type(value, traits_type::eof()))
        return traits_type::not_eof(value);
    const char character = traits_type::to_char_type(value);
    return xsputn(&character, 1) == 1 ? value : traits_type::eof();
}
int ConsoleStreams::Buffer::sync() {
    std::lock_guard lock(mutex_);
    if (!pending_.empty()) {
        messages_->append(pending_, error_);
        pending_.clear();
    }
    return previous_->pubsync();
}
ConsoleStreams::ConsoleStreams(std::shared_ptr<DebugMessages> messages)
    : out_(std::cout, messages, false), error_(std::cerr, messages, true),
      log_(std::clog, std::move(messages), false) {}
ConsoleStreams::~ConsoleStreams() = default;
} // namespace roadwars::extensions

#pragma once
#include "game_src/extensions/debug_overlay.hpp"
#include <ostream>
#include <streambuf>

namespace roadwars::extensions {
// Tee C++ console output without changing the host process's native stdout handles.
class ConsoleStreams {
  public:
    explicit ConsoleStreams(std::shared_ptr<DebugMessages> messages);
    ~ConsoleStreams();

  private:
    class Buffer : public std::streambuf {
      public:
        Buffer(std::ostream& stream, std::shared_ptr<DebugMessages> messages, bool error);
        ~Buffer() override;

      protected:
        int_type overflow(int_type value) override;
        std::streamsize xsputn(const char* data, std::streamsize size) override;
        int sync() override;

      private:
        void append(const char* data, std::size_t size);
        std::ostream& stream_;
        std::streambuf* previous_;
        std::shared_ptr<DebugMessages> messages_;
        bool error_;
        std::mutex mutex_;
        std::string pending_;
    };
    Buffer out_, error_, log_;
};
} // namespace roadwars::extensions

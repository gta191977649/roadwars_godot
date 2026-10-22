#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <utility>
#include <godot_cpp/classes/time.hpp>

namespace roadwars {
enum class FrameStage : std::size_t {
    debug_snapshot, overlay, display_poll, dpi_poll, simulation, presentation,
    race_logic, combat_visuals, skid_upload, audio, count
};
enum class FrameResource : std::size_t {
    mesh, instance, texture, material, particles, audio_voice, file_read, ui_item, count
};
struct FrameTraceRecord {
    std::uint64_t frame{}, start_usec{}, interval_usec{}, total_usec{}, ticks{}, probe_usec{};
    std::array<std::uint64_t, static_cast<std::size_t>(FrameStage::count)> stages{};
    std::array<std::uint64_t, static_cast<std::size_t>(FrameResource::count)> resources{};
    std::array<std::int64_t, 5> pipelines{};
    double render_cpu_ms{}, render_gpu_ms{};
    std::string events;
};
inline thread_local FrameTraceRecord* active_frame_trace{};
inline void trace_resource(FrameResource kind, std::string_view event = {}) {
    if (!active_frame_trace)
        return;
    ++active_frame_trace->resources[static_cast<std::size_t>(kind)];
    if (!event.empty() && active_frame_trace->events.size() < 512) {
        active_frame_trace->events.append(event);
        active_frame_trace->events += ';';
    }
}
class FrameStageTimer {
  public:
    explicit FrameStageTimer(FrameStage stage) : record_(active_frame_trace), stage_(stage) {
        if (record_)
            start_ = godot::Time::get_singleton()->get_ticks_usec();
    }
    ~FrameStageTimer() {
        if (record_)
            record_->stages[static_cast<std::size_t>(stage_)] +=
                godot::Time::get_singleton()->get_ticks_usec() - start_;
    }
  private:
    FrameTraceRecord* record_{};
    FrameStage stage_;
    std::uint64_t start_{};
};
class FrameTrace {
  public:
    static constexpr std::size_t kCapacity = 8192;
    void enable() { records_.reserve(kCapacity); enabled_ = true; }
    bool enabled() const { return enabled_; }
    void append(FrameTraceRecord record) {
        if (records_.size() < kCapacity)
            records_.push_back(std::move(record));
        else
            records_[next_] = std::move(record);
        next_ = (next_ + 1) % kCapacity;
    }
    const std::vector<FrameTraceRecord>& records() const { return records_; }
    std::size_t offset() const { return records_.size() == kCapacity ? next_ : 0; }
    std::uint64_t last_start{};
  private:
    bool enabled_{};
    std::vector<FrameTraceRecord> records_;
    std::size_t next_{};
};
// Captures all early returns/exceptions without logging or disk I/O in the loop.
class FrameTraceScope {
  public:
    explicit FrameTraceScope(FrameTrace& trace) : trace_(trace) {
        if (!trace_.enabled())
            return;
        record.start_usec = godot::Time::get_singleton()->get_ticks_usec();
        record.interval_usec = trace_.last_start ? record.start_usec - trace_.last_start : 0;
        trace_.last_start = record.start_usec;
        previous_ = active_frame_trace;
        active_frame_trace = &record;
    }
    ~FrameTraceScope() {
        if (!trace_.enabled())
            return;
        record.total_usec = godot::Time::get_singleton()->get_ticks_usec() - record.start_usec;
        active_frame_trace = previous_;
        trace_.append(std::move(record));
    }
    FrameTraceRecord record;
  private:
    FrameTrace& trace_;
    FrameTraceRecord* previous_{};
};
} // namespace roadwars

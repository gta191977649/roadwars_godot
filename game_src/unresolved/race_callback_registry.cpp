#include "game_src/unresolved/race_callback_registry.hpp"
namespace roadwars {
RaceCallbackRegistry::RaceCallbackRegistry() : cursor_(entries_.end()) {}
void RaceCallbackRegistry::reset() {
    entries_.clear();
    cursor_ = entries_.end();
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004edfc0
bool RaceCallbackRegistry::search(RaceCallbackKey key) {
    cursor_ = entries_.begin();
    while (cursor_ != entries_.end()) {
        if (*cursor_ == key)
            return true;
        auto following = cursor_;
        ++following;
        if (following == entries_.end())
            return false;
        cursor_ = following;
    }
    return false;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004d724f
void RaceCallbackRegistry::insert(RaceCallbackKey key) {
    if (!search(key))
        entries_.push_back(key);
    // Native AddTail does not repair the shared cursor, even on an empty list.
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004d727e
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004eded0
void RaceCallbackRegistry::remove(RaceCallbackKey key) {
    if (!search(key))
        return;
    const auto removed = cursor_;
    if (cursor_ != entries_.begin())
        --cursor_;
    else
        ++cursor_;
    entries_.erase(removed);
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004ede50
void RaceCallbackRegistry::next() {
    if (cursor_ == entries_.end())
        return;
    auto following = cursor_;
    ++following;
    if (following != entries_.end())
        cursor_ = following;
}
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004d72a9
Result RaceCallbackRegistry::dispatch(const std::function<Result(RaceCallbackKey)>& callback,
                                      std::string& error) {
    cursor_ = entries_.begin();
    const auto count = entries_.size();
    for (std::size_t index = 0; index < count; ++index) {
        if (cursor_ == entries_.end()) {
            error = "Original callback list cursor became null during captured-count dispatch";
            return Result::unavailable;
        }
        const auto result = callback(*cursor_);
        if (result != Result::ok)
            return result;
        next();
    }
    return Result::ok;
}
} // namespace roadwars

#include "game_src/extensions/debug_overlay.hpp"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <imgui.h>
#include <imgui_console.h>
#include <imgui_internal.h>

namespace roadwars::extensions {
class OverlayConsole final : public ImGuiConsole {
  public:
    OverlayConsole() : ImGuiConsole("Console") {}
    void append(const DebugMessage& message) {
        System().Log(message.error ? csys::ERROR : csys::LOG) << message.text << csys::endl;
        m_ScrollToBottom = true;
    }
};
void DebugMessages::append(std::string_view text, bool error) {
    if (text.empty())
        return;
    std::lock_guard lock(mutex_);
    DebugMessage message{std::string(text.substr(0, 65536)), error};
    pending_.push_back(message);
    history_.push_back(std::move(message));
    if (pending_.size() > 4096)
        pending_.pop_front();
    if (history_.size() > 4096)
        history_.pop_front();
    ++count_;
}
std::deque<DebugMessage> DebugMessages::drain() {
    std::lock_guard lock(mutex_);
    std::deque<DebugMessage> result;
    result.swap(pending_);
    return result;
}
bool DebugMessages::contains(std::string_view text) const {
    std::lock_guard lock(mutex_);
    return std::any_of(history_.begin(), history_.end(),
                       [&](const auto& m) { return m.text.find(text) != std::string::npos; });
}
std::uint64_t DebugMessages::count() const {
    std::lock_guard lock(mutex_);
    return count_;
}
DebugOverlay::DebugOverlay(std::shared_ptr<DebugMessages> messages, std::vector<unsigned char> font)
    : messages_(std::move(messages)), font_(std::move(font)) {
    context_ = ImGui::CreateContext();
    current();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::GetIO().LogFilename = nullptr;
    console_ = std::make_unique<OverlayConsole>();
    console_->System().RegisterCommand(
        "echo", "Print a message",
        [this](const csys::String& value) { messages_->append(value.m_String); },
        csys::Arg<csys::String>("message"));
    messages_->append(
        "RoadWars debug console ready. ESC toggles mouse capture. Type help for commands.");
}
DebugOverlay::~DebugOverlay() {
    current();
    console_.reset();
    ImGui::DestroyContext(context_);
}
void DebugOverlay::current() const { ImGui::SetCurrentContext(context_); }
ImDrawData* DebugOverlay::frame(float width, float height, float delta, float dpi,
                                std::uint64_t triangles, bool interactive) {
    current();
    auto& io = ImGui::GetIO();
    dpi = std::clamp(dpi, 0.75f, 4.0f);
    if (std::abs(dpi - scale_) > 0.01f) {
        if (scale_ > 0) {
            const float ratio = dpi / scale_;
            for (auto window : context_->Windows) {
                window->Pos.x *= ratio;
                window->Pos.y *= ratio;
                window->SizeFull.x *= ratio;
                window->SizeFull.y *= ratio;
            }
        }
        scale_ = dpi;
        ImGui::GetStyle() = ImGuiStyle{};
        ImGui::StyleColorsDark();
        ImGui::GetStyle().ScaleAllSizes(scale_);
        io.Fonts->Clear();
        ImFontConfig config;
        config.SizePixels = std::floor(16.0f * scale_);
        if (font_.empty())
            io.Fonts->AddFontDefault(&config);
        else {
            config.FontDataOwnedByAtlas = false;
            io.Fonts->AddFontFromMemoryTTF(font_.data(), static_cast<int>(font_.size()),
                                           config.SizePixels, &config);
        }
        io.Fonts->Build();
        io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(1));
        ++font_revision_;
    }
    io.DisplaySize = ImVec2(width, height);
    io.DeltaTime = std::isfinite(delta) && delta > 0 ? delta : 1.0f / 60.0f;
    if (!interactive)
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
    for (const auto& message : messages_->drain())
        console_->append(message);
    auto& items = console_->System().Items();
    if (items.size() > 4096)
        items.erase(items.begin(), items.begin() + (items.size() - 4096));
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(width - 12 * scale_, 12 * scale_), ImGuiCond_Always,
                            ImVec2(1, 0));
    ImGui::SetNextWindowBgAlpha(0.78f);
    ImGui::Begin("Render profile", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::Text("FPS          %.1f", io.Framerate);
    ImGui::Text("Frame time   %.2f ms", delta * 1000.0f);
    ImGui::Text("Triangles    %llu", static_cast<unsigned long long>(triangles));
    ImGui::End();
    ImGui::SetNextWindowPos(ImVec2(12 * scale_, 12 * scale_), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(
        ImVec2(std::min(
                   700 * scale_,
                   std::max(240 * scale_,
                            width - ImGui::CalcTextSize("Frame time   999.99 ms").x - 60 * scale_)),
               std::min(380 * scale_, height - 24 * scale_)),
        ImGuiCond_FirstUseEver);
    if (collapse_requested_) {
        ImGui::SetNextWindowCollapsed(collapse_value_, ImGuiCond_Always);
        collapse_requested_ = false;
    }
    console_->Draw();
    if (auto profile = ImGui::FindWindowByName("Render profile"))
        ImGui::BringWindowToDisplayFront(profile);
    ImGui::Render();
    return ImGui::GetDrawData();
}
void DebugOverlay::mouse_position(float x, float y) {
    current();
    ImGui::GetIO().AddMousePosEvent(x, y);
}
void DebugOverlay::mouse_button(int button, bool down) {
    current();
    ImGui::GetIO().AddMouseButtonEvent(button, down);
}
void DebugOverlay::mouse_wheel(float x, float y) {
    current();
    ImGui::GetIO().AddMouseWheelEvent(x, y);
}
void DebugOverlay::key(int key, bool down) {
    current();
    if (key)
        ImGui::GetIO().AddKeyEvent(static_cast<ImGuiKey>(key), down);
}
void DebugOverlay::text(std::uint32_t character) {
    current();
    if (character)
        ImGui::GetIO().AddInputCharacter(character);
}
void DebugOverlay::focus(bool focused) {
    current();
    ImGui::GetIO().AddFocusEvent(focused);
}
bool DebugOverlay::hit_test(float x, float y) const {
    current();
    for (const auto window : context_->Windows)
        if (window->WasActive && !(window->Flags & ImGuiWindowFlags_NoInputs) &&
            window->OuterRectClipped.Contains(ImVec2(x, y)))
            return true;
    return false;
}
void DebugOverlay::set_collapsed(bool value) {
    collapse_requested_ = true;
    collapse_value_ = value;
}
bool DebugOverlay::collapsed() const {
    current();
    const auto window = ImGui::FindWindowByName("Console");
    return window ? window->Collapsed : collapse_value_;
}
std::array<float, 2> DebugOverlay::console_input_position() const {
    current();
    const auto window = ImGui::FindWindowByName("Console");
    if (!window)
        return {};
    return {window->Pos.x + ImGui::GetStyle().WindowPadding.x + 20 * scale_,
            window->Pos.y + window->SizeFull.y - ImGui::GetStyle().WindowPadding.y -
                ImGui::GetFrameHeight() * 0.5f};
}
const unsigned char* DebugOverlay::font_pixels(int& width, int& height) {
    current();
    unsigned char* pixels;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    return pixels;
}
} // namespace roadwars::extensions

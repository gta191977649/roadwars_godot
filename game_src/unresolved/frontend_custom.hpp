#pragma once
#include "game_src/unresolved/frontend_canvas.hpp"
#include <string>

namespace roadwars {
// Explicit presentation inputs. Preview values are never race or network state.
struct FrontendOptionRow {
    std::string label;
    std::vector<std::string> values;
    std::size_t selected{};
    bool available{true};
    std::string id{}; // Stable modern option identifier; independent of display text.
};
struct FrontendPreviewRacer {
    std::string character, name;
    int points{}, cash{};
    bool finished{true};
    std::string total_time;
};
struct FrontendCustomState {
    std::string screen, variant;
    std::size_t selected{};
    double elapsed{};
    std::vector<FrontendOptionRow> rows;
    std::vector<FrontendPreviewRacer> racers;
    std::string track{"Dark Forest"}, message;
    std::string best_time{"NO RECORD"}, best_lap{"NO RECORD"};
    std::map<std::string, std::string> fields;
    float loading_progress{};
    bool preview_data{true};
};
Result frontend_custom_rows(ResourceStore&, std::string_view screen,
                            std::vector<FrontendOptionRow>&, std::string& error,
                            std::string_view variant = {}, std::size_t item_index = 0);
class FrontendCustom {
  public:
    static bool supports(std::string_view screen);
    static Result draw(FrontendCanvas&, const FrontendCustomState&, std::string& error);
};
} // namespace roadwars

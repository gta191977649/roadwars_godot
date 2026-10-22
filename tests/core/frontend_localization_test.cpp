#include "game_src/unresolved/frontend_localization.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace roadwars;
namespace {
std::vector<std::byte> encode(std::string_view text) {
    std::vector<std::byte> output;
    for (unsigned char c : text)
        output.push_back(static_cast<std::byte>((c - 1) & 255));
    return output;
}
class Files final : public ResourceReader {
  public:
    explicit Files(std::filesystem::path root) : root_(std::move(root)) {}
    Result read(std::string_view name, std::vector<std::byte>& output) override {
        std::ifstream file(root_ / std::filesystem::path(name), std::ios::binary | std::ios::ate);
        if (!file)
            return Result::not_found;
        const auto size = file.tellg();
        if (size < 0)
            return Result::invalid_argument;
        output.resize(static_cast<std::size_t>(size));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(output.data()), size);
        return file ? Result::ok : Result::invalid_argument;
    }

  private:
    std::filesystem::path root_;
};
} // namespace
int main(int argc, char** argv) {
    FrontendLocalization table;
    std::string error, output;
    // Assembly-derived observations: +1 byte transform, case-sensitive keys,
    // quotes removed, first duplicate wins, language zero preserves the supplied key.
    auto bytes = encode("SELECT GAME\t\"SPIEL W\xc4HLEN\"\tKIES SPEL\n"
                        "SELECT GAME\tSECOND\tSECOND\n"
                        "EMPTY\tFIRST\t\"\"\tTHIRD\n");
    assert(table.decode(bytes, error) == Result::ok);
    assert(table.size() == 2);
    assert(table.lookup("SELECT GAME", 1, output) == Result::ok && output == "SPIEL W\xc4HLEN");
    assert(table.lookup("SELECT GAME", 2, output) == Result::ok && output == "KIES SPEL");
    assert(table.lookup("select game", 1, output) == Result::not_found && output == "select game");
    assert(table.lookup("unknown", 0, output) == Result::ok && output == "unknown");
    assert(table.lookup("EMPTY", 2, output) == Result::ok && output == "FIRST");
    assert(table.lookup("EMPTY", 3, output) == Result::ok && output == "THIRD");
    assert(table.lookup("EMPTY", 4, output) == Result::not_found && output == "EMPTY");
    assert(table.lookup("EMPTY", -1, output) == Result::invalid_argument);
    assert(table.lookup("EMPTY", 32, output) == Result::invalid_argument);
    assert(table.decode({}, error) == Result::invalid_argument);
    assert(table.decode(encode("NO TAB\n"), error) == Result::invalid_argument);
    assert(table.decode(encode("\tMISSING KEY\n"), error) == Result::invalid_argument);
    assert(table.decode(encode("KEY\t" + std::string(512, 'A') + "\n"), error) ==
           Result::invalid_argument);
    bytes = encode("KEY\tVALUE");
    bytes.push_back(std::byte{255}); // Decoded NUL must not become an unbounded C string.
    assert(table.decode(bytes, error) == Result::invalid_argument);
    // Failed replacements retain the last usable table transactionally.
    assert(table.lookup("SELECT GAME", 2, output) == Result::ok && output == "KIES SPEL");

    assert(frontend_title_alphabet().size() == 56);
    assert(frontend_text_alphabet().size() == 138);
    assert(frontend_option_alphabet().size() == 140);
    assert(frontend_title_alphabet()[26] == 0xc4);
    assert(frontend_text_alphabet()[52] == '.');
    assert(frontend_option_alphabet()[52] == '1');

    if (argc > 1) {
        Files files(argv[1]);
        ResourceStore resources(files);
        assert(resources.open() == Result::ok);
        assert(table.load(resources, "../Misc", error) == Result::invalid_argument);
        for (const auto* name : {"ScreenTitles", "ScreenIconLabels", "Misc", "Option", "Option2",
                                 "Garage", "GarageDescriptions", "Config"}) {
            assert(table.load(resources, name, error) == Result::ok);
            assert(table.size());
        }
        assert(table.load(resources, "ScreenTitles", error) == Result::ok);
        assert(table.lookup("SELECT GAME", 1, output) == Result::ok && output == "SPIEL W\xc4HLEN");
        assert(table.lookup("SELECT GAME", 6, output) == Result::ok &&
               output == "SELECCIONAR JOGO");
        std::cout << "Original localization assets passed\n";
    }
    std::cout << "Localization format and boundary tests passed\n";
}

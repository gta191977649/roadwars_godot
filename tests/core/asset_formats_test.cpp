#include "game_src/unresolved/bitmap.hpp"
#include "game_src/unresolved/resource_store.hpp"
#include <map>
#include <stdexcept>
using namespace roadwars;
namespace {
void require(bool value) {
    if (!value)
        throw std::runtime_error("Asset format contract failed");
}
void u32(std::vector<std::byte>& out, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        out.push_back(std::byte((value >> shift) & 255));
}
class MemoryReader final : public ResourceReader {
  public:
    std::map<std::string, std::vector<std::byte>> files;
    Result read(std::string_view name, std::vector<std::byte>& out) override {
        auto found = files.find(std::string(name));
        if (found == files.end())
            return Result::not_found;
        out = found->second;
        return Result::ok;
    }
};
} // namespace
int main() {
    MemoryReader reader;
    auto& index = reader.files["Data.pkh"];
    const std::string name = "Mixed/Case.bin";
    u32(index, static_cast<std::uint32_t>(name.size() + 1));
    for (unsigned char character : name)
        index.push_back(std::byte(character + 1));
    index.push_back(std::byte{});
    u32(index, 1);
    u32(index, 0);
    u32(index, 2);
    reader.files["Data.pak"] = {std::byte{9}, std::byte{42}, std::byte{43}};
    reader.files[name] = {std::byte{99}};
    ResourceStore store(reader);
    require(store.open() == Result::ok);
    std::vector<std::byte> data;
    require(store.read("mixed\\CASE.bin", data) == Result::ok);
    require(data.size() == 2 && data[0] == std::byte{42});
    require(store.read("../escape", data) == Result::invalid_argument);
    require(store.read("C:/absolute", data) == Result::invalid_argument);
    index.pop_back();
    require(store.open() == Result::invalid_argument);
    // Failed refresh preserves the previous valid archive transactionally.
    require(store.read(name, data) == Result::ok && data.size() == 2);

    std::vector<std::byte> bmp{std::byte{'B'}, std::byte{'M'}};
    u32(bmp, 58);
    u32(bmp, 0);
    u32(bmp, 54);
    u32(bmp, 40);
    u32(bmp, 1);
    u32(bmp, 1);
    u32(bmp, 0x180001);
    u32(bmp, 0);
    u32(bmp, 4);
    u32(bmp, 0);
    u32(bmp, 0);
    u32(bmp, 0);
    u32(bmp, 0);
    bmp.insert(bmp.end(), {std::byte{3}, std::byte{2}, std::byte{1}, std::byte{0}});
    Bitmap image;
    require(decode_bitmap(bmp, image) == Result::ok);
    require(image.width == 1 && image.height == 1 && image.source_bits == 24);
    require(image.rgba ==
            std::vector<std::byte>({std::byte{1}, std::byte{2}, std::byte{3}, std::byte{255}}));
    bmp.pop_back();
    require(decode_bitmap(bmp, image) == Result::invalid_argument);
    require(image.rgba[0] == std::byte{1});
}

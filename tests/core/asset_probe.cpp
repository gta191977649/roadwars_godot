#include "game_src/unresolved/mesh_scene.hpp"
#include "game_src/unresolved/resource_store.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace roadwars;
class DirectoryReader final : public ResourceReader {
  public:
    explicit DirectoryReader(std::filesystem::path root) : root_(std::move(root)) {}
    Result read(std::string_view path, std::vector<std::byte>& output) override {
        std::ifstream file(root_ / std::filesystem::path(path), std::ios::binary | std::ios::ate);
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
void counts(const MeshObject& object, std::size_t& nodes, std::size_t& vertices) {
    ++nodes;
    for (const auto& part : object.parts)
        vertices += part.positions.size();
    for (const auto& child : object.children)
        counts(child, nodes, vertices);
}
int main(int argc, char** argv) {
    if (argc < 2)
        return 2;
    DirectoryReader reader(argv[1]);
    ResourceStore store(reader);
    if (store.open() != Result::ok) {
        std::cerr << "Archive open failed\n";
        return 1;
    }
    std::cout << "Archive entries: " << store.entries().size() << '\n';
    for (int index = 2; index < argc; ++index) {
        std::vector<std::byte> data;
        if (store.read(argv[index], data) != Result::ok) {
            std::cerr << "Missing: " << argv[index] << '\n';
            return 1;
        }
        MeshObject scene;
        std::string error;
        if (decode_msh(data, scene, error) != Result::ok) {
            std::cerr << argv[index] << ": " << error << '\n';
            return 1;
        }
        std::size_t nodes{}, vertices{};
        counts(scene, nodes, vertices);
        std::cout << argv[index] << ": " << nodes << " objects, " << vertices << " vertices\n";
    }
}

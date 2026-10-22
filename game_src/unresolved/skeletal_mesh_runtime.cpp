#include "game_src/unresolved/skeletal_mesh_runtime.hpp"
#include "game_src/unresolved/binary_reader.hpp"
#include <cmath>
#include <functional>

namespace roadwars {
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004ee8c6
// The original skeletal world matrix; Direct3D material/state submission is platform-owned.
Transform original_skeletal_transform(Vec3 origin, float yaw, float pitch, float roll, Vec3 scale) {
    using Matrix = std::array<std::array<float, 3>, 3>;
    const auto multiply = [](const Matrix& a, const Matrix& b) {
        Matrix out{};
        for (unsigned i = 0; i < 3; ++i)
            for (unsigned j = 0; j < 3; ++j)
                for (unsigned k = 0; k < 3; ++k)
                    out[i][j] += a[i][k] * b[k][j];
        return out;
    };
    constexpr float radians = .017455555498600006f;
    const float cy = std::cos(yaw * radians), sy = std::sin(yaw * radians);
    const float cx = std::cos(pitch * radians), sx = std::sin(pitch * radians);
    const float cz = std::cos(roll * radians), sz = std::sin(roll * radians);
    const Matrix y{{{cy, 0, -sy}, {0, 1, 0}, {sy, 0, cy}}};
    const Matrix x{{{1, 0, 0}, {0, cx, sx}, {0, -sx, cx}}};
    const Matrix z{{{cz, sz, 0}, {-sz, cz, 0}, {0, 0, 1}}};
    const Matrix scaling{{{scale.x, 0, 0}, {0, scale.y, 0}, {0, 0, scale.z}}};
    // Native004d74e3(dest,A,B) computes B*A. Its draw calls yield Scale*Z*X*Y.
    const auto matrix = multiply(scaling, multiply(z, multiply(x, y)));
    Transform result;
    result.origin = origin;
    for (unsigned i = 0; i < 3; ++i)
        result.basis[i] = {matrix[i][0], matrix[i][1], matrix[i][2]};
    return result;
}
namespace {
Vec3 read_vector(BinaryReader& reader) { return {reader.f32(), reader.f32(), reader.f32()}; }
bool finite(Vec3 p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
Vec3 apply(const std::array<float, 16>& matrix, Vec3 p, bool position) {
    // Preserve original004ef50d product/addition ordering and row-vector serialized matrix.
    const Vec3 result{p.z * matrix[8] + p.y * matrix[4] + p.x * matrix[0],
                      p.z * matrix[9] + p.y * matrix[5] + p.x * matrix[1],
                      p.z * matrix[10] + p.y * matrix[6] + p.x * matrix[2]};
    return position ? Vec3{result.x + matrix[12], result.y + matrix[13], result.z + matrix[14]}
                    : result;
}
} // namespace
Result SkeletalMeshRuntime::load(ResourceStore& resources, std::string_view mesh_path,
                                 std::string_view animation_path, std::string& error) {
    std::vector<std::byte> mesh, animation;
    if (resources.read(mesh_path, mesh) != Result::ok ||
        resources.read(animation_path, animation) != Result::ok) {
        error = "Original skeletal mesh/animation resource unavailable";
        return Result::unavailable;
    }
    return decode(mesh, animation, error);
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004eee40
// Serialized vertex/index/texture subset, not the native Direct3D allocation.
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004eef32
// Serialized bone hierarchy with original preorder flattening.
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004ee4ac
// One actual animation file; multi-animation ownership is not claimed here.
Result SkeletalMeshRuntime::decode(std::span<const std::byte> mesh,
                                   std::span<const std::byte> animation, std::string& error) {
    try {
        SkeletalMeshRuntime next;
        BinaryReader reader(mesh);
        const auto count = reader.u32();
        if (count == 0 || count > 65536 || count > reader.remaining() / 32)
            throw std::runtime_error("Invalid SKA vertex count");
        for (std::uint32_t i = 0; i < count; ++i) {
            const auto position = read_vector(reader), normal = read_vector(reader);
            const Vec2 uv{reader.f32(), reader.f32()};
            if (!finite(position) || !finite(normal) || !std::isfinite(uv.x) ||
                !std::isfinite(uv.y))
                throw std::runtime_error("Nonfinite SKA vertex");
            next.original_.positions.push_back(position);
            next.original_.normals.push_back(normal);
            next.original_.uv.push_back(uv);
        }
        const auto triangles = reader.u32();
        if (triangles == 0 || triangles > reader.remaining() / 6)
            throw std::runtime_error("Invalid SKA triangle count");
        for (std::uint64_t i = 0; i < static_cast<std::uint64_t>(triangles) * 3; ++i) {
            const auto index = reader.u16();
            if (index >= count)
                throw std::runtime_error("SKA triangle index outside vertices");
            next.original_.indices.push_back(index);
        }
        next.original_.texture = reader.text(64);
        std::vector<bool> assigned(count);
        const std::function<void(unsigned)> bone = [&](unsigned depth) {
            if (depth > 128 || next.bones_.size() >= 4096)
                throw std::runtime_error("SKA bone hierarchy limit exceeded");
            Bone current;
            const auto vertices = reader.u32();
            if (vertices > count || vertices > reader.remaining() / 2)
                throw std::runtime_error("Invalid SKA bone vertex count");
            for (std::uint32_t i = 0; i < vertices; ++i) {
                const auto index = reader.u16();
                if (index >= count)
                    throw std::runtime_error("SKA bone vertex index outside vertices");
                current.vertices.push_back(index);
                assigned[index] = true;
            }
            next.bones_.push_back(std::move(current));
            const auto children = reader.u32();
            if (children > 4096)
                throw std::runtime_error("Invalid SKA child count");
            for (std::uint32_t i = 0; i < children; ++i)
                bone(depth + 1);
        };
        bone(0);
        if (reader.remaining() != 0)
            throw std::runtime_error("Unexpected SKA mesh trailing bytes");
        for (const bool covered : assigned)
            if (!covered)
                throw std::runtime_error("SKA unassigned vertex has undefined native output");
        BinaryReader motion(animation);
        next.frames_ = motion.u32();
        const auto frames_per_second = motion.u32();
        if (next.frames_ == 0 || frames_per_second == 0 || next.frames_ > 100000 ||
            next.frames_ > motion.remaining() / 64 / next.bones_.size())
            throw std::runtime_error("Invalid SKA animation count/rate");
        next.period_ = 1.0f / static_cast<float>(frames_per_second);
        next.matrices_.resize(next.frames_ * next.bones_.size());
        for (auto& matrix : next.matrices_)
            for (auto& value : matrix) {
                value = motion.f32();
                if (!std::isfinite(value))
                    throw std::runtime_error("Nonfinite SKA animation matrix");
            }
        if (motion.remaining() != 0)
            throw std::runtime_error("Unexpected SKA animation trailing bytes");
        next.transformed_ = next.original_;
        next.transform();
        *this = std::move(next);
        return Result::ok;
    } catch (const std::exception& exception) {
        error = exception.what();
        return Result::invalid_argument;
    }
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004ee6b5
// Recovered single looping animation timing; other native animation-mode transitions are separate.
Result SkeletalMeshRuntime::advance(float delta) {
    if (frames_ == 0)
        return Result::unavailable;
    if (!std::isfinite(delta) || delta < 0 || !std::isfinite(elapsed_ + delta))
        return Result::invalid_argument;
    elapsed_ += delta;
    if (period_ <= elapsed_) {
        elapsed_ = 0; // Original drops the remainder and advances at most one frame per callback.
        frame_ = (frame_ + 1) % frames_;
        transform();
    }
    return Result::ok;
}
void SkeletalMeshRuntime::reset() {
    elapsed_ = 0;
    frame_ = 0;
    if (frames_)
        transform();
}
// RE-STUB: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004ee7fc
// Recovered direct preorder bone application, not a conventional parent-composed skeleton.
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004ef50d
void SkeletalMeshRuntime::transform() {
    for (std::size_t bone = 0; bone < bones_.size(); ++bone) {
        const auto& matrix = matrices_[bone * frames_ + frame_];
        for (const auto vertex : bones_[bone].vertices) {
            transformed_.positions[vertex] = apply(matrix, original_.positions[vertex], true);
            transformed_.normals[vertex] = apply(matrix, original_.normals[vertex], false);
        }
    }
}
} // namespace roadwars

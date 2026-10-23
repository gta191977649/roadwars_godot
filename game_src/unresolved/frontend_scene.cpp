#include "game_src/unresolved/frontend_scene.hpp"
#include "game_src/unresolved/chrome_mapping.hpp"
#include "game_src/unresolved/binary_reader.hpp"
#include "game_src/unresolved/bitmap.hpp"
#include "game_src/unresolved/contact_orientation.hpp"
#include "game_src/unresolved/original_camera.hpp"
#include "game_src/unresolved/track_animation.hpp"
#include "game_src/unresolved/weapon_mounts.hpp"
#include "game_src/unresolved/wheel_pose.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <stdexcept>

namespace roadwars {
namespace {
constexpr float kRadians = 0.017455555498600006f;
constexpr float kDegrees = 57.288352966308594f;
Vec3 add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 subtract(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 scale(Vec3 a, float b) { return {a.x * b, a.y * b, a.z * b}; }
float length(Vec3 a) { return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z); }
Vec3 normalized(Vec3 a) {
    const auto size = length(a);
    return size == 0 ? Vec3{} : scale(a, 1 / size);
}
Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
Vec3 vector(BinaryReader& reader) { return {reader.f32(), reader.f32(), reader.f32()}; }
Vec3 reflect(Vec3 value) { return {-value.x, value.y, -value.z}; }
struct Face {
    std::array<std::uint32_t, 3> vertices;
    std::uint32_t material;
    std::array<Vec2, 3> uv;
};
MeshObject read_d3d_object(BinaryReader& stream, unsigned depth, std::size_t& total_vertices) {
    if (depth > 128)
        throw std::runtime_error("D3D hierarchy exceeds depth limit");
    MeshObject object;
    object.name = stream.text(32);
    std::from_chars(object.name.data(), object.name.data() + object.name.size(), object.flags);
    object.type = stream.u32();
    const auto children = stream.u32();
    stream.u32(); // Exporter field read into local_14 but not used by 004d7c7b.
    if (children > 100000)
        throw std::runtime_error("Invalid D3D child count");
    if (object.type == 4) {
        vector(stream);
        const auto forward = reflect(vector(stream));
        vector(stream);
        object.position = reflect(vector(stream));
        if (length(forward) == 0)
            throw std::runtime_error("Invalid D3D dummy orientation");
        object.dummy_heading_degrees =
            std::acos(std::clamp(forward.z / length(forward), -1.0f, 1.0f)) * kDegrees;
        if (forward.x < 0)
            object.dummy_heading_degrees = 360 - object.dummy_heading_degrees;
    } else if (object.type == 1) {
        stream.u32(); // TAG.
        BinaryReader header(stream.take(64));
        header.take(8);
        const auto material_count = header.u32();
        const auto vertex_count = header.u32();
        const auto face_count = header.u32();
        header.take(32);
        object.position = reflect(vector(header));
        if (material_count > 4096 || vertex_count > 65536 || face_count > 500000 ||
            total_vertices + static_cast<std::size_t>(face_count) * 3 > 10000000)
            throw std::runtime_error("Invalid D3D mesh counts");
        total_vertices += static_cast<std::size_t>(face_count) * 3;
        std::vector<MeshPart> parts;
        for (std::uint32_t i = 0; i < material_count; ++i) {
            BinaryReader material(stream.take(80));
            MeshPart part;
            part.texture = material.text(32);
            material.take(8);
            part.color[0] = material.f32();
            part.color[1] = material.f32();
            part.color[2] = material.f32();
            material.take(24);
            part.color[3] = 1 - material.f32();
            parts.push_back(std::move(part));
        }
        std::vector<Vec3> vertices, normals(vertex_count);
        for (std::uint32_t i = 0; i < vertex_count; ++i)
            vertices.push_back(vector(stream));
        stream.take(static_cast<std::size_t>(vertex_count) * 12); // Recomputed by the loader.
        std::vector<Face> faces;
        for (std::uint32_t i = 0; i < face_count; ++i) {
            Face face;
            for (auto& index : face.vertices) {
                index = stream.u32();
                if (index >= vertices.size())
                    throw std::runtime_error("D3D face vertex outside array");
            }
            face.material = stream.u32();
            if (face.material >= parts.size())
                throw std::runtime_error("D3D face material outside array");
            for (auto& uv : face.uv)
                uv = {stream.f32(), stream.f32()};
            const auto normal =
                normalized(cross(subtract(vertices[face.vertices[0]], vertices[face.vertices[1]]),
                                 subtract(vertices[face.vertices[0]], vertices[face.vertices[2]])));
            for (auto index : face.vertices)
                normals[index] = add(normals[index], normal);
            faces.push_back(face);
        }
        for (auto& normal : normals)
            normal = normalized(normal);
        for (const auto& face : faces) {
            auto& part = parts[face.material];
            for (std::size_t corner = 0; corner < 3; ++corner) {
                const auto index = face.vertices[corner];
                part.indices.push_back(static_cast<std::uint32_t>(part.positions.size()));
                part.positions.push_back(reflect(vertices[index]));
                part.normals.push_back(reflect(normals[index]));
                part.uv.push_back(face.uv[corner]);
                part.colors.push_back({1, 1, 1, 1});
            }
        }
        for (auto& part : parts)
            if (!part.indices.empty())
                object.parts.push_back(std::move(part));
    } else if (object.type != 0) {
        throw std::runtime_error("Unsupported D3D object type " + std::to_string(object.type));
    }
    for (std::uint32_t i = 0; i < children; ++i)
        object.children.push_back(read_d3d_object(stream, depth + 1, total_vertices));
    return object;
}
bool locate(const MeshObject& object, std::string_view name, Vec3& position, Vec3 parent = {}) {
    parent = add(parent, object.position);
    if (object.name == name) {
        position = parent;
        return true;
    }
    for (const auto& child : object.children)
        if (locate(child, name, position, parent))
            return true;
    return false;
}
Vec3 require_anchor(const MeshObject& object, std::string_view name) {
    Vec3 result;
    if (!locate(object, name, result))
        throw std::runtime_error("Original frontend anchor unavailable: " + std::string(name));
    return result;
}
float require_number(const Configuration& config, std::string_view section, std::string_view key) {
    float value{};
    if (config.number(section, key, value) != Result::ok)
        throw std::runtime_error("Original frontend setting unavailable: " + std::string(key));
    return value;
}
const MeshObject* named(const MeshObject& object, std::string_view name) {
    if (object.name == name)
        return &object;
    for (const auto& child : object.children)
        if (const auto* found = named(child, name))
            return found;
    return nullptr;
}
Vec3 mesh_size(const MeshObject& object) {
    Vec3 lower{1e20f, 1e20f, 1e20f}, upper{-1e20f, -1e20f, -1e20f};
    for (const auto& part : object.parts)
        for (const auto p : part.positions) {
            lower = {std::min(lower.x, p.x), std::min(lower.y, p.y), std::min(lower.z, p.z)};
            upper = {std::max(upper.x, p.x), std::max(upper.y, p.y), std::max(upper.z, p.z)};
        }
    if (lower.x > upper.x)
        throw std::runtime_error("Frontend mesh bounds unavailable");
    return subtract(upper, lower);
}
} // namespace

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004d7c7b
// Serialized fallback and normal recovery; legacy GPU allocation/caches are not reconstructed.
Result decode_frontend_d3d(std::span<const std::byte> bytes, MeshObject& output,
                           std::string& error) {
    try {
        BinaryReader stream(bytes);
        const auto count = stream.u32();
        if (!count || count > 100000)
            throw std::runtime_error("Invalid D3D root count");
        MeshObject result;
        std::size_t vertices{};
        if (count == 1)
            result = read_d3d_object(stream, 0, vertices);
        else
            for (std::uint32_t i = 0; i < count; ++i)
                result.children.push_back(read_d3d_object(stream, 0, vertices));
        if (stream.remaining())
            throw std::runtime_error("Trailing D3D bytes");
        output = std::move(result);
        error.clear();
        return Result::ok;
    } catch (const std::exception& exception) {
        error = exception.what();
        return Result::invalid_argument;
    }
}

FrontendScene::~FrontendScene() { stop(); }
void FrontendScene::stop() {
    chrome_texture_ = {};
    chrome_enabled_ = false;
    for (const auto& draw : draws_)
        renderer_.destroy_instance(draw.instance);
    draws_.clear();
    for (const auto mesh : meshes_)
        renderer_.destroy_mesh(mesh);
    meshes_.clear();
    for (const auto& [key, material] : materials_)
        renderer_.destroy_material(material);
    materials_.clear();
    for (const auto texture : textures_)
        renderer_.destroy_texture(texture);
    textures_.clear();
    cameras_.clear();
    anchors_.clear();
    vehicle_angles_.clear();
    vehicle_heights_.clear();
    sky_angles_.clear();
    loaded_ = false;
    elapsed_ = debt_ = 0;
    camera_index_ = 0;
    camera_seconds_ = 0;
    camera_heading_ = camera_pitch_ = heading_speed_ = 0;
    victory_finished_ = false;
    eye_ = velocity_ = target_ = stand_ = {};
    product_anchor_ = product_angles_ = product_speeds_ = {};
    character_.clear();
    product_draw_begin_ = product_mesh_begin_ = 0;
}
Result FrontendScene::read_mesh(const std::string& path, MeshObject& output, std::string& error) {
    std::vector<std::byte> bytes;
    auto result = resources_.read(path + ".msh", bytes);
    if (result == Result::ok)
        return decode_msh(bytes, output, error);
    if (result != Result::not_found)
        return result;
    result = resources_.read(path + ".D3D", bytes);
    if (result != Result::ok) {
        error = "Original scene mesh missing: " + path;
        return result;
    }
    return decode_frontend_d3d(bytes, output, error);
}

MaterialHandle FrontendScene::material(const MeshPart& part, const std::string& base,
                                       std::uint32_t flags, bool chrome) {
    auto key = ResourceStore::canonical(base + "/" + part.texture) + ":" + std::to_string(flags);
    key += ":chrome=" + std::to_string(chrome ? chrome_texture_.value : 0);
    for (const auto channel : part.color)
        key += ":" + std::to_string(std::bit_cast<std::uint32_t>(channel));
    if (const auto found = materials_.find(key); found != materials_.end())
        return found->second;
    Bitmap bitmap, alpha;
    TextureTransparency transparency{TextureTransparency::opaque};
    if (part.texture.empty()) {
        bitmap.width = bitmap.height = 1;
        bitmap.rgba.assign(4, std::byte{255});
    } else {
        std::vector<std::string> candidates{base + "/" + part.texture,
                                            base + "/Textures/" + part.texture};
        if (base.starts_with("Characters/"))
            candidates.push_back("Characters/Shared/Garage/" + part.texture);
        if (base.starts_with("Cars/")) {
            candidates.push_back("Cars/Shared/Textures/" + part.texture);
            const auto context = ResourceStore::canonical(base.substr(0, base.find_last_of('/')));
            const auto suffix = "/" + ResourceStore::canonical(part.texture);
            for (const auto& [path, entry] : resources_.entries())
                if (path.ends_with(suffix) &&
                    (path.starts_with(context + "/") || path.starts_with("CARS/SHARED/")))
                    candidates.push_back(path);
        }
        std::vector<std::byte> bytes;
        std::string resolved;
        for (const auto& path : candidates) {
            if (resources_.read(path, bytes) == Result::ok) {
                resolved = path;
                break;
            }
        }
        if (resolved.empty() || decode_bitmap(bytes, bitmap) != Result::ok)
            throw std::runtime_error("Original frontend texture missing or malformed: " + key);
        const auto companion = original_alpha_path(resolved);
        if (!companion.empty() && (resources_.read(companion, bytes) != Result::ok ||
                                   decode_bitmap(bytes, alpha) != Result::ok))
            throw std::runtime_error("Original frontend alpha missing: " + companion);
        if (prepare_original_texture(resolved, bitmap, transparency,
                                     companion.empty() ? nullptr : &alpha) != Result::ok)
            throw std::runtime_error("Original frontend texture conversion failed: " + resolved);
    }
    const auto texture = renderer_.upload_texture(bitmap.width, bitmap.height, bitmap.rgba);
    if (!texture.value)
        throw std::runtime_error("Frontend scene texture upload failed");
    textures_.push_back(texture);
    MaterialSettings settings;
    settings.color =
        part.texture.empty() ? part.color : std::array<float, 4>{1, 1, 1, part.color[3]};
    settings.unshaded = (flags & 4) != 0;
    settings.billboard_y = (flags & 0x40) != 0;
    settings.depth_write = (flags & 1) == 0;
    settings.depth_test = (flags & 0x10) == 0;
    settings.fog = false;
    if (chrome)
        settings.chrome_map = chrome_texture_;
    settings.opaque_alpha_only = (flags & 0x80) != 0;
    if (flags & 2)
        settings.alpha_mode = AlphaMode::additive;
    else if ((flags & 0x20) || transparency == TextureTransparency::alpha_blend ||
             settings.color[3] < 1)
        settings.alpha_mode = AlphaMode::blend;
    else if (transparency == TextureTransparency::color_key)
        settings.alpha_mode = AlphaMode::cutout;
    const auto handle = renderer_.create_material(texture, settings);
    if (!handle.value)
        throw std::runtime_error("Frontend scene material upload failed");
    materials_.emplace(key, handle);
    return handle;
}

void FrontendScene::upload(const MeshObject& object, const std::string& base, Vec3 parent,
                           int vehicle, int turntable, int sky) {
    if (!object.visible)
        return;
    const auto position = add(parent, object.position);
    if (victory_ && vehicle < 0) {
        for (int index = 0; index < 3; ++index)
            if (object.name.starts_with("Mount" + std::to_string(index + 1) + "_"))
                turntable = index;
        if (object.name.starts_with("12ULSky1_"))
            sky = 0;
        if (object.name.starts_with("12ULSky2_"))
            sky = 1;
    }
    for (std::size_t part_index = 0; part_index < object.parts.size(); ++part_index) {
        const auto& part = object.parts[part_index];
        if (part.indices.empty())
            continue;
        std::vector<std::array<float, 2>> uv;
        for (const auto coordinate : part.uv)
            uv.push_back({coordinate.x, coordinate.y});
        const auto mesh =
            renderer_.upload_surface(part.positions, uv, part.indices,
                                     material(part, base, object.flags,
                                         chrome_mapping_applies(chrome_enabled_, object.flags,
                                             part_index, vehicle >= 0 && chrome_texture_.value)),
                                     part.normals, part.colors);
        if (!mesh.value)
            throw std::runtime_error("Frontend scene mesh upload failed");
        meshes_.push_back(mesh);
        const auto instance = renderer_.create_instance(mesh);
        if (!instance.value)
            throw std::runtime_error("Frontend scene instance upload failed");
        Transform local;
        local.origin = position;
        draws_.push_back({instance, local, vehicle, turntable, sky});
        renderer_.set_instance_static(instance, vehicle < 0 && turntable < 0 && sky < 0);
        if (vehicle >= 0 && object.name == "295ULTailL") {
            draws_.back().visible = false;
            renderer_.set_instance_visible(instance, false);
        }
    }
    for (const auto& child : object.children)
        upload(child, base, position, vehicle, turntable, sky);
}

void FrontendScene::upload_vehicle(std::string_view name, Vec3 anchor, int index) {
    MeshObject body, tire;
    std::string error;
    const auto path = "Cars/" + std::string(name);
    if (read_mesh(path + "/Body/SpatialObject", body, error) != Result::ok ||
        read_mesh(path + "/Tires/Street/SpatialObject", tire, error) != Result::ok)
        throw std::runtime_error(error);
    initialize_stock_weapon_mounts(body);
    Vec3 body_center;
    if (!locate(body, "1024Body", body_center) && !locate(body, "Body", body_center))
        throw std::runtime_error("Original vehicle body anchor missing");
    anchors_.push_back(anchor);
    vehicle_angles_.push_back(0);
    const auto* chassis = named(body, "1024Body");
    if (!chassis)
        chassis = named(body, "Body");
    if (!chassis)
        throw std::runtime_error("Original chassis bounds unavailable");
    vehicle_heights_.push_back(mesh_size(*chassis).y);
    upload(body, path + "/Body", scale(body_center, -1), index);
    float wheel_scale{};
    if (load_stock_wheel_scale(resources_, name, wheel_scale, error) != Result::ok)
        throw std::runtime_error(error);
    tire.position = {};
    for (std::size_t wheel = 0; wheel < 4; ++wheel) {
        const auto tire_anchor =
            subtract(require_anchor(body, "Tire" + std::to_string(wheel + 1)), body_center);
        const auto begin = draws_.size();
        upload(tire, path + "/Tires/Street", {}, index);
        const auto pose = original_wheel_transform(wheel, tire_anchor, 0, 0, wheel_scale);
        for (std::size_t draw = begin; draw < draws_.size(); ++draw)
            draws_[draw].local = compose_track_transform(pose, draws_[draw].local);
    }
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004c9db0
// Glyph layout uses the original meshes and bounds; values are explicit preview fixtures.
void FrontendScene::upload_prizes(std::size_t rank, int money, int points) {
    constexpr std::array<std::string_view, 3> metals{"Gold", "Silver", "Bronze"};
    const auto base = "VictoryDefeat Room/Numbers/Textures/" + std::string(metals[rank]);
    auto y = anchors_[rank].y + 2 * vehicle_heights_[rank];
    for (int row = 0; row < 2; ++row) {
        auto text = std::to_string(row == 0 ? money : points);
        for (int position = static_cast<int>(text.size()) - 3; position > 0; position -= 3)
            text.insert(static_cast<std::size_t>(position), 1, ',');
        if (row == 0)
            text.insert(0, 1, '$');
        std::vector<MeshObject> glyphs;
        std::vector<float> widths;
        float width{}, height{};
        for (const auto character : text) {
            MeshObject glyph;
            std::string error;
            if (read_mesh("VictoryDefeat Room/Numbers/" + std::string(1, character), glyph,
                          error) != Result::ok)
                throw std::runtime_error(error);
            const auto bounds = mesh_size(glyph);
            const auto advance = bounds.x + (character == '$' ? .2f : 0);
            height = bounds.y;
            width += advance;
            widths.push_back(advance);
            glyphs.push_back(std::move(glyph));
        }
        if (row != 0)
            y += height + .2f;
        auto x = anchors_[rank].x + width * .5f - widths.front() * .5f;
        for (std::size_t index = 0; index < glyphs.size(); ++index) {
            glyphs[index].position = {x, y, anchors_[rank].z};
            upload(glyphs[index], base, {});
            x -= widths[index];
        }
    }
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:00438393
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004c9db0
// Presentation subset only. Purchasing, race results and original resource ownership are excluded.
Result FrontendScene::load(std::string_view id, std::string_view character, int variant,
                           std::string& error) {
    if ((id != "modify_vehicle" && id != "victory_defeat" && id != "victory" && id != "defeat") ||
        character.empty() || character.find_first_of("/\\:\r\n") != std::string_view::npos ||
        character.find("..") != std::string_view::npos ||
        character.find('\0') != std::string_view::npos || variant < 0 || variant > 6) {
        error = "Invalid frontend scene selection";
        return Result::invalid_argument;
    }
    stop();
    victory_ = id != "modify_vehicle";
    store_ = !victory_ && variant != 0;
    character_ = character;
    store_variant_ = variant;
    try {
        if (load_chrome_mapping(resources_, renderer_, chrome_texture_, chrome_enabled_, error) !=
            Result::ok)
            throw std::runtime_error(error);
        if (chrome_texture_.value)
            textures_.push_back(chrome_texture_);
        MeshObject room;
        const auto base = victory_ ? std::string("VictoryDefeat Room")
                          : store_ ? std::string("ModifyVehicle/Store")
                                   : std::string("Characters/Shared/Garage");
        if (read_mesh(base + "/SpatialObject", room, error) != Result::ok)
            throw std::runtime_error(error);
        if (!victory_)
            room.position = {}; // 00438393 resets both garage and store scene roots.
        SceneSettings settings;
        settings.near_distance = 0.1f;
        settings.far_distance = 400;
        float light_heading{}, light_pitch{};
        if (!victory_) {
            Configuration lighting;
            if (lighting.load(resources_, "Characters/Shared/Garage/Data.ini") != Result::ok)
                throw std::runtime_error("Original garage lighting configuration missing");
            const auto red = require_number(lighting, "LIGHTING", "AMBIENTRED");
            const auto green = require_number(lighting, "LIGHTING", "AMBIENTGREEN");
            const auto blue = require_number(lighting, "LIGHTING", "AMBIENTBLUE");
            if (red != green || red != blue)
                throw std::runtime_error(
                    "Colored garage ambient lighting requires renderer support");
            settings.ambient_light = red / 255;
            light_heading = require_number(lighting, "LIGHTING", "DIRECTIONALHEADING");
            light_pitch = require_number(lighting, "LIGHTING", "DIRECTIONALPITCH");
        } else {
            // Fixed-function prelight baseline in 004d4a1e; directional setup at 004ca0b1.
            settings.ambient_light = .39f;
            light_heading = 225;
            light_pitch = -35;
        }
        constexpr float kLightRadians = .01745329238474369f;
        settings.directional_light = {
            std::sin(light_heading * kLightRadians) * std::cos(light_pitch * kLightRadians),
            -std::sin(light_pitch * kLightRadians),
            std::cos(light_heading * kLightRadians) * std::cos(light_pitch * kLightRadians)};
        scene_settings_ = settings;
        renderer_.configure_scene(settings);
        sky_angles_.assign(2, 0);
        upload(room, victory_ || store_ ? base : "Characters/" + std::string(character) + "/Garage",
               {});
        if (victory_) {
            // Explicit screen-preview fixture, not fabricated live race results.
            std::array<std::string, 3> racers{std::string(character), {}, {}};
            std::size_t next_racer = 1;
            for (const auto name : {"BUD THE CRUD", "NIKI ROADKILL", "ANIMAL"})
                if (name != character && next_racer < racers.size())
                    racers[next_racer++] = name;
            if (id == "defeat")
                std::rotate(racers.begin(), racers.begin() + 1, racers.end());
            constexpr std::array<int, 3> money{10000, 5000, 2500}, points{10, 6, 4};
            for (int rank = 0; rank < 3; ++rank) {
                upload_vehicle(racers[static_cast<std::size_t>(rank)],
                               require_anchor(room, "Mount" + std::to_string(rank + 1) + "_0"),
                               rank);
                MeshObject picture;
                if (read_mesh("Characters/" + racers[static_cast<std::size_t>(rank)] +
                                  "/Picture/SpatialObject",
                              picture, error) != Result::ok)
                    throw std::runtime_error(error);
                picture.position = require_anchor(room, "Picture" + std::to_string(rank + 1));
                upload(picture, "Characters/Shared/Textures", {});
                upload_prizes(static_cast<std::size_t>(rank), money[static_cast<std::size_t>(rank)],
                              points[static_cast<std::size_t>(rank)]);
            }
            const auto start = require_anchor(room, "CamStart_2");
            const auto first = require_anchor(room, "Cam1_0");
            cameras_.push_back(
                {start, add(first, scale(normalized(subtract(first, start)), 10)), 2});
            for (int rank = 3; rank >= 1; --rank)
                cameras_.push_back({require_anchor(room, "Cam" + std::to_string(rank) + "_0"),
                                    require_anchor(room, "CamStand" + std::to_string(rank)), 0});
            rotation_speed_ = 45;
        } else if (store_) {
            const auto camera = require_anchor(room, "Camera1");
            cameras_.push_back({camera, require_anchor(room, "Object1"), 0});
            eye_ = require_anchor(room, "CameraStart");
            product_anchor_ = require_anchor(room, "Object1");
            rotation_speed_ = 0;
            product_draw_begin_ = draws_.size();
            product_mesh_begin_ = meshes_.size();
            if (select_store_product({}, {}, error) != Result::ok)
                throw std::runtime_error(error);
        } else {
            stand_ = require_anchor(room, "Stand");
            upload_vehicle(character, stand_, 0);
            Configuration config;
            if (config.load(resources_, "ModifyVehicle/Data.ini") != Result::ok)
                throw std::runtime_error("Original garage camera configuration missing");
            rotation_speed_ = require_number(config, "GENERAL", "VEHICLEROTVELOCITY(DGS_S)");
            const auto count = require_number(config, "CAMERA", "NUMPOSITIONS");
            if (count < 1 || count > 10 || std::floor(count) != count)
                throw std::runtime_error("Invalid garage camera count");
            for (int index = 0; index < static_cast<int>(count); ++index) {
                const auto section = "CAMERAPOS" + std::to_string(index);
                const auto distance = require_number(config, section, "DISTANCE");
                const auto height = require_number(config, section, "HEIGHT");
                const auto heading = require_number(config, section, "HEADING") * kRadians;
                const auto seconds = require_number(config, section, "NUMSECS");
                if (seconds <= 0 || distance <= 0)
                    throw std::runtime_error("Invalid garage camera pose");
                cameras_.push_back({add(stand_, {std::sin(heading) * distance, height,
                                                 std::cos(heading) * distance}),
                                    stand_, seconds});
            }
        }
        if (victory_)
            eye_ = cameras_.front().eye;
        else if (!store_) {
            const auto direction = normalized(subtract(cameras_.front().eye, stand_));
            const auto distance = length(subtract(cameras_.front().eye, stand_));
            eye_ = add(stand_, {direction.x * distance * .5f, 0, direction.z * distance * .5f});
        }
        target_ = cameras_.front().target;
        const auto direction = subtract(target_, eye_);
        camera_heading_ = std::atan2(direction.x, direction.z) * kDegrees;
        camera_pitch_ =
            victory_ ? -std::atan2(-direction.y, std::hypot(direction.x, direction.z)) * kDegrees
                     : 0;
        loaded_ = true;
        update_draws();
        update_camera();
        error.clear();
        return Result::ok;
    } catch (const std::exception& exception) {
        error = exception.what();
        stop();
        return Result::unavailable;
    }
}

void FrontendScene::update_draws() {
    for (const auto& draw : draws_) {
        auto pose = draw.local;
        if (draw.product) {
            pose = compose_track_transform(
                original_vehicle_transform(product_anchor_, product_angles_.x, product_angles_.y,
                                           product_angles_.z),
                pose);
        } else if (draw.vehicle >= 0) {
            const auto index = static_cast<std::size_t>(draw.vehicle);
            const auto vehicle =
                original_vehicle_transform(anchors_[index], vehicle_angles_[index], 0, 0);
            pose = compose_track_transform(vehicle, pose);
        } else if (draw.turntable >= 0) {
            const auto index = static_cast<std::size_t>(draw.turntable);
            pose = original_vehicle_transform(pose.origin, vehicle_angles_[index], 0, 0);
        } else if (draw.sky >= 0) {
            pose = original_vehicle_transform(
                pose.origin, sky_angles_[static_cast<std::size_t>(draw.sky)], 0, 0);
        }
        if (renderer_.update_instance(draw.instance, pose) != Result::ok)
            throw std::runtime_error("Frontend scene instance update failed");
    }
}
void FrontendScene::update_camera() {
    const auto frame = original_camera_frame(eye_, target_, camera_heading_, camera_pitch_);
    renderer_.update_camera(frame.transform, frame.vertical_fov_degrees);
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0044000a
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004e553d
// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:004e433a
// Position acceleration subset of the menu camera; run on a modern fixed presentation step.
void FrontendScene::step(float dt) {
    auto& camera = cameras_[camera_index_];
    const auto desired = camera.eye;
    if (!victory_ || length(subtract(eye_, desired)) < .1f)
        camera_seconds_ += dt;
    if (!store_ && !victory_finished_ && camera_seconds_ > camera.seconds) {
        if (victory_) {
            // 004cbee1 visits Start, Cam3, Cam2, Cam1, then returns to Start once.
            if (camera_index_ + 1 < cameras_.size())
                ++camera_index_;
            else {
                camera_index_ = 0;
                victory_finished_ = true;
            }
        } else
            camera_index_ = (camera_index_ + 1) % cameras_.size();
        camera_seconds_ = 0;
    }
    const auto position_axis = [dt](float& position, float& speed, float target) {
        const float difference = target - position;
        if (std::abs(difference) / 2 <= std::abs(speed))
            speed = difference / 2;
        else
            speed += difference * dt;
        position += speed * dt;
    };
    const auto& next = cameras_[camera_index_];
    const bool eased_angles = !victory_ || camera_index_ > 0 || victory_finished_;
    // Mode 8 computes look angles before the position update; mode 9 computes them after it.
    const auto look_eye = eye_;
    position_axis(eye_.x, velocity_.x, next.eye.x);
    position_axis(eye_.y, velocity_.y, next.eye.y);
    position_axis(eye_.z, velocity_.z, next.eye.z);
    target_ = next.target;
    const auto direction = subtract(target_, eased_angles ? look_eye : eye_);
    const auto desired_heading = std::atan2(direction.x, direction.z) * kDegrees;
    const auto desired_pitch =
        std::atan2(direction.y, std::hypot(direction.x, direction.z)) * kDegrees;
    if (eased_angles) {
        const auto wrap = [](float value) {
            if (value < 0)
                return value + 360;
            return value >= 360 ? value - 360 : value;
        };
        camera_heading_ = wrap(camera_heading_);
        camera_pitch_ = wrap(camera_pitch_);
        const auto wanted_heading = wrap(desired_heading);
        const auto wanted_pitch = wrap(desired_pitch);
        const auto difference = wanted_heading - camera_heading_;
        if (std::abs(difference) / 2 <= std::abs(heading_speed_))
            heading_speed_ = difference / 2;
        else
            heading_speed_ += difference * dt;
        camera_heading_ += (std::abs(difference) <= 180 ? dt : -dt) * heading_speed_;
        // 004e2be0 wraps once and blends through the shorter arc, without a final clamp.
        const auto pitch_difference = wanted_pitch - camera_pitch_;
        if (std::abs(pitch_difference) <= 180)
            camera_pitch_ += pitch_difference * 2.5f * dt;
        else if (wanted_pitch <= camera_pitch_)
            camera_pitch_ += ((360 - camera_pitch_) + wanted_pitch) * 2.5f * dt;
        else
            camera_pitch_ -= ((360 - wanted_pitch) + camera_pitch_) * 2.5f * dt;
    } else {
        camera_heading_ = desired_heading;
        camera_pitch_ = desired_pitch;
    }
    if (victory_) {
        if (camera_index_ > 0)
            vehicle_angles_[3 - camera_index_] += rotation_speed_ * dt;
        sky_angles_[0] += dt;
        sky_angles_[1] += 3 * dt;
    } else if (!store_)
        vehicle_angles_[0] += rotation_speed_ * dt;
    if (store_)
        product_angles_ = add(product_angles_, scale(product_speeds_, dt));
}
void FrontendScene::set_visible(bool visible) {
    for (const auto& draw : draws_)
        renderer_.set_instance_visible(draw.instance, visible && draw.visible);
    if (visible && loaded_) {
        renderer_.configure_scene(scene_settings_);
        update_draws();
        update_camera();
    }
}
Result FrontendScene::advance(double elapsed_seconds) {
    if (!loaded_)
        return Result::unavailable;
    if (!std::isfinite(elapsed_seconds) || elapsed_seconds < 0)
        return Result::invalid_argument;
    elapsed_ += elapsed_seconds;
    debt_ += elapsed_seconds;
    // Deterministic modernization: recover per-callback formulas without display-rate dependence.
    // Preserve excess time; never enlarge dt or discard debt after a long host frame.
    constexpr double kStep = 1.0 / 60.0;
    unsigned ticks{};
    while (debt_ + 1e-12 >= kStep && ticks < 240) {
        step(static_cast<float>(kStep));
        debt_ -= kStep;
        ++ticks;
    }
    if (debt_ < 0)
        debt_ = 0;
    update_draws();
    update_camera();
    return Result::ok;
}

// RE: 93b4e4c301a5694b55e16977c2e52f4971bd9089659e3d58617c8af2368436c8:function:0044bac7
// Original preview mesh selection, independent of the purchasing/economy state.
Result FrontendScene::select_store_product(std::string_view item, std::string_view quality,
                                           std::string& error) {
    if (!store_ || cameras_.empty() || store_variant_ < 1 || store_variant_ > 6)
        return Result::unavailable;
    const auto valid_label = [](std::string_view label) {
        return label.size() < 128 && label.find_first_of("/\\:\r\n") == std::string_view::npos &&
               label.find("..") == std::string_view::npos &&
               label.find('\0') == std::string_view::npos;
    };
    if (!valid_label(item) || !valid_label(quality)) {
        error = "Invalid store preview selection";
        return Result::invalid_argument;
    }
    try {
        constexpr std::array<std::string_view, 6> categories{"Tires", "Drive Train", "Weapons",
                                                             "Armor", "Repair",      "Paint Job"};
        const auto category = std::string(categories[static_cast<std::size_t>(store_variant_ - 1)]);
        const auto config_root = "Cars/Shared/" + category;
        Configuration config, type;
        if (config.load(resources_, config_root + "/Data.ini") != Result::ok)
            throw std::runtime_error("Store preview configuration missing: " + category);
        auto item_name = item.empty() ? config.value("ORDERING", "ITEM0") : std::string(item);
        if (item_name.empty())
            throw std::runtime_error("Original store item missing");
        if (type.load(resources_, config_root + "/" + item_name + "/Data.ini") != Result::ok)
            throw std::runtime_error("Original store item configuration missing: " + item_name);
        auto quality_name =
            quality.empty() ? type.value("ORDERING", "ITEM0") : std::string(quality);
        const auto from_quality = require_number(config, "GARAGE3DOBJECT", "GETFROMQUALITY") != 0;
        product_angles_ = {require_number(config, "GARAGE3DOBJECT", "INITIALOBJECTHEADING(DGS)"),
                           require_number(config, "GARAGE3DOBJECT", "INITIALOBJECTPITCH(DGS)"),
                           require_number(config, "GARAGE3DOBJECT", "INITIALOBJECTROLL(DGS)")};
        product_speeds_ = {require_number(config, "GARAGE3DOBJECT", "HEADINGVELOCITY(DGS_S)"),
                           require_number(config, "GARAGE3DOBJECT", "PITCHVELOCITY(DGS_S)"),
                           require_number(config, "GARAGE3DOBJECT", "ROLLVELOCITY(DGS_S)")};
        auto mesh_root = store_variant_ == 1   ? "Cars/" + character_ + "/Tires"
                         : store_variant_ == 6 ? "Cars/" + character_ + "/Paint"
                                               : config_root;
        mesh_root += "/" + item_name;
        if (from_quality) {
            if (quality_name.empty())
                throw std::runtime_error("Original store quality missing");
            mesh_root += "/" + quality_name;
        }
        MeshObject product;
        if (read_mesh(mesh_root + (store_variant_ == 1 ? "/SpatialObject" : "/SpatialObject_Hi"),
                      product, error) != Result::ok)
            throw std::runtime_error(error);
        // Product wrapper is attached at Object1, replacing the exporter's root position.
        product.position = {};
        for (std::size_t index = product_draw_begin_; index < draws_.size(); ++index)
            renderer_.destroy_instance(draws_[index].instance);
        draws_.resize(product_draw_begin_);
        for (std::size_t index = product_mesh_begin_; index < meshes_.size(); ++index)
            renderer_.destroy_mesh(meshes_[index]);
        meshes_.resize(product_mesh_begin_);
        upload(product, "Cars/Shared/Textures", {});
        for (std::size_t index = product_draw_begin_; index < draws_.size(); ++index) {
            draws_[index].product = true;
            renderer_.set_instance_static(draws_[index].instance, false);
        }
        error.clear();
        return Result::ok;
    } catch (const std::exception& exception) {
        error = exception.what();
        return Result::unavailable;
    }
}
} // namespace roadwars

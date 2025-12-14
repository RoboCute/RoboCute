#include <rbc_world_v2/mesh.h>
#include <tiny_obj_loader.h>
#include <luisa/core/fiber.h>
#include <luisa/runtime/rtx/triangle.h>

namespace rbc::world {
using namespace luisa;
using namespace luisa::compute;
static void calculate_tangent(
    luisa::span<float3 const> positions,
    luisa::span<float2 const> uvs,
    luisa::span<float4> tangents,
    luisa::span<Triangle const> triangles,
    float tangent_w) {
    luisa::vector<std::array<std::atomic<float>, 3>> atomic_tangents;
    atomic_tangents.resize(tangents.size());
    luisa::fiber::parallel(
        triangles.size(),
        [&](size_t i) {
            auto &tri = triangles[i];
            auto &p0 = positions[tri.i0];
            auto &p1 = positions[tri.i1];
            auto &p2 = positions[tri.i2];
            auto &uv0 = uvs[tri.i0];
            auto &uv1 = uvs[tri.i1];
            auto &uv2 = uvs[tri.i2];
            auto edge1 = p1 - p0;
            auto edge2 = p2 - p0;
            auto delta_uv1 = uv1 - uv0;
            auto delta_uv2 = uv2 - uv0;
            auto f_outer_dot = (delta_uv1.x * delta_uv2.y - delta_uv2.x * delta_uv1.y);
            f_outer_dot = sign(f_outer_dot) * std::max<float>(abs(f_outer_dot), 1e-5);
            auto f = 1.0f / f_outer_dot;
            auto tangent = f * (delta_uv2.y * edge1 - delta_uv1.y * edge2);
            tangent /= max(length(tangent), 1e-5f);// safe normalize
            for (auto j = 0; j < 3; ++j) {
                atomic_tangents[tri.i0][j] += atomic_tangents[tri.i0][j];
                atomic_tangents[tri.i1][j] += atomic_tangents[tri.i1][j];
                atomic_tangents[tri.i2][j] += atomic_tangents[tri.i2][j];
            }
        },
        1024);
    luisa::fiber::parallel(
        tangents.size(),
        [&](size_t i) {
            float3 tan;
            for (auto j = 0; j < 3; ++j)
                tan[j] = atomic_tangents[i][j];
            tan /= max(1e-5f, length(tan));
            for (auto j = 0; j < 3; ++j)
                tangents[i][j] = tan[j];
            tangents[i].w = tangent_w;
        },
        2048);
}
bool Mesh::decode(luisa::filesystem::path const &path) {
    // TODO: init mesh
    std::lock_guard lck{_async_mtx};
    wait_load();
    if (loaded()) [[unlikely]] {
        LUISA_ERROR("Can not create on exists mesh.");
    }
    auto ext = luisa::to_string(path.extension());
    for (auto &i : ext) {
        i = std::tolower(i);
    }
    BinaryFileStream file_stream(luisa::to_string(path));

    if (!file_stream.valid()) return false;
    if (ext == ".obj") {
        tinyobj::ObjReaderConfig obj_reader_config;
        obj_reader_config.triangulate = true;
        obj_reader_config.vertex_color = false;
        tinyobj::ObjReader obj_reader;
        std::string str;
        str.resize(file_stream.length());
        file_stream.read(
            {reinterpret_cast<std::byte *>(str.data()),
             str.size()});
        if (!obj_reader.ParseFromString(str, "", obj_reader_config)) {
            luisa::string_view error_message = "unknown error.";
            if (auto &&e = obj_reader.Error(); !e.empty()) { error_message = e; }
            return false;
        }
        if (auto &&e = obj_reader.Warning(); !e.empty()) {
            LUISA_WARNING_WITH_LOCATION("{}", e);
        }
        auto &attri = obj_reader.GetAttrib();
        // TODO
    }
    return true;
}
}// namespace rbc::world
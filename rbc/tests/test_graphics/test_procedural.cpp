#include "test_procedural.h"

#include <random>
#include <rbc_world/resources/aabb_voxel.h>
#include <rbc_world/resources/voxel_sdf.h>
#include <rbc_world/resources/gaussian_splat.h>
#include <rbc_world/resources/height_map.h>
#include <rbc_world/resources/scene.h>
#include <rbc_project/project.h>
#include <rbc_graphics/render_device.h>
#include <luisa/runtime/rtx/aabb.h>
#include <rbc_graphics/shader_manager.h>

namespace rbc {

void TestProcedural::init(IProject *proj) {
    // gus_res = proj->import_assets("nerf_blender_lego_30000.ply", TypeInfo::get<world::GaussianSplatResource>().md5());
    // gus_res->install();
    // [[maybe_unused]] auto id = gus_res->emplace_procedural_instance(scaling(1.f));
    _init_voxel_res();
    // _init_sdf_res();
    // _init_gs_res();
    // _init_height_map_res();
}
class Random {
    std::mt19937 gen;
public:
    Random() : gen(std::random_device{}()) {}

    int next(int min, int max) {
        return std::uniform_int_distribution<>{min, max}(gen);
    }
};
void TestProcedural::_init_voxel_res() {
    // Create voxel resource with some random AABB boxes
    constexpr uint32_t num_voxels = 100;
    voxel_res = RC<world::VoxelResource>{world::create_object<world::VoxelResource>()};
    voxel_res->create_empty(num_voxels);
    Random rng;
    auto aabbs = voxel_res->host_aabbs();
    for (uint32_t i = 0; i < num_voxels; ++i) {
        int random_x = rng.next(-20, 20);
        int random_y = rng.next(-20, 20);
        int random_z = rng.next(-20, 20);
        auto min_value = make_float3(
                             random_x,
                             random_y,
                             random_z) *
                         0.3f;
        auto max_value = min_value + float3(0.3f);
        aabbs[i].packed_min = {min_value.x, min_value.y, min_value.z};
        aabbs[i].packed_max = {max_value.x, max_value.y, max_value.z};
    }
    auto materials = voxel_res->host_materials();
    // Initialize materials with random properties
    for (uint32_t i = 0; i < num_voxels; ++i) {
        auto &mat = materials[i];
        // Random base color (HSV-like variety)
        float hue = static_cast<float>(rng.next(0, 360));
        float sat = static_cast<float>(rng.next(50, 100)) / 100.0f;
        float val = static_cast<float>(rng.next(60, 100)) / 100.0f;
        // HSV to RGB conversion
        float c = val * sat;
        float x = c * (1.0f - std::abs(std::fmod(hue / 60.0f, 2.0f) - 1.0f));
        float m = val - c;
        float r, g, b;
        if (hue < 60.0f) {
            r = c;
            g = x;
            b = 0;
        } else if (hue < 120.0f) {
            r = x;
            g = c;
            b = 0;
        } else if (hue < 180.0f) {
            r = 0;
            g = c;
            b = x;
        } else if (hue < 240.0f) {
            r = 0;
            g = x;
            b = c;
        } else if (hue < 300.0f) {
            r = x;
            g = 0;
            b = c;
        } else {
            r = c;
            g = 0;
            b = x;
        }
        mat.weight.diffuse_roughness = static_cast<float>(rng.next(0, 50)) / 100.0f;
        mat.weight.specular = static_cast<float>(rng.next(20, 80)) / 100.0f;
        mat.weight.metallic = static_cast<float>(rng.next(0, 30)) / 100.0f;
        mat.weight.transmission = 0.0f;
        mat.weight.coat = static_cast<float>(rng.next(0, 20)) / 100.0f;
        mat.base.albedo = {r + m, g + m, b + m};
        mat.specular.roughness = static_cast<float>(rng.next(10, 60)) / 100.0f;
        mat.specular.roughness_anisotropy = 0.0f;
        mat.specular.roughness_anisotropy_angle = 0.0f;
        mat.specular.ior = 1.4f + static_cast<float>(rng.next(0, 20)) / 100.0f;
        mat.emission.luminance = {0.0f, 0.0f, 0.0f};
        // Random emission for some voxels (10% chance)
        if (rng.next(0, 9) == 0) {
            float emission_strength = static_cast<float>(rng.next(10, 50)) / 10.0f;
            mat.emission.luminance = {(r + m) * emission_strength,
                                      (g + m) * emission_strength,
                                      (b + m) * emission_strength};
        }
        mat.transmission.transmission_color = {1.0f, 1.0f, 1.0f};
        mat.transmission.transmission_depth = 0.0f;
        mat.transmission.transmission_scatter = {0.0f, 0.0f, 0.0f};
        mat.transmission.transmission_scatter_anisotropy = 0.0f;
        mat.transmission.transmission_dispersion_scale = 0.0f;
        mat.transmission.transmission_dispersion_abbe_number = 20.0f;
        mat.coat.coat_color = {1.0f, 1.0f, 1.0f};
        mat.coat.coat_roughness = static_cast<float>(rng.next(0, 30)) / 100.0f;
        mat.coat.coat_roughness_anisotropy = 0.0f;
        mat.coat.coat_roughness_anisotropy_angle = 0.0f;
        mat.coat.coat_ior = 1.6f;
        mat.coat.coat_darkening = 1.0f;
        mat.coat.coat_roughening = 1.0f;
    }

    // Install and emplace as procedural instance
    voxel_res->install();
    [[maybe_unused]] auto voxel_inst_id = voxel_res->emplace_procedural_instance(
        scaling(1.0f));
}

void TestProcedural::_init_sdf_res() {
    // Create SDF voxel resource with 256^3 resolution
    constexpr uint3 grid_size{256, 256, 256};
    sdf_res = RC<world::SDFVoxelResource>{world::create_object<world::SDFVoxelResource>()};
    sdf_res->create_empty(grid_size);

    // Initialize with simple SDF data (sphere at center)
    auto data = sdf_res->host_data();
    const float3 center{0.5f, 0.5f, 0.5f};
    const float radius = 0.3f;

    for (uint32_t z = 0; z < grid_size.z; ++z) {
        for (uint32_t y = 0; y < grid_size.y; ++y) {
            for (uint32_t x = 0; x < grid_size.x; ++x) {
                // Normalize coordinates to [0, 1]
                float3 pos{
                    static_cast<float>(x) / grid_size.x,
                    static_cast<float>(y) / grid_size.y,
                    static_cast<float>(z) / grid_size.z};
                // Distance from center
                float dist = std::sqrt(
                    (pos.x - center.x) * (pos.x - center.x) +
                    (pos.y - center.y) * (pos.y - center.y) +
                    (pos.z - center.z) * (pos.z - center.z));
                // Signed distance (negative inside, positive outside)
                uint64_t idx = static_cast<uint64_t>(x) + static_cast<uint64_t>(y) * grid_size.x +
                               static_cast<uint64_t>(z) * grid_size.x * grid_size.y;
                data[idx] = dist - radius;
            }
        }
    }

    // Set UVW scale and offset for world space mapping
    sdf_res->set_uvw_scale(float3{10.0f, 10.0f, 10.0f});
    sdf_res->set_uvw_offset(float3{-5.0f, -5.0f, -5.0f});

    // Install and emplace as procedural instance
    sdf_res->install();
    [[maybe_unused]] auto sdf_inst_id = sdf_res->emplace_procedural_instance(
        float4x4::eye(1));
}

void TestProcedural::_init_gs_res() {
    // Empty implementation - Gaussian Splat resource is not initialized
    // This function intentionally does nothing as per requirements
}

void TestProcedural::_init_height_map_res() {
    // Create height map resource with 512x512 resolution
    constexpr uint2 resolution{512, 512};
    height_map_res = RC<world::HeightMapResource>{world::create_object<world::HeightMapResource>()};
    height_map_res->create_empty(resolution);
    Shader2D<
        Image<float>,//& output,
        float2,      //uv_scale,
        float2,      //uv_offset,
        float,       //frequency,
        uint         // octave_count
        > const *perlin{};
    ShaderManager::instance()->load("texture_process/perlin_noise.bin", perlin);
    LUISA_ASSERT(perlin);
    RenderDevice::instance().lc_main_cmd_list()
        << (*perlin)(
               height_map_res->height_img()->get_float_image(),
               float2(1),
               float2(0),
               1.f,
               1)
               .dispatch(resolution);
    // Install and emplace as procedural instance
    height_map_res->install();
    [[maybe_unused]] auto height_map_inst_id = height_map_res->emplace_procedural_instance(
        scaling(1.0f));
}

void TestProcedural::dispose() {
    // Remove procedural instances if they exist
    if (voxel_res) {
        voxel_res->remove_procedural_instance();
    }
    if (sdf_res) {
        sdf_res->remove_procedural_instance();
    }
    if (gus_res) {
        gus_res->remove_procedural_instance();
    }
    if (height_map_res) {
        height_map_res->remove_procedural_instance();
    }
    voxel_res.reset();
    sdf_res.reset();
    gus_res.reset();
    height_map_res.reset();
}
TestProcedural::TestProcedural() {}
TestProcedural::~TestProcedural() {}
}// namespace rbc

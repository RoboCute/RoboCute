#include "test_procedural.h"

#include <random>
#include <rbc_world/resources/aabb_voxel.h>
#include <rbc_world/resources/voxel_sdf.h>
#include <rbc_world/resources/gaussian_splat.h>
#include <rbc_world/resources/scene.h>
#include <luisa/runtime/rtx/aabb.h>

namespace rbc {

void TestProcedural::init() {
    _init_voxel_res();
    // _init_sdf_res();
    // _init_gs_res();
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

void TestProcedural::dispose() {
    // Remove procedural instances if they exist
    if (voxel_res && voxel_res->has_procedural_primitive()) {
        voxel_res->remove_procedural_instance();
    }
    if (sdf_res && sdf_res->has_procedural_primitive()) {
        sdf_res->remove_procedural_instance();
    }
    if (gus_res && gus_res->has_procedural_primitive()) {
        gus_res->remove_procedural_instance();
    }
    voxel_res.reset();
    sdf_res.reset();
    gus_res.reset();
}
TestProcedural::TestProcedural() {}
TestProcedural::~TestProcedural() {}
}// namespace rbc

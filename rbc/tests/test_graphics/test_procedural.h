#pragma once
#include <rbc_world/entity.h>

namespace rbc {
namespace world {
struct VoxelResource;
struct SDFVoxelResource;
struct GaussianSplatResource;
}// namespace world
struct TestProcedural {
    RC<world::VoxelResource> voxel_res;
    RC<world::SDFVoxelResource> sdf_res;
    RC<world::GaussianSplatResource> gus_res;
    TestProcedural();
    ~TestProcedural();
    void init();
    void _init_voxel_res();
    void _init_sdf_res();
    void _init_gs_res();
    void dispose();
};
}// namespace rbc
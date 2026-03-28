#pragma once
#include <rbc_world/entity.h>

namespace rbc {
namespace world {
struct VoxelResource;
struct SDFVoxelResource;
struct GaussianSplatResource;
struct HeightMapResource;
}// namespace world
struct IProject;
struct TestProcedural {
    RC<world::VoxelResource> voxel_res;
    RC<world::SDFVoxelResource> sdf_res;
    RC<world::GaussianSplatResource> gus_res;
    RC<world::HeightMapResource> height_map_res;
    TestProcedural();
    ~TestProcedural();
    void init(IProject* proj);
    void _init_voxel_res();
    void _init_sdf_res();
    void _init_gs_res();
    void _init_height_map_res();
    void dispose();
};
}// namespace rbc
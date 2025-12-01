#pragma once
#include <rbc_graphics/scene_manager.h>
#include <rbc_graphics/device_assets/device_mesh.h>
#include <rbc_graphics/lights.h>
#include <rbc_graphics/mat_code.h>
struct SimpleScene {
    rbc::Lights& lights;
    rbc::MatCode default_mat_code;
    rbc::MatCode default_mat_code_orange; // 哦润橘
    rbc::MatCode light_mat_code;
    luisa::vector<rbc::RC<rbc::DeviceMesh>> device_meshes;
    luisa::vector<uint32_t> tlas_indices;
    luisa::vector<uint32_t> light_id;
    luisa::float3 cube_pos;
    luisa::float3 light_pos;
    bool tlas_loaded{};

    SimpleScene(rbc::Lights& lights);
    ~SimpleScene();
    void move_cube(luisa::float3 pos);
    void move_light(luisa::float3 pos);

private:

    void _init_mesh();
    void _init_material();
    void _init_tlas();
};
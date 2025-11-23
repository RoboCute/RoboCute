#pragma once
#include <rbc_graphics/scene_manager.h>
#include <rbc_graphics/device_assets/device_mesh.h>
#include <rbc_graphics/lights.h>
#include <rbc_graphics/mat_code.h>
struct SimpleScene {
    rbc::Lights lights;
    rbc::MatCode default_mat_code;
    luisa::vector<rbc::RC<rbc::DeviceMesh>> device_meshes;
    luisa::vector<uint32_t> tlas_indices;
    luisa::vector<uint32_t> light_id;
    bool tlas_loaded{};

    SimpleScene();
    ~SimpleScene();

    void tick();
private:

    void _init_mesh();
    void _init_material();
};
#pragma once
#include <rbc_world/resources/mesh.h>
#include <rbc_world/resources/material.h>
#include <rbc_world/resources/texture.h>

#include <rbc_world/resources/skeleton.h>
#include <rbc_world/resources/anim_sequence.h>
#include <rbc_world/resources/skin.h>

#include <luisa/core/stl/filesystem.h>

namespace tinygltf {
struct Model;
}// namespace tinygltf

namespace rbc::world {

// helper function load gltf
bool load_gltf_model(tinygltf::Model &model, luisa::filesystem::path const &path, bool is_binary);

struct GltfLoadConfig {
    bool load_skeleton = false;
    bool load_skin = false;
    bool load_anim_seq = false;
    bool load_mesh = true;
    bool load_materials = true;
};

struct GltfSceneData {
    GltfLoadConfig config;
    RC<MeshResource> mesh;
    RC<SkeletonResource> skel;// skeleton
    RC<SkinResource> skin;
    RC<AnimSequenceResource> anim;
    luisa::vector<RC<MaterialResource>> materials;
    luisa::vector<RC<TextureResource>> textures;
};

/**
 * @brief GLTF/GLB scene loader that loads mesh, materials, and textures
 * This class encapsulates tinygltf usage and provides a clean interface
 */
struct RBC_RUNTIME_API GltfSceneLoader {
    /**
     * @brief Load a GLTF scene from file
     * @param gltf_path Path to the .gltf or .glb file
     * @return GltfSceneData containing mesh, materials, and textures
     */
    static GltfSceneData load_scene(luisa::filesystem::path const &gltf_path, GltfLoadConfig config = {});

    /**
     * @brief Load a GLTF scene from file (binary GLB format)
     * @param glb_path Path to the .glb file
     * @return GltfSceneData containing mesh, materials, and textures
     */
    static GltfSceneData load_scene_binary(luisa::filesystem::path const &glb_path, GltfLoadConfig config = {});
};

}// namespace rbc::world

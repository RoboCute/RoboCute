#pragma once

#include <rbc_importer/scene_asset_importer.h>
#include <rbc_world/resources/mesh.h>
#include <rbc_world/resources/material.h>
#include <rbc_world/resources/texture.h>
#include <rbc_world/resources/skeleton.h>
#include <rbc_world/resources/skin.h>
#include <rbc_world/resources/anim_sequence.h>
#include <rbc_world/resources/skelmesh.h>
#include <tiny_gltf.h>

namespace rbc::world {

/**
 * @brief GLTF/GLB scene asset importer
 * 
 * Handles importing of GLTF files including:
 * - Meshes (with skinning data)
 * - Materials (PBR)
 * - Textures (with automatic import of referenced images)
 * - Skeletons/Bones
 * - Skins
 * - Animations
 */
class GltfAssetImporter : public ISceneAssetImporter {
public:
    [[nodiscard]] bool can_import(luisa::filesystem::path const &path) const override;
    [[nodiscard]] luisa::vector<luisa::string> supported_extensions() const override;
    [[nodiscard]] luisa::vector<SubAssetInfo> analyze(
        luisa::filesystem::path const &path,
        SceneImportContext const &ctx) const override;
    [[nodiscard]] SceneImportResult import(
        luisa::filesystem::path const &path,
        SceneImportContext const &ctx) const override;
    [[nodiscard]] luisa::string_view name() const override { return "gltf"; }

private:
    // Internal structures for GLTF processing
    struct GltfImportState {
        tinygltf::Model model;
        luisa::filesystem::path source_dir;
        luisa::filesystem::path intermediate_dir;
        SceneImportContext::Settings settings;
        
        // Resource mappings
        luisa::vector<RC<MeshResource>> meshes;
        luisa::vector<RC<MaterialResource>> materials;
        luisa::vector<RC<TextureResource>> textures;
        luisa::vector<RC<SkeletonResource>> skeletons;
        luisa::vector<RC<SkinResource>> skins;
        luisa::vector<RC<AnimSequenceResource>> animations;
        
        // Name to resource mapping
        luisa::unordered_map<luisa::string, vstd::Guid> name_to_guid;
    };
    
    // Load GLTF model from file
    [[nodiscard]] bool load_model(
        luisa::filesystem::path const &path,
        tinygltf::Model &model) const;
    
    // Import specific resource types
    void import_meshes(GltfImportState &state) const;
    void import_materials(GltfImportState &state) const;
    void import_textures(GltfImportState &state, SceneImportContext const &ctx) const;
    void import_skeletons(GltfImportState &state) const;
    void import_skins(GltfImportState &state) const;
    void import_animations(GltfImportState &state) const;
    
    // Create skelmesh from imported resources
    [[nodiscard]] RC<SkelMeshResource> create_skelmesh(
        GltfImportState &state,
        const tinygltf::Node &node,
        int skin_index) const;
    
    // Helper functions
    [[nodiscard]] luisa::string get_node_name(const tinygltf::Node &node, int index) const;
    [[nodiscard]] luisa::filesystem::path resolve_uri(
        const luisa::string &uri,
        const luisa::filesystem::path &base_dir) const;
};

} // namespace rbc::world

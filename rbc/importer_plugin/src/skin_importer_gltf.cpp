#include "rbc_importer/skin_importer_gltf.h"
#include <tiny_gltf.h>

namespace rbc {

bool GltfSkinImporter::import(world::Resource *resource_base, luisa::filesystem::path const &path) {
    auto resource = static_cast<world::SkinResource *>(resource_base);
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;
    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, path.string().c_str());
    if (!ret) {
        LUISA_ERROR("Failed to load GLTF model: {}", err);
        return false;
    }

    for (auto const &skin : model.skins) {
        name_ref(resource) = skin.name;
        auto &joint_remaps = joint_remaps_ref(resource);
        // joint_remaps maps from skin joint index (0, 1, 2, ...) to joint name
        // skin.joints array contains GLTF node indices
        // Mesh JOINTS_0 attribute contains indices into skin.joints array
        joint_remaps.resize(skin.joints.size());
        for (size_t i = 0; i < skin.joints.size(); ++i) {
            auto node_idx = skin.joints[i];
            joint_remaps[i] = model.nodes[node_idx].name;
        }
        auto &inverse_bind_poses = inverse_bind_poses_ref(resource);
        inverse_bind_poses.resize(skin.inverseBindMatrices >= 0 ? model.accessors[skin.inverseBindMatrices].count : 0);

        // ozz和gltf的矩阵都是列主序，所以可以直接复制
        if (skin.inverseBindMatrices >= 0) {
            auto const &accessor = model.accessors[skin.inverseBindMatrices];
            auto const &buffer_view = model.bufferViews[accessor.bufferView];
            auto const &buffer = model.buffers[buffer_view.buffer];
            size_t data_offset = buffer_view.byteOffset + accessor.byteOffset;
            memcpy(inverse_bind_poses.data(), buffer.data.data() + data_offset, inverse_bind_poses.size() * sizeof(AnimFloat4x4));
        }
    }

    return true;
}

}// namespace rbc

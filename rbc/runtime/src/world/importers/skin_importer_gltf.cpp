#include "rbc_world/importers/skin_importer_gltf.h"
#include <tiny_gltf.h>

namespace rbc {

bool GltfSkinImporter::import(SkinResource *resource, luisa::filesystem::path const &path) {
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
        joint_remaps.resize(skin.joints.size());
        for (auto const &joint : skin.joints) {
            joint_remaps[joint] = model.nodes[joint].name;
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

void SkinResource::generate_LUT() {
    if (!ref_skel || !ref_mesh) {
        LUISA_ERROR("Skeleton or mesh not set");
        return;
    }

    joint_remaps_LUT.resize(joint_remaps.size());
    auto *skel = ref_skel.get();
    auto *mesh = ref_mesh.get();
    for (size_t i = 0; i < joint_remaps.size(); i++) {
        auto it = std::find(
            skel->ref_skel().RawJointNames().begin(),
            skel->ref_skel().RawJointNames().end(), joint_remaps[i]);
        if (it == skel->ref_skel().RawJointNames().end()) {
            LUISA_ERROR("Joint {} not found in skeleton", joint_remaps[i]);
            return;
        }
        joint_remaps_LUT[i] = static_cast<BoneIndexType>(it - skel->ref_skel().RawJointNames().begin());
    }
}
}// namespace rbc
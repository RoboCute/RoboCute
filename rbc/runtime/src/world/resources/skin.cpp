#include "rbc_world/resources/skin.h"
#include "rbc_world/type_register.h"
#include <rbc_core/binary_file_writer.h>
#include <luisa/core/binary_file_stream.h>

namespace rbc::world {

SkinResource::SkinResource() = default;
SkinResource::~SkinResource() = default;

void SkinResource::serialize_meta(world::ObjSerialize const &ser) const {
    std::shared_lock lck{_async_mtx};
    // Serialize resource dependencies
    if (ref_skel) {
        ser.ar.value(ref_skel->guid(), "ref_skel");
    }
    if (ref_mesh) {
        ser.ar.value(ref_mesh->guid(), "ref_mesh");
    }
    ser.ar.value(_name, "name");
}

void SkinResource::deserialize_meta(world::ObjDeSerialize const &ser) {
    std::shared_lock lck{_async_mtx};
    // Deserialize resource dependencies
    vstd::Guid ref_skel_guid;
    if (ser.ar.value(ref_skel_guid, "ref_skel")) {
        auto res = get_resource(ref_skel_guid, true);
        if (res && res->is_type_of(TypeInfo::get<SkeletonResource>())) {
            ref_skel = res;
        } else {
            ref_skel = nullptr;
        }
    }

    vstd::Guid ref_mesh_guid;
    if (ser.ar.value(ref_mesh_guid, "ref_mesh")) {
        auto res = get_resource(ref_mesh_guid, true);
        if (res && res->is_type_of(TypeInfo::get<MeshResource>())) {
            ref_mesh = res;
        } else {
            ref_mesh = nullptr;
        }
    }
    ser.ar.value(_name, "name");
}

rbc::coroutine SkinResource::_async_load() {
    // Wait for dependencies to load
    if (ref_skel) {
        co_await ref_skel->await_loading();
    }
    if (ref_mesh) {
        co_await ref_mesh->await_loading();
    }

    std::shared_lock lck{_async_mtx};
    auto path = this->path();
    if (path.empty()) { co_return; }

    luisa::BinaryFileStream file_stream(luisa::to_string(path));
    if (!file_stream.valid()) { co_return; }

    luisa::BinaryBlob blob = file_stream.read(file_stream.length());
    BinDeSerializer deser{blob};
    deser._load(_joint_remaps, "joint_remaps");
    deser._load(_inverse_bind_poses, "inverse_bind_poses");
    deser._load(_joint_remaps_LUT, "joint_remaps_LUT");

    co_return;
}

bool SkinResource::unsafe_save_to_path() const {
    std::shared_lock lck{_async_mtx};
    BinSerializer ser;
    ser._store(_joint_remaps, "joint_remaps");
    ser._store(_inverse_bind_poses, "inverse_bind_poses");
    ser._store(_joint_remaps_LUT, "joint_remaps_LUT");

    auto path = this->path();
    BinaryFileWriter writer{luisa::to_string(path)};
    if (!writer._file) [[unlikely]] {
        return false;
    }
    LUISA_INFO("Skin Writing to {}", path.string());
    auto bytes = ser.write_to();
    writer.write(bytes);
    return true;
}

void SkinResource::log_brief() const {
    LUISA_INFO("Skin has {} inverse bind poses and {} joint_remaps", _inverse_bind_poses.size(), _joint_remaps.size());
    auto log_matrix = [](const AnimFloat4x4 &m) {
        std::array<std::array<float, 4>, 4> mat;
        for (size_t i = 0; i < 4; i++) {
            ozz::math::StorePtr(m.cols[i], mat[i].data());
        }
        LUISA_INFO("AnimFloat4x4:[");
        for (size_t i = 0; i < 4; i++) {
            LUISA_INFO("{} {} {} {}", mat[0][i], mat[1][i], mat[2][i], mat[3][i]);
        }
        LUISA_INFO("]");
    };
    if (!_inverse_bind_poses.empty()) {
        auto &inverse_bind_pose = _inverse_bind_poses[0];
        log_matrix(inverse_bind_pose);
    }
}

// dispose declared here
DECLARE_WORLD_OBJECT_REGISTER(SkinResource)

// ISkinImporter
luisa::string &ISkinImporter::name_ref(SkinResource *resource) {
    return resource->_name;
}
luisa::vector<luisa::string> &ISkinImporter::joint_remaps_ref(SkinResource *resource) {
    return resource->_joint_remaps;
}
luisa::vector<AnimFloat4x4> &ISkinImporter::inverse_bind_poses_ref(SkinResource *resource) {
    return resource->_inverse_bind_poses;
}
luisa::vector<BoneIndexType> &ISkinImporter::joint_remaps_LUT_ref(SkinResource *resource) {
    return resource->_joint_remaps_LUT;
}
RC<SkeletonResource> &ISkinImporter::ref_skel_ref(SkinResource *resource) {
    return resource->ref_skel;
}
RC<world::MeshResource> &ISkinImporter::ref_mesh_ref(SkinResource *resource) {
    return resource->ref_mesh;
}

}// namespace rbc::world

namespace rbc::world {

void SkinResource::generate_LUT() {
    if (!ref_skel || !ref_mesh) {
        LUISA_ERROR("Skeleton or mesh not set");
        return;
    }

    _joint_remaps_LUT.resize(_joint_remaps.size());
    auto *skel = ref_skel.get();
    for (size_t i = 0; i < _joint_remaps.size(); i++) {
        auto it = std::find(
            skel->ref_skel().raw_joint_names().begin(),
            skel->ref_skel().raw_joint_names().end(), _joint_remaps[i]);
        if (it == skel->ref_skel().raw_joint_names().end()) {
            LUISA_ERROR("Joint {} not found in skeleton", _joint_remaps[i]);
            return;
        }
        _joint_remaps_LUT[i] = static_cast<BoneIndexType>(it - skel->ref_skel().raw_joint_names().begin());
    }
}

}// namespace rbc::world

#include "rbc_world/importers/skin_importer_gltf.h"

namespace rbc {

bool GltfSkinImporter::import(SkinResource *resource, luisa::filesystem::path const &path) {
    return false;
}
bool GltfSkinImporter::import_from_data(SkinResource *resource, GltfImportData &import_data) {
    // 由于ozz::math::Float4x4 和 gltf matrix 都是列主序，所以可以直接复制
    return false;
}

}// namespace rbc
#include <rbc_world/resource_importer.h>
#include <rbc_world/resources/mesh.h>
#include <rbc_world/resources/texture.h>
#include <rbc_core/runtime_static.h>
#include <algorithm>
#include <cctype>

namespace rbc::world {

bool IResourceImporter::can_import(luisa::filesystem::path const &path) const {
    auto ext = normalize_extension(path.extension().string());
    return ext == extension();
}

luisa::string normalize_extension(luisa::string_view ext) {
    luisa::string result;
    if (ext.empty()) return result;

    result = luisa::to_string(ext);
    for (auto &c : result) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    // Ensure it starts with a dot
    if (!result.empty() && result[0] != '.') {
        result.insert(result.begin(), '.');
    }

    return result;
}

void ResourceImporterRegistry::register_importer(IResourceImporter *importer) {
    if (!importer) return;

    auto ext = normalize_extension(importer->extension());
    auto key = std::make_pair(ext, importer->resource_type());

    std::lock_guard lock(_mtx);
    auto iter = _importers.try_emplace(key);
    iter.first.value() = importer;
}

void ResourceImporterRegistry::unregister_importer(luisa::string_view extension, ResourceType type) {
    auto ext = normalize_extension(extension);
    auto key = std::make_pair(ext, type);

    std::lock_guard lock(_mtx);
    auto iter = _importers.find(key);
    if (iter) {
        // vstd::HashMap doesn't have erase, so we set to nullptr or use a different approach
        // For now, we'll just leave it (unregistering is rarely needed)
        // If needed, we could use a flag or rebuild the map
    }
}

IResourceImporter *ResourceImporterRegistry::find_importer(luisa::string_view extension, ResourceType type) const {
    auto ext = normalize_extension(extension);
    auto key = std::make_pair(ext, type);

    std::lock_guard lock(_mtx);
    auto iter = _importers.find(key);
    return iter ? iter.value() : nullptr;
}

IResourceImporter *ResourceImporterRegistry::find_importer(luisa::filesystem::path const &path, ResourceType type) const {
    // First try exact match
    auto ext = normalize_extension(path.extension().string());
    auto key = std::make_pair(ext, type);

    std::lock_guard lock(_mtx);
    auto iter = _importers.find(key);
    if (iter) {
        auto *importer = iter.value();
        // Verify the importer can actually handle this path (for multi-extension importers)
        if (importer->can_import(path)) {
            return importer;
        }
    }
    if (type == ResourceType::Texture) {
        if (ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".gif" || ext == ".psd" || ext == ".pnm" || ext == ".tga") {
            auto alt_ext = ".png";
            auto alt_key = std::make_pair(alt_ext, type);
            auto alt_iter = _importers.find(alt_key);

            if (alt_iter) {
                auto *importer = alt_iter.value();
                if (importer->can_import(path)) {
                    return importer;
                }
            }
        }
    }

    return nullptr;
}
RuntimeStatic<ResourceImporterRegistry> _resource_importer_registry;
ResourceImporterRegistry &ResourceImporterRegistry::instance() {
    return *_resource_importer_registry.ptr;
}

// IMeshImporter accessor method implementations
// rbc::DeviceMesh* IMeshImporter::device_mesh_ref(MeshResource *resource) {
//     return resource->device_mesh();
// }

// uint32_t &IMeshImporter::vertex_count_ref(MeshResource *resource) {
//     return resource->_vertex_count;
// }

// uint32_t &IMeshImporter::triangle_count_ref(MeshResource *resource) {
//     return resource->_triangle_count;
// }

// uint32_t &IMeshImporter::uv_count_ref(MeshResource *resource) {
//     return resource->_uv_count;
// }

// bool IMeshImporter::contained_normal(MeshResource *resource) {
//     return resource->_contained_normal;
// }

// void IMeshImporter::set_contained_normal(MeshResource *resource, bool value) {
//     resource->_contained_normal = value;
// }

// bool IMeshImporter::contained_tangent(MeshResource *resource) {
//     return resource->_contained_tangent;
// }

// void IMeshImporter::set_contained_tangent(MeshResource *resource, bool value) {
//     resource->_contained_tangent = value;
// }

// vstd::vector<uint> &IMeshImporter::submesh_offsets_ref(MeshResource *resource) {
//     return resource->_submesh_offsets;
// }

// ITextureImporter accessor method implementations
RC<rbc::DeviceResource> &ITextureImporter::tex_ref(TextureResource *resource) {
    return resource->_tex;
}

luisa::uint2 &ITextureImporter::size_ref(TextureResource *resource) {
    return resource->_size;
}

LCPixelStorage &ITextureImporter::pixel_storage_ref(TextureResource *resource) {
    return resource->_pixel_storage;
}

uint32_t &ITextureImporter::mip_level_ref(TextureResource *resource) {
    return resource->_mip_level;
}

bool &ITextureImporter::is_vt_ref(TextureResource *resource) {
    return resource->_is_vt;
}

}// namespace rbc::world

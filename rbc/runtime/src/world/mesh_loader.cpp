#include <rbc_world/resources/mesh.h>
#include <rbc_graphics/device_assets/device_mesh.h>
#include <tiny_obj_loader.h>
#include <luisa/core/fiber.h>
#include <luisa/runtime/rtx/triangle.h>
#include <rbc_graphics/mesh_builder.h>
#include <rbc_world/resource_importer.h>
#include <rbc_world/importers/mesh_importer_obj.h>

namespace rbc::world {
using namespace luisa;
using namespace luisa::compute;

bool MeshResource::decode(luisa::filesystem::path const &path) {
    if (!empty()) [[unlikely]] {
        LUISA_WARNING("Can not create on exists mesh.");
        return false;
    }
    
    auto &registry = ResourceImporterRegistry::instance();
    auto *importer = registry.find_importer(path, ResourceType::Mesh);
    
    if (!importer) {
        LUISA_WARNING("No importer found for mesh file: {}", luisa::to_string(path));
        return false;
    }
    
    auto *mesh_importer = dynamic_cast<IMeshImporter*>(importer);
    if (!mesh_importer) {
        LUISA_WARNING("Invalid importer type for mesh file: {}", luisa::to_string(path));
        return false;
    }
    
    return mesh_importer->import(this, path);
}

}// namespace rbc::world

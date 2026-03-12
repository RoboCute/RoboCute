#include <rbc_importer/scene_asset_importer.h>

namespace rbc::world {

SceneAssetImporterRegistry &SceneAssetImporterRegistry::instance() {
    static SceneAssetImporterRegistry registry;
    return registry;
}

void SceneAssetImporterRegistry::register_importer(ImporterPtr importer) {
    if (importer) {
        _importers.push_back(std::move(importer));
    }
}

ISceneAssetImporter *SceneAssetImporterRegistry::find_importer(
    luisa::filesystem::path const &path) const {
    
    for (const auto &importer : _importers) {
        if (importer->can_import(path)) {
            return importer.get();
        }
    }
    return nullptr;
}

ISceneAssetImporter *SceneAssetImporterRegistry::find_importer_by_name(
    luisa::string_view name) const {
    
    for (const auto &importer : _importers) {
        if (importer->name() == name) {
            return importer.get();
        }
    }
    return nullptr;
}

luisa::span<const SceneAssetImporterRegistry::ImporterPtr> 
SceneAssetImporterRegistry::all_importers() const {
    return _importers;
}

} // namespace rbc::world

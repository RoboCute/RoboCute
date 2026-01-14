#include <rbc_world/importers/scene_importer.h>

namespace rbc::world {
bool SceneImporter::import(Resource *res, luisa::filesystem::path const &path) {
    return static_cast<SceneResource *>(res)->load_from_json(path);
}

}// namespace rbc::world
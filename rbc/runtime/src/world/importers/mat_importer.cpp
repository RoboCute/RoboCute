#include <rbc_world/importers/mat_importer.h>
#include <rbc_world/resources/material.h>

namespace rbc::world {
luisa::string_view MatJsonImporter::extension() const {
    return ".json";
}
bool MatJsonImporter::import(Resource *resource_base, luisa::filesystem::path const &path) {
    auto resource = static_cast<MaterialResource *>(resource_base);
    luisa::BinaryFileStream file_stream(luisa::to_string(path));
    if (!file_stream.valid()) return false;
    luisa::vector<std::byte> vec;
    vec.push_back_uninitialized(file_stream.length());
    file_stream.read(vec);
    resource->load_from_json(
        {(char *)vec.data(),
         vec.size()});
    return true;
}
}// namespace rbc::world
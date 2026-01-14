#pragma once
#include <rbc_world/resource_importer.h>
#include <rbc_world/resources/scene.h>
namespace rbc::world {
struct SceneImporter : IResourceImporter {
    luisa::string_view extension() const override {
        return ".scene";
    }
    MD5 resource_type() const override {
        return TypeInfo::get<SceneResource>().md5();
    }
    bool import(Resource *res, luisa::filesystem::path const &path) override;
};
}// namespace rbc::world
#pragma once
#include <rbc_world/resource_importer.h>
namespace rbc::world {
struct IMaterialImporter : IResourceImporter {
    [[nodiscard]] MD5 resource_type() const override {
        return MD5{"rbc::world::MaterialResource"sv};
    }
};

struct RBC_RUNTIME_API MatJsonImporter final : IMaterialImporter {
    luisa::string_view extension() const;
    bool import(Resource *resource, luisa::filesystem::path const &path) override;
};
}// namespace rbc::world
#pragma once
#include "rbc_world/gpu_resource.h"
#include "rbc_world/resource_loader.h"

namespace rbc {

class RBC_WORLD_API MeshLoader : public ResourceLoader {
public:
    [[nodiscard]] bool can_load(const std::string &path) const override;
    luisa::shared_ptr<void> load(const std::string &path,
                                 const std::string &options_json) override;

    [[nodiscard]] size_t get_resource_size(const luisa::shared_ptr<void> &resource) const override {
        auto mesh = luisa::static_pointer_cast<Mesh>(resource);
        return mesh->get_memory_size();
    }

private:
    luisa::shared_ptr<Mesh> load_rbm(const std::string &path);
    luisa::shared_ptr<Mesh> load_obj(const std::string &path);
};

// Factory function
RBC_WORLD_API luisa::unique_ptr<ResourceLoader> create_mesh_loader();
}// namespace rbc
#include <rbc_world_v2/material.h>
#include "type_register.h"
namespace rbc::world {
struct MaterialImpl : Material {
    using BaseType = ResourceBaseImpl<Material>;

    bool loaded() const override {
        // return _device_mat.mat_code.value != ~0u;
        return true;
    }
    bool async_load_from_file() override {
        return true;
    }
    void unload() override {

    }
    void wait_load() const override {}
    void rbc_objser(JsonSerializer &ser) const {
        BaseType::rbc_objser(ser);
    }
    void rbc_objdeser(JsonDeSerializer &ser) {
        BaseType::rbc_objdeser(ser);
    }
    void dispose() override;
};
DECLARE_TYPE_REGISTER(Material);
}// namespace rbc::world
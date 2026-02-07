#include <rbc_world/components/data_component.h>
#include <rbc_world/type_register.h>

namespace rbc::world {
DataComponent::DataComponent() {}
DataComponent::~DataComponent() {}

void DataComponent::serialize_meta(ObjSerialize const &obj) const {
    // Serialize resources (just GUIDs)
    obj.ar.start_array();
    for (auto &pair : _resources) {
        obj.ar.value(pair.second->guid());
    }
    obj.ar.end_array("resources");

    // Serialize infos
    obj.ar.start_array();
    for (auto &pair : _infos) {
        obj.ar.value(pair.first);
        pair.second.visit([&](auto &&t) {
            obj.ar.value(t);
        });
    }
    obj.ar.end_array("infos");
}

void DataComponent::deserialize_meta(ObjDeSerialize const &obj) {
    // Deserialize resources (just GUIDs, resources loaded on demand)
    uint64_t resource_count;
    if (obj.ar.start_array(resource_count, "resources")) {
        _resources.reserve(resource_count);
        for (uint64_t i = 0; i < resource_count; ++i) {
            vstd::Guid guid;
            if (!obj.ar.value(guid)) break;
            auto res = load_resource(guid);
            if (!res) continue;
            _resources.try_emplace(std::move(guid), std::move(res));
        }
        obj.ar.end_scope();
    }

    // Deserialize infos
    uint64_t info_count;
    if (obj.ar.start_array(info_count, "infos")) {
        _infos.reserve(info_count / 2);
        for (auto i : vstd::range(info_count / 2)) {
            luisa::string key;
            BasicDeserDataType value;
            if (!obj.ar.value(key)) break;
            if (!obj.ar.value(value)) break;
            _infos.try_emplace(std::move(key), std::move(value));
        }
        obj.ar.end_scope();
    }
}

DECLARE_WORLD_OBJECT_REGISTER(DataComponent)
}// namespace rbc::world

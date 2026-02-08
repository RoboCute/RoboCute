#include <rbc_world/components/data_component.h>
#include <rbc_world/type_register.h>

namespace rbc::world {
DataComponent::DataComponent() {}
DataComponent::~DataComponent() {}

// Info API
BasicDeserDataType DataComponent::get_info(luisa::string_view name) const {
    auto iter = _infos.find(name);
    if (!~iter) {
        return BasicDeserDataType{};
    }
    return iter.value();
}

void DataComponent::set_info(luisa::string_view name, BasicDeserDataType const &data) {
    _infos.try_emplace(luisa::string{name}, data);
}

bool DataComponent::has_info(luisa::string_view name) const {
    return _infos.find(name);
}

bool DataComponent::remove_info(luisa::string_view name) {
    auto iter = _infos.find(name);
    if (iter) {
        _infos.remove(iter);
        return true;
    }
    return false;
}

uint64_t DataComponent::info_count() const noexcept {
    return _infos.size();
}

void DataComponent::clear_infos() noexcept {
    _infos.clear();
}

// Resource API
RC<Resource> DataComponent::get_resource(vstd::Guid const &guid) const {
    auto iter = _resources.find(guid);
    if (!iter) {
        return {};
    }
    return iter.value();
}

void DataComponent::set_resource(RC<Resource> const &resource) {
    LUISA_DEBUG_ASSERT(resource);
    _resources.try_emplace(resource->guid(), resource);
}

bool DataComponent::has_resource(vstd::Guid const &guid) const {
    return _resources.find(guid);
}

void DataComponent::remove_resource(vstd::Guid const &guid) {
    _resources.remove(guid);
}

uint64_t DataComponent::resource_count() const noexcept {
    return _resources.size();
}

void DataComponent::clear_resources() noexcept {
    _resources.clear();
}

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

#include "rbc_core/state_map.h"
#include <luisa/core/logging.h>
#include <luisa/vstl/v_guid.h>
namespace rbc {
void StateMap::_log_err_no_copy(luisa::string_view name) {
    LUISA_ERROR("Type {} has no copy constructor.", name);
}

void StateMap::init_json(luisa::string_view json) {
    std::scoped_lock lck{_map_mtx, _deser_mtx};
    _map.clear();
    _json_reader.reset();
    _json_reader.reset_as<luisa::string>(json);
}

void StateMap::_deser(TypeInfo const &type_info, HeapObject &heap_obj) {
    if (!_json_reader.valid() || !heap_obj.json_reader) return;
    if (_json_reader.is_type_of<luisa::string>()) {
        auto str = std::move(_json_reader.force_get<luisa::string>());
        _json_reader.reset_as<JsonDeSerializer>(str);
    }
    auto &json_reader = _json_reader.force_get<JsonDeSerializer>();
    auto name = type_info.name_c_str();
    if (json_reader.start_object(name)) {
        heap_obj.json_reader(heap_obj.data, &json_reader);
        json_reader.end_scope();
    }
}

luisa::BinaryBlob StateMap::serialize_to_json() {
    JsonSerializer ser;
    luisa::vector<luisa::string> strs;
    {
        std::lock_guard lck{_map_mtx};
        for (auto &i : _map) {
            auto &v = i.second;
            if (!v.json_writer) continue;
            auto &name = strs.emplace_back(i.first.name());
            ser.start_object();
            v.json_writer(v.data, &ser);
            ser.add_last_scope_to_object(name.c_str());
        }
    }
    return ser.write_to();
}

StateMap::~StateMap() {
}
}// namespace rbc
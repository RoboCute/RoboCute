#include "dummy_resource.h"
#include <rbc_core/binary_file_writer.h>
#include <rbc_world/type_register.h>
#include <luisa/core/binary_file_stream.h>
namespace rbc {
void DummyResource::serialize_meta(world::ObjSerialize const &ser) const {
    // write basic informations: type_id, file_path, etc...
    // write dependencies
    ser.ar.start_array();
    for (auto &i : _depended) {
        ser.ar.value(i->guid());
    }
    ser.ar.end_array("depend");
}
void DummyResource::deserialize_meta(world::ObjDeSerialize const &ser) {
    uint64_t size;
    LUISA_ASSERT(ser.ar.start_array(size, "depend"), "Bad meta");
    _depended.reserve(size);

    for (size_t i = 0; i < size; ++i) {
        vstd::Guid depended_guid{};
        LUISA_ASSERT(ser.ar.value(depended_guid), "Bad meta");
        LUISA_ASSERT(_depended.emplace_back(world::load_resource(depended_guid)), "Load depended failed.");
    }
    ser.ar.end_scope();
}
void DummyResource::create_empty(std::initializer_list<RC<DummyResource>> depended, luisa::string_view name) {
    _depended.clear();
    _value.clear();
    for (auto &i : depended) {
        _depended.emplace_back(i);
    }
    vstd::push_back_all(_value, luisa::span{name.data(), name.size()});
    // mark as loaded
    unsafe_set_loaded();
}
rbc::coroutine DummyResource::_async_load() {
    // load current binary
    luisa::BinaryFileStream fs(luisa::to_string(path()));
    LUISA_ASSERT(fs.valid());
    _value.resize_uninitialized(fs.length());
    fs.read({(std::byte *)_value.data(),
             _value.size_bytes()});
    // print result
    luisa::string print_result{value()};
    if (_depended.size() > 0) {
        print_result += "    Depending on: ";
        for (auto &i : _depended) {
            // await on depended resource
            co_await i->await_loading();
            print_result += i->value();
            print_result += ", ";
        }
    }
    LUISA_INFO("Value: {}", print_result);
    co_return;
}
bool DummyResource::unsafe_save_to_path() const {
    BinaryFileWriter writer{luisa::to_string(path())};
    writer.write({(std::byte *)_value.data(),
                  _value.size_bytes()});
    return true;
}

luisa::string_view DummyResource::value() const {
    return {_value.data(), _value.size()};
}

DECLARE_WORLD_OBJECT_REGISTER(DummyResource)
}// namespace rbc
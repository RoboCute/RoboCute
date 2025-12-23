#include "dummy_json_resource.h"
#include <rbc_core/binary_file_writer.h>
#include <rbc_world_v2/type_register.h>
#include <rbc_world_v2/resource_loader.h>
#include <luisa/core/binary_file_stream.h>
namespace rbc {
void DummyJsonResource::serialize_meta(world::ObjSerialize const &ser) const {
    // write basic informations: type_id, file_path, etc...
    BaseType::serialize_meta(ser);
    // write dependencies
    ser.ser.start_array();
    for (auto &i : _depended) {
        ser.ser._store(i->guid());
    }
    ser.ser.add_last_scope_to_object("depend");
}
void DummyJsonResource::deserialize_meta(world::ObjDeSerialize const &ser) {
    BaseType::deserialize_meta(ser);
    size_t size;
    LUISA_ASSERT(ser.ser.start_array(size, "depend"), "Bad meta");
    _depended.reserve(size);

    for (size_t i = 0; i < size; ++i) {
        vstd::Guid depended_guid{};
        LUISA_ASSERT(ser.ser._load(depended_guid), "Bad meta");
        LUISA_ASSERT(_depended.emplace_back(world::load_resource(depended_guid)), "Load depended failed.");
    }
    ser.ser.end_scope();
}
void DummyJsonResource::create_empty(std::initializer_list<RC<DummyJsonResource>> depended, luisa::string_view name) {
    _depended.clear();
    _value.clear();
    for (auto &i : depended) {
        _depended.emplace_back(i);
    }
    vstd::push_back_all(_value, luisa::span{name.data(), name.size()});
}
rbc::coro::coroutine DummyJsonResource::_async_load() {
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
bool DummyJsonResource::unsafe_save_to_path() const {
    BinaryFileWriter writer{luisa::to_string(path())};
    writer.write({(std::byte *)_value.data(),
                  _value.size_bytes()});
    return true;
}
void DummyJsonResource::_unload() {
    _value.clear();
    _depended.clear();
}
luisa::string_view DummyJsonResource::value() const {
    return {_value.data(), _value.size()};
}

DECLARE_WORLD_OBJECT_REGISTER(DummyJsonResource)
}// namespace rbc
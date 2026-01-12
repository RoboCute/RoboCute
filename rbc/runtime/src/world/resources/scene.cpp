#include <rbc_world/resources/scene.h>
#include <rbc_world/type_register.h>
#include <rbc_world/entity.h>
#include <rbc_core/binary_file_writer.h>
#include <luisa/core/binary_file_stream.h>

namespace rbc::world {
using namespace luisa;
rbc::coroutine SceneResource::_async_load() {
    auto path = this->path();
    if (path.empty()) co_return;
    BinaryFileStream file_stream(luisa::to_string(path));
    if (!file_stream.valid()) co_return;
    luisa::vector<char> json_vec;
    json_vec.push_back_uninitialized(file_stream.length());
    file_stream.read(
        {reinterpret_cast<std::byte *>(json_vec.data()),
         json_vec.size()});
    JsonDeSerializer deser{luisa::string_view{json_vec.data(), json_vec.size()}};
    LUISA_ASSERT(deser.valid());
    ArchiveReadJson read_adapter(deser);
    uint64_t size = deser.last_array_size();
    _entities.reserve(size);
    for (auto i : vstd::range(size)) {
        auto e = _entities.emplace_back(world::create_object<world::Entity>());
        read_adapter.start_object();
        e->deserialize_meta(world::ObjDeSerialize{read_adapter});
        read_adapter.end_scope();
    }
    co_return;
}
SceneResource::SceneResource() {}
SceneResource::~SceneResource() {}
bool SceneResource::unsafe_save_to_path() const {
    JsonSerializer SceneResource_ser{true};
    ArchiveWriteJson SceneResource_adapter(SceneResource_ser);
    world::ObjSerialize ser{SceneResource_adapter};
    for (auto &i : _entities) {
        SceneResource_adapter.start_object();
        i->serialize_meta(ser);
        SceneResource_adapter.end_object();
    }
    BinaryFileWriter file_writer(luisa::to_string(path()));
    file_writer.write(SceneResource_ser.write_to());
    return true;
}

void SceneResource::update_data() {
    for (auto &i : _entities) {
        i->unsafe_call_update();
    }
}

void SceneResource::enable() {
    if (_enabled.exchange(true)) return;
    for (auto &i : _entities) {
        i->unsafe_call_awake();
    }
    for (auto &i : _entities) {
        i->unsafe_call_update();
    }
}
DECLARE_WORLD_OBJECT_REGISTER(SceneResource)
}// namespace rbc::world
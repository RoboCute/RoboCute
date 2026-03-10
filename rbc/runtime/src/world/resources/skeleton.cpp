#include "rbc_world/resources/skeleton.h"
#include "rbc_world/type_register.h"
#include <rbc_core/binary_file_writer.h>
#include <luisa/core/binary_file_stream.h>

namespace rbc::world {

void SkeletonResource::serialize_meta(world::ObjSerialize const &ser) const {
    ser.ar.value(skeleton, "skeleton");
}

void SkeletonResource::deserialize_meta(world::ObjDeSerialize const &ser) {

    ser.ar.value(skeleton, "skeleton");
}

rbc::coroutine SkeletonResource::_async_load() {
    std::shared_lock lck{_async_mtx};
    auto path = this->path();
    luisa::BinaryFileStream file_stream(luisa::to_string(path));
    if (!file_stream.valid()) { co_return; }

    luisa::BinaryBlob blob = file_stream.read(file_stream.length());
    BinDeSerializer deser{blob};
    deser._load(skeleton, "skeleton");

    co_return;
}

bool SkeletonResource::unsafe_save_to_path() const {
    std::shared_lock lck{_async_mtx};
    BinSerializer ser;
    ser._store(skeleton, "skeleton");

    auto path = this->path();
    BinaryFileWriter writer{luisa::to_string(path)};
    if (!writer._file) [[unlikely]] {
        return false;
    }
    LUISA_INFO("Skeleton Writing to {}", path.string());
    auto bytes = ser.write_to();
    writer.write(bytes);
    return true;
}
void SkeletonResource::log_brief() {
    skeleton.log_brief();
}

// dispose declared here
DECLARE_WORLD_OBJECT_REGISTER(SkeletonResource)

}// namespace rbc::world

bool rbc::Serialize<rbc::world::SkeletonResource>::write(rbc::ArchiveWrite &w, const rbc::world::SkeletonResource &v) {
    rbc::world::ObjSerialize ser_obj{w};
    v.serialize_meta(ser_obj);
    return true;
}

bool rbc::Serialize<rbc::world::SkeletonResource>::read(rbc::ArchiveRead &r, rbc::world::SkeletonResource &v) {
    rbc::world::ObjDeSerialize deser_obj{r};
    v.deserialize_meta(deser_obj);
    return true;
}

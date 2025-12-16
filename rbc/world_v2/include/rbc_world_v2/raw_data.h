#include <rbc_world_v2/resource_base.h>
#include <rbc_graphics/device_assets/device_buffer.h>
namespace rbc::world {
using namespace luisa;
using namespace luisa::compute;
struct RBC_WORLD_API RawData final : ResourceBaseImpl<RawData> {
    DECLARE_WORLD_OBJECT_FRIEND(RawData)
private:
    RC<DeviceBuffer> _device_buffer;
    mutable rbc::shared_atomic_mutex _async_mtx;
    bool _upload_device{false};
    RawData();
    ~RawData();
public:
    luisa::vector<std::byte> *host_data() const;
    auto const &device_buffer() const { return _device_buffer; }
    void create_empty(
        size_t size_bytes,
        bool upload_device);
    bool init_device_resource() override;
    bool loaded() const override;
    void serialize(ObjSerialize const &obj) const override;
    void deserialize(ObjDeSerialize const &obj) override;
    void dispose() override;
    bool async_load_from_file() override;
    void unload() override;
    void wait_load() const override;
protected:
    bool unsafe_save_to_path() const override;
};
}// namespace rbc::world
RBC_RTTI(rbc::world::RawData)

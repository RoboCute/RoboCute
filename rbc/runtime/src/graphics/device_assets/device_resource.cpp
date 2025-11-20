#include <rbc_graphics/device_assets/device_resource.h>
#include <rbc_graphics/device_assets/assets_manager.h>
namespace rbc
{
bool DeviceResource::load_finished() const
{
    return _gpu_load_frame == 0 || _gpu_load_frame <= AssetsManager::instance()->load_finished_index();
}
void DeviceResource::sync_wait()
{
    AssetsManager::instance()->wake_load_thread();
    while (!load_finished())
    {
        std::this_thread::yield();
    }
}
} // namespace rbc
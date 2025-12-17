#include <rbc_graphics/device_assets/device_resource.h>
#include <rbc_graphics/device_assets/assets_manager.h>
namespace rbc {
bool DeviceResource::load_finished() const {
    return _gpu_load_frame != 0 && _gpu_load_frame <= AssetsManager::instance()->load_finished_index();
}
bool DeviceResource::load_executed() const {
    return _gpu_load_frame != 0 && _gpu_load_frame <= AssetsManager::instance()->load_executed_index();
}
void DeviceResource::wait_executed() {
    if (_gpu_load_frame == 0) return;
    AssetsManager::instance()->wake_load_thread();
    while (_gpu_load_frame > AssetsManager::instance()->load_executed_index()) {
        std::this_thread::yield();
    }
}
void DeviceResource::wait_finished() {
    if (_gpu_load_frame == 0) return;
    AssetsManager::instance()->wake_load_thread();
    while (_gpu_load_frame > AssetsManager::instance()->load_finished_index()) {
        std::this_thread::yield();
    }
}
}// namespace rbc
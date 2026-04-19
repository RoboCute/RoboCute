#include <rbc_graphics/device_assets/device_resource.h>
#include <rbc_graphics/device_assets/assets_manager.h>
#include <thread>
namespace rbc {
namespace {
template<typename F>
void spin_wait_for(F &&condition) {
    while (condition()) {
        std::this_thread::yield();
    }
}
}// namespace

bool DeviceResource::load_finished() const {
    auto const gpu_frame = _gpu_load_frame.load();
    return gpu_frame != 0 && gpu_frame <= AssetsManager::instance()->load_finished_index();
}
bool DeviceResource::load_executed() const {
    auto const gpu_frame = _gpu_load_frame.load();
    return gpu_frame != 0 && gpu_frame <= AssetsManager::instance()->load_executed_index();
}
void DeviceResource::wait_executed() const {
    auto const gpu_frame = _gpu_load_frame.load();
    if (gpu_frame == 0) return;
    AssetsManager::instance()->wake_load_thread();
    spin_wait_for([gpu_frame] {
        return gpu_frame > AssetsManager::instance()->load_executed_index();
    });
}
void DeviceResource::wait_finished() const {
    auto const gpu_frame = _gpu_load_frame.load();
    if (gpu_frame == 0) return;
    AssetsManager::instance()->wake_load_thread();
    spin_wait_for([gpu_frame] {
        return gpu_frame > AssetsManager::instance()->load_finished_index();
    });
}
}// namespace rbc
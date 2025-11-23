#include <rbc_world/system.h>
#include <algorithm>

namespace rbc {

void SystemScheduler::update_all_systems(float delta_time) {
    for (auto &system : systems_) {
        if (system) {
            system->update(delta_time);
        }
    }
}

void SystemScheduler::sort_systems_by_priority() {
    // Sort systems by priority (higher priority first)
    std::sort(systems_.begin(), systems_.end(),
              [](const luisa::unique_ptr<ISystem> &a, const luisa::unique_ptr<ISystem> &b) {
                  return a->get_priority() > b->get_priority();
              });
}

}// namespace rbc

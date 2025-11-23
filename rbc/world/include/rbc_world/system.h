#pragma once

namespace rbc {

using SystemTypeID = uint32_t;

class ISystem {
public:
    virtual ~ISystem() = default;
    virtual void initialize() {}
    virtual void update(float delta_time) = 0;
    virtual void shutdown() {}
    [[nodiscard]] virtual int get_priority() const { return 0; }
    [[nodiscard]] virtual bool is_parallel_safe() const { return false; }

    // system dependencies
    [[nodiscard]] virtual luisa::vector<SystemTypeID> get_dependencies() const { return {}; }
};

class RBC_WORLD_API SystemScheduler {
public:
    SystemScheduler() = default;
    ~SystemScheduler() = default;
    // delete copy and move
    SystemScheduler(const SystemScheduler &) = delete;
    SystemScheduler(SystemScheduler &&) = delete;
    SystemScheduler &operator=(const SystemScheduler &) = delete;
    SystemScheduler &operator=(SystemScheduler &&) = delete;

    // Register a system
    template<typename T, typename... Args>
    void register_system(Args &&...args) {
        auto system = luisa::make_unique<T>(std::forward<Args>(args)...);
        systems_.emplace_back(std::move(system));
        sort_systems_by_priority();
    }

    // Update all systems
    void update_all_systems(float delta_time);

    // Get system count
    [[nodiscard]] size_t system_count() const { return systems_.size(); }

private:
    luisa::vector<luisa::unique_ptr<ISystem>> systems_;

    void sort_systems_by_priority();
};

}// namespace rbc
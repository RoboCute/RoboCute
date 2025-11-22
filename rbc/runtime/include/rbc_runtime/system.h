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

class SystemScheduler {
};

}// namespace rbc
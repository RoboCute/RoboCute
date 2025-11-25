#pragma once

#include "rbc_world/resource.h"
#include <memory>
#include <string>
#include <string_view>

namespace rbc {

/**
 * ResourceLoader base class
 * - Each resource type implements its own loader
 */
class ResourceLoader {
public:
    virtual ~ResourceLoader() = default;

    [[nodiscard]] virtual bool can_load(const std::string &path) const = 0;
    virtual std::shared_ptr<void> load(const std::string &path,
                                       const std::string &options_json) = 0;
    [[nodiscard]] virtual size_t get_resource_size(const std::shared_ptr<void> &resource) const = 0;
};

}// namespace rbc

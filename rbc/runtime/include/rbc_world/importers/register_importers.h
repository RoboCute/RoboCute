#pragma once
#include <rbc_config.h>

namespace rbc::world {

/**
 * @brief Register all built-in resource importers
 * This function should be called during system initialization
 */
RBC_RUNTIME_API void register_builtin_importers();

}// namespace rbc::world


#pragma once

namespace rbc {

enum struct EventType {
    // Core Layout

    // Scene
    SceneUpdated,
    EntitySelected,
    EntityCreated,
    EntityDeleted,
    EntityModified,

    // Custom (Start from 1000)
    Custom = 1000
};

}// namespace rbc
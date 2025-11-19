#pragma once

#include <array>

namespace rbc
{
struct MatVolumeHandle {
    uint index{ ~0u };
};
struct MatImageHandle {
    uint index{ ~0u };
};
struct MatBufferHandle {
    uint index{ ~0u };
};
} // namespace rbc
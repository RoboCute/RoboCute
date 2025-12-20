#pragma once

namespace rbc {
using ResourceID = uint64_t;
constexpr ResourceID INVALID_RESOURCE = 0;

struct RenderMesh {};

template<typename T>
struct AsyncResource {};

}// namespace rbc
#pragma once

#include <luisa/vstl/vector.h>
#include "rbc_anim/types.h"

namespace rbc {

struct ReferenceSkeleton final {
public:
    ReferenceSkeleton();
    ~ReferenceSkeleton();
    ReferenceSkeleton(const ReferenceSkeleton &) = delete;
    ReferenceSkeleton &operator=(const ReferenceSkeleton &) = delete;
    ReferenceSkeleton(ReferenceSkeleton &&Other);
    ReferenceSkeleton &operator=(ReferenceSkeleton &&Other);
    explicit ReferenceSkeleton(SkeletonRuntimeAsset &&InSkeleton);
    ReferenceSkeleton &operator=(SkeletonRuntimeAsset &&InSkeleton);

public:
    [[nodiscard]] const SkeletonRuntimeAsset &GetRawSkeleton() const { return skeleton; }
private:
    SkeletonRuntimeAsset skeleton;
};

}// namespace rbc

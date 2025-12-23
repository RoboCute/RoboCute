#include <rbc_anim/asset/reference_skeleton.h>

namespace rbc {
ReferenceSkeleton::ReferenceSkeleton() {}
ReferenceSkeleton::~ReferenceSkeleton() {}
ReferenceSkeleton::ReferenceSkeleton(ReferenceSkeleton &&Other) {
    skeleton = std::move(Other.skeleton);
}
ReferenceSkeleton &ReferenceSkeleton::operator=(ReferenceSkeleton &&Other) {
    skeleton = std::move(Other.skeleton);
    return *this;
}
// expclit construct from raw
ReferenceSkeleton::ReferenceSkeleton(SkeletonRuntimeAsset &&InSkeleton) {
    skeleton = std::move(InSkeleton);
}
ReferenceSkeleton &ReferenceSkeleton::operator=(SkeletonRuntimeAsset &&InSkeleton) {
    skeleton = std::move(InSkeleton);
    return *this;
}

}// namespace rbc
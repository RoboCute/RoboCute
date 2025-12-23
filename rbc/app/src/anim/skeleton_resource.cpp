#include "rbc_anim/resources/skeleton_resource.h"

namespace rbc {

SkeletonResource::SkeletonResource() {}
SkeletonResource::~SkeletonResource() {}

void SkeletonResource::serialize_meta(world::ObjSerialize const &ser) const {}
void SkeletonResource::deserialize_meta(world::ObjDeSerialize const &ser) {}
void SkeletonResource::dispose() {
}

}// namespace rbc
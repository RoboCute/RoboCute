#pragma once

#include <lighting/area_light.hpp>
#include <lighting/disk_light.hpp>
#include <lighting/mesh_light.hpp>
#include <lighting/point_light.hpp>
#include <lighting/spot_light.hpp>
#include <lighting/types.hpp>
#include <std/ex/type_list.hpp>

namespace lighting {

using PolymorphicLight = stdex::type_list<
    PointLight,
    SpotLight,
    AreaLight,
    MeshLight,
    DiskLight>;

}// namespace lighting

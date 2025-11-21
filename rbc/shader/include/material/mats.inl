#pragma once

#include <std/ex/type_list.hpp>
#include <material/openpbr.hpp>
#include <material/unlit.hpp>

namespace material {

using PolymorphicMaterial = stdex::type_list<OpenPBR, Unlit>;

}// namespace material

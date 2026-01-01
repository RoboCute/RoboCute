/**
 * @file type.cpp
 * @brief The Impl of IRType
 * @author sailing-innocent
 * @date 2024-05-30
 */

#include "rbc_particle/ir/type.h"

namespace rbc::ps {

luisa::string_view IRType::identifier() const noexcept {
    using namespace std::string_view_literals;
    switch (m_primitive) {
        case Primitive::I32:
            return "int32"sv;
        case Primitive::F32:
            return "float32"sv;
        case Primitive::U32:
            return "uint32"sv;
        case Primitive::BUFFER_F32:
            return "buffer_f32"sv;
        case Primitive::BUFFER_I32:
            return "buffer_i32"sv;
        case Primitive::VECTOR:
            return "vector"sv;
        case Primitive::MATRIX:
            return "matrix"sv;
    }
    LUISA_ERROR_WITH_LOCATION("Unknown primitive type.");
}

luisa::string IRType::dump() const noexcept {
    return luisa::string(identifier());
}

const luisa::compute::Type *IRType::to_lc_type() const noexcept {
    using namespace luisa::compute;
    switch (m_primitive) {
        case Primitive::I32:
            return Type::of<int>();
        case Primitive::F32:
            return Type::of<float>();
        case Primitive::U32:
            return Type::of<uint>();
        // TODO
        default:
            return Type::of<int>();
    }
}

}// namespace rbc::ps
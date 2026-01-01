#pragma once
#include <luisa/ast/type.h>

namespace rbc::ps {

class IRType {
public:
    enum struct Primitive {
        I32,
        F32,
        U32,
        BUFFER_F32,
        BUFFER_I32,
        VECTOR,
        MATRIX
    };

private:
    Primitive m_primitive;

public:
    explicit IRType(Primitive p) noexcept
        : m_primitive(p) {
    }
    // disable copy and move
    IRType(const IRType &) = delete;
    IRType &operator=(const IRType &) = delete;
    IRType(IRType &&) = delete;
    IRType &operator=(IRType &&) = delete;
    ~IRType() noexcept = default;

    [[nodiscard]] Primitive primitive() const noexcept { return m_primitive; }

    [[nodiscard]] bool is_int() const noexcept { return m_primitive == Primitive::I32; }
    [[nodiscard]] bool is_uint() const noexcept { return m_primitive == Primitive::U32; }
    [[nodiscard]] bool is_float() const noexcept { return m_primitive == Primitive::F32; }

    [[nodiscard]] bool is_buffer() const noexcept { return m_primitive == Primitive::BUFFER_F32 || m_primitive == Primitive::BUFFER_I32; }
    [[nodiscard]] bool is_vector() const noexcept { return m_primitive == Primitive::VECTOR; }
    [[nodiscard]] bool is_matrix() const noexcept { return m_primitive == Primitive::MATRIX; }
    [[nodiscard]] luisa::string dump() const noexcept;

    [[nodiscard]] luisa::string_view identifier() const noexcept;

    [[nodiscard]] const luisa::compute::Type *to_lc_type() const noexcept;
};

}// namespace rbc::ps
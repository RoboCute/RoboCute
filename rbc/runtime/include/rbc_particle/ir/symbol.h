#pragma once

#include <luisa/luisa-compute.h>

namespace rbc::ps {

class IRType;
class IRLiteral;

// symbol tree node
// e.g. a float4 symbol s may have 4 float symbols as children (s.x, s.y, s.z, s.w)
class Symbol final {
public:
    enum struct Tag {
        SYM_PARAM,
        SYM_LOCAL,
        SYM_TEMP,
        SYM_CONST,
        SYM_BUILTIN
    };

    [[nodiscard]] static luisa::string_view dump(Tag tag) noexcept;

private:
    Tag m_tag;
    int m_arr_len;
    const IRType *m_type;
    const Symbol *m_parent;
    luisa::string m_identifier;
    eastl::vector<IRLiteral> m_initial_values;
    eastl::vector<const Symbol *> m_children;

public:
    Symbol(Tag tag, const IRType *type, int arr_len, const Symbol *parent, luisa::string identifier, eastl::vector<IRLiteral> initial_value) noexcept;

    ~Symbol() noexcept;
    Symbol(Symbol &&) noexcept;
    Symbol(const Symbol &) noexcept = delete;
    Symbol &operator=(Symbol &&) noexcept = delete;
    Symbol &operator=(const Symbol &) noexcept = delete;

    [[nodiscard]] auto tag() const noexcept { return m_tag; }
    [[nodiscard]] auto type() const noexcept { return m_type; }
    [[nodiscard]] auto identifier() const noexcept { return luisa::string_view(m_identifier); }

    [[nodiscard]] eastl::span<const IRLiteral> initial_values() const noexcept;
    [[nodiscard]] auto children() const noexcept { return m_children; }
    [[nodiscard]] auto parent() const noexcept { return m_parent; }
    // hints

    // for debugging
    [[nodiscard]] luisa::string dump() const noexcept;
};

}// namespace rbc::ps
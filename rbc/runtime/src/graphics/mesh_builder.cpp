#include <rbc_graphics/mesh_builder.h>
#include <rbc_io/binary_file_writer.h>
#include <luisa/core/stl/format.h>
#include <luisa/core/logging.h>

namespace rbc {
MeshBuilder::MeshBuilder() = default;
MeshBuilder::~MeshBuilder() {}
MeshBuilder::MeshBuilder(MeshBuilder &&rhs) noexcept = default;
MeshBuilderSpan::MeshBuilderSpan() = default;
MeshBuilderSpan::~MeshBuilderSpan() {}
MeshBuilderSpan::MeshBuilderSpan(MeshBuilderSpan &&rhs) noexcept = default;
template<typename Derive>
uint MeshBuilderBase<Derive>::vertex_count() const {
    return static_cast<Derive const *>(this)->position.size();
}
template<typename Derive>
bool MeshBuilderBase<Derive>::contained_normal() const {
    return !static_cast<Derive const *>(this)->normal.empty();
}
template<typename Derive>
bool MeshBuilderBase<Derive>::contained_tangent() const {
    return !static_cast<Derive const *>(this)->tangent.empty();
}
template<typename Derive>
uint MeshBuilderBase<Derive>::uv_count() const {
    return static_cast<Derive const *>(this)->uvs.size();
}
template<typename Derive>
uint MeshBuilderBase<Derive>::submesh_count() const {
    return static_cast<Derive const *>(this)->triangle_indices.size();
}
template<typename Derive>
uint MeshBuilderBase<Derive>::indices_count() const {
    uint result = 0;
    for (auto &i : static_cast<Derive const *>(this)->triangle_indices) {
        result += i.size();
    }
    return result;
}
template<typename Derive>
luisa::string MeshBuilderBase<Derive>::check() const {
    luisa::string result;
    auto &&position = static_cast<Derive const *>(this)->position;
    auto &&normal = static_cast<Derive const *>(this)->normal;
    auto &&tangent = static_cast<Derive const *>(this)->tangent;
    auto &&uvs = static_cast<Derive const *>(this)->uvs;
    auto &&triangle_indices = static_cast<Derive const *>(this)->triangle_indices;

    if (!normal.empty() && normal.size() != position.size()) [[unlikely]] {
        result += luisa::format("Normal size {} not match position size. {}\n", normal.size(), position.size());
    };
    if (!tangent.empty() && tangent.size() != position.size()) [[unlikely]] {
        result += luisa::format("Tangent size {} not match position size. {}\n", tangent.size(), position.size());
    };
    size_t idx = 0;
    for (auto &uv : uvs) {
        if (!uv.empty() && uv.size() != position.size()) [[unlikely]] {
            result += luisa::format("UV{} size {} not match position size. {}\n", idx, uv.size(), position.size());
        };
        ++idx;
    }
    idx = 0;
    for (auto &i : triangle_indices) {
        if ((i.size() % 3) != 0) [[unlikely]] {
            result += luisa::format("Submesh {} size {} is illegal.\n", idx, i.size());
            ++idx;
            continue;
        }
        for (auto &j : i) {
            if (j >= position.size()) [[unlikely]] {
                result += luisa::format("At least one of indices in submesh {} out of vertex range {}.\n", idx, position.size());
                break;
            }
        }
        ++idx;
    }
    return result;
}
template<typename Derive>
luisa::span<uint const> MeshBuilderBase<Derive>::get_submesh_indices(size_t submesh_index) const {
    auto &&triangle_indices = static_cast<Derive const *>(this)->triangle_indices;
    return {triangle_indices[submesh_index]};
}

template<typename Derive>
void MeshBuilderBase<Derive>::write_to(luisa::filesystem::path const &dst_path, luisa::vector<uint> &submesh_offsets) const {
    auto &&position = static_cast<Derive const *>(this)->position;
    auto &&normal = static_cast<Derive const *>(this)->normal;
    auto &&tangent = static_cast<Derive const *>(this)->tangent;
    auto &&uvs = static_cast<Derive const *>(this)->uvs;
    auto &&triangle_indices = static_cast<Derive const *>(this)->triangle_indices;
#ifndef NDEBUG
    auto err_msg = check();
    if (!err_msg.empty()) [[unlikely]] {
        LUISA_ERROR("{}", err_msg);
    }
#endif
    BinaryFileWriter writer{luisa::to_string(dst_path)};
    writer.write({reinterpret_cast<const std::byte *>(position.data()), position.size_bytes()});
    writer.write({reinterpret_cast<const std::byte *>(normal.data()), normal.size_bytes()});
    writer.write({reinterpret_cast<const std::byte *>(tangent.data()), tangent.size_bytes()});
    for (auto &uv : uvs) {
        writer.write({reinterpret_cast<const std::byte *>(uv.data()), uv.size_bytes()});
    }
    for (auto &i : triangle_indices) {
        writer.write({reinterpret_cast<const std::byte *>(i.data()), i.size_bytes()});
    }
    _gen_submesh_offsets(submesh_offsets);
}
template<typename Derive>
void MeshBuilderBase<Derive>::write_to(luisa::vector<std::byte> &buffer, luisa::vector<uint> &submesh_offsets) const {
    auto &&position = static_cast<Derive const *>(this)->position;
    auto &&normal = static_cast<Derive const *>(this)->normal;
    auto &&tangent = static_cast<Derive const *>(this)->tangent;
    auto &&uvs = static_cast<Derive const *>(this)->uvs;
    auto &&triangle_indices = static_cast<Derive const *>(this)->triangle_indices;
#ifndef NDEBUG
    auto err_msg = check();
    if (!err_msg.empty()) [[unlikely]] {
        LUISA_ERROR("{}", err_msg);
    }
#endif
    buffer.clear();
    size_t capacity = position.size_bytes() + normal.size_bytes() + tangent.size_bytes();
    for (auto &uv : uvs) {
        capacity += uv.size_bytes();
    }
    for (auto &i : triangle_indices) {
        capacity += i.size_bytes();
    }
    buffer.reserve(capacity);
    vstd::push_back_all(buffer, {reinterpret_cast<const std::byte *>(position.data()), position.size_bytes()});
    vstd::push_back_all(buffer, {reinterpret_cast<const std::byte *>(normal.data()), normal.size_bytes()});
    vstd::push_back_all(buffer, {reinterpret_cast<const std::byte *>(tangent.data()), tangent.size_bytes()});
    for (auto &uv : uvs) {
        vstd::push_back_all(buffer, {reinterpret_cast<const std::byte *>(uv.data()), uv.size_bytes()});
    }
    for (auto &i : triangle_indices) {
        vstd::push_back_all(buffer, {reinterpret_cast<const std::byte *>(i.data()), i.size_bytes()});
    }
    _gen_submesh_offsets(submesh_offsets);
}
template<typename Derive>
void MeshBuilderBase<Derive>::_gen_submesh_offsets(luisa::vector<uint> &submesh_offsets) const {
    auto &&position = static_cast<Derive const *>(this)->position;
    auto &&normal = static_cast<Derive const *>(this)->normal;
    auto &&tangent = static_cast<Derive const *>(this)->tangent;
    auto &&uvs = static_cast<Derive const *>(this)->uvs;
    auto &&triangle_indices = static_cast<Derive const *>(this)->triangle_indices;
    submesh_offsets.clear();
    if (triangle_indices.size() <= 1) {
        return;
    }
    size_t offset = 0;
    submesh_offsets.reserve(triangle_indices.size());
    for (auto &i : triangle_indices) {
        submesh_offsets.emplace_back(offset);
        offset += i.size() / 3;
    }
}
uint MeshBuilder::vertex_count() const {
    return BaseType::vertex_count();
}
bool MeshBuilder::contained_normal() const {
    return BaseType::contained_normal();
}
bool MeshBuilder::contained_tangent() const {
    return BaseType::contained_tangent();
}
uint MeshBuilder::uv_count() const {
    return BaseType::uv_count();
}
uint MeshBuilder::submesh_count() const {
    return BaseType::submesh_count();
}
uint MeshBuilder::indices_count() const {
    return BaseType::indices_count();
}
luisa::span<uint const> MeshBuilder::get_submesh_indices(size_t submesh_index) const {
    return BaseType::get_submesh_indices(submesh_index);
}
luisa::string MeshBuilder::check() const {
    return BaseType::check();
}
// property write

void MeshBuilder::write_to(luisa::filesystem::path const &dst_path, luisa::vector<uint> &submesh_offsets) const {
    BaseType::write_to(dst_path, submesh_offsets);
}
void MeshBuilder::write_to(luisa::vector<std::byte> &buffer, luisa::vector<uint> &submesh_offsets) const {
    BaseType::write_to(buffer, submesh_offsets);
}

uint MeshBuilderSpan::vertex_count() const {
    return BaseType::vertex_count();
}
bool MeshBuilderSpan::contained_normal() const {
    return BaseType::contained_normal();
}
bool MeshBuilderSpan::contained_tangent() const {
    return BaseType::contained_tangent();
}
uint MeshBuilderSpan::uv_count() const {
    return BaseType::uv_count();
}
uint MeshBuilderSpan::submesh_count() const {
    return BaseType::submesh_count();
}
uint MeshBuilderSpan::indices_count() const {
    return BaseType::indices_count();
}
luisa::span<uint const> MeshBuilderSpan::get_submesh_indices(size_t submesh_index) const {
    return BaseType::get_submesh_indices(submesh_index);
}
luisa::string MeshBuilderSpan::check() const {
    return BaseType::check();
}
// property write

void MeshBuilderSpan::write_to(luisa::filesystem::path const &dst_path, luisa::vector<uint> &submesh_offsets) const {
    BaseType::write_to(dst_path, submesh_offsets);
}
void MeshBuilderSpan::write_to(luisa::vector<std::byte> &buffer, luisa::vector<uint> &submesh_offsets) const {
    BaseType::write_to(buffer, submesh_offsets);
}
}// namespace rbc
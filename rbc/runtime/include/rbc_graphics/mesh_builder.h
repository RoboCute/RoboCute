#pragma once
#include <luisa/core/binary_file_stream.h>
#include <luisa/core/mathematics.h>
#include <luisa/core/stl/vector.h>
#include <luisa/core/stl/filesystem.h>
#include <luisa/vstl/common.h>
#include <luisa/core/stl/string.h>
#include <rbc_config.h>
namespace rbc {
using namespace luisa;
template<typename Derive>
struct MeshBuilderBase {
    [[nodiscard]] uint vertex_count() const;
    [[nodiscard]] bool contained_normal() const;
    [[nodiscard]] bool contained_tangent() const;
    [[nodiscard]] uint uv_count() const;
    [[nodiscard]] uint submesh_count() const;
    [[nodiscard]] uint indices_count() const;
    [[nodiscard]] luisa::span<uint const> get_submesh_indices(size_t submesh_index) const;
    [[nodiscard]] luisa::string check() const;// return: error message
    // property write

    void write_to(luisa::filesystem::path const &dst_path, luisa::vector<uint> &submesh_offsets) const;
    void write_to(luisa::vector<std::byte> &buffer, luisa::vector<uint> &submesh_offsets) const;

private:
    void _gen_submesh_offsets(luisa::vector<uint> &submesh_offsets) const;
};
struct RBC_RUNTIME_API MeshBuilder : MeshBuilderBase<MeshBuilder> {
    using BaseType = MeshBuilderBase<MeshBuilder>;
    luisa::vector<float3> position;
    luisa::vector<float3> normal;
    luisa::vector<float4> tangent;
    luisa::vector<luisa::vector<float2>> uvs;
    luisa::vector<luisa::vector<uint>> triangle_indices;

    MeshBuilder();
    ~MeshBuilder();
    MeshBuilder(MeshBuilder &&rhs) noexcept;
    MeshBuilder(MeshBuilder const &) = delete;
    MeshBuilder &operator=(MeshBuilder const &) = delete;
    MeshBuilder &operator=(MeshBuilder &&rhs) noexcept {
        std::destroy_at(this);
        vstd::construct_at(this, std::move(rhs));
        return *this;
    }
    // property read
    [[nodiscard]] uint vertex_count() const;
    [[nodiscard]] bool contained_normal() const;
    [[nodiscard]] bool contained_tangent() const;
    [[nodiscard]] uint uv_count() const;
    [[nodiscard]] uint submesh_count() const;
    [[nodiscard]] uint indices_count() const;
    [[nodiscard]] luisa::span<uint const> get_submesh_indices(size_t submesh_index) const;
    [[nodiscard]] luisa::string check() const;// return: error message
    // property write

    void write_to(luisa::filesystem::path const &dst_path, luisa::vector<uint> &submesh_offsets) const;
    void write_to(luisa::vector<std::byte> &buffer, luisa::vector<uint> &submesh_offsets) const;

private:
    void _gen_submesh_offsets(luisa::vector<uint> &submesh_offsets) const;
};
struct RBC_RUNTIME_API MeshBuilderSpan : MeshBuilderBase<MeshBuilderSpan> {
    using BaseType = MeshBuilderBase<MeshBuilderSpan>;
    luisa::span<float3 const> position;
    luisa::span<float3 const> normal;
    luisa::span<float4 const> tangent;
    luisa::span<luisa::span<float2 const> const> uvs;
    luisa::span<luisa::span<uint const> const> triangle_indices;

    MeshBuilderSpan();
    ~MeshBuilderSpan();
    MeshBuilderSpan(MeshBuilderSpan &&rhs) noexcept;
    MeshBuilderSpan(MeshBuilderSpan const &) = delete;
    MeshBuilderSpan &operator=(MeshBuilderSpan const &) = delete;
    MeshBuilderSpan &operator=(MeshBuilderSpan &&rhs) noexcept {
        std::destroy_at(this);
        vstd::construct_at(this, std::move(rhs));
        return *this;
    }
    // property read
    [[nodiscard]] uint vertex_count() const;
    [[nodiscard]] bool contained_normal() const;
    [[nodiscard]] bool contained_tangent() const;
    [[nodiscard]] uint uv_count() const;
    [[nodiscard]] uint submesh_count() const;
    [[nodiscard]] uint indices_count() const;
    [[nodiscard]] luisa::span<uint const> get_submesh_indices(size_t submesh_index) const;
    [[nodiscard]] luisa::string check() const;// return: error message
    // property write

    void write_to(luisa::filesystem::path const &dst_path, luisa::vector<uint> &submesh_offsets) const;
    void write_to(luisa::vector<std::byte> &buffer, luisa::vector<uint> &submesh_offsets) const;

private:
    void _gen_submesh_offsets(luisa::vector<uint> &submesh_offsets) const;
};
}// namespace rbc
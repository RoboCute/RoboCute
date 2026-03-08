#include <rbc_world/resources/mesh.h>
#include <rbc_world/importers/mesh_importer_abc.h>
#include <rbc_graphics/device_assets/device_mesh.h>
#include <rbc_graphics/mesh_builder.h>
#include <luisa/core/binary_io.h>
#include <luisa/core/logging.h>
#include <luisa/runtime/rtx/triangle.h>

#include <Alembic/AbcCoreFactory/All.h>
#include <Alembic/AbcGeom/All.h>
#include <Alembic/Abc/All.h>

namespace rbc::world {
using namespace luisa;
using namespace luisa::compute;

// Alembic namespace aliases
namespace Abc = Alembic::Abc;
namespace AbcGeom = Alembic::AbcGeom;
namespace AbcCoreFactory = Alembic::AbcCoreFactory;

/**
 * @brief Recursively traverse Alembic object hierarchy and extract mesh data
 * @param object The current Alembic object to process
 * @param mesh_builder The mesh builder to populate
 * @param mesh_count Counter for meshes found
 */
static void traverse_alembic_object(
    AbcGeom::IObject const &object,
    MeshBuilder &mesh_builder,
    size_t &mesh_count) {
    if (!object.valid()) {
        return;
    }

    // Check if this object is a polymesh
    if (AbcGeom::IPolyMesh::matches(object.getHeader())) {
        AbcGeom::IPolyMesh mesh(object);
        if (!mesh.valid()) {
            return;
        }

        // Get schema and data
        AbcGeom::IPolyMeshSchema &schema = mesh.getSchema();

        // Get samples - use default time sampling (usually first frame for static meshes)
        AbcGeom::IPolyMeshSchema::Sample sample;
        if (!schema.getNumSamples() > 0) {
            schema.get(sample, 0);
        } else {
            return;
        }

        // Get positions
        AbcGeom::P3fArraySamplePtr positions = sample.getPositions();
        if (!positions || positions->size() == 0) {
            return;
        }

        // Get face indices and counts
        Abc::Int32ArraySamplePtr face_indices = sample.getFaceIndices();
        Abc::Int32ArraySamplePtr face_counts = sample.getFaceCounts();

        if (!face_indices || !face_counts) {
            return;
        }

        // Store vertex offset for this submesh
        uint32_t vertex_offset = static_cast<uint32_t>(mesh_builder.position.size());

        // Add positions
        mesh_builder.position.reserve(mesh_builder.position.size() + positions->size());
        for (size_t i = 0; i < positions->size(); ++i) {
            const AbcGeom::V3f &p = (*positions)[i];
            mesh_builder.position.push_back(make_float3(p.x, p.y, p.z));
        }

        // Process UVs if available
        AbcGeom::IV2fGeomParam uv_param = schema.getUVsParam();
        if (uv_param.valid() && mesh_builder.uvs.empty()) {
            mesh_builder.uvs.emplace_back();
        }

        if (uv_param.valid()) {
            AbcGeom::IV2fGeomParam::Sample uv_sample;
            uv_param.getIndexed(uv_sample, 0);

            if (uv_sample.valid()) {
                AbcGeom::V2fArraySamplePtr uvs = uv_sample.getVals();
                if (uvs && uvs->size() > 0) {
                    auto &uv_vec = mesh_builder.uvs[0];
                    size_t prev_size = uv_vec.size();
                    uv_vec.reserve(prev_size + uvs->size());
                    for (size_t i = 0; i < uvs->size(); ++i) {
                        const AbcGeom::V2f &uv = (*uvs)[i];
                        uv_vec.push_back(make_float2(uv.x, uv.y));
                    }
                }
            }
        }

        // Process normals if available
        AbcGeom::IN3fGeomParam normal_param = schema.getNormalsParam();
        if (normal_param.valid()) {
            AbcGeom::IN3fGeomParam::Sample normal_sample;
            normal_param.getIndexed(normal_sample, 0);

            if (normal_sample.valid()) {
                AbcGeom::N3fArraySamplePtr normals = normal_sample.getVals();
                if (normals && normals->size() > 0) {
                    size_t prev_size = mesh_builder.normal.size();
                    mesh_builder.normal.reserve(prev_size + normals->size());
                    for (size_t i = 0; i < normals->size(); ++i) {
                        const AbcGeom::N3f &n = (*normals)[i];
                        mesh_builder.normal.push_back(make_float3(n.x, n.y, n.z));
                    }
                }
            }
        }

        // Process faces and create triangle indices
        auto &tri_indices = mesh_builder.triangle_indices.emplace_back();

        // Convert polygon faces to triangles
        size_t face_start = 0;
        for (size_t f = 0; f < face_counts->size(); ++f) {
            int32_t face_vert_count = (*face_counts)[f];
            if (face_vert_count < 3) {
                face_start += face_vert_count;
                continue;
            }

            // Triangulate the polygon using fan triangulation
            for (int32_t i = 1; i < face_vert_count - 1; ++i) {
                tri_indices.push_back(vertex_offset + (*face_indices)[face_start]);
                tri_indices.push_back(vertex_offset + (*face_indices)[face_start + i]);
                tri_indices.push_back(vertex_offset + (*face_indices)[face_start + i + 1]);
            }
            face_start += face_vert_count;
        }

        mesh_count++;
    }
    // Check for subdivision surfaces
    else if (AbcGeom::ISubD::matches(object.getHeader())) {
        AbcGeom::ISubD mesh(object);
        if (!mesh.valid()) {
            return;
        }

        AbcGeom::ISubDSchema &schema = mesh.getSchema();

        AbcGeom::ISubDSchema::Sample sample;
        if (!schema.getNumSamples() > 0) {
            schema.get(sample, 0);
        } else {
            return;
        }

        AbcGeom::P3fArraySamplePtr positions = sample.getPositions();
        if (!positions || positions->size() == 0) {
            return;
        }

        Abc::Int32ArraySamplePtr face_indices = sample.getFaceIndices();
        Abc::Int32ArraySamplePtr face_counts = sample.getFaceCounts();

        if (!face_indices || !face_counts) {
            return;
        }

        uint32_t vertex_offset = static_cast<uint32_t>(mesh_builder.position.size());

        mesh_builder.position.reserve(mesh_builder.position.size() + positions->size());
        for (size_t i = 0; i < positions->size(); ++i) {
            const AbcGeom::V3f &p = (*positions)[i];
            mesh_builder.position.push_back(make_float3(p.x, p.y, p.z));
        }

        // Process UVs
        AbcGeom::IV2fGeomParam uv_param = schema.getUVsParam();
        if (uv_param.valid() && mesh_builder.uvs.empty()) {
            mesh_builder.uvs.emplace_back();
        }

        if (uv_param.valid()) {
            AbcGeom::IV2fGeomParam::Sample uv_sample;
            uv_param.getIndexed(uv_sample, 0);

            if (uv_sample.valid()) {
                AbcGeom::V2fArraySamplePtr uvs = uv_sample.getVals();
                if (uvs && uvs->size() > 0) {
                    auto &uv_vec = mesh_builder.uvs[0];
                    size_t prev_size = uv_vec.size();
                    uv_vec.reserve(prev_size + uvs->size());
                    for (size_t i = 0; i < uvs->size(); ++i) {
                        const AbcGeom::V2f &uv = (*uvs)[i];
                        uv_vec.push_back(make_float2(uv.x, uv.y));
                    }
                }
            }
        }

        // Note: ISubDSchema doesn't have getNormalsParam() - normals are computed
        // during subdivision, not stored explicitly like in polygon meshes

        // Process faces
        auto &tri_indices = mesh_builder.triangle_indices.emplace_back();
        size_t capacity = 0;
        // reserve
        for (size_t f = 0; f < face_counts->size(); ++f) {
            int32_t face_vert_count = (*face_counts)[f];
            capacity += std::max<size_t>(0, face_vert_count - 2) * 3;
        }
        tri_indices.reserve(capacity);
        size_t face_start = 0;
        for (size_t f = 0; f < face_counts->size(); ++f) {
            int32_t face_vert_count = (*face_counts)[f];
            if (face_vert_count < 3) {
                face_start += face_vert_count;
                continue;
            }

            for (int32_t i = 1; i < face_vert_count - 1; ++i) {
                tri_indices.push_back(vertex_offset + (*face_indices)[face_start]);
                tri_indices.push_back(vertex_offset + (*face_indices)[face_start + i]);
                tri_indices.push_back(vertex_offset + (*face_indices)[face_start + i + 1]);
            }
            face_start += face_vert_count;
        }

        mesh_count++;
    }

    // Recursively process children
    size_t num_children = object.getNumChildren();
    for (size_t i = 0; i < num_children; ++i) {
        traverse_alembic_object(object.getChild(i), mesh_builder, mesh_count);
    }
}

bool AbcMeshImporter::import(Resource *resource_base, luisa::filesystem::path const &path) {
    auto resource = static_cast<MeshResource *>(resource_base);
    if (!resource || resource->empty() == false) [[unlikely]] {
        LUISA_WARNING("Can not create on exists mesh.");
        return false;
    }

    // Open the Alembic archive using Ogawa format (most common)
    AbcCoreFactory::IFactory factory;
    Abc::IArchive archive;

    archive = factory.getArchive(path.string());

    if (!archive.valid()) {
        LUISA_WARNING("Invalid Alembic archive: {}", path.string());
        return false;
    }

    // Get the top-level object
    AbcGeom::IObject root = archive.getTop();
    if (!root.valid()) {
        LUISA_WARNING("Empty Alembic archive: {}", path.string());
        return false;
    }

    MeshBuilder mesh_builder;
    size_t mesh_count = 0;

    // Traverse the hierarchy and extract mesh data
    traverse_alembic_object(root, mesh_builder, mesh_count);

    if (mesh_count == 0 || mesh_builder.position.empty()) {
        LUISA_WARNING("No valid mesh found in Alembic file: {}", path.string());
        return false;
    }

    // Calculate tangents if we have UVs and normals
    if (mesh_builder.uv_count() > 0 && mesh_builder.normal.size() > 0) {
        mesh_builder.tangent.push_back_uninitialized(mesh_builder.vertex_count());
        if (mesh_builder.triangle_indices.size() > 1) {
            luisa::vector<Triangle> triangles;
            uint64_t size = 0;
            for (auto &i : mesh_builder.triangle_indices) {
                size += i.size() / 3;
            }
            triangles.reserve(size);
            for (auto &i : mesh_builder.triangle_indices) {
                vstd::push_back_all(triangles, luisa::span{(Triangle *)i.data(), i.size() / 3});
            }
            calculate_tangent(
                mesh_builder.position,
                mesh_builder.uvs[0],
                mesh_builder.tangent,
                triangles,
                1);
        } else {
            calculate_tangent(
                mesh_builder.position,
                mesh_builder.uvs[0],
                mesh_builder.tangent,
                luisa::span{
                    (Triangle const *)(mesh_builder.triangle_indices[0].data()),
                    mesh_builder.triangle_indices[0].size() / 3},
                1);
        }
    }

    luisa::vector<uint> submesh_offsets;
    luisa::vector<std::byte> resource_bytes;
    mesh_builder.write_to(resource_bytes, submesh_offsets);
    resource->create_empty(
        std::move(submesh_offsets),
        mesh_builder.vertex_count(),
        mesh_builder.indices_count() / 3,
        mesh_builder.uv_count(),
        mesh_builder.contained_normal(),
        mesh_builder.contained_tangent());
    *(resource->host_data()) = std::move(resource_bytes);


    return true;
}

}// namespace rbc::world

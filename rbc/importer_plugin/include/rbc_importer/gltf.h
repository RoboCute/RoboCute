#pragma once
// GLTF Utilityes
#include <rbc_graphics/mesh_builder.h>
#include <rbc_world/resources/mesh.h>
#include <tiny_gltf.h>

namespace rbc {

// Helper function to process GLTF model and build MeshBuilder
// Returns MeshBuilder and additional data (skin weights, vertex colors)
struct GltfImportData {
    MeshBuilder mesh_builder;
    size_t max_weight_count = 0;// joint weight suite
    luisa::vector<uint16_t> all_joint_index;
    luisa::vector<float> all_joint_weight;
    luisa::vector<float> all_vertex_colors;
    size_t vertex_color_channels = 0;
};

// Helper function to read accessor data
template<typename T>
static bool read_accessor_data(
    tinygltf::Model const &model,
    int accessor_index,
    luisa::vector<T> &out_data) {
    if (accessor_index < 0 || accessor_index >= static_cast<int>(model.accessors.size())) {
        return false;
    }

    auto const &accessor = model.accessors[accessor_index];
    if (accessor.bufferView < 0 || accessor.bufferView >= static_cast<int>(model.bufferViews.size())) {
        return false;
    }

    auto const &buffer_view = model.bufferViews[accessor.bufferView];
    if (buffer_view.buffer < 0 || buffer_view.buffer >= static_cast<int>(model.buffers.size())) {
        return false;
    }

    auto const &buffer = model.buffers[buffer_view.buffer];

    size_t component_size = tinygltf::GetComponentSizeInBytes(accessor.componentType);
    int num_components = tinygltf::GetNumComponentsInType(accessor.type);
    if (component_size <= 0 || num_components <= 0) {
        return false;
    }

    size_t element_size = component_size * num_components;
    size_t byte_stride = buffer_view.byteStride > 0 ? buffer_view.byteStride : element_size;

    size_t data_offset = buffer_view.byteOffset + accessor.byteOffset;
    size_t data_size = accessor.count * element_size;

    if (data_offset + data_size > buffer.data.size()) {
        return false;
    }

    out_data.reserve(accessor.count);

    // Read and convert data
    for (size_t i = 0; i < accessor.count; ++i) {
        size_t byte_offset = data_offset + i * byte_stride;
        uint8_t const *data_ptr = buffer.data.data() + byte_offset;

        if constexpr (std::is_same_v<T, float3>) {
            if (num_components == 3 && accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
                float const *float_ptr = reinterpret_cast<float const *>(data_ptr);
                out_data.emplace_back(float_ptr[0], float_ptr[1], float_ptr[2]);
            } else {
                return false;
            }
        } else if constexpr (std::is_same_v<T, float2>) {
            if (num_components == 2 && accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
                float const *float_ptr = reinterpret_cast<float const *>(data_ptr);
                out_data.emplace_back(float_ptr[0], float_ptr[1]);
            } else {
                return false;
            }
        } else if constexpr (std::is_same_v<T, float4>) {
            if (num_components == 4 && accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
                float const *float_ptr = reinterpret_cast<float const *>(data_ptr);
                out_data.emplace_back(float_ptr[0], float_ptr[1], float_ptr[2], float_ptr[3]);
            } else {
                return false;
            }
        } else if constexpr (std::is_same_v<T, uint>) {
            if (num_components == 1) {
                uint32_t index_value = 0;
                switch (accessor.componentType) {
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                        index_value = static_cast<uint32_t>(*reinterpret_cast<uint8_t const *>(data_ptr));
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                        index_value = static_cast<uint32_t>(*reinterpret_cast<uint16_t const *>(data_ptr));
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                        index_value = *reinterpret_cast<uint32_t const *>(data_ptr);
                        break;
                    default:
                        return false;
                }
                out_data.push_back(index_value);
            } else {
                return false;
            }
        } else {
            return false;
        }
    }

    return true;
}

// Helper function to read accessor data as raw bytes (for skin weights, colors, etc.)
inline bool read_accessor_raw(
    tinygltf::Model const &model,
    int accessor_index,
    luisa::vector<std::byte> &out_data) {

    if (accessor_index < 0 || accessor_index >= static_cast<int>(model.accessors.size())) {
        return false;
    }

    auto const &accessor = model.accessors[accessor_index];
    if (accessor.bufferView < 0 || accessor.bufferView >= static_cast<int>(model.bufferViews.size())) {
        return false;
    }

    auto const &buffer_view = model.bufferViews[accessor.bufferView];
    if (buffer_view.buffer < 0 || buffer_view.buffer >= static_cast<int>(model.buffers.size())) {
        return false;
    }

    auto const &buffer = model.buffers[buffer_view.buffer];

    size_t component_size = tinygltf::GetComponentSizeInBytes(accessor.componentType);
    int num_components = tinygltf::GetNumComponentsInType(accessor.type);
    if (component_size <= 0 || num_components <= 0) {
        return false;
    }

    size_t element_size = component_size * num_components;
    size_t byte_stride = buffer_view.byteStride > 0 ? buffer_view.byteStride : element_size;

    size_t data_offset = buffer_view.byteOffset + accessor.byteOffset;
    size_t data_size = accessor.count * element_size;

    if (data_offset + data_size > buffer.data.size()) {
        return false;
    }

    out_data.resize(data_size);

    // Copy data from unsigned char buffer to std::byte vector
    if (byte_stride == element_size) {
        // Tightly packed - single memcpy
        std::memcpy(out_data.data(), buffer.data.data() + data_offset, data_size);
    } else {
        // Strided - copy element by element
        std::byte *out_ptr = out_data.data();
        for (size_t i = 0; i < accessor.count; ++i) {
            size_t byte_offset = data_offset + i * byte_stride;
            std::memcpy(out_ptr, buffer.data.data() + byte_offset, element_size);
            out_ptr += element_size;
        }
    }

    return true;
}

GltfImportData process_gltf_model(tinygltf::Model const &model);

}// namespace rbc
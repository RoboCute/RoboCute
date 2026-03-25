#include <rbc_importer/gs_importer_ply.h>
#include <rbc_world/resources/gaussian_splat.h>

#include <luisa/core/binary_file_stream.h>
#include <luisa/core/logging.h>

#include <cstring>
#include <cctype>
#include <string>
#include <cmath>

#include "happly.h"

namespace rbc::world {

using namespace luisa;
using namespace luisa::compute;

namespace {

inline float opacity_activation(float x) {
    // sigmoid
    return 1.0f / (1.0f + std::exp(-x));
}

inline float scaling_activation(float x) {
    // exp
    return std::exp(x);
}

inline void rotation_activation(float &r, float &x, float &y, float &z) {
    // normalize
    float norm = std::sqrt(x * x + y * y + z * z + r * r);
    r /= norm;
    x /= norm;
    y /= norm;
    z /= norm;
}

}// namespace

MD5 PlyGaussianSplatImporter::resource_type() const {
    return MD5{"rbc::world::GaussianSplatResource"sv};
}

bool PlyGaussianSplatImporter::import(Resource *resource_base, luisa::filesystem::path const &path) {
    auto resource = static_cast<GaussianSplatResource *>(resource_base);
    if (!resource) [[unlikely]] {
        LUISA_WARNING("Can not create on exists gaussian splat.");
        return false;
    }

    // Read PLY file using happly
    happly::PLYData ply_in(path.string());
    
    if (!ply_in.hasElement("vertex")) [[unlikely]] {
        LUISA_WARNING("No vertex element in PLY file: {}", path.string());
        return false;
    }
    
    auto &vertex_element = ply_in.getElement("vertex");
    int N = static_cast<int>(vertex_element.count);
    
    if (N == 0) [[unlikely]] {
        LUISA_WARNING("No vertices in PLY file: {}", path.string());
        return false;
    }
    
    // Detect SH degree from available properties
    // f_dc_0,1,2 are always present (degree 0)
    // f_rest_0 to f_rest_44 means degree 3 (15 coeffs per channel, 3 channels, minus 3 DC = 42 rest)
    int sh_degree = 0;
    if (vertex_element.hasProperty("f_rest_44")) {
        sh_degree = 3;
    } else if (vertex_element.hasProperty("f_rest_23")) {
        sh_degree = 2;
    } else if (vertex_element.hasProperty("f_rest_8")) {
        sh_degree = 1;
    }
    
    int stride = (sh_degree + 1) * (sh_degree + 1);
    
    // Create resource with detected parameters
    resource->create_empty(static_cast<uint32_t>(N), static_cast<uint32_t>(sh_degree));
    
    // Get host data pointers
    auto probes = resource->host_probes();
    auto sh_coeffs = resource->host_sh_coeffs();
    
    // Read position data
    std::vector<float> x = vertex_element.getProperty<float>("x");
    std::vector<float> y = vertex_element.getProperty<float>("y");
    std::vector<float> z = vertex_element.getProperty<float>("z");
    
    for (int i = 0; i < N; i++) {
        probes[i].position[0] = x[i];
        probes[i].position[1] = y[i];
        probes[i].position[2] = z[i];
    }
    
    // Read DC features (f_dc_0, f_dc_1, f_dc_2) - always present
    for (int channel = 0; channel < 3; channel++) {
        std::string dc_feat_name = "f_dc_" + std::to_string(channel);
        std::vector<float> dc_feat = vertex_element.getProperty<float>(dc_feat_name);
        int offset = 0;
        for (int j = 0; j < N; j++) {
            sh_coeffs[offset * 3 + channel + j * stride * 3] = dc_feat[j];
        }
    }
    
    // Read REST features (f_rest_*) if present
    int num_rest_coeffs = (stride - 1) * 3;
    for (int i = 0; i < num_rest_coeffs; i++) {
        std::string rest_feat_name = "f_rest_" + std::to_string(i);
        int channel = i / (stride - 1);
        int offset = i % (stride - 1) + 1;
        std::vector<float> rest_feat = vertex_element.getProperty<float>(rest_feat_name);
        for (int j = 0; j < N; j++) {
            sh_coeffs[offset * 3 + channel + j * stride * 3] = rest_feat[j];
        }
    }
    
    // Read opacity and apply activation
    std::vector<float> opacity = vertex_element.getProperty<float>("opacity");
    for (int i = 0; i < N; i++) {
        probes[i].opacity = opacity_activation(opacity[i]);
    }
    
    // Read scales and apply activation
    for (int i = 0; i < 3; i++) {
        std::string scale_name = "scale_" + std::to_string(i);
        std::vector<float> scale_feat = vertex_element.getProperty<float>(scale_name);
        for (int j = 0; j < N; j++) {
            probes[j].scale[i] = scaling_activation(scale_feat[j]);
        }
    }
    
    // Read rotations
    for (int i = 0; i < 4; i++) {
        std::string rotation_name = "rot_" + std::to_string(i);
        std::vector<float> rotation_feat = vertex_element.getProperty<float>(rotation_name);
        for (int j = 0; j < N; j++) {
            // rot_0 is real part (w), stored in rotation.x
            // rot_1,2,3 are imaginary parts (x,y,z), stored in rotation.yzw
            switch (i) {
                case 0: probes[j].rotation.x = rotation_feat[j]; break;
                case 1: probes[j].rotation.y = rotation_feat[j]; break;
                case 2: probes[j].rotation.z = rotation_feat[j]; break;
                case 3: probes[j].rotation.w = rotation_feat[j]; break;
            }
        }
    }
    
    // Apply rotation normalization
    for (int i = 0; i < N; i++) {
        rotation_activation(
            probes[i].rotation.x,
            probes[i].rotation.y,
            probes[i].rotation.z,
            probes[i].rotation.w);
    }
    
    return true;
}

}// namespace rbc::world

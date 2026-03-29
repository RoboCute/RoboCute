#include <luisa/std.hpp>

using namespace luisa::shader;


// Sobel operator for X direction (horizontal edges)
float sobel_x(float3x3 data) {
    return data[0][0] * -1.0f +
           data[0][1] * -2.0f +
           data[0][2] * -1.0f +
           data[2][0] * 1.0f +
           data[2][1] * 2.0f +
           data[2][2] * 1.0f;
}

// Sobel operator for Y direction (vertical edges)
float sobel_y(float3x3 data) {
    return data[0][0] * -1.0f +
           data[1][0] * -2.0f +
           data[2][0] * -1.0f +
           data[0][2] * 1.0f +
           data[1][2] * 2.0f +
           data[2][2] * 1.0f;
}

// Edge detection function using Sobel filter
// Returns edge magnitude in range [0, 1]
template<typename T>
float edge_detect(
    Image<T>& src_img,
    int2 center_id,
    int2 img_size,
    float sensitivity) {
    
    // Sample 3x3 neighborhood - using a float array instead
    float lum[3][3];
    for (int x = 0; x < 3; ++x) {
        for (int y = 0; y < 3; ++y) {
            auto sample_id = center_id + int2(x - 1, y - 1);
            // Clamp to image bounds (edge address mode)
            sample_id = clamp(sample_id, 0, int2(img_size - 1));
            auto color = src_img.read(uint2(sample_id));
            lum[x][y] = (float)color.x;
        }
    }
    
    // Create matrix from array
    float3x3 luminance_data = float3x3(
        lum[0][0], lum[0][1], lum[0][2],
        lum[1][0], lum[1][1], lum[1][2],
        lum[2][0], lum[2][1], lum[2][2]
    );
    
    // Compute gradients
    float gx = sobel_x(luminance_data);
    float gy = sobel_y(luminance_data);
    
    // Compute gradient magnitude
    float magnitude = sqrt(gx * gx + gy * gy);
    
    // Apply sensitivity and clamp
    return saturate(magnitude * sensitivity);
}

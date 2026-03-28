// The MIT License
// Copyright © 2013 Inigo Quilez
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions: The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// https://www.youtube.com/c/InigoQuilez
// https://iquilezles.org

// Simplex Noise (http://en.wikipedia.org/wiki/Simplex_noise), a type of gradient noise
// that uses N+1 vertices for random gradient interpolation instead of 2^N as in regular
// latice based Gradient Noise.

#include <luisa/std.hpp>
using namespace luisa::shader;

float2 hash(float2 p) {
    p = float2(dot(p, float2(127.1f, 311.7f)),
               dot(p, float2(269.5f, 183.3f)));
    return -1.0f + 2.0f * fract(sin(p) * 43758.5453123f);
}

float noise(float2 p) {
    const float K1 = 0.366025404f; // (sqrt(3)-1)/2;
    const float K2 = 0.211324865f; // (3-sqrt(3))/6;

    float2 i = floor(p + (p.x + p.y) * K1);
    float2 a = p - i + (i.x + i.y) * K2;
    float m = step(a.y, a.x);
    float2 o = float2(m, 1.0f - m);
    float2 b = a - o + K2;
    float2 c = a - 1.0f + 2.0f * K2;
    float3 h = max(0.5f - float3(dot(a, a), dot(b, b), dot(c, c)), float3(0.0f));
    float3 n = h * h * h * h * float3(dot(a, hash(i + 0.0f)),
                                       dot(b, hash(i + o)),
                                       dot(c, hash(i + 1.0f)));
    return dot(n, float3(70.0f));
}

// Fractal Brownian Motion - 4 octaves
float fbm(float2 uv) {
    float2x2 m = float2x2(1.6f, 1.2f, -1.2f, 1.6f);
    float f = 0.0f;
    f += 0.5000f * noise(uv); uv = m * uv;
    f += 0.2500f * noise(uv); uv = m * uv;
    f += 0.1250f * noise(uv); uv = m * uv;
    f += 0.0625f * noise(uv);
    return f;
}

[[kernel_2d(16, 8)]] int kernel(
    Image<float>& output,
    float2 uv_scale,
    float2 uv_offset,
    float frequency,
    uint octave_count) {
    
    auto id = dispatch_id().xy;
    auto size = dispatch_size().xy;
    float2 uv = (float2(id) + 0.5f) / float2(size);
    uv = uv * uv_scale + uv_offset;
    
    float f = 0.0f;
    
    if (octave_count <= 1) {
        // Single octave noise
        f = noise(uv * frequency);
    } else {
        // Fractal noise with multiple octaves
        float2 octave_uv = uv * frequency;
        float2x2 m = float2x2(1.6f, 1.2f, -1.2f, 1.6f);
        float amplitude = 1.0f;
        float total_amplitude = 0.0f;
        
        for (uint i = 0; i < octave_count; i++) {
            f += amplitude * noise(octave_uv);
            total_amplitude += amplitude;
            amplitude *= 0.5f;
            octave_uv = m * octave_uv;
        }
        f /= total_amplitude;
    }
    
    // Normalize to [0, 1]
    f = 0.5f + 0.5f * f;
    
    output.write(id, float4(f, f, f, 1.0f));
    return 0;
}

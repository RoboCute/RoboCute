---
name: shader
---

# Luisa Shader Skill

This skill provides guidance for writing GPU shaders using the Luisa shading language. Luisa is a C++-embedded shading language that compiles to GPU code (DXIL, SPIR-V, etc.) for compute, rasterization, and ray tracing pipelines.

## Overview

Luisa shaders are written in C++ with special attributes and intrinsics. The language provides:
- Compute kernels (1D/2D/3D thread dispatch)
- Rasterization shaders (vertex/pixel shaders)
- Ray tracing kernels with BVH traversal
- Bindless resources for dynamic indexing

## File Structure

```
include/luisa/        # Core language headers
  std.hpp             # Main include (includes everything)
  attributes.hpp      # Kernel attributes and annotations
  types.hpp           # Vector/matrix/ray types
  types/vec.hpp       # Vector types (float2, float3, float4, etc.)
  types/matrix.hpp    # Matrix types (float2x2, float3x3, float4x4)
  types/ray.hpp       # Ray and hit types
  types/array.hpp     # SharedArray for group shared memory
  resources.hpp       # Resource types
  resources/buffer.hpp      # Buffer<T> and ByteBuffer
  resources/texture.hpp     # Image<T> and Volume<T>
  resources/accel.hpp       # Accel for ray tracing
  resources/bindless_*.hpp  # Bindless resource arrays
  functions.hpp       # Built-in functions
  functions/dispatch.hpp    # Dispatch intrinsics
  functions/warp.hpp        # Wave/warp operations
  functions/atomic.hpp      # Atomic operations
  functions/math.hpp        # Math functions
  raytracing.hpp      # Ray tracing headers
  raytracing/ray_query.hpp  # Ray query objects

src/                  # Shader source files
  builtin/            # Built-in utility kernels
  geometry/           # Geometry processing kernels
  gui/                # GUI/display kernels
  path_tracer/        # Path tracing kernels
  post_process/       # Post-processing kernels
  raster/             # Rasterization shaders
  surfel/             # Surfel GI kernels
  texture_process/    # Texture processing kernels
```

## Basic Shader Structure

### Compute Kernel

```cpp
#include <luisa/std.hpp>
using namespace luisa::shader;

// 1D kernel with 256 threads per group
[[kernel_1d(128)]] int kernel(
    Buffer<float>& input,
    Buffer<float>& output,
    uint count) {
    auto id = dispatch_id().x;
    if (id >= count) return 0;
    output.write(id, input.read(id) * 2.0f);
    return 0;
}
```

### 2D Kernel (Image Processing)

```cpp
#include <luisa/std.hpp>
using namespace luisa::shader;

[[kernel_2d(16, 8)]] int kernel(
    Image<float>& dst,
    SampleImage& src) {
    auto id = dispatch_id().xy;
    auto uv = (float2(id) + 0.5f) / float2(dispatch_size().xy);
    auto color = src.sample(uv, Filter::LINEAR_POINT, Address::EDGE);
    dst.write(id, color);
    return 0;
}
```

### 3D Kernel (Volume Processing)

```cpp
[[kernel_3d(8, 8, 8)]] int kernel(
    Volume<float>& dst_volume,
    SampleVolume& src_volume) {
    auto id = dispatch_id().xyz;
    auto uv = float3(id) / float3(dispatch_size() - 1u);
    auto color = src_volume.sample(uv, Filter::LINEAR_POINT, Address::EDGE);
    dst_volume.write(id, color);
    return 0;
}
```

## Kernel Attributes

| Attribute | Description | Example |
|-----------|-------------|---------|
| `[[kernel_1d(N)]]` | 1D compute kernel with N threads per group | `[[kernel_1d(128)]]` |
| `[[kernel_2d(X, Y)]]` | 2D compute kernel with X×Y threads per group | `[[kernel_2d(16, 8)]]` |
| `[[kernel_3d(X, Y, Z)]]` | 3D compute kernel with X×Y×Z threads per group | `[[kernel_3d(8, 8, 8)]]` |
| `[[warp_size(N)]]` | Specify wave/warp size (e.g., 32 for NVIDIA) | `[[warp_size(32)]]` |
| `[[VERTEX_SHADER]]` | Vertex shader entry point | `[[VERTEX_SHADER]] v2p vert(...)` |
| `[[PIXEL_SHADER]]` | Pixel/fragment shader entry point | `[[PIXEL_SHADER]] float4 pixel(...)` |

## Resource Types

### Buffer

Typed buffer with structured elements:

```cpp
Buffer<float>       // Buffer of floats
Buffer<float4>      // Buffer of float4 vectors
Buffer<uint>        // Buffer with atomic operations
Buffer<MyStruct>    // Buffer of custom structures

// Methods:
buffer.read(index)                    // Read element
buffer.write(index, value)            // Write element
buffer.volatile_read(index)           // Volatile read
buffer.volatile_write(index, value)   // Volatile write

// Atomic operations (uint/int buffers only):
buffer.atomic_fetch_add(index, value)
buffer.atomic_fetch_min(index, value)
buffer.atomic_fetch_max(index, value)
buffer.atomic_exchange(index, value)
buffer.atomic_compare_exchange(index, expected, desired)
```

### ByteBuffer

Untyped buffer for raw byte access:

```cpp
ByteBuffer<>  // Alias for Buffer<void>

// Methods:
buffer.byte_read<T>(byte_offset)       // Read type T at byte offset
buffer.byte_write<T>(byte_offset, value) // Write type T at byte offset
```

### Image

2D texture with read/write and sampling:

```cpp
Image<float>        // Read/write 2D image
SampleImage         // Alias for Image<float, CacheFlags::ReadOnly> with sampling

// Methods:
image.read(coord)                     // Read texel at integer coord
image.write(coord, value)             // Write texel
image.size()                          // Get image size as uint2

// Sampling (SampleImage only):
image.sample(uv, filter, address)     // Sample with filter/address mode
image.sample_level(uv, level, filter, address)  // Sample specific mip level
image.sample_grad(uv, ddx, ddy, filter, address) // Sample with gradients

// Filter modes: Filter::POINT, Filter::LINEAR_POINT, Filter::LINEAR_LINEAR, Filter::ANISOTROPIC
// Address modes: Address::EDGE, Address::REPEAT, Address::MIRROR, Address::ZERO
```

### Volume

3D texture with read/write and sampling:

```cpp
Volume<float>       // Read/write 3D volume
SampleVolume        // Alias for Volume<float, CacheFlags::ReadOnly> with sampling

// Methods same as Image but with uint3 coords and float3 uv
volume.read(coord)                    // Read at integer coord (uint3)
volume.write(coord, value)            // Write at integer coord
volume.sample(uv, filter, address)    // Sample with float3 uv
```

### Accel (Ray Tracing)

Acceleration structure for ray tracing:

```cpp
Accel accel;

// Trace rays:
TriangleHit hit = accel.trace_closest(ray, mask);  // Closest hit
bool any_hit = accel.trace_any(ray, mask);         // Any hit (faster)

// Ray queries for custom traversal:
RayQueryAll query = accel.query_all(ray, mask);    // All hits query
RayQueryAny query = accel.query_any(ray, mask);    // Any hit query

// Instance access:
float4x4 transform = accel.instance_transform(index);
uint user_id = accel.instance_user_id(index);
uint mask = accel.instance_visibility_mask(index);

// Dynamic updates:
accel.set_instance_transform(index, transform);
accel.set_instance_visibility(index, visibility);
accel.set_instance_opacity(index, opaque);
accel.set_instance_user_id(index, user_id);
```

### Bindless Resources

Dynamic indexing into resource arrays:

```cpp
BindlessArray       // Mixed bindless resources (images, volumes, buffers)
BindlessBuffer      // Typed bindless buffer array
BindlessImage       // Typed bindless image array

// BindlessArray methods:
array.image_sample(index, uv)
array.image_sample_level(index, uv, mip)
array.volume_sample(index, uv)
array.buffer_read<T>(buffer_index, elem_index)
array.byte_buffer_read<T>(buffer_index, byte_offset)
array.uniform_buffer_read<T>(index, elem_index)  // Uniform index version

// Uniform variants ensure all threads access same index (better performance)
```

### Global Bindless Resources

Path tracing kernels have access to predefined global bindless heaps:

```cpp
extern BindlessBuffer& g_buffer_heap;    // Global buffer heap
extern BindlessImage& g_image_heap;      // Global image heap
extern BindlessVolume& g_volume_heap;    // Global volume heap
extern Accel& g_accel;                   // Global acceleration structure
```

## Types

### Vectors

```cpp
// Common vector types:
float2, float3, float4    // 32-bit float vectors
int2, int3, int4          // 32-bit signed int vectors  
uint2, uint3, uint4       // 32-bit unsigned int vectors
half2, half3, half4       // 16-bit half-precision vectors
bool2, bool3, bool4       // Boolean vectors

// Construction:
float3 v = float3(1.0f, 2.0f, 3.0f);
float3 v = float3(1.0f);  // All components = 1.0f
float4 v = float4(float3(1,2,3), 1.0f);  // From vec3 + scalar

// Swizzling:
float3 rgb = rgba.xyz;
float2 xy = pos.xy;
float4 rev = rgba.wzyx;
```

### Matrices

```cpp
float2x2, float3x3, float4x4    // Column-major matrices

// Construction:
float4x4 m = float4x4(
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1
);
float4x4 m = float4x4(col0, col1, col2, col3);  // From column vectors

// Special matrices:
float4x4 m = float4x4::identity();

// Operations:
float4x4 inv = inverse(m);
float4x4 trans = transpose(m);
float det = determinant(m);
float4 transformed = m * float4(pos, 1.0f);
```

### Ray and Hit Types

```cpp
// Ray construction:
Ray ray(origin, direction, t_min, t_max);
Ray ray(origin, direction);  // Default t range

// Access:
float3 o = ray.origin();
float3 d = ray.dir();

// Hit results from trace:
TriangleHit hit = accel.trace_closest(ray);
if (hit.hitted()) {
    uint instance = hit.inst;
    uint primitive = hit.prim;
    float2 bary = hit.bary;  // Barycentric coordinates
    float t = hit.ray_t;
    
    // Interpolate vertex attributes:
    float3 pos = hit.interpolate(v0, v1, v2);
}

// CommittedHit (from ray queries):
CommittedHit hit = query.committed_hit();
if (hit.hit_triangle()) { ... }
if (hit.hit_procedural()) { ... }
if (hit.miss()) { ... }
```

## Dispatch Intrinsics

```cpp
// Thread identification:
uint3 id = dispatch_id();      // Global thread ID
uint3 block = block_id();        // Workgroup/block ID
uint3 thread = thread_id();      // Local thread ID within group
uint3 size = dispatch_size();    // Total dispatch size
uint32 kid = kernel_id();        // Kernel/thread group index

// Rasterization (pixel shaders):
uint prim_id = primitive_id();   // Primitive ID
float3 bary = barycentrics();    // Barycentric coordinates

// Control flow:
sync_block();    // Synchronize all threads in workgroup

// Pixel shader control:
discard();       // Discard pixel
set_z_depth(z);  // Set depth value
```

## Math Functions

### Basic Math

```cpp
// Trigonometry:
sin, cos, tan, asin, acos, atan, atan2
sinh, cosh, tanh, asinh, acosh, atanh

// Exponential/Logarithmic:
exp, exp2, exp10, log, log2, log10, pow, sqrt, rsqrt

// Common:
saturate(x)       // Clamp to [0, 1]
clamp(x, min, max)
lerp(a, b, t)     // Linear interpolation
smoothstep(a, b, x)
step(edge, x)
abs, min, max
floor, ceil, round, trunc, fract

// Vector math:
dot(a, b), cross(a, b)
length(v), length_squared(v), distance(a, b)
normalize(v)
reduce_sum(v), reduce_min(v), reduce_max(v)
faceforward(n, i, ng)
reflect(i, n), refract(i, n, eta)

// Matrix math:
determinant(m), transpose(m), inverse(m)
```

### Derivatives (Pixel Shaders)

```cpp
ddx(value)        // Derivative in X (screen space)
ddy(value)        // Derivative in Y (screen space)
```

### Type Conversion

```cpp
// Bit cast (reinterpret bits):
uint u = bit_cast<uint>(1.0f);
float f = bit_cast<float>(0x3f800000u);

// Select/ternary:
float result = select(false_val, true_val, condition);
float result = ite(condition, true_val, false_val);  // if-then-else
```

## Warp/Wave Operations

```cpp
// Thread IDs within wave:
uint lane_id = warp_lane_id();       // Lane index in warp (0-31)
uint lane_count = warp_lane_count(); // Warp size (e.g., 32)

// Reduction operations (all threads in warp):
float sum = warp_active_sum(value);
float product = warp_active_product(value);
float min_val = warp_active_min(value);
float max_val = warp_active_max(value);
bool all = warp_active_all(condition);
bool any = warp_active_any(condition);
uint4 ballot = warp_active_bit_mask(condition);

// Prefix operations:
float prefix_sum = warp_prefix_sum(value);
float prefix_product = warp_prefix_product(value);

// Thread communication:
float val = warp_read_lane(value, lane_index);
float first = warp_read_first_active_lane(value);
bool is_first = wave_is_first_lane();

// Note: "wave_*" aliases provided for DirectX terminology
```

## Shared Memory

```cpp
// Define shared array (group shared memory):
SharedArray<float, 256> shared_data;  // 256 floats
SharedArray<uint, 32> shared_counters;

// Usage within kernel:
shared_data[thread_id().x] = computed_value;
sync_block();  // Ensure all writes complete
float neighbor = shared_data[thread_id().x + 1];

// Atomics on shared memory (int/uint/float):
shared_counters.atomic_fetch_add(index, 1);
shared_counters.atomic_fetch_min(index, value);
```

## Atomic Operations

### Integer Atomics

```cpp
Buffer<uint> buf;
uint old = buf.atomic_fetch_add(index, 1);
uint old = buf.atomic_fetch_min(index, value);
uint old = buf.atomic_fetch_max(index, value);
uint old = buf.atomic_exchange(index, new_value);
uint old = buf.atomic_compare_exchange(index, expected, desired);
```

### Float Atomics

Float atomics are not natively supported on all hardware. Use helper functions:

```cpp
#include <luisa/functions/atomic.hpp>

Buffer<uint> buf;  // Store floats as uint bits

// Pack float to sortable uint representation:
uint packed = float_pack_to_uint(1.0f);
float unpacked = uint_unpack_to_float(packed);

// Float atomics via uint buffer:
float old_min = float_atomic_min(buf, index, value);
float old_max = float_atomic_max(buf, index, value);
float old_add = float_atomic_add(buf, index, value);
```

## Ray Tracing

### Basic Ray Tracing

```cpp
[[kernel_2d(16, 8)]] int trace_kernel(
    Accel& accel,
    Image<float>& output) {
    
    auto coord = dispatch_id().xy;
    
    // Setup ray
    float3 origin = camera_pos;
    float3 dir = compute_ray_direction(coord);
    Ray ray(origin, dir, 0.001f, 1e30f);
    
    // Trace
    TriangleHit hit = accel.trace_closest(ray);
    
    if (hit.hitted()) {
        // Get instance data
        float4x4 transform = accel.instance_transform(hit.inst);
        uint user_id = accel.instance_user_id(hit.inst);
        
        // Output hit info
        output.write(coord, float4(hit.bary, 0, 1));
    }
    
    return 0;
}
```

### Ray Queries (Custom Traversal)

```cpp
RayQueryAll query = accel.query_all(ray);
while (query.proceed()) {
    if (query.is_triangle_candidate()) {
        TriangleHit hit = query.triangle_candidate();
        // ... evaluate hit ...
        query.commit_triangle();  // Accept hit
        // or query.ignore_hit(); (implied by not committing)
    }
    if (query.is_procedural_candidate()) {
        ProceduralHit hit = query.procedural_candidate();
        // ... evaluate procedural primitive ...
        query.commit_procedural(distance);
    }
}
CommittedHit final_hit = query.committed_hit();
```

## Rasterization

### Vertex Shader

```cpp
#include <geometry/raster.hpp>
using namespace luisa::shader;

struct v2p {
    [[POSITION]] float4 proj_pos;  // Required output
    float3 world_pos;
    float2 uv;
};

[[VERTEX_SHADER]] v2p vert(
    raster::AppDataBase data,      // Input: position, instance_id
    Buffer<geometry::RasterElement> inst_buffer,
    raster::VertArgs args) {       // View/projection matrices
    
    v2p o;
    
    // Get instance transform
    auto inst_data = raster::get_instance_data(data, inst_buffer);
    float3 world_pos = (inst_data.local_to_world * float4(data.pos.xyz, 1.0f)).xyz;
    
    // Transform to clip space
    o.proj_pos = args.view_proj * float4(world_pos, 1.0f);
    raster::transform_projection(o.proj_pos);  // Platform adjustment
    
    o.world_pos = world_pos;
    o.uv = ...;
    
    return o;
}
```

### Pixel Shader

```cpp
[[PIXEL_SHADER]] float4 pixel(v2p i) {
    // Access barycentric coordinates and primitive ID
    float3 bary = barycentrics();
    uint prim = primitive_id();
    uint obj_id = object_id();    // Object/instance ID
    
    // Discard if needed
    if (alpha < 0.5f) discard();
    
    return float4(color, 1.0f);
}
```

## Real-World Usage Examples

### Ambient Occlusion Ray Tracing

From `src/path_tracer/ao_trace.cpp`:

```cpp
#include <luisa/std.hpp>
#include <luisa/resources.hpp>
#include <path_tracer/pt_args.hpp>
#include <sampling/sample_funcs.hpp>
#include <sampling/heitz_sobol.hpp>
#include <utils/onb.hpp>
#include <path_tracer/trace.hpp>

using namespace luisa::shader;

[[kernel_2d(16, 8)]] int kernel(
    Image<float>& out_img,
    PTArgs args,
    float4 ray_radius,
    bool use_cosine_sample) {
    
    auto coord = dispatch_id().xy;
    auto size = dispatch_size().xy;
    
    // Setup sampler with frame-based randomization
    sampling::HeitzSobol sampler(coord, args.frame_index);
    
    // Generate primary ray from camera
    auto uv = (float2(coord) + 0.5f) / float2(size);
    auto proj = float4((uv * 2.f - 1.0f), 0.f, 1);
    auto world_pos = args.inv_vp * proj;
    world_pos /= world_pos.w;
    
    float3 dir = normalize(world_pos.xyz - args.cam_pos);
    Ray ray(args.cam_pos, dir, t_min, t_max);
    
    // Trace primary ray
    ProceduralGeometry procedural_geometry;
    auto hit = rbc_trace_closest(ray, args, sampler, procedural_geometry);
    
    if (hit.hit_triangle()) {
        // Get instance transform
        auto inst_transform = g_accel.instance_transform(hit.inst);
        
        // Read and interpolate vertex data
        auto local_pos = hit.interpolate(vertices[0].pos, vertices[1].pos, vertices[2].pos);
        auto world_pos = (inst_transform * float4(local_pos, 1.0f)).xyz;
        
        // Sample AO ray direction
        float3 local_dir = sampling::cosine_sample_hemisphere(pcg_sampler.next2f());
        mtl::Onb onb(geometry_normal);
        auto sample_dir = onb.to_world(local_dir);
        
        ray.set_origin(sampling::offset_ray_origin(world_pos, geometry_normal));
        ray.set_dir(sample_dir);
        ray.t_max = reduce_max(ray_radius);
        
        // Trace AO ray
        hit = rbc_trace_closest(ray, args, sampler, procedural_geometry);
        float ao = hit.hit_triangle() ? hit.ray_t / reduce_max(ray_radius) : 1.0f;
        out_img.write(coord, float4(ao));
    }
    return 0;
}
```

### Gaussian Probe AABB Computation

From `src/gaussian/compute_aabb.cpp`:

```cpp
#include <luisa/std.hpp>
#include <geometry/gaussian_probe.hpp>

using namespace luisa::shader;

// Compute rotated extent using quaternion rotation
[[nodiscard]] float3 compute_gaussian_extent(float3 scale, float4 rotation) {
    float3 base_extent = scale * 3.0f;  // ~99.7% coverage
    
    // Build rotation matrix from quaternion
    float3 q_vec = float3(rotation.x, rotation.y, rotation.z);
    float q_w = rotation.w;
    
    float xx = q_vec.x * q_vec.x, yy = q_vec.y * q_vec.y, zz = q_vec.z * q_vec.z;
    float xy = q_vec.x * q_vec.y, xz = q_vec.x * q_vec.z, yz = q_vec.y * q_vec.z;
    float wx = q_w * q_vec.x, wy = q_w * q_vec.y, wz = q_w * q_vec.z;
    
    float3x3 rot_mat;
    rot_mat[0] = float3(1.0f - 2.0f * (yy + zz), 2.0f * (xy + wz), 2.0f * (xz - wy));
    rot_mat[1] = float3(2.0f * (xy - wz), 1.0f - 2.0f * (xx + zz), 2.0f * (yz + wx));
    rot_mat[2] = float3(2.0f * (xz + wy), 2.0f * (yz - wx), 1.0f - 2.0f * (xx + yy));
    
    // Compute projected AABB extents
    float3 extent;
    extent.x = abs(rot_mat[0].x) * base_extent.x + abs(rot_mat[1].x) * base_extent.y + abs(rot_mat[2].x) * base_extent.z;
    extent.y = abs(rot_mat[0].y) * base_extent.x + abs(rot_mat[1].y) * base_extent.y + abs(rot_mat[2].y) * base_extent.z;
    extent.z = abs(rot_mat[0].z) * base_extent.x + abs(rot_mat[1].z) * base_extent.y + abs(rot_mat[2].z) * base_extent.z;
    
    return extent;
}

[[kernel_1d(128)]] int kernel(
    Buffer<AABB>& output_buffer,
    Buffer<GaussianProbe>& probe_buffer) {
    
    uint32 probe_idx = dispatch_id().x;
    GaussianProbe probe = probe_buffer.read(probe_idx);
    
    float3 extent = compute_gaussian_extent(float3(probe.scale), probe.rotation);
    AABB aabb = build_aabb(float3(probe.position), extent);
    
    output_buffer.write(probe_idx, aabb);
    return 0;
}
```

### Procedural Sky Generation

From `src/hdri/sky.cpp`:

```cpp
#include <sampling/sample_funcs.hpp>
#include <sampling/pcg.hpp>

using namespace luisa::shader;

float noise(float2 uv) {
    float2 i = floor(uv);
    float2 f = fract(uv);
    f = f * f * (3.f - 2.f * f);  // Smoothstep
    
    // Use PCG sampler for random values
    float lb = sampling::PCGSampler(uint2(i + float2(0.f, 0.f) / 64.f)).next();
    float rb = sampling::PCGSampler(uint2(i + float2(1.f, 0.f) / 64.f)).next();
    float lt = sampling::PCGSampler(uint2(i + float2(0.f, 1.f) / 64.f)).next();
    float rt = sampling::PCGSampler(uint2(i + float2(1.f, 1.f) / 64.f)).next();
    
    return lerp(lerp(lb, rb, f.x), lerp(lt, rt, f.x), f.y);
}

float fbm(float2 uv) {
    float value = 0.f;
    float amplitude = .5f;
    for (int i = 0; i < 8; i++) {
        value += noise(uv) * amplitude;
        amplitude *= .5f;
        uv *= 2.f;
    }
    return value;
}

[[kernel_2d(16, 8)]] int kernel(Image<float>& img, float time) {
    float2 fragCoord = float2(dispatch_id().xy);
    float2 iResolution = float2(dispatch_size().xy);
    
    float2 uv = (fragCoord + 0.5f) / iResolution;
    float3 rd = sampling::sphere_uv_to_direction(float3x3::identity(), uv);
    
    // Calculate sky color
    float3 lightDir = normalize(float3(.4f, .8f, -.5f));
    float sundot = clamp(dot(rd, lightDir), 0.0f, 1.0f);
    
    float3 skyCol = float3(0.4f, 0.6f, 0.85f) * 1.5f - rd.y * rd.y * 0.5f;
    skyCol = lerp(skyCol, 0.85f * float3(0.7f, 0.75f, 0.85f), pow(1.0f - max(rd.y, 0.0f), 4.0f));
    
    // Add sun
    skyCol += 0.25f * float3(1.0f, 0.9f, 0.85f) * pow(sundot, 5.0f);
    skyCol += 0.25f * float3(1.0f, 0.8f, 0.6f) * pow(sundot, 64.0f);
    
    // Add clouds
    float den = fbm(uv * 2.0f);
    skyCol = lerp(skyCol, float3(1.f), smoothstep(.4f, .8f, den));
    
    img.write(dispatch_id().xy, float4(skyCol, 1.0f));
    return 0;
}
```

### Post-Processing (Uber Shader)

From `src/post_process/uber.cpp`:

```cpp
#include <luisa/std.hpp>
#include <spectrum/color_space.hpp>
#include <post_process/local_exposure.hpp>

using namespace luisa::shader;

static float2 distort(float2 uv, float4 distortion_amount) {
    uv = (uv - float2(0.5)) * distortion_amount.z + float2(0.5);
    float2 ruv = distortion_CenterScale.zw * (uv - float2(0.5) - distortion_CenterScale.xy);
    float ru = length(float2(ruv));
    
    if (distortion_amount.w > 0.0) {
        float wu = ru * distortion_amount.x;
        ru = tan(wu) * (1.0 / (ru * distortion_amount.y));
    } else {
        ru = (1.0 / ru) * distortion_amount.x * atan(ru * distortion_amount.y);
    }
    return uv + ruv * float2(ru - 1.0);
}

[[kernel_2d(16, 8)]] int kernel(
    SampleImage& src_img,
    SampleVolume& tonemap_volume,
    Buffer<float>& exposure_buffer,
    Image<float>& result) {
    
    auto id = dispatch_id().xy;
    float2 uv = (float2(id) + 0.5f) / float2(dispatch_size().xy);
    
    // Apply lens distortion
    uv = distort(uv, args.distortion_Amount);
    
    // Sample source with chromatic aberration
    float4 tex_val = src_img.sample(uv, Filter::POINT, Address::EDGE);
    float3 col = tex_val.xyz;
    
    // Apply exposure
    col *= exposure_buffer.read(0);
    
    // Tonemap using 3D LUT
    col = tonemap_volume.sample(LUT_SPACE_ENCODE(col), Filter::LINEAR_POINT, Address::EDGE).xyz;
    
    // Apply gamma correction
    col = pow(col, args.gamma);
    
    result.write(id, float4(col, tex_val.w));
    return 0;
}
```

### Dual Quaternion Skinning

From `src/geometry/skinning.cpp`:

```cpp
#include <luisa/std.hpp>
#include <geometry/dual_quaternion.hpp>
#include <geometry/vertices.hpp>

using namespace luisa::shader;

[[kernel_2d(128, 1)]] int kernel(
    ByteBuffer<>& src_buffer,
    ByteBuffer<>& dst_buffer,
    Buffer<geometry::DualQuaternion>& dq_bone_buffer,
    Buffer<uint>& bone_indices,
    Buffer<float>& bone_weights,
    uint bones_count_per_vert,
    uint vertex_count,
    bool contained_normal,
    bool contained_tangent) {
    
    auto vert_id = dispatch_id().x;
    auto pos_normal = geometry::read_pos_normal(src_buffer, vert_id, vertex_count, contained_normal, contained_tangent);
    
    auto buffer_idx = vert_id * bones_count_per_vert;
    geometry::DualQuaternion blend_dq;
    geometry::DualQuaternion dq0;
    
    // Blend dual quaternions
    for (uint i = 0; i < bones_count_per_vert; ++i) {
        uint index = bone_indices.read(buffer_idx + i);
        float weight = bone_weights.read(buffer_idx + i);
        geometry::DualQuaternion dq = dq_bone_buffer.read(index);
        
        if (i > 0) {
            dq = DualQuaternionShortestPath(dq, dq0);
        } else {
            dq0 = dq;
        }
        
        blend_dq.rotation_quaternion += dq.rotation_quaternion * weight;
        blend_dq.translation_quaternion += dq.translation_quaternion * weight;
    }
    
    // Normalize
    float mag = length(blend_dq.rotation_quaternion);
    blend_dq.rotation_quaternion /= mag;
    blend_dq.translation_quaternion /= mag;
    
    // Apply skinning
    pos_normal.pos = geometry::QuaternionApplyRotation(
        float4(pos_normal.pos, 1.f), blend_dq.rotation_quaternion).xyz;
    pos_normal.pos += geometry::QuaternionMultiply(
        blend_dq.translation_quaternion, 
        geometry::QuaternionInvert(blend_dq.rotation_quaternion)).xyz;
    
    geometry::write_pos_normal(dst_buffer, vert_id, vertex_count, contained_normal, contained_tangent, pos_normal);
    return 0;
}
```

### Rasterization with Transform Feedback

From `src/raster/draw_gizmos.cpp`:

```cpp
#include <raster/raster_args.hpp>
#include <geometry/raster.hpp>
#include <luisa/raster/attributes.hpp>

using namespace luisa::shader;

struct AppData {
    [[POSITION]] float4 pos;
    [[COLOR]] float4 color;
};

struct v2p {
    [[POSITION]] float4 proj_pos;
    float4 color;
    float4 local_pos;
};

[[VERTEX_SHADER]] v2p vert(AppData appdata, float4x4 vp) {
    v2p o;
    o.proj_pos = vp * float4(appdata.pos.xyz, 1.f);
    o.color = appdata.color;
    o.local_pos = appdata.pos;
    raster::transform_projection(o.proj_pos);
    return o;
}

[[PIXEL_SHADER]] float4 pixel(
    v2p i,
    Buffer<float4>& clicked_id,
    PixelArgs args) {
    
    auto curr_id = uint2(i.proj_pos.xy);
    if (all(curr_id == args.clicked_pixel)) {
        auto inst_id = object_id();
        float4 result;
        result.xyz = i.local_pos.xyz;
        result.w = bit_cast<float>(primitive_id());
        clicked_id.write(inst_id, result);
    }
    
    // Color remapping
    if (distance(args.from_mapped_color, i.color.xyz) < 1e-2f) {
        i.color.xyz = args.to_mapped_color;
    }
    i.color.w = 1.f;
    return i.color;
}
```

### Debug Logging

From `include/luisa/printer.hpp`:

```cpp
// Enable debug logging by defining DEBUG before including printer
#define DEBUG
#include <luisa/printer.hpp>

// Usage (only logs in debug builds):
device_log("Position: {}, Normal: {}", world_pos, normal);
device_log("Hit instance {} at distance {}", hit.inst, hit.ray_t);
```

## Best Practices

### Thread Divergence

Minimize thread divergence within warps:

```cpp
// Good: All threads take same path
if (dispatch_id().x < count) {  // Aligned to warp boundaries
    // ...
}

// Use wave operations to reduce divergence:
float sum = warp_active_sum(value);  // Efficient reduction
```

### Memory Access

```cpp
// Coalesced memory access (good):
float val = buffer.read(dispatch_id().x);

// Use uniform variants when all threads access same index:
array.uniform_buffer_read<float>(buffer_idx, elem_idx);

// Avoid random access patterns when possible
```

### Resource Binding

```cpp
// Pass resources by reference
[[kernel_1d(128)]] int kernel(
    Buffer<float>& buffer,      // Use & for resources
    Image<float>& image,
    Accel& accel);
```

### Common Constants

```cpp
// Predefined constants:
max_uint32    // Maximum uint32 value (0xFFFFFFFF)
pi            // π
inv_pi        // 1/π
```

## Build Commands

The project provides batch scripts for compiling shaders:

```
dx_compile.cmd          # Compile to DXIL (DirectX)
dx_clean_compile.cmd    # Clean build to DXIL
vk_compile.cmd          # Compile to SPIR-V (Vulkan)
vk_clean_compile.cmd    # Clean build to SPIR-V
gen_json.cmd            # Generate shader metadata JSON
```

## Complete Example

```cpp
#include <luisa/std.hpp>
using namespace luisa::shader;

struct Particle {
    float3 position;
    float3 velocity;
};

[[kernel_1d(128)]] int particle_kernel(
    Buffer<Particle>& particles,
    Buffer<float3>& forces,
    float dt,
    uint count) {
    
    auto id = dispatch_id().x;
    if (id >= count) return 0;
    
    // Read particle
    Particle p = particles.read(id);
    
    // Update
    float3 force = forces.read(id);
    p.velocity += force * dt;
    p.position += p.velocity * dt;
    
    // Write back
    particles.write(id, p);
    
    return 0;
}
```
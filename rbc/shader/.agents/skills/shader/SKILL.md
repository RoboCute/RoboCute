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
| `[[warp_size(N)]]` | Set Warp Size | `[[warp_size(32)]]` |
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

// Check if current thread is in the first warp of the block:
uint is_first_thread = all(thread_id_xy == 0u) ? max_uint32 : 0u;
bool thread_in_first_warp = warp_active_max(is_first_thread) != 0u;
```

### Computing Unique Warp ID within Block

To get a unique warp ID for each warp in a thread block (useful for allocating shared memory per warp):

```cpp


// In kernel:
[[kernel_2d(8, 8)]] int kernel(...) {
    // Define shared counter in shared memory (group shared)
    SharedArray<uint, 1> warp_counter;

    uint2 thread_id_xy = thread_id().xy;
    uint lane_id = warp_lane_id();
    // This is WRONG, ERROR, BAD: warp_id = thread_id_xy.x + thread_id_xy.y * 8
    // MUST use atomic, to get right warp_id
    uint warp_id;
    
    // Reset counter from first thread in block
    if (all(thread_id_xy == 0u)) {
        warp_counter[0] = 0u;
    }
    sync_block();  // Ensure reset is visible to all threads
    
    // First lane of each warp atomically increments to get unique ID
    if (lane_id == 0) {
        warp_id = warp_counter.atomic_fetch_add(0, 1);
    }
    // Broadcast warp_id to all lanes in the warp
    warp_id = warp_read_first_active_lane(warp_id);
    
    // Now warp_id is unique per warp (0, 1, 2, ... num_warps-1)
    // Can be used to index into per-warp shared memory
}
```

From `src/procedural_prim/height_compute_aabb.cpp`.

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


#include "RBCEditor/dummyrt.h"
#include "luisa/core/dynamic_module.h"
#include "luisa/runtime/rhi/pixel.h"
#include <luisa/backends/ext/native_resource_ext.hpp>
#include "RBCEditor/base/device_config.h"

#include "RBCEditor/misc/cornell_box.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "RBCEditor/misc/tiny_obj_loader.h"

using namespace luisa;
using namespace luisa::compute;
// clang-format off
LUISA_STRUCT(rbc::Onb, tangent, binormal, normal){
    [[nodiscard]] Float3 to_world(Expr<float3> v) const noexcept {
        return v.x * tangent + v.y * binormal + v.z * normal;
    }
};

LUISA_STRUCT(rbc::Camera, position, front, up, right, fov) {
    [[nodiscard]] auto generate_ray(Expr<float2> p/* normalized pixel coordinate */) const noexcept {
        auto fov_radians = radians(fov);
        auto wi_local = make_float3(p * tan(0.5f * fov_radians), -1.0f);
        auto wi_world = normalize(wi_local.x * right + wi_local.y * up - wi_local.z * front);
        return make_ray(position, wi_world);
    }
};
// clang-format on

namespace rbc {
void App::init(
    luisa::compute::Context &ctx, const char *backend_name) {
    luisa::string_view backend = backend_name;
    bool gpu_dump;
#ifdef NDEBUG
    gpu_dump = false;
#else
    gpu_dump = true;
#endif
    DeviceConfig device_config = {};
#ifdef LUISA_QT_SAMPLE_ENABLE_DX
    if (backend == "dx") {
        device_config.extension = make_dx_device_config(nullptr, gpu_dump);
    }
#endif
#ifdef LUISA_QT_SAMPLE_ENABLE_VK
    if (backend == "vk") {
        device_config.extension = make_vk_device_config(nullptr, nullptr, nullptr);
    }
#endif
#ifdef LUISA_QT_SAMPLE_ENABLE_METAL
    if (backend == "metal") {
        device_config.extension = nullptr;
    }
#endif
    device_config_ext = device_config.extension.get();
    device = ctx.create_device(backend, &device_config);
#ifdef LUISA_QT_SAMPLE_ENABLE_DX
    void *native_device;
    if (backend == "dx") {
        get_dx_device(device_config_ext, native_device, dx_adaptor_luid);
    }
#endif
#ifdef LUISA_QT_SAMPLE_ENABLE_VK
    if (backend == "vk") {
        device_config.extension = make_vk_device_config(nullptr, nullptr, nullptr);
        get_vk_device(device_config_ext, native_device, vk_physical_device, vk_instance, vk_queue_family_idx);
    }
#endif
    stream = device.create_stream(StreamTag::GRAPHICS);

    // load the Cornell Box scene
    tinyobj::ObjReaderConfig obj_reader_config;
    obj_reader_config.triangulate = true;
    obj_reader_config.vertex_color = false;
    tinyobj::ObjReader obj_reader;
    if (!obj_reader.ParseFromString(obj_string, "", obj_reader_config)) {
        luisa::string_view error_message = "unknown error.";
        if (auto &&e = obj_reader.Error(); !e.empty()) { error_message = e; }
        LUISA_ERROR_WITH_LOCATION("Failed to load OBJ file: {}", error_message);
    }
    if (auto &&e = obj_reader.Warning(); !e.empty()) {
        LUISA_WARNING_WITH_LOCATION("{}", e);
    }
    auto &&p = obj_reader.GetAttrib().vertices;
    luisa::vector<float3> vertices;
    vertices.reserve(p.size() / 3u);
    for (uint i = 0u; i < p.size(); i += 3u) {
        vertices.emplace_back(float3{
            p[i + 0u],
            p[i + 1u],
            p[i + 2u]});
    }
    LUISA_INFO(
        "Loaded mesh with {} shape(s) and {} vertices.",
        obj_reader.GetShapes().size(), vertices.size());

    heap = device.create_bindless_array();
    vertex_buffer = device.create_buffer<float3>(vertices.size());
    stream << vertex_buffer.view().copy_from(vertices.data());
    luisa::vector<Mesh> meshes;
    luisa::vector<Buffer<Triangle>> triangle_buffers;
    for (auto &&shape : obj_reader.GetShapes()) {
        uint index = static_cast<uint>(meshes.size());
        std::vector<tinyobj::index_t> const &t = shape.mesh.indices;
        uint triangle_count = t.size() / 3u;
        LUISA_INFO(
            "Processing shape '{}' at index {} with {} triangle(s).",
            shape.name, index, triangle_count);
        luisa::vector<uint> indices;
        indices.reserve(t.size());
        for (tinyobj::index_t i : t) { indices.emplace_back(i.vertex_index); }
        Buffer<Triangle> &triangle_buffer = triangle_buffers.emplace_back(device.create_buffer<Triangle>(triangle_count));
        Mesh &mesh = meshes.emplace_back(device.create_mesh(vertex_buffer, triangle_buffer));
        heap.emplace_on_update(index, triangle_buffer);
        stream << triangle_buffer.view().copy_from(indices.data())
               << mesh.build();
    }

    accel = device.create_accel({});
    for (Mesh &m : meshes) {
        accel.emplace_back(m, make_float4x4(1.0f));
    }
    stream << heap.update()
           << accel.build()
           << synchronize();

    Constant materials{
        make_float3(0.725f, 0.710f, 0.680f),// floor
        make_float3(0.725f, 0.710f, 0.680f),// ceiling
        make_float3(0.725f, 0.710f, 0.680f),// back wall
        make_float3(0.140f, 0.450f, 0.091f),// right wall
        make_float3(0.630f, 0.065f, 0.050f),// left wall
        make_float3(0.725f, 0.710f, 0.680f),// short box
        make_float3(0.725f, 0.710f, 0.680f),// tall box
        make_float3(0.000f, 0.000f, 0.000f),// light
    };

    Callable linear_to_srgb = [](Var<float3> x) noexcept {
        return clamp(select(1.055f * pow(x, 1.0f / 2.4f) - 0.055f,
                            12.92f * x,
                            x <= 0.00031308f),
                     0.0f, 1.0f);
    };

    Callable tea = [](UInt v0, UInt v1) noexcept {
        UInt s0 = def(0u);
        for (uint n = 0u; n < 4u; n++) {
            s0 += 0x9e3779b9u;
            v0 += ((v1 << 4) + 0xa341316cu) ^ (v1 + s0) ^ ((v1 >> 5u) + 0xc8013ea4u);
            v1 += ((v0 << 4) + 0xad90777du) ^ (v0 + s0) ^ ((v0 >> 5u) + 0x7e95761eu);
        }
        return v0;
    };

    Kernel2D make_sampler_kernel = [&](ImageUInt seed_image) noexcept {
        UInt2 p = dispatch_id().xy();
        UInt state = tea(p.x, p.y);
        seed_image.write(p, make_uint4(state));
    };

    Callable lcg = [](UInt &state) noexcept {
        constexpr uint lcg_a = 1664525u;
        constexpr uint lcg_c = 1013904223u;
        state = lcg_a * state + lcg_c;
        return cast<float>(state & 0x00ffffffu) *
               (1.0f / static_cast<float>(0x01000000u));
    };

    Callable make_onb = [](const Float3 &normal) noexcept {
        Float3 binormal = normalize(ite(
            abs(normal.x) > abs(normal.z),
            make_float3(-normal.y, normal.x, 0.0f),
            make_float3(0.0f, -normal.z, normal.y)));
        Float3 tangent = normalize(cross(binormal, normal));
        return def<Onb>(tangent, binormal, normal);
    };

    Callable cosine_sample_hemisphere = [](Float2 u) noexcept {
        Float r = sqrt(u.x);
        Float phi = 2.0f * constants::pi * u.y;
        return make_float3(r * cos(phi), r * sin(phi), sqrt(1.0f - u.x));
    };

    Callable balanced_heuristic = [](Float pdf_a, Float pdf_b) noexcept {
        return pdf_a / max(pdf_a + pdf_b, 1e-4f);
    };

    Kernel2D raytracing_kernel = [&](ImageFloat image, ImageUInt seed_image,
                                     Var<Camera> camera, AccelVar accel, UInt2 resolution) noexcept {
        set_block_size(16u, 16u, 1u);
        UInt2 coord = dispatch_id().xy();
        Float frame_size = min(resolution.x, resolution.y).cast<float>();
        UInt state = seed_image.read(coord).x;
        Float rx = lcg(state);
        Float ry = lcg(state);
        Float2 pixel = (make_float2(coord) + make_float2(rx, ry)) / frame_size * 2.0f - 1.0f;
        Float3 radiance = def(make_float3(0.0f));
        Var<Ray> ray = camera->generate_ray(pixel * make_float2(1.0f, -1.0f));
        Float3 beta = def(make_float3(1.0f));
        Float pdf_bsdf = def(0.0f);
        constexpr float3 light_position = make_float3(-0.24f, 1.98f, 0.16f);
        constexpr float3 light_u = make_float3(-0.24f, 1.98f, -0.22f) - light_position;
        constexpr float3 light_v = make_float3(0.23f, 1.98f, 0.16f) - light_position;
        constexpr float3 light_emission = make_float3(17.0f, 12.0f, 4.0f);
        Float light_area = length(cross(light_u, light_v));
        Float3 light_normal = normalize(cross(light_u, light_v));
        $for (depth, 10u) {
            // trace
            Var<TriangleHit> hit = accel.intersect(ray, {});
            $if (hit->miss()) { $break; };
            Var<Triangle> triangle = heap->buffer<Triangle>(hit.inst).read(hit.prim);
            Float3 p0 = vertex_buffer->read(triangle.i0);
            Float3 p1 = vertex_buffer->read(triangle.i1);
            Float3 p2 = vertex_buffer->read(triangle.i2);
            Float3 p = triangle_interpolate(hit.bary, p0, p1, p2);
            Float3 n = normalize(cross(p1 - p0, p2 - p0));
            Float cos_wo = dot(-ray->direction(), n);
            $if (cos_wo < 1e-4f) { $break; };

            // hit light
            $if (hit.inst == static_cast<uint>(meshes.size() - 1u)) {
                $if (depth == 0u) {
                    radiance += light_emission;
                }
                $else {
                    Float pdf_light = length_squared(p - ray->origin()) / (light_area * cos_wo);
                    Float mis_weight = balanced_heuristic(pdf_bsdf, pdf_light);
                    radiance += mis_weight * beta * light_emission;
                };
                $break;
            };

            // sample light
            Float ux_light = lcg(state);
            Float uy_light = lcg(state);
            Float3 p_light = light_position + ux_light * light_u + uy_light * light_v;
            Float3 pp = offset_ray_origin(p, n);
            Float3 pp_light = offset_ray_origin(p_light, light_normal);
            Float d_light = distance(pp, pp_light);
            Float3 wi_light = normalize(pp_light - pp);
            Var<Ray> shadow_ray = make_ray(offset_ray_origin(pp, n), wi_light, 0.f, d_light);
            Bool occluded = accel.intersect_any(shadow_ray, {});
            Float cos_wi_light = dot(wi_light, n);
            Float cos_light = -dot(light_normal, wi_light);
            Float3 albedo = materials.read(hit.inst);
            $if (!occluded & cos_wi_light > 1e-4f & cos_light > 1e-4f) {
                Float pdf_light = (d_light * d_light) / (light_area * cos_light);
                Float pdf_bsdf = cos_wi_light * inv_pi;
                Float mis_weight = balanced_heuristic(pdf_light, pdf_bsdf);
                Float3 bsdf = albedo * inv_pi * cos_wi_light;
                radiance += beta * bsdf * mis_weight * light_emission / max(pdf_light, 1e-4f);
            };

            // sample BSDF
            Var<Onb> onb = make_onb(n);
            Float ux = lcg(state);
            Float uy = lcg(state);
            Float3 wi_local = cosine_sample_hemisphere(make_float2(ux, uy));
            Float cos_wi = abs(wi_local.z);
            Float3 new_direction = onb->to_world(wi_local);
            ray = make_ray(pp, new_direction);
            pdf_bsdf = cos_wi * inv_pi;
            beta *= albedo;// * cos_wi * inv_pi / pdf_bsdf => * 1.f

            // rr
            Float l = dot(make_float3(0.212671f, 0.715160f, 0.072169f), beta);
            $if (l == 0.0f) { $break; };
            Float q = max(l, 0.05f);
            Float r = lcg(state);
            $if (r >= q) { $break; };
            beta *= 1.0f / q;
        };
        seed_image.write(coord, make_uint4(state));
        $if (any(dsl::isnan(radiance))) { radiance = make_float3(0.0f); };

        UInt2 uv = dispatch_id().xy();
        // uv.y = resolution.y - uv.y - 1u;// flip y
        image.write(uv, make_float4(clamp(radiance, 0.0f, 30.0f), 1.0f));
    };

    Kernel2D accumulate_kernel = [&](ImageFloat accum_image, ImageFloat curr_image) noexcept {
        UInt2 p = dispatch_id().xy();
        Float4 accum = accum_image.read(p);
        Float3 curr = curr_image.read(p).xyz();
        accum_image.write(p, accum + make_float4(curr, 1.f));
    };

    Callable aces_tonemapping = [](Float3 x) noexcept {
        static constexpr float a = 2.51f;
        static constexpr float b = 0.03f;
        static constexpr float c = 2.43f;
        static constexpr float d = 0.59f;
        static constexpr float e = 0.14f;
        return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0f, 1.0f);
    };

    Kernel2D clear_kernel = [](ImageFloat image) noexcept {
        image.write(dispatch_id().xy(), make_float4(0.0f));
    };

    Kernel2D hdr2ldr_kernel = [&](ImageFloat hdr_image, ImageFloat ldr_image, Float scale, Bool is_hdr) noexcept {
        UInt2 coord = dispatch_id().xy();
        Float4 hdr = hdr_image.read(coord);
        Float3 ldr = hdr.xyz() / hdr.w * scale;
        $if (!is_hdr) {
            ldr = linear_to_srgb(ldr);
        };
        ldr_image.write(coord, make_float4(ldr, 1.0f));
    };

    clear_shader = device.compile(clear_kernel);
    hdr2ldr_shader = device.compile(hdr2ldr_kernel);
    accumulate_shader = device.compile(accumulate_kernel);
    raytracing_shader = device.compile(raytracing_kernel);
    make_sampler_shader = device.compile(make_sampler_kernel);

    camera = Camera{
        .position = make_float3(-0.01f, 0.995f, 5.0f),
        .front = make_float3(0.f, 0.f, -1.f),
        .up = make_float3(0.f, 1.f, 0.f),
        .right = make_float3(1.f, 0.f, 0.f),
        .fov = 27.8f};
    camera_controller = luisa::make_unique<FPVCameraController>(camera, 1.f, 20.f, .5f);
    last_time = 0.0;
    is_dirty = true;
    delta_time = 0.;

    // ================================================================

    Kernel2D draw_kernel = [](ImageFloat image, Float time, Float2 resolution) noexcept {
        auto p = dispatch_id().xy();
        auto uv = make_float2(p) / make_float2(resolution) * 2.0f - 1.0f;
        auto color = def(make_float4());
        Constant scales{pi, luisa::exp(1.f), luisa::sqrt(2.f)};
        for (auto i = 0u; i < 3u; i++) {
            color[i] = cos(time * scales[i] + uv.y * 11.f +
                           sin(-time * scales[2u - i] + uv.x * 7.f) * 4.f) *
                           .5f +
                       .5f;
        }
        color[3] = 1.0f;
        image.write(p, color);
    };

    draw_shader = device.compile(draw_kernel);
}

uint64_t App::create_texture(uint width, uint height) {

    resolution = {width, height};
    if (dummy_image && any(dummy_image.size() != uint2(width, height))) {
        cmd_list.add_callback([dummy_image = std::move(dummy_image)] {});
    }
    if (!dummy_image) {
        dummy_image = device.create_image<float>(PixelStorage::BYTE4, width, height);
        framebuffer = device.create_image<float>(PixelStorage::HALF4, resolution);
        accum_image = device.create_image<float>(PixelStorage::FLOAT4, resolution);
        luisa::vector<std::array<uint8_t, 4u>> host_image(resolution.x * resolution.y);
        seed_image = device.create_image<uint>(PixelStorage::INT1, resolution);
    }

    // resolution changed, all textures should reload

    return (int64_t)dummy_image.native_handle();
}

void App::handle_key(luisa::compute::Key key) {
    if (!camera_controller.get()) { return; }
    auto dt = static_cast<float>(delta_time / 1000.0);
    switch (key) {
        case KEY_W:
            camera_controller->rotate_pitch(dt);
            is_dirty = true;
            break;
        case KEY_A:
            camera_controller->rotate_yaw(dt);
            is_dirty = true;
            break;
        case KEY_S:
            camera_controller->rotate_pitch(-dt);
            is_dirty = true;
            break;
        case KEY_D:
            camera_controller->rotate_yaw(-dt);
            is_dirty = true;
            break;
        default:
            return;
    }
}

void App::update() {
    // cmd_list << clear_shader(dummy_image).dispatch(resolution);
    // float2 f_res = {(float)resolution.x, (float)resolution.y};
    // cmd_list
    //     << draw_shader(dummy_image, clk.toc() * 1e-3, f_res).dispatch(resolution);
    // stream << cmd_list.commit();
    delta_time = clk.toc() - last_time;
    last_time = clk.toc();
    if (is_dirty) {
        cmd_list << clear_shader(accum_image).dispatch(resolution);
        is_dirty = false;
    }
    cmd_list << raytracing_shader(framebuffer, seed_image, camera, accel, resolution)
                    .dispatch(resolution);
    cmd_list << accumulate_shader(accum_image, framebuffer)
                    .dispatch(resolution);
    // cmd_list << hdr2ldr_shader(accum_image, ldr_image, 1.0f, false).dispatch(resolution);
    cmd_list << hdr2ldr_shader(accum_image, dummy_image, 1.0f, false).dispatch(resolution);
    stream << cmd_list.commit();

    // Post Update
    if (device.backend_name() == "dx") {
        set_dx_before_state(device_config_ext, Argument::Texture{dummy_image.handle(), 0}, D3D12EnhancedResourceUsageType::RasterRead);
    } else {
        set_vk_before_state(device_config_ext, Argument::Texture{dummy_image.handle(), 0}, VkResourceUsageType::RasterRead);
    }
}
App::~App() {
    camera_controller.reset();
    stream.synchronize();
}
void *App::init_vulkan(luisa::compute::Context &ctx) {
    auto const &vk_backend = ctx.load_backend("vk");
    return vk_backend.invoke<void *(bool enable_validation, const luisa::string *extra_instance_exts, size_t extra_instance_ext_count, const char *custom_vk_lib_path, const char *custom_vk_lib_name)>(
        "init_vk_instance",
        false,
        nullptr, 0,
        nullptr, nullptr);
}
}// namespace rbc

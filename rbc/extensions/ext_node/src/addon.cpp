// rbc/extensions/ext_node/src/addon.cpp
//
// Hand-written N-API frontend for the RoboCute engine: the Node.js/Electron
// counterpart of the pybind11 frontend in rbc/extensions/ext_c.
//
// SPIKE SCOPE: bind only what samples/app_graphics_scene.py needs —
//   context/project/scene setup, material + mesh + entity creation, headless
//   display rendering, per-frame CPU readback streamed to JS via TSFN.
//
// Design notes:
//   * All engine API calls happen on ONE dedicated "engine thread" (created in
//     Start). JS threads only enqueue atomic commands (camera deltas / stop).
//   * Frames are transferred to JS as external ArrayBuffers (malloc'd, moved
//     to the V8 GC via finalizer). TSFN queue is capped; overflow drops frames.
//   * Object handles are intentionally never released (same convention as the
//     Python frontend); everything dies with the process.

#include <node_api.h>

#include <windows.h>
#include <dbghelp.h>

#include "platform/viewport.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <generated/world.h>
#include <external_display.h>
#include <luisa/runtime/rhi/command.h>
#include <luisa/runtime/rhi/pixel.h>
#include <luisa/vstl/v_guid.h>
#include <rbc_graphics/render_device.h>

namespace {

using luisa::compute::PixelStorage;
using luisa::compute::TextureDownloadCommand;

struct Options {
    std::string project_path;
    std::string backend = "dx";
    std::string program_path;
    uint32_t width = 1280;
    uint32_t height = 720;
    // shared-present mode: render straight into a DXGI swapchain on a native
    // child window (borrowed from the Electron browser window) — no CPU
    // readback, no frame IPC. parent_hwnd is the Electron window's HWND.
    bool shared_present = false;
    uint64_t parent_hwnd = 0;
    // UI chrome insets: the viewport covers the parent client area minus a
    // top toolbar band and a right sidebar (HTML UI lives there).
    uint32_t viewport_top = 0;
    uint32_t viewport_right = 0;
};

// Camera input sink, registered with the platform viewport layer (defined
// after g_engine; declared here because EngineDemo::start references them).
void viewportCameraRotate(float yaw, float pitch);
void viewportCameraZoom(float dz);

struct FrameMsg {
    uint8_t *rgba = nullptr;  // malloc'd w*h*4 RGBA8, ownership moves to JS
    uint32_t w = 0;
    uint32_t h = 0;
    uint32_t index = 0;
    bool screenshot = false;  // one-shot readback (shared mode), not a stream frame
    char error[512] = {0};
};

float halfToFloat(uint16_t h) {
    uint32_t sign = (h & 0x8000u) << 16;
    uint32_t exp = (h & 0x7C00u) >> 10;
    uint32_t mant = h & 0x03FFu;
    uint32_t f;
    if (exp == 0) {
        if (mant == 0) {
            f = sign;
        } else {
            // subnormal
            exp = 1;
            while ((mant & 0x0400u) == 0) { mant <<= 1; --exp; }
            mant &= 0x03FFu;
            uint32_t e = 127u - 15u - exp + 1u;
            f = sign | (e << 23) | (mant << 13);
        }
    } else if (exp == 0x1Fu) {
        f = sign | 0x7F800000u | (mant << 13);  // inf / nan
    } else {
        uint32_t e = exp - 15u + 127u;
        f = sign | (e << 23) | (mant << 13);
    }
    float out;
    std::memcpy(&out, &f, sizeof(float));
    return out;
}

uint8_t toU8(float v) {
    if (!(v > 0.0f)) return 0;      // also catches NaN
    if (v > 1.0f) v = 1.0f;
    return static_cast<uint8_t>(v * 255.0f + 0.5f);
}

void convertFrame(const uint8_t *raw, PixelStorage storage, size_t pixel_count, uint8_t *out) {
    switch (storage) {
        case PixelStorage::BYTE4:
            std::memcpy(out, raw, pixel_count * 4);
            break;
        case PixelStorage::FLOAT4: {
            const float *f = reinterpret_cast<const float *>(raw);
            for (size_t i = 0; i < pixel_count * 4; ++i) out[i] = toU8(f[i]);
            break;
        }
        case PixelStorage::HALF4: {
            const uint16_t *hf = reinterpret_cast<const uint16_t *>(raw);
            for (size_t i = 0; i < pixel_count * 4; ++i) out[i] = toU8(halfToFloat(hf[i]));
            break;
        }
        default:
            throw std::runtime_error("unsupported display pixel storage for RGBA8 conversion");
    }
}

// The shared-present viewport itself lives in src/platform/(see
// platform/viewport.h): a native child window of the Electron browser window
// whose handle is lent to RBC via set_external_display_handles; RBC creates
// a GPU swapchain on it and tick() presents directly. This file only owns
// the singleton instance and the camera sink that feeds EngineDemo.
extern rbcnode::Viewport g_viewport;

// ---------------------------------------------------------------------------
// EngineDemo: mirrors samples/app_graphics_scene.py on a dedicated thread.
// ---------------------------------------------------------------------------
class EngineDemo {
public:
    void start(Options opt, napi_threadsafe_function tsfn) {
        opt_ = std::move(opt);
        tsfn_ = tsfn;
        auto t0 = std::chrono::steady_clock::now();
        auto stamp = [t0](const char *what) {
            fprintf(stderr, "[rbc_ext_node] timing: %s @%.0fms\n", what,
                    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
        };
        if (opt_.shared_present) {
            if (!opt_.parent_hwnd) throw std::runtime_error("shared present requires a parent HWND");
            g_viewport.parent_handle = opt_.parent_hwnd;
            g_viewport.inset_top = opt_.viewport_top;
            g_viewport.inset_right = opt_.viewport_right;
            rbcnode::setCameraSink({viewportCameraRotate, viewportCameraZoom});
            // initEngine needs the handle; wait for the platform thread to create it
            stamp("viewport thread spawned");
            rbcnode::viewportStart(g_viewport);
            for (int i = 0; i < 500 && !g_viewport.viewport_handle; ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            stamp("viewport hwnd ready");
            if (!g_viewport.viewport_handle)
                throw std::runtime_error("shared viewport window failed to create (or platform unsupported)");
        }
        running_ = true;
        stamp("engine thread spawning");
        thread_ = std::thread([this] { run(); });
    }

    void stop() {
        running_ = false;
        if (thread_.joinable()) thread_.join();
        rbcnode::viewportStop(g_viewport);
    }

    void cameraRotate(float yaw, float pitch) {
        yaw_accum_ += yaw;
        pitch_accum_ += pitch;
    }

    void cameraZoom(float dz) { dolly_accum_ += dz; }

    void setTurntable(bool on) { turntable_.store(on); }
    bool turntable() const { return turntable_.load(); }

    // One-shot CPU readback of the current display image (shared mode):
    // picked up by the render loop, delivered through the TSFN with the
    // screenshot flag set. No-op in readback mode (JS captures the canvas).
    void requestScreenshot() { screenshot_pending_ = true; }

    std::string lastError() {
        std::lock_guard<std::mutex> lk(mtx_);
        return last_error_;
    }

    // Stats for the UI (called from the JS thread; all sources are atomics).
    struct Stats {
        double fps;
        uint32_t width, height;
        bool shared, turntable;
    };
    Stats stats() const {
        Stats s{};
        s.fps = fps_.load();
        if (opt_.shared_present) {
            s.width = viewport_size_.x;
            s.height = viewport_size_.y;
        } else {
            s.width = last_frame_w_.load();
            s.height = last_frame_h_.load();
        }
        s.shared = opt_.shared_present;
        s.turntable = turntable();
        return s;
    }

private:
    void setError(const std::string &msg) {
        std::lock_guard<std::mutex> lk(mtx_);
        last_error_ = msg;
    }

    void sendError(const char *what) {
        setError(what);
        auto *msg = new FrameMsg{};
        std::strncpy(msg->error, what, sizeof(msg->error) - 1);
        napi_call_threadsafe_function(tsfn_, msg, napi_tsfn_nonblocking);
    }

    void sendFrame(uint8_t *rgba, uint32_t w, uint32_t h, uint32_t index, bool screenshot = false) {
        auto *msg = new FrameMsg{};
        msg->rgba = rgba;
        msg->w = w;
        msg->h = h;
        msg->index = index;
        msg->screenshot = screenshot;
        last_frame_w_.store(w);
        last_frame_h_.store(h);
        tallyFrame();
        napi_status st = napi_call_threadsafe_function(tsfn_, msg, napi_tsfn_nonblocking);
        if (st == napi_queue_full) {
            free(rgba);
            delete msg;
        } else if (st != napi_ok) {
            setError("napi_call_threadsafe_function failed");
            free(rgba);
            delete msg;
        }
    }

    // Shared fps tally for getStats(); the stderr telemetry logs stay separate.
    void tallyFrame() {
        fps_frames_++;
        auto now = std::chrono::steady_clock::now();
        if (now - fps_last_ >= std::chrono::seconds(2)) {
            fps_.store(fps_frames_ / std::chrono::duration<double>(now - fps_last_).count());
            fps_frames_ = 0;
            fps_last_ = now;
        }
    }

    static std::string guidString(void *object_handle) {
        vstd::Guid g = rbc::Object::guid(object_handle);
        auto s = g.to_base64();
        return std::string(s.data(), s.size());
    }

    void makeMaterial(void *mat, const char *albedo_json, const std::string &tex_guid) {
        std::string json = std::string("{\"type\":\"pbr\",\"base_albedo\":") + albedo_json +
            ",\"specular_roughness\":0.6,\"weight_metallic\":0.3,\"base_albedo_tex\":[\"" +
            tex_guid + "\",0]}";
        rbc::MaterialResource::load_from_json(mat, json.c_str());
    }

    void addCubeToMesh(float *P,          // pos: float4 x vertex_count
                       float *UV,         // uv0: float2 x vertex_count
                       uint32_t *I,       // indices: uint3 x triangle_count
                       float ox, float oy, float oz, float s,
                       uint32_t vertex_start, uint32_t triangle_start) {
        static const float pos[8][3] = {
            {-0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, 0.5f},
            {-0.5f, 0.5f, -0.5f},  {-0.5f, 0.5f, 0.5f},  {0.5f, 0.5f, -0.5f},  {0.5f, 0.5f, 0.5f},
        };
        for (uint32_t i = 0; i < 8; ++i) {
            P[(vertex_start + i) * 4 + 0] = pos[i][0] * s + ox;
            P[(vertex_start + i) * 4 + 1] = pos[i][1] * s + oy;
            P[(vertex_start + i) * 4 + 2] = pos[i][2] * s + oz;
            P[(vertex_start + i) * 4 + 3] = 1.0f;
            UV[(vertex_start + i) * 2 + 0] = (i == 2 || i == 3 || i == 6 || i == 7) ? 1.0f : 0.0f;
            UV[(vertex_start + i) * 2 + 1] = (i & 1) ? 1.0f : 0.0f;
        }
        static const uint32_t tri[12][3] = {
            {0, 1, 2}, {1, 3, 2}, {4, 5, 6}, {5, 7, 6}, {0, 1, 4},
            {1, 5, 4}, {2, 3, 6}, {3, 7, 6}, {0, 2, 4}, {2, 6, 4}, {1, 3, 5}, {3, 7, 5},
        };
        for (uint32_t i = 0; i < 12; ++i) {
            I[(triangle_start + i) * 3 + 0] = vertex_start + tri[i][0];
            I[(triangle_start + i) * 3 + 1] = vertex_start + tri[i][1];
            I[(triangle_start + i) * 3 + 2] = vertex_start + tri[i][2];
        }
    }

    void makeCubeEntity() {
        std::string tex_guid = guidString(tex_);

        mat0_ = rbc::MaterialResource::_create_();
        makeMaterial(mat0_, "[0.8,0.8,0.8]", tex_guid);
        mat1_ = rbc::MaterialResource::_create_();
        makeMaterial(mat1_, "[0.14,0.45,0.091]", tex_guid);

        // 2 submeshes: cube A (12 tris) + cube B (12 tris)
        uint32_t submesh_offsets[2] = {0, 12};
        mesh_ = rbc::MeshResource::_create_();
        rbc::MeshResource::create_empty(
            mesh_, luisa::span<std::byte>(reinterpret_cast<std::byte *>(submesh_offsets), sizeof(submesh_offsets)),
            16, 24, 1, false, false);

        auto pos_span = rbc::MeshResource::pos_buffer(mesh_);
        auto uv_span = rbc::MeshResource::uv_buffer(mesh_, 0);
        auto idx_span = rbc::MeshResource::triangle_indices_buffer(mesh_);
        std::memset(pos_span.data(), 0, pos_span.size());
        std::memset(uv_span.data(), 0, uv_span.size());
        std::memset(idx_span.data(), 0, idx_span.size());
        addCubeToMesh(reinterpret_cast<float *>(pos_span.data()),
                      reinterpret_cast<float *>(uv_span.data()),
                      reinterpret_cast<uint32_t *>(idx_span.data()),
                      0.0f, 0.0f, 0.0f, 1.0f, 0, 0);
        addCubeToMesh(reinterpret_cast<float *>(pos_span.data()),
                      reinterpret_cast<float *>(uv_span.data()),
                      reinterpret_cast<uint32_t *>(idx_span.data()),
                      0.0f, 1.0f, 0.0f, 0.4f, 8, 12);
        rbc::Resource::install(mesh_);

        entity_ = rbc::Scene::add_entity(scene_);
        rbc::Entity::set_name(entity_, "electron_cube");
        cube_trans_ = rbc::Entity::add_component(entity_, "TransformComponent");
        void *render = rbc::Entity::add_component(entity_, "RenderComponent");
        rbc::TransformComponent::set_pos(cube_trans_, luisa::double3{0.0, 0.0, 1.0}, false);

        luisa::vector<rbc::RC<rbc::RCBase>> mats;
        mats.emplace_back(reinterpret_cast<rbc::RCBase *>(mat0_));
        mats.emplace_back(reinterpret_cast<rbc::RCBase *>(mat1_));
        rbc::RenderComponent::update_object(render, mats, mesh_);
    }

    void initEngine() {
        auto t0 = std::chrono::steady_clock::now();
        auto stamp = [t0](const char *what) {
            fprintf(stderr, "[rbc_ext_node] timing: %s @%.0fms\n", what,
                    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
        };
        stamp("initEngine begin");
        ctx_ = rbc::RBCContext::_create_();
        if (!ctx_) throw std::runtime_error("RBCContext creation failed");
        stamp("RBCContext created");

        std::string shader_path = opt_.program_path + "/shader_build_" + opt_.backend;
        rbc::RBCContext::init_device(ctx_, opt_.backend.c_str(), opt_.program_path.c_str(),
                                     shader_path.c_str(), false);
        stamp("init_device done");
        rbc::RBCContext::init_render(ctx_);
        stamp("init_render done");

        std::string world_path = opt_.project_path + "/library";
        rbc::RBCContext::init_world(ctx_, world_path.c_str(), world_path.c_str());

        project_ = rbc::Project::_create_();
        rbc::Project::init(project_, opt_.project_path.c_str());
        rbc::Project::scan_project(project_);

        scene_ = rbc::Project::import_scene(project_, "test_scene.scene", "");
        if (!scene_) throw std::runtime_error("failed to import default scene 'test_scene.scene'");
        rbc::Resource::install(scene_);

        if (opt_.shared_present) {
            // Hand the native viewport handle to RBC: init_display creates a
            // real swapchain on it, and every tick() presents GPU-direct.
            rbcnode::ViewportRect vr = rbcnode::viewportRect(g_viewport);
            viewport_size_ = luisa::uint2{(uint32_t)vr.w, (uint32_t)vr.h};
            rbcnode::viewportMove(g_viewport, vr);
            rbc::set_external_display_handles(0, g_viewport.viewport_handle);
            rbc::RBCContext::init_display(ctx_, "electron_shared",
                                          viewport_size_, false, false, false, false);
        } else {
            rbc::RBCContext::init_display(ctx_, "electron_headless",
                                          luisa::uint2{opt_.width, opt_.height},
                                          false, false, false, false);
        }
        cam_ = rbc::RBCContext::create_display_cam(ctx_);
        if (!cam_) throw std::runtime_error("create_display_cam failed");
        rbc::CameraComponent::enable_camera(cam_);
        // Allows control_camera_add_pos/rotate (native viewport + JS path).
        // Works without an LC window since the embedding patch.
        rbc::RBCContext::enable_camera_control(ctx_);
        void *settings = rbc::CameraComponent::render_settings(cam_);
        if (settings) rbc::RenderSettings::set_use_auto_exposure(settings, false);

        void *cam_entity = rbc::Component::entity(cam_);
        void *cam_trans = rbc::Entity::get_component(cam_entity, "TransformComponent");
        if (cam_trans) rbc::TransformComponent::set_pos(cam_trans, luisa::double3{0.0, 0.0, -1.0}, false);

        tex_ = rbc::Project::import_texture(project_, "test_grid.png", 1, false);
        if (!tex_) throw std::runtime_error("failed to import test_grid.png");

        makeCubeEntity();
    }

    void run() {
        try {
            initEngine();
        } catch (const std::exception &e) {
            std::string msg = std::string("init failed: ") + e.what();
            fprintf(stderr, "[rbc_ext_node] %s\n", msg.c_str());
            sendError(msg.c_str());
            return;
        }
        fprintf(stderr, "[rbc_ext_node] engine initialized, entering render loop\n");

        if (!rbc::RenderDevice::instance_ptr()) {
            sendError("RenderDevice singleton not available after init");
            return;
        }

        auto last = std::chrono::steady_clock::now();
        float angle = 0.0f;
        uint64_t frame_index = 0;

        while (running_) {
            try {
                auto now = std::chrono::steady_clock::now();
                float dt = std::chrono::duration<float>(now - last).count();
                last = now;

                // external camera control (from JS mouse gestures)
                float yaw = yaw_accum_.exchange(0.0f);
                float pitch = pitch_accum_.exchange(0.0f);
                if (yaw != 0.0f || pitch != 0.0f)
                    rbc::RBCContext::control_camera_add_rotate(ctx_, yaw, pitch, 0.0f);
                float dz = dolly_accum_.exchange(0.0f);
                if (dz != 0.0f) rbc::RBCContext::control_camera_add_pos(ctx_, luisa::float3{0.0f, 0.0f, dz});

                // turntable animation (UI-toggleable) to prove per-frame engine interaction
                if (turntable_) {
                    angle += dt * 0.4f;
                    float ha = angle * 0.5f;
                    rbc::TransformComponent::set_rotation(cube_trans_, luisa::float4{0.0f, std::sin(ha), 0.0f, std::cos(ha)}, false);
                }

                rbc::CameraComponent::set_frame_index(cam_, frame_index);
                auto frame_begin = std::chrono::steady_clock::now();
                bool reset = rbc::RBCContext::tick(ctx_, dt, rbc::TickStage::PathTracingPreview, false);
                frame_index = reset ? 0 : frame_index + 1;

                if (opt_.shared_present) {
                    // tick() already blitted dst->present image and presented
                    // to the swapchain on our viewport window (GPU-direct,
                    // zero CPU copies). Only housekeeping left: follow parent
                    // resizes, pace the loop, and serve one-shot screenshots.
                    if (!rbcnode::viewportParentAlive(g_viewport)) {
                        fprintf(stderr, "[rbc_ext_node] parent window gone, stopping\n");
                        return;
                    }
                    rbcnode::ViewportRect vr = rbcnode::viewportRect(g_viewport);
                    luisa::uint2 new_size{(uint32_t)vr.w, (uint32_t)vr.h};
                    if (any(new_size != viewport_size_)) {
                        viewport_size_ = new_size;
                        rbcnode::viewportMove(g_viewport, vr);
                        rbc::RBCContext::reset_view(ctx_, new_size);
                        frame_index = 0;
                    }
                    // cheap z-order insurance: Chromium occasionally re-positions
                    // its compositor child; stay on top of the sibling stack
                    if ((frame_index % 60) == 0)
                        rbcnode::viewportRaise(g_viewport);
                    constexpr auto kTargetFrame = std::chrono::duration<float, std::milli>(1000.0f / 60.0f);
                    auto elapsed = std::chrono::steady_clock::now() - frame_begin;
                    if (elapsed < kTargetFrame)
                        std::this_thread::sleep_for(kTargetFrame - elapsed);
                    tallyFrame();
                    auto fps_now = std::chrono::steady_clock::now();
                    if (fps_now - fps_last_ >= std::chrono::seconds(2)) {
                        fprintf(stderr, "[rbc_ext_node] shared present: %.1f fps (%ux%u)\n",
                                fps_.load(), viewport_size_.x, viewport_size_.y);
                        fflush(stderr);
                    }
                    if (screenshot_pending_.exchange(false)) {
                        // one-shot CPU readback of the freshly presented frame
                        captureScreenshotOnce(frame_index);
                    }
                    continue;
                }

                uint32_t fw = 0, fh = 0;
                uint8_t *rgba = readbackRGBA(fw, fh);
                sendFrame(rgba, fw, fh, static_cast<uint32_t>(frame_index));
            } catch (const std::exception &e) {
                std::string msg = std::string("render loop error: ") + e.what();
                fprintf(stderr, "[rbc_ext_node] %s\n", msg.c_str());
                sendError(msg.c_str());
                return;
            }
        }
    }

private:
    // GPU->CPU readback of the current display image, converted to RGBA8.
    // Caller owns the returned buffer (malloc'd). Must run on the engine
    // thread. Uses the texture's REAL size — the engine's display image may
    // differ from the requested display resolution (e.g. headless defaults),
    // and downloading with the wrong size yields a garbled frame.
    uint8_t *readbackRGBA(uint32_t &fw, uint32_t &fh) {
        auto *rd = rbc::RenderDevice::instance_ptr();
        if (!rd) throw std::runtime_error("RenderDevice not available");
        auto info = rbc::RBCContext::display_image(ctx_);
        if (info.handle == ~0ull) throw std::runtime_error("display_image returned invalid handle");
        fw = info.width ? info.width : opt_.width;
        fh = info.height ? info.height : opt_.height;
        auto storage = luisa::compute::pixel_format_to_storage(info.format);
        size_t raw_size = luisa::compute::pixel_storage_size(
            storage, luisa::uint3{fw, fh, 1});
        if (raw_.size() < raw_size) raw_.resize(raw_size);

        rd->lc_main_cmd_list() << luisa::make_unique<TextureDownloadCommand>(
            info.handle, storage, 0, luisa::uint3{fw, fh, 1}, raw_.data());
        if (!rd->lc_main_cmd_list().empty()) {
            rd->execute_before_cmdlist_commit_task();
            rd->lc_main_stream() << rd->lc_main_cmd_list().commit();
        }
        rd->execute_after_cmdlist_commit_task();
        rd->lc_main_stream().synchronize();

        size_t pixels = size_t(fw) * fh;
        auto *rgba = static_cast<uint8_t *>(malloc(pixels * 4));
        if (!rgba) throw std::runtime_error("out of memory for frame buffer");
        convertFrame(raw_.data(), storage, pixels, rgba);
        return rgba;
    }

    void captureScreenshotOnce(uint64_t frame_index) {
        try {
            uint32_t fw = 0, fh = 0;
            uint8_t *rgba = readbackRGBA(fw, fh);
            sendFrame(rgba, fw, fh, static_cast<uint32_t>(frame_index), /*screenshot=*/true);
        } catch (const std::exception &e) {
            sendError((std::string("screenshot failed: ") + e.what()).c_str());
        }
    }

public:

    Options opt_;
    luisa::uint2 viewport_size_{0u, 0u};
    std::vector<uint8_t> raw_;
    uint32_t fps_frames_ = 0;
    std::atomic<double> fps_{0.0};
    std::atomic<uint32_t> last_frame_w_{0}, last_frame_h_{0};
    std::atomic<bool> turntable_{true};
    std::atomic<bool> screenshot_pending_{false};
    std::chrono::steady_clock::time_point fps_last_{std::chrono::steady_clock::now()};
    napi_threadsafe_function tsfn_ = nullptr;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<float> yaw_accum_{0.0f};
    std::atomic<float> pitch_accum_{0.0f};
    std::atomic<float> dolly_accum_{0.0f};
    std::mutex mtx_;
    std::string last_error_;

    void *ctx_ = nullptr;
    void *project_ = nullptr;
    void *scene_ = nullptr;
    void *cam_ = nullptr;
    void *tex_ = nullptr;
    void *entity_ = nullptr;
    void *cube_trans_ = nullptr;
    void *mesh_ = nullptr;
    void *mat0_ = nullptr;
    void *mat1_ = nullptr;
};

EngineDemo g_engine;
napi_threadsafe_function g_tsfn = nullptr;
rbcnode::Viewport g_viewport;

// Camera sink registered with the platform viewport (platform/viewport.h);
// the platform layer translates native input into these deltas.
void viewportCameraRotate(float yaw, float pitch) { g_engine.cameraRotate(yaw, pitch); }
void viewportCameraZoom(float dz) { g_engine.cameraZoom(dz); }

// ---------------------------------------------------------------------------
// Crash diagnostics: Electron's crashpad swallows native crashes silently, so
// install a VEH that writes a minidump before the process dies.
// ---------------------------------------------------------------------------
LONG WINAPI crashVehHandler(EXCEPTION_POINTERS *ep) {
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (code == EXCEPTION_BREAKPOINT || code == EXCEPTION_SINGLE_STEP ||
        code == DBG_PRINTEXCEPTION_C || code == DBG_PRINTEXCEPTION_WIDE_C) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    wchar_t path[MAX_PATH];
    GetTempPathW(MAX_PATH, path);
    wcscat_s(path, L"rbc_ext_node_crash.dmp");
    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei{};
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = ep;
        mei.ClientPointers = FALSE;
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), f,
                          MiniDumpWithFullMemory, &mei, nullptr, nullptr);
        CloseHandle(f);
        fprintf(stderr, "[rbc_ext_node] crash dump written: (see %%TEMP%%\\rbc_ext_node_crash.dmp) code=%08lx addr=%p\n",
                code, ep->ExceptionRecord->ExceptionAddress);
        fflush(stderr);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

// ---------------------------------------------------------------------------
// N-API glue
// ---------------------------------------------------------------------------
void frameFinalize(napi_env /*env*/, void *data, void * /*hint*/) { free(data); }

// JS callback signature: (frame, error, width, height, kind)
//   frame: ArrayBuffer RGBA8 (null on error)
//   error: string (null on success)
//   width/height: frame size
//   kind: 'frame' (readback stream) | 'screenshot' (one-shot shared capture)
void tsfnCallback(napi_env env, napi_value js_cb, void * /*ctx*/, void *data) {
    FrameMsg *msg = static_cast<FrameMsg *>(data);
    napi_value args[5];
    napi_get_undefined(env, &args[4]);
    napi_create_string_utf8(env, msg->screenshot ? "screenshot" : "frame",
                            NAPI_AUTO_LENGTH, &args[4]);
    napi_create_uint32(env, msg->w, &args[2]);
    napi_create_uint32(env, msg->h, &args[3]);
    if (msg->rgba) {
        size_t nbytes = size_t(msg->w) * msg->h * 4;
        napi_value ab;
        napi_status st = napi_create_external_arraybuffer(env, msg->rgba, nbytes,
                                                          frameFinalize, nullptr, &ab);
        if (st == napi_ok) {
            msg->rgba = nullptr;  // ownership moved to the ArrayBuffer finalizer
        } else {
            // Fallback: some hosts (e.g. Electron in RUN_AS_NODE) refuse
            // external array buffers; copy into a V8-owned buffer instead.
            void *copy = nullptr;
            if (napi_create_arraybuffer(env, nbytes, &copy, &ab) == napi_ok) {
                memcpy(copy, msg->rgba, nbytes);
            } else {
                napi_get_undefined(env, &ab);
            }
            free(msg->rgba);
            msg->rgba = nullptr;
        }
        args[0] = ab;
        napi_get_undefined(env, &args[1]);
    } else {
        napi_get_null(env, &args[0]);
        napi_create_string_utf8(env, msg->error, NAPI_AUTO_LENGTH, &args[1]);
    }
    napi_value undefined;
    napi_get_undefined(env, &undefined);
    napi_call_function(env, undefined, js_cb, 5, args, nullptr);
    delete msg;
}

bool getObjString(napi_env env, napi_value obj, const char *name, std::string &out) {
    napi_value v;
    if (napi_get_named_property(env, obj, name, &v) != napi_ok) return false;
    size_t len = 0;
    if (napi_get_value_string_utf8(env, v, nullptr, 0, &len) != napi_ok) return false;
    out.resize(len);
    napi_get_value_string_utf8(env, v, out.data(), len + 1, &len);
    return true;
}

bool getObjUint32(napi_env env, napi_value obj, const char *name, uint32_t &out) {
    napi_value v;
    if (napi_get_named_property(env, obj, name, &v) != napi_ok) return false;
    return napi_get_value_uint32(env, v, &out) == napi_ok;
}

napi_value Start(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    if (argc < 2) {
        napi_throw_error(env, nullptr, "start(options, callback) expects 2 arguments");
        return nullptr;
    }

    Options opt;
    getObjString(env, args[0], "projectPath", opt.project_path);
    getObjString(env, args[0], "backend", opt.backend);
    getObjString(env, args[0], "programPath", opt.program_path);
    getObjUint32(env, args[0], "width", opt.width);
    getObjUint32(env, args[0], "height", opt.height);
    std::string present, parent_hwnd;
    if (getObjString(env, args[0], "present", present))
        opt.shared_present = present == "shared";
    if (getObjString(env, args[0], "parentHwnd", parent_hwnd) && !parent_hwnd.empty())
        opt.parent_hwnd = std::strtoull(parent_hwnd.c_str(), nullptr, 10);
    getObjUint32(env, args[0], "viewportTop", opt.viewport_top);
    getObjUint32(env, args[0], "viewportRight", opt.viewport_right);
    if (opt.project_path.empty() || opt.program_path.empty() || opt.width == 0 || opt.height == 0 ||
        (opt.shared_present && opt.parent_hwnd == 0)) {
        napi_throw_error(env, nullptr, "start(options, callback): invalid options");
        return nullptr;
    }

    napi_value resource_name;
    napi_create_string_utf8(env, "rbc-frame", NAPI_AUTO_LENGTH, &resource_name);
    if (napi_create_threadsafe_function(env, args[1], nullptr, resource_name, 2, 1,
                                        nullptr, nullptr, nullptr, tsfnCallback,
                                        &g_tsfn) != napi_ok) {
        napi_throw_error(env, nullptr, "failed to create threadsafe function");
        return nullptr;
    }
    try {
        g_engine.start(std::move(opt), g_tsfn);
    } catch (const std::exception &e) {
        napi_throw_error(env, nullptr, e.what());
    }
    return nullptr;
}

napi_value Stop(napi_env /*env*/, napi_callback_info /*info*/) {
    g_engine.stop();
    if (g_tsfn) {
        napi_release_threadsafe_function(g_tsfn, napi_tsfn_release);
        g_tsfn = nullptr;
    }
    return nullptr;
}

napi_value CameraRotate(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    double yaw = 0.0, pitch = 0.0;
    napi_get_value_double(env, args[0], &yaw);
    napi_get_value_double(env, args[1], &pitch);
    g_engine.cameraRotate(static_cast<float>(yaw), static_cast<float>(pitch));
    return nullptr;
}

napi_value CameraZoom(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value arg;
    napi_get_cb_info(env, info, &argc, &arg, nullptr, nullptr);
    double dz = 0.0;
    napi_get_value_double(env, arg, &dz);
    g_engine.cameraZoom(static_cast<float>(dz));
    return nullptr;
}

napi_value GetLastError(napi_env env, napi_callback_info /*info*/) {
    std::string err = g_engine.lastError();
    napi_value v;
    napi_create_string_utf8(env, err.c_str(), err.size(), &v);
    return v;
}

napi_value CameraTurntable(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value arg;
    napi_get_cb_info(env, info, &argc, &arg, nullptr, nullptr);
    bool on = false;
    napi_get_value_bool(env, arg, &on);
    g_engine.setTurntable(on);
    return nullptr;
}

napi_value RequestScreenshot(napi_env env, napi_callback_info /*info*/) {
    g_engine.requestScreenshot();
    return nullptr;
}

napi_value GetStats(napi_env env, napi_callback_info /*info*/) {
    auto s = g_engine.stats();
    napi_value obj, v;
    napi_create_object(env, &obj);
    napi_create_double(env, s.fps, &v);
    napi_set_named_property(env, obj, "fps", v);
    napi_create_uint32(env, s.width, &v);
    napi_set_named_property(env, obj, "width", v);
    napi_create_uint32(env, s.height, &v);
    napi_set_named_property(env, obj, "height", v);
    napi_get_boolean(env, s.shared, &v);
    napi_set_named_property(env, obj, "shared", v);
    napi_get_boolean(env, s.turntable, &v);
    napi_set_named_property(env, obj, "turntable", v);
    return obj;
}

napi_value PreloadRuntimeDlls(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value arg;
    napi_get_cb_info(env, info, &argc, &arg, nullptr, nullptr);
    char buf[MAX_PATH] = {0};
    size_t len = 0;
    napi_get_value_string_utf8(env, arg, buf, MAX_PATH - 16, &len);
    if (len == 0) {
        napi_throw_error(env, nullptr, "preloadRuntimeDlls(programDir) expects a string");
        return nullptr;
    }
    // Load dxil.dll/dxcompiler.dll (D3D12 Agility SDK) from the engine runtime
    // directory BEFORE Chromium gets a chance to load its own copies from the
    // Electron distribution directory. Windows dedups LoadLibrary by module
    // name, so whoever loads first wins; the engine cannot work with
    // Chromium's dxil build (null vtable deref inside the host process).
    for (const char *name : {"dxil.dll", "dxcompiler.dll"}) {
        char full[MAX_PATH];
        snprintf(full, sizeof(full), "%s\\%s", buf, name);
        HMODULE m = LoadLibraryA(full);
        fprintf(stderr, "[rbc_ext_node] preload %s -> %s\n", full, m ? "ok" : "FAILED");
    }
    return nullptr;
}

napi_value Init(napi_env env, napi_value exports) {
    napi_property_descriptor descs[] = {
        {"start", nullptr, Start, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"stop", nullptr, Stop, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"cameraRotate", nullptr, CameraRotate, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"cameraZoom", nullptr, CameraZoom, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"getLastError", nullptr, GetLastError, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"cameraTurntable", nullptr, CameraTurntable, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"requestScreenshot", nullptr, RequestScreenshot, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"getStats", nullptr, GetStats, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"preloadRuntimeDlls", nullptr, PreloadRuntimeDlls, nullptr, nullptr, nullptr, napi_default, nullptr},
    };
    napi_define_properties(env, exports, sizeof(descs) / sizeof(descs[0]), descs);
    return exports;
}

}  // namespace

NAPI_MODULE_INIT(/* env, exports */) {
    AddVectoredExceptionHandler(0, crashVehHandler);
    return Init(env, exports);
}

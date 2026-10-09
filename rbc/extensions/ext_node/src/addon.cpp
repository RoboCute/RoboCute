// rbc/extensions/ext_node/src/addon.cpp
//
// N-API frontend for the RoboCute engine, feeding an Electron viewport through
// the sharedTexture API. Protocol: samples/electron/protocol.js (RVP v1).
//
// Threading
//   * One dedicated engine thread owns every engine / GPU call.
//   * JS-thread entry points only post atomics (input, resize, turntable) or
//     push release tickets into a mutex-guarded queue.
//   * Engine -> JS goes through one threadsafe function with an UNBOUNDED queue:
//     a frame lease must never be dropped silently, or its slot would leak.
//
// Frame transport (per ring slot: FREE -> GPU -> LEASED -> FREE)
//   1. after tick() the display image is blitted into the slot's LC-owned
//      staging texture and an LC timeline fence is signalled; a private copy
//      queue waits that fence and CopyResource's staging -> exported texture
//      (the exported texture is never registered with LC: its barrier tracker
//      would transition a SIMULTANEOUS_ACCESS resource out of COMMON, which
//      faults the GPU)                                          (FREE -> GPU)
//   2. once the copy fence passed the slot is leased to the host:
//      event {type:'frame', epoch, slot, frameId, handle, ...}   (GPU -> LEASED)
//   3. the host imports + displays it and calls releaseFrame() exactly once
//      after the renderer's GPU work on it completed             (LEASED -> FREE)
//   No free slot -> the frame is not published (counted as `skipped`); the
//   engine never blocks on the UI. A resize retires the ring (new epoch);
//   retired slots are destroyed when their last lease comes back. A lease that
//   is never returned stops the engine (a slot is never reused while a reader
//   may still sample it).
//
// There is no CPU readback path: capabilities() reports unsupported
// platforms/backends and start() refuses to run on them.

#include <node_api.h>

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#endif

#include "platform/shared_surface.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <generated/world.h>
#include <luisa/runtime/event.h>
#include <luisa/runtime/image.h>
#include <luisa/runtime/rhi/pixel.h>
#include <luisa/vstl/v_guid.h>
#include <rbc_graphics/render_device.h>
#include <rbc_graphics/scene_manager.h>
#include <rbc_graphics/texture_uploader.h>

namespace {

using Clock = std::chrono::steady_clock;
using luisa::compute::PixelStorage;

constexpr uint32_t kProtocolVersion = 1;
constexpr uint32_t kMinSize = 16;
constexpr uint32_t kMaxSize = 8192;

struct Options {
    std::string project_path;
    std::string backend = "dx";
    std::string program_path;
    uint32_t width = 1280;
    uint32_t height = 720;
    uint32_t host_pid = 0;          // Electron main process (handle import side)
    uint32_t slot_count = 3;        // ring depth: render / in transit / on screen
    uint32_t max_fps = 60;          // engine pacing
    uint32_t lease_timeout_ms = 3000;
};

// ---------------------------------------------------------------------------
// Engine -> JS events
// ---------------------------------------------------------------------------
enum class EventKind : uint8_t { State, Surface, Frame };

struct EngineEvent {
    EventKind kind = EventKind::State;
    std::string state;    // State: initializing | running | stopped | error
    std::string message;  // State: detail
    uint32_t epoch = 0;
    uint32_t slot = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t slot_count = 0;
    uint64_t frame_id = 0;
    uint64_t input_seq = 0;
    uint64_t host_handle = 0;
    double timestamp_us = 0.0;  // engine clock at submit
    double gpu_ms = 0.0;        // submit -> fence observed
};

// ---------------------------------------------------------------------------
// Ring of exportable textures
// ---------------------------------------------------------------------------
enum class SlotState : uint8_t { Free, Gpu, Leased };

struct Slot {
    rbcnode::ExportedTexture tex;          // cross-process texture, never seen by LC
    luisa::compute::Image<float> staging;  // LC-owned blit target, copied into `tex`
    SlotState state = SlotState::Free;
    uint64_t copy_ticket = 0;              // copier fence value: staging -> tex done
    uint64_t frame_id = 0;
    uint64_t input_seq = 0;
    Clock::time_point t_submit{};
    Clock::time_point t_leased{};
};

struct Ring {
    uint32_t epoch = 0;
    luisa::uint2 size{0u, 0u};
    std::vector<Slot> slots;
    bool anyLeased() const {
        return std::any_of(slots.begin(), slots.end(),
                           [](const Slot &s) { return s.state == SlotState::Leased; });
    }
};

struct ReleaseTicket {
    uint32_t epoch;
    uint32_t slot;
    uint64_t frame_id;
};

// ---------------------------------------------------------------------------
// Engine
// ---------------------------------------------------------------------------
class Engine {
public:
    void start(Options opt, napi_threadsafe_function tsfn) {
        opt_ = std::move(opt);
        opt_.slot_count = std::clamp(opt_.slot_count, 2u, 8u);
        opt_.max_fps = std::clamp(opt_.max_fps, 1u, 1000u);
        tsfn_ = tsfn;
        auto caps = rbcnode::surfaceCaps(opt_.backend);
        if (!caps.supported) throw std::runtime_error("unsupported: " + caps.reason);
        std::string err;
        if (!rbcnode::surfaceAttachHost(opt_.host_pid, err))
            throw std::runtime_error("cannot attach host process: " + err);
        t0_ = Clock::now();
        // a resize posted before start() (viewport known early) wins
        if (!pending_size_.load()) requestResize(opt_.width, opt_.height);
        running_ = true;
        thread_ = std::thread([this] { run(); });
    }

    void stop() {
        running_ = false;
        if (thread_.joinable()) thread_.join();
        rbcnode::surfaceDetachHost();
    }

    // JS thread ---------------------------------------------------------------
    void requestResize(uint32_t w, uint32_t h) {
        w = std::clamp(w, kMinSize, kMaxSize);
        h = std::clamp(h, kMinSize, kMaxSize);
        pending_size_.store((uint64_t(w) << 32) | h, std::memory_order_release);
    }

    // Deltas first, then the sequence number (release): the engine reads the
    // seq (acquire) before draining deltas, so a frame stamped inputSeq=N
    // contains at least every delta of packets <= N.
    void input(uint64_t seq, float yaw, float pitch, float dolly) {
        if (yaw != 0.0f) addAtomic(yaw_accum_, yaw);
        if (pitch != 0.0f) addAtomic(pitch_accum_, pitch);
        if (dolly != 0.0f) addAtomic(dolly_accum_, dolly);
        uint64_t prev = input_seq_.load(std::memory_order_relaxed);
        while (seq > prev && !input_seq_.compare_exchange_weak(prev, seq, std::memory_order_release)) {}
    }

    void release(const ReleaseTicket &t) {
        std::lock_guard<std::mutex> lk(release_mtx_);
        releases_.push_back(t);
    }

    void setTurntable(bool on) { turntable_.store(on); }

    struct Stats {
        double fps, publish_fps;
        uint32_t width, height, epoch, slot_count, slots_free;
        uint64_t frames, published, skipped, lease_expired, input_seq;
        bool turntable, running;
    };
    Stats stats() const {
        Stats s{};
        s.fps = fps_.load();
        s.publish_fps = publish_fps_.load();
        s.width = cur_w_.load();
        s.height = cur_h_.load();
        s.epoch = cur_epoch_.load();
        s.slot_count = opt_.slot_count;
        s.slots_free = slots_free_.load();
        s.frames = frames_.load();
        s.published = published_.load();
        s.skipped = skipped_.load();
        s.lease_expired = lease_expired_.load();
        s.input_seq = applied_seq_.load();
        s.turntable = turntable_.load();
        s.running = running_.load();
        return s;
    }

private:
    static void addAtomic(std::atomic<float> &a, float v) {
        float cur = a.load(std::memory_order_relaxed);
        while (!a.compare_exchange_weak(cur, cur + v, std::memory_order_relaxed)) {}
    }

    double sinceStartUs(Clock::time_point t) const {
        return std::chrono::duration<double, std::micro>(t - t0_).count();
    }

    // -- events ---------------------------------------------------------------
    bool emit(EngineEvent *ev) {
        if (napi_call_threadsafe_function(tsfn_, ev, napi_tsfn_nonblocking) != napi_ok) {
            delete ev;
            return false;
        }
        return true;
    }
    void emitState(const char *state, const std::string &message) {
        fprintf(stderr, "[rbc_ext_node] state=%s %s\n", state, message.c_str());
        fflush(stderr);
        auto *ev = new EngineEvent{};
        ev->kind = EventKind::State;
        ev->state = state;
        ev->message = message;
        emit(ev);
    }

    // -- scene setup (mirrors samples/app_graphics_scene.py) ---------------------
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

    void addCubeToMesh(float *P, float *UV, uint32_t *I, float ox, float oy, float oz, float s,
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
            {0, 1, 2}, {1, 3, 2}, {4, 5, 6}, {5, 7, 6}, {0, 1, 4}, {1, 5, 4},
            {2, 3, 6}, {3, 7, 6}, {0, 2, 4}, {2, 6, 4}, {1, 3, 5}, {3, 7, 5},
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
        auto *P = reinterpret_cast<float *>(pos_span.data());
        auto *UV = reinterpret_cast<float *>(uv_span.data());
        auto *I = reinterpret_cast<uint32_t *>(idx_span.data());
        addCubeToMesh(P, UV, I, 0.0f, 0.0f, 0.0f, 1.0f, 0, 0);
        addCubeToMesh(P, UV, I, 0.0f, 1.0f, 0.0f, 0.4f, 8, 12);
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

    void initEngine(luisa::uint2 size) {
        auto t0 = Clock::now();
        auto stamp = [t0](const char *what) {
            fprintf(stderr, "[rbc_ext_node] timing: %s @%.0fms\n", what,
                    std::chrono::duration<double, std::milli>(Clock::now() - t0).count());
        };
        ctx_ = rbc::RBCContext::_create_();
        if (!ctx_) throw std::runtime_error("RBCContext creation failed");
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

        // Headless display: no window, no swapchain — the render target is
        // blitted into exported textures instead (see submitFrame).
        rbc::RBCContext::init_display(ctx_, "electron_texture", size, false, false, false, false);
        cam_ = rbc::RBCContext::create_display_cam(ctx_);
        if (!cam_) throw std::runtime_error("create_display_cam failed");
        rbc::CameraComponent::enable_camera(cam_);
        rbc::RBCContext::enable_camera_control(ctx_);
        void *settings = rbc::CameraComponent::render_settings(cam_);
        if (settings) rbc::RenderSettings::set_use_auto_exposure(settings, false);
        void *cam_entity = rbc::Component::entity(cam_);
        void *cam_trans = rbc::Entity::get_component(cam_entity, "TransformComponent");
        if (cam_trans) rbc::TransformComponent::set_pos(cam_trans, luisa::double3{0.0, 0.0, -1.0}, false);

        tex_ = rbc::Project::import_texture(project_, "test_grid.png", 1, false);
        if (!tex_) throw std::runtime_error("failed to import test_grid.png");
        makeCubeEntity();

        auto &device = rbc::RenderDevice::instance().lc_device();
        native_device_ = device.native_handle();
        fence_event_ = device.create_timeline_event();
        std::string err;
        if (!rbcnode::surfaceCopierCreate(native_device_, copier_, err))
            throw std::runtime_error("texture copier: " + err);
        stamp("engine ready");
    }

    // -- ring management (engine thread) ------------------------------------------
    void createRing() {
        display_ = rbc::RBCContext::display_image(ctx_);
        if (display_.handle == ~0ull || !display_.width || !display_.height)
            throw std::runtime_error("display image unavailable");
        display_storage_ = luisa::compute::pixel_format_to_storage(display_.format);

        auto &device = rbc::RenderDevice::instance().lc_device();
        auto ring = std::make_unique<Ring>();
        ring->epoch = ++epoch_;
        ring->size = luisa::uint2{display_.width, display_.height};
        ring->slots.resize(opt_.slot_count);
        for (auto &slot : ring->slots) {
            std::string err;
            if (!rbcnode::surfaceCreate(native_device_, ring->size.x, ring->size.y, slot.tex, err))
                throw std::runtime_error("texture export failed: " + err);
            // same format/size as the exported texture (CopyResource requirement)
            slot.staging = device.create_image<float>(PixelStorage::BYTE4, ring->size, 1u);
        }
        cur_w_ = ring->size.x;
        cur_h_ = ring->size.y;
        cur_epoch_ = ring->epoch;

        auto *ev = new EngineEvent{};
        ev->kind = EventKind::Surface;
        ev->epoch = ring->epoch;
        ev->width = ring->size.x;
        ev->height = ring->size.y;
        ev->slot_count = opt_.slot_count;
        emit(ev);
        fprintf(stderr, "[rbc_ext_node] surface epoch=%u %ux%u slots=%u\n",
                ring->epoch, ring->size.x, ring->size.y, opt_.slot_count);
        rings_.push_back(std::move(ring));
    }

    void destroyRing(Ring &ring) {
        for (auto &slot : ring.slots) {
            slot.staging = {};
            rbcnode::surfaceDestroy(slot.tex);
        }
        ring.slots.clear();
    }

    // Current ring stays; retired rings go once no lease is outstanding.
    void collectRings() {
        for (size_t i = 0; i + 1 < rings_.size();) {
            if (!rings_[i]->anyLeased()) {
                destroyRing(*rings_[i]);
                rings_.erase(rings_.begin() + i);
            } else {
                ++i;
            }
        }
    }

    bool applyResize() {
        uint64_t packed = pending_size_.exchange(0, std::memory_order_acquire);
        if (!packed) return false;
        luisa::uint2 size{uint32_t(packed >> 32), uint32_t(packed & 0xffffffffu)};
        if (!rings_.empty() && all(rings_.back()->size == size)) return false;
        if (!rings_.empty()) {
            rbc::RBCContext::reset_view(ctx_, size);
            // in-flight copies of the old ring are simply not published
            fence_event_.synchronize(fence_value_);
            rbcnode::surfaceCopierWaitIdle(copier_);
            for (auto &slot : rings_.back()->slots)
                if (slot.state == SlotState::Gpu) slot.state = SlotState::Free;
        }
        createRing();
        collectRings();
        return true;
    }

    void drainReleases() {
        std::vector<ReleaseTicket> tickets;
        {
            std::lock_guard<std::mutex> lk(release_mtx_);
            tickets.swap(releases_);
        }
        for (const auto &t : tickets) {
            for (auto &ring : rings_) {
                if (ring->epoch != t.epoch || t.slot >= ring->slots.size()) continue;
                Slot &s = ring->slots[t.slot];
                // idempotent: stale or duplicate tickets are ignored
                if (s.state == SlotState::Leased && s.frame_id == t.frame_id) s.state = SlotState::Free;
            }
        }
        if (!tickets.empty()) collectRings();
    }

    void reclaimExpiredLeases(Clock::time_point now) {
        auto timeout = std::chrono::milliseconds(opt_.lease_timeout_ms);
        for (auto &ring : rings_) {
            for (auto &s : ring->slots) {
                if (s.state == SlotState::Leased && now - s.t_leased > timeout) {
                    lease_expired_++;
                    throw std::runtime_error("frame lease timed out; shared texture connection stopped");
                }
            }
        }
    }

    void submitFrame(uint64_t input_seq) {
        Ring &ring = *rings_.back();
        Slot *slot = nullptr;
        for (auto &s : ring.slots)
            if (s.state == SlotState::Free) { slot = &s; break; }
        if (!slot) {
            skipped_++;
            return;
        }
        auto &rd = rbc::RenderDevice::instance();
        auto &cmdlist = rd.lc_main_cmd_list();
        luisa::compute::ImageView<float> src{display_.native_handle, display_.handle, display_storage_, 0u, ring.size};
        rbc::SceneManager::instance().tex_uploader().blit(
            cmdlist, src, slot->staging.view(0), luisa::float2(1.0f), luisa::float2(0.0f),
            luisa::uint2(0u), ring.size);
        rd.execute_before_cmdlist_commit_task();
        rd.lc_main_stream() << cmdlist.commit();
        rd.execute_after_cmdlist_commit_task();
        rd.lc_main_stream() << fence_event_.signal(++fence_value_);

        // staging -> exported on the private copy queue, after the LC fence
        std::string err;
        uint64_t ticket = rbcnode::surfaceCopySubmit(copier_, fence_event_.native_handle(), fence_value_,
                                                     slot->staging.native_handle(), slot->tex, err);
        if (!ticket) throw std::runtime_error("texture copy failed: " + err);

        slot->state = SlotState::Gpu;
        slot->copy_ticket = ticket;
        slot->frame_id = frames_.load();
        slot->input_seq = input_seq;
        slot->t_submit = Clock::now();
    }

    // Lease every slot whose copy into the exported texture finished, oldest first.
    void publishCompleted() {
        Ring &ring = *rings_.back();
        std::vector<uint32_t> ready;
        for (uint32_t i = 0; i < ring.slots.size(); ++i) {
            const Slot &s = ring.slots[i];
            if (s.state == SlotState::Gpu && rbcnode::surfaceCopyCompleted(copier_, s.copy_ticket)) ready.push_back(i);
        }
        std::sort(ready.begin(), ready.end(),
                  [&](uint32_t a, uint32_t b) { return ring.slots[a].copy_ticket < ring.slots[b].copy_ticket; });
        auto now = Clock::now();
        for (uint32_t i : ready) {
            Slot &s = ring.slots[i];
            auto *ev = new EngineEvent{};
            ev->kind = EventKind::Frame;
            ev->epoch = ring.epoch;
            ev->slot = i;
            ev->width = ring.size.x;
            ev->height = ring.size.y;
            ev->frame_id = s.frame_id;
            ev->input_seq = s.input_seq;
            ev->host_handle = s.tex.host_handle;
            ev->timestamp_us = sinceStartUs(s.t_submit);
            ev->gpu_ms = std::chrono::duration<double, std::milli>(now - s.t_submit).count();
            s.t_leased = now;
            if (emit(ev)) {
                s.state = SlotState::Leased;
                published_++;
                publish_tally_++;
            } else {
                s.state = SlotState::Free;
            }
        }
        uint32_t free_count = 0;
        for (const auto &s : ring.slots) free_count += s.state == SlotState::Free;
        slots_free_ = free_count;
    }

    void tallyFps() {
        fps_tally_++;
        auto now = Clock::now();
        double secs = std::chrono::duration<double>(now - fps_last_).count();
        if (secs >= 1.0) {
            fps_.store(fps_tally_ / secs);
            publish_fps_.store(publish_tally_ / secs);
            fps_tally_ = 0;
            publish_tally_ = 0;
            fps_last_ = now;
        }
    }

    void run() {
        try {
            emitState("initializing", "");
            uint64_t packed = pending_size_.exchange(0);
            initEngine(luisa::uint2{uint32_t(packed >> 32), uint32_t(packed & 0xffffffffu)});
            createRing();
        } catch (const std::exception &e) {
            emitState("error", std::string("init failed: ") + e.what());
            running_ = false;
            return;
        }
        emitState("running", "");

        const auto budget = std::chrono::duration<double>(1.0 / opt_.max_fps);
        auto last = Clock::now();
        float angle = 0.0f;
        uint64_t pt_frame_index = 0;
        fps_last_ = Clock::now();
        std::string fatal;
        while (running_) {
            try {
                auto frame_begin = Clock::now();
                float dt = std::chrono::duration<float>(frame_begin - last).count();
                last = frame_begin;

                if (applyResize()) pt_frame_index = 0;
                drainReleases();
                reclaimExpiredLeases(frame_begin);

                uint64_t seq = input_seq_.load(std::memory_order_acquire);
                float yaw = yaw_accum_.exchange(0.0f);
                float pitch = pitch_accum_.exchange(0.0f);
                float dz = dolly_accum_.exchange(0.0f);
                if (yaw != 0.0f || pitch != 0.0f)
                    rbc::RBCContext::control_camera_add_rotate(ctx_, yaw, pitch, 0.0f);
                if (dz != 0.0f) rbc::RBCContext::control_camera_add_pos(ctx_, luisa::float3{0.0f, 0.0f, dz});
                applied_seq_ = seq;

                if (turntable_) {
                    angle += dt * 0.4f;
                    float ha = angle * 0.5f;
                    rbc::TransformComponent::set_rotation(
                        cube_trans_, luisa::float4{0.0f, std::sin(ha), 0.0f, std::cos(ha)}, false);
                }

                rbc::CameraComponent::set_frame_index(cam_, pt_frame_index);
                bool reset = rbc::RBCContext::tick(ctx_, dt, rbc::TickStage::PathTracingPreview, false);
                pt_frame_index = reset ? 0 : pt_frame_index + 1;
                frames_++;

                submitFrame(seq);
                publishCompleted();
                auto elapsed = Clock::now() - frame_begin;
                if (elapsed < budget)
                    std::this_thread::sleep_for(budget - elapsed);
                publishCompleted();
                tallyFps();
            } catch (const std::exception &e) {
                fatal = std::string("render loop error: ") + e.what();
                break;
            }
        }
        try {
            if (fence_value_) fence_event_.synchronize(fence_value_);
            rbcnode::surfaceCopierWaitIdle(copier_);
            for (auto &ring : rings_) destroyRing(*ring);
            rings_.clear();
            rbcnode::surfaceCopierDestroy(copier_);
        } catch (...) {}
        running_ = false;
        if (!fatal.empty()) emitState("error", fatal);
        else emitState("stopped", "");
    }

    Options opt_;
    napi_threadsafe_function tsfn_ = nullptr;
    std::thread thread_;
    std::atomic<bool> running_{false};
    Clock::time_point t0_{Clock::now()};

    // JS -> engine
    std::atomic<uint64_t> pending_size_{0};
    std::atomic<uint64_t> input_seq_{0};
    std::atomic<float> yaw_accum_{0.0f}, pitch_accum_{0.0f}, dolly_accum_{0.0f};
    std::atomic<bool> turntable_{true};
    std::mutex release_mtx_;
    std::vector<ReleaseTicket> releases_;

    // stats
    std::atomic<double> fps_{0.0}, publish_fps_{0.0};
    std::atomic<uint32_t> cur_w_{0}, cur_h_{0}, cur_epoch_{0}, slots_free_{0};
    std::atomic<uint64_t> frames_{0}, published_{0}, skipped_{0}, lease_expired_{0}, applied_seq_{0};
    uint32_t fps_tally_ = 0, publish_tally_ = 0;
    Clock::time_point fps_last_{Clock::now()};

    // engine thread only
    std::vector<std::unique_ptr<Ring>> rings_;  // back() = current, others retired
    uint32_t epoch_ = 0;
    luisa::compute::TextureCreationInfo display_{};
    PixelStorage display_storage_ = PixelStorage::FLOAT4;
    rbcnode::SurfaceCopier *copier_ = nullptr;
    void *native_device_ = nullptr;
    luisa::compute::TimelineEvent fence_event_;
    uint64_t fence_value_ = 0;

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

Engine g_engine;
napi_threadsafe_function g_tsfn = nullptr;
bool g_started = false;

#ifdef _WIN32
// Electron's crashpad swallows native crashes silently: write a minidump.
LONG WINAPI crashVehHandler(EXCEPTION_POINTERS *ep) {
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (code == EXCEPTION_BREAKPOINT || code == EXCEPTION_SINGLE_STEP ||
        code == DBG_PRINTEXCEPTION_C || code == DBG_PRINTEXCEPTION_WIDE_C ||
        code == 0xE06D7363u /* MSVC C++ throw: caught normally, not a crash */) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    wchar_t path[MAX_PATH];
    GetTempPathW(MAX_PATH, path);
    wcscat_s(path, L"rbc_ext_node_crash.dmp");
    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei{};
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = ep;
        mei.ClientPointers = FALSE;
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), f, MiniDumpWithFullMemory, &mei, nullptr, nullptr);
        CloseHandle(f);
        fprintf(stderr, "[rbc_ext_node] crash dump written to %%TEMP%%\\rbc_ext_node_crash.dmp code=%08lx addr=%p\n",
                code, ep->ExceptionRecord->ExceptionAddress);
        fflush(stderr);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

// ---------------------------------------------------------------------------
// N-API glue
// ---------------------------------------------------------------------------
void setStr(napi_env env, napi_value obj, const char *k, const std::string &v) {
    napi_value x;
    napi_create_string_utf8(env, v.c_str(), v.size(), &x);
    napi_set_named_property(env, obj, k, x);
}
void setNum(napi_env env, napi_value obj, const char *k, double v) {
    napi_value x;
    napi_create_double(env, v, &x);
    napi_set_named_property(env, obj, k, x);
}
void setBool(napi_env env, napi_value obj, const char *k, bool v) {
    napi_value x;
    napi_get_boolean(env, v, &x);
    napi_set_named_property(env, obj, k, x);
}

// onEvent(evt): evt.type = 'state' | 'surface' | 'frame' (see protocol.js)
void tsfnCallback(napi_env env, napi_value js_cb, void *, void *data) {
    std::unique_ptr<EngineEvent> ev(static_cast<EngineEvent *>(data));
    if (!env || !js_cb) return;
    napi_value obj;
    napi_create_object(env, &obj);
    switch (ev->kind) {
        case EventKind::State:
            setStr(env, obj, "type", "state");
            setStr(env, obj, "state", ev->state);
            setStr(env, obj, "message", ev->message);
            break;
        case EventKind::Surface:
            setStr(env, obj, "type", "surface");
            setNum(env, obj, "epoch", ev->epoch);
            setNum(env, obj, "width", ev->width);
            setNum(env, obj, "height", ev->height);
            setNum(env, obj, "slots", ev->slot_count);
            break;
        case EventKind::Frame:
            setStr(env, obj, "type", "frame");
            setNum(env, obj, "epoch", ev->epoch);
            setNum(env, obj, "slot", ev->slot);
            setNum(env, obj, "frameId", double(ev->frame_id));
            setNum(env, obj, "width", ev->width);
            setNum(env, obj, "height", ev->height);
            setNum(env, obj, "inputSeq", double(ev->input_seq));
            // pointer-sized handle: decimal string (exceeds Number safe range)
            setStr(env, obj, "handle", std::to_string(ev->host_handle));
            setNum(env, obj, "timestampUs", ev->timestamp_us);
            setNum(env, obj, "gpuMs", ev->gpu_ms);
            break;
    }
    napi_value undefined;
    napi_get_undefined(env, &undefined);
    napi_call_function(env, undefined, js_cb, 1, &obj, nullptr);
}

bool getStr(napi_env env, napi_value obj, const char *name, std::string &out) {
    napi_value v;
    napi_valuetype t;
    if (napi_get_named_property(env, obj, name, &v) != napi_ok) return false;
    if (napi_typeof(env, v, &t) != napi_ok || t != napi_string) return false;
    size_t len = 0;
    napi_get_value_string_utf8(env, v, nullptr, 0, &len);
    out.resize(len);
    napi_get_value_string_utf8(env, v, out.data(), len + 1, &len);
    return true;
}

bool getU32(napi_env env, napi_value obj, const char *name, uint32_t &out) {
    napi_value v;
    napi_valuetype t;
    if (napi_get_named_property(env, obj, name, &v) != napi_ok) return false;
    if (napi_typeof(env, v, &t) != napi_ok || t != napi_number) return false;
    return napi_get_value_uint32(env, v, &out) == napi_ok;
}

double argNum(napi_env env, napi_value v) {
    double d = 0.0;
    napi_get_value_double(env, v, &d);
    return d;
}

template<size_t N>
size_t getArgs(napi_env env, napi_callback_info info, napi_value (&args)[N]) {
    size_t argc = N;
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    return argc;
}

// capabilities(backend) -> {supported, reason, platform, handleType, pixelFormat, protocol}
napi_value Capabilities(napi_env env, napi_callback_info info) {
    napi_value args[1];
    std::string backend = "dx";
    if (getArgs(env, info, args) >= 1) {
        size_t len = 0;
        if (napi_get_value_string_utf8(env, args[0], nullptr, 0, &len) == napi_ok) {
            backend.resize(len);
            napi_get_value_string_utf8(env, args[0], backend.data(), len + 1, &len);
        }
    }
    auto caps = rbcnode::surfaceCaps(backend);
    napi_value obj;
    napi_create_object(env, &obj);
    setBool(env, obj, "supported", caps.supported);
    setStr(env, obj, "reason", caps.reason);
    setStr(env, obj, "platform", caps.platform);
    setStr(env, obj, "handleType", caps.handle_type);
    setStr(env, obj, "pixelFormat", caps.pixel_format);
    setNum(env, obj, "protocol", kProtocolVersion);
    return obj;
}

// start(options, onEvent)
napi_value Start(napi_env env, napi_callback_info info) {
    napi_value args[2];
    if (getArgs(env, info, args) < 2) {
        napi_throw_error(env, nullptr, "start(options, onEvent) expects 2 arguments");
        return nullptr;
    }
    if (g_started) {
        napi_throw_error(env, nullptr, "engine already started (one engine per process)");
        return nullptr;
    }
    Options opt;
    getStr(env, args[0], "projectPath", opt.project_path);
    getStr(env, args[0], "backend", opt.backend);
    getStr(env, args[0], "programPath", opt.program_path);
    getU32(env, args[0], "width", opt.width);
    getU32(env, args[0], "height", opt.height);
    getU32(env, args[0], "hostPid", opt.host_pid);
    getU32(env, args[0], "slots", opt.slot_count);
    getU32(env, args[0], "maxFps", opt.max_fps);
    getU32(env, args[0], "leaseTimeoutMs", opt.lease_timeout_ms);
    if (opt.project_path.empty() || opt.program_path.empty() || opt.host_pid == 0) {
        napi_throw_error(env, nullptr, "start(): projectPath, programPath and hostPid are required");
        return nullptr;
    }

    napi_value resource_name;
    napi_create_string_utf8(env, "rbc-engine-events", NAPI_AUTO_LENGTH, &resource_name);
    // max_queue_size = 0 (unbounded): frame leases must never be dropped
    if (napi_create_threadsafe_function(env, args[1], nullptr, resource_name, 0, 1, nullptr, nullptr,
                                        nullptr, tsfnCallback, &g_tsfn) != napi_ok) {
        napi_throw_error(env, nullptr, "failed to create threadsafe function");
        return nullptr;
    }
    try {
        g_engine.start(std::move(opt), g_tsfn);
        g_started = true;
    } catch (const std::exception &e) {
        napi_release_threadsafe_function(g_tsfn, napi_tsfn_abort);
        g_tsfn = nullptr;
        napi_throw_error(env, nullptr, e.what());
    }
    return nullptr;
}

napi_value Stop(napi_env, napi_callback_info) {
    g_engine.stop();
    if (g_tsfn) {
        napi_release_threadsafe_function(g_tsfn, napi_tsfn_release);
        g_tsfn = nullptr;
    }
    return nullptr;
}

// resize(width, height) — device pixels of the viewport element
napi_value Resize(napi_env env, napi_callback_info info) {
    napi_value args[2];
    if (getArgs(env, info, args) < 2) return nullptr;
    g_engine.requestResize(uint32_t(argNum(env, args[0])), uint32_t(argNum(env, args[1])));
    return nullptr;
}

// input(seq, yaw, pitch, dolly) — one coalesced input packet
napi_value Input(napi_env env, napi_callback_info info) {
    napi_value args[4];
    if (getArgs(env, info, args) < 4) return nullptr;
    g_engine.input(uint64_t(argNum(env, args[0])), float(argNum(env, args[1])),
                   float(argNum(env, args[2])), float(argNum(env, args[3])));
    return nullptr;
}

// releaseFrame(epoch, slot, frameId) — returns a frame lease
napi_value ReleaseFrame(napi_env env, napi_callback_info info) {
    napi_value args[3];
    if (getArgs(env, info, args) < 3) return nullptr;
    g_engine.release({uint32_t(argNum(env, args[0])), uint32_t(argNum(env, args[1])),
                      uint64_t(argNum(env, args[2]))});
    return nullptr;
}

napi_value SetTurntable(napi_env env, napi_callback_info info) {
    napi_value args[1];
    if (getArgs(env, info, args) < 1) return nullptr;
    bool on = false;
    napi_get_value_bool(env, args[0], &on);
    g_engine.setTurntable(on);
    return nullptr;
}

napi_value GetStats(napi_env env, napi_callback_info) {
    auto s = g_engine.stats();
    napi_value obj;
    napi_create_object(env, &obj);
    setNum(env, obj, "fps", s.fps);
    setNum(env, obj, "publishFps", s.publish_fps);
    setNum(env, obj, "width", s.width);
    setNum(env, obj, "height", s.height);
    setNum(env, obj, "epoch", s.epoch);
    setNum(env, obj, "slots", s.slot_count);
    setNum(env, obj, "slotsFree", s.slots_free);
    setNum(env, obj, "frames", double(s.frames));
    setNum(env, obj, "published", double(s.published));
    setNum(env, obj, "skipped", double(s.skipped));
    setNum(env, obj, "leaseExpired", double(s.lease_expired));
    setNum(env, obj, "inputSeq", double(s.input_seq));
    setBool(env, obj, "turntable", s.turntable);
    setBool(env, obj, "running", s.running);
    return obj;
}

napi_value Init(napi_env env, napi_value exports) {
    napi_property_descriptor descs[] = {
        {"capabilities", nullptr, Capabilities, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"start", nullptr, Start, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"stop", nullptr, Stop, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"resize", nullptr, Resize, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"input", nullptr, Input, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"releaseFrame", nullptr, ReleaseFrame, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"setTurntable", nullptr, SetTurntable, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"getStats", nullptr, GetStats, nullptr, nullptr, nullptr, napi_default, nullptr},
    };
    napi_define_properties(env, exports, sizeof(descs) / sizeof(descs[0]), descs);
    return exports;
}

}// namespace

NAPI_MODULE_INIT(/* env, exports */) {
#ifdef _WIN32
    AddVectoredExceptionHandler(0, crashVehHandler);
#endif
    return Init(env, exports);
}

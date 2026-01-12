#pragma once
#include "RBCEditorRuntime/config.h"
#include "RBCEditorRuntime/engine/app.h"
#include <rbc_graphics/graphics_utils.h>
#include <rbc_app/camera_controller.h>
#include <luisa/gui/window.h>

namespace rbc {

/**
 * 渲染模式枚举
 * - Editor: 编辑器预览模式（光栅化快速预览，支持选择/框选等交互）
 * - Realistic: 真实感渲染模式（路径追踪，PBR渲染）
 */
enum class RenderMode {
    Editor,  // 编辑器模式 (VisApp)
    Realistic// 真实感渲染模式 (PBRApp)
};

/**
 * AppBase - 渲染应用基类
 * 
 * 提取了 VisApp 和 PBRApp 的共同成员：
 * - GraphicsUtils: 图形工具
 * - resolution: 分辨率
 * - dx_adaptor_luid: DX适配器LUID
 * - frame_index: 帧索引
 * - last_frame_time: 上一帧时间
 * - clk: 时钟
 * - reset: 重置标志
 * - camera_input / cam_controller: 相机控制
 */
struct RBC_EDITOR_RUNTIME_API AppBase : public IApp {
    luisa::uint2 resolution;
    luisa::uint2 dx_adaptor_luid;

    luisa::fiber::scheduler scheduler;
    GraphicsUtils utils;
    uint64_t frame_index = 0;
    double last_frame_time = 0;
    luisa::Clock clk;
    bool reset = false;

    // 相机控制（共享）
    CameraController::Input camera_input;
    CameraController cam_controller;
    RenderPlugin::PipeCtxStub* pipe_ctx{};

public:
    [[nodiscard]] unsigned int GetDXAdapterLUIDHigh() const override { return dx_adaptor_luid.x; }
    [[nodiscard]] unsigned int GetDXAdapterLUIDLow() const override { return dx_adaptor_luid.y; }
    [[nodiscard]] void *GetStreamNativeHandle() const override;
    [[nodiscard]] void *GetDeviceNativeHandle() const override;
    [[nodiscard]] virtual RenderMode getRenderMode() const = 0;

    /**
     * 通用初始化（设备、图形、渲染）
     */
    void init(const char *program_path, const char *backend_name) override;

    /**
     * 创建/调整纹理大小
     */
    uint64_t create_texture(uint width, uint height) override;

    /**
     * 处理重置帧逻辑
     */
    void handle_reset();

    /**
     * 准备DX状态
     */
    void prepare_dx_states();

    /**
     * 更新相机（子类可覆盖）
     */
    virtual void update_camera(float delta_time);

    /**
     * 销毁资源
     */
    virtual void dispose();

    ~AppBase() override;

protected:
    /**
     * 子类可以覆盖的初始化钩子
     */
    virtual void on_init() {}
};

}// namespace rbc

#pragma once
#include "RBCEditorRuntime/engine/app_base.h"
#include "RBCEditorRuntime/engine/ViewportInteractionManager.h"

namespace rbc {

/**
 * VisApp - 编辑器可视化应用
 * 
 * 用于编辑器预览模式：
 * - 光栅化快速预览
 * - 支持选择、框选、拖动等交互
 * - 物体轮廓高亮显示
 */
struct VisApp : public AppBase {
    bool dst_image_reseted = false;

    // 交互管理器：处理选择、拖动、框选逻辑
    ViewportInteractionManager interaction_manager;

    // 当前选择的对象ID列表（从interaction_manager同步）
    luisa::vector<uint> dragged_object_ids;

public:
    [[nodiscard]] RenderMode getRenderMode() const override { return RenderMode::Editor; }

    void update() override;
    void handle_key(luisa::compute::Key key, luisa::compute::Action action) override;
    void handle_mouse(luisa::compute::MouseButton button, luisa::compute::Action action, luisa::float2 xy) override;
    void handle_cursor_position(luisa::float2 xy) override;

    /**
     * 获取当前选中的对象ID列表
     */
    [[nodiscard]] const luisa::vector<uint> &getSelectedObjectIds() const { return dragged_object_ids; }

    ~VisApp() override;

protected:
    /**
     * 更新相机（考虑交互模式）
     */
    void update_camera(float delta_time) override;
};

}// namespace rbc

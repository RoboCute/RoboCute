#pragma once

#include <luisa/luisa-compute.h>
#include <luisa/dsl/sugar.h>
#include <luisa/gui/input.h>
#include <luisa/vstl/common.h>
#include <utility>

namespace rbc {

/**
 * ViewportInteractionManager - 视口交互管理器
 * 
 * 职责：
 * - 管理视口交互状态（选择、拖动、框选）
 * - 处理键盘修饰键（Ctrl等）
 * - 实现交互优先级：拖动 > 框选 > 点击选择
 * - 检测重复选择并保持选择状态
 * 
 * 设计原则：
 * - 将交互逻辑从update循环中分离
 * - 使用状态机模式管理交互状态
 * - 遵循通用DCC软件的交互模式
 */
class ViewportInteractionManager {
public:
    enum class InteractionMode {
        None,           // 无交互
        ClickSelect,    // 点击选择
        DragSelect,     // 框选
        Dragging        // 拖动已选物体
    };

    enum class MouseState {
        Idle,           // 空闲
        Pressed,        // 按下
        Dragging,       // 拖动中
        Released,       // 释放（等待查询结果）
        WaitingResult   // 等待查询结果（已添加请求，等待下一帧查询）
    };

    struct InteractionState {
        InteractionMode mode = InteractionMode::None;
        MouseState mouse_state = MouseState::Idle;
        
        // 鼠标位置（归一化UV坐标）
        luisa::float2 start_uv{0.f, 0.f};
        luisa::float2 end_uv{0.f, 0.f};
        
        // 当前选择的对象ID列表
        luisa::vector<uint> selected_object_ids;
        
        // 是否按下Ctrl键（用于框选模式）
        bool is_ctrl_down = false;
        
        // 拖动阈值（用于区分点击和拖动）
        static constexpr float drag_threshold = 1e-2f;
    };

public:
    ViewportInteractionManager() = default;
    ~ViewportInteractionManager() = default;

    /**
     * 处理键盘事件
     */
    void handle_key(luisa::compute::Key key, luisa::compute::Action action);

    /**
     * 处理鼠标事件
     * @return true 如果事件已被处理，false 如果应该传递给相机控制
     */
    bool handle_mouse(luisa::compute::MouseButton button, luisa::compute::Action action, luisa::float2 xy, luisa::uint2 resolution);

    /**
     * 处理鼠标移动事件
     */
    void handle_cursor_position(luisa::float2 xy, luisa::uint2 resolution);

    /**
     * 更新交互状态（每帧调用）
     * @param click_manager 点击管理器引用，用于查询选择结果
     */
    void update(class ClickManager &click_manager);

    /**
     * 获取当前选择的对象ID列表
     */
    const luisa::vector<uint> &get_selected_object_ids() const { return state_.selected_object_ids; }

    /**
     * 获取当前交互模式
     */
    InteractionMode get_interaction_mode() const { return state_.mode; }

    /**
     * 获取框选区域（归一化UV坐标）
     * @return pair<min_uv, max_uv>
     */
    std::pair<luisa::float2, luisa::float2> get_selection_region() const {
        return {luisa::min(state_.start_uv, state_.end_uv), luisa::max(state_.start_uv, state_.end_uv)};
    }

    /**
     * 是否正在进行框选
     */
    bool is_drag_selecting() const {
        return state_.mode == InteractionMode::DragSelect && state_.mouse_state == MouseState::Dragging;
    }

    /**
     * 是否正在进行点击选择（需要添加点击请求）
     */
    bool is_click_selecting() const {
        return state_.mode == InteractionMode::ClickSelect && 
               (state_.mouse_state == MouseState::Pressed || state_.mouse_state == MouseState::WaitingResult);
    }

    /**
     * 重置交互状态
     */
    void reset();

private:
    /**
     * 确定交互模式
     */
    InteractionMode determine_interaction_mode(bool has_selection, bool is_ctrl_down) const;

    /**
     * 检测重复选择
     * @param new_selection 新选择的对象ID列表
     * @return true 如果新选择与当前选择有重叠
     */
    bool is_repeat_selection(const luisa::vector<uint> &new_selection) const;

    /**
     * 更新选择状态
     */
    void update_selection(const luisa::vector<uint> &new_selection, bool is_repeat);

    InteractionState state_;
};

}// namespace rbc


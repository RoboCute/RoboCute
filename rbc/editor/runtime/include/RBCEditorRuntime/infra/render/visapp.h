#pragma once

#include "RBCEditorRuntime/infra/render/app_base.h"
#include "RBCEditorRuntime/infra/render/ViewportInteractionManager.h"

namespace rbc {

/**
 * VisApp - Editor visualization application
 * 
 * Used for editor preview mode:
 * - Rasterized quick preview
 * - Supports selection, drag-select, drag, etc.
 * - Object outline highlighting
 */
struct VisApp : public RenderAppBase {
    bool dst_image_reset = false;

    // Interaction manager: handles selection, drag, and box-select logic
    ViewportInteractionManager interaction_manager;

    // Currently selected object ID list (synced from interaction_manager)
    luisa::vector<uint> dragged_object_ids;

public:
    [[nodiscard]] RenderMode getRenderMode() const override { return RenderMode::Editor; }

    void update() override;
    void handle_key(luisa::compute::Key key, luisa::compute::Action action) override;
    void handle_mouse(luisa::compute::MouseButton button, luisa::compute::Action action, luisa::float2 xy) override;
    void handle_cursor_position(luisa::float2 xy) override;

    /**
     * Get currently selected object ID list
     */
    [[nodiscard]] const luisa::vector<uint> &get_selected_object_ids() const { return dragged_object_ids; }

    ~VisApp() override;

protected:
    /**
     * Update camera (taking interaction mode into account)
     */
    void update_camera(float delta_time) override;
};

}// namespace rbc

#pragma once
/**
 * RenderSceneView - A rendering view into the EditorScene
 * 
 * This is a lightweight view that provides render-specific access to the scene.
 * It does not own entities - those are managed by EditorScene.
 * 
 * Created by SceneService to interface with the renderer.
 */

#include <rbc_config.h>
#include "RBCEditorRuntime/infra/editor/EditorScene.h"

namespace rbc {

class GraphicsUtils;

/**
 * RenderSceneView - Provides render-specific access to EditorScene
 * 
 * Used by the rendering pipeline to:
 * - Access entity transforms and render components
 * - Query scene readiness
 * - Get skybox and materials
 * 
 * This view is created and managed by SceneService.
 */
struct RBC_EDITOR_RUNTIME_API RenderSceneView {
    EditorScene *scene_ = nullptr;

    // ========== Scene State ==========

    /**
     * Check if the scene is ready for rendering
     */
    [[nodiscard]] bool isReady() const {
        return scene_ && scene_->isReady();
    }

    /**
     * Check if a scene is bound
     */
    [[nodiscard]] bool isValid() const {
        return scene_ != nullptr;
    }

    // ========== Entity Access ==========

    /**
     * Get entity count
     */
    [[nodiscard]] size_t entityCount() const {
        return scene_ ? scene_->entityCount() : 0;
    }

    /**
     * Get all entity IDs
     */
    [[nodiscard]] luisa::vector<int> getAllEntityIds() const {
        return scene_ ? scene_->getAllEntityIds() : luisa::vector<int>{};
    }

    /**
     * Get entity by local ID
     */
    [[nodiscard]] world::Entity *getEntity(int localId) const {
        return scene_ ? scene_->getEntity(localId) : nullptr;
    }

    /**
     * Get entity ID from TLAS instance ID (for picking)
     */
    [[nodiscard]] int getEntityIdFromInstanceId(uint32_t instanceId) const {
        return scene_ ? scene_->getEntityIdFromInstanceId(instanceId) : -1;
    }

    // ========== Resource Access ==========

    /**
     * Get skybox texture
     */
    [[nodiscard]] RC<world::TextureResource> getSkybox() const {
        return scene_ ? scene_->getSkybox() : RC<world::TextureResource>{};
    }
};

}// namespace rbc
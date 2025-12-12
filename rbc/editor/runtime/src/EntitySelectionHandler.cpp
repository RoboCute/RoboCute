#include "RBCEditorRuntime/EntitySelectionHandler.h"
#include "RBCEditorRuntime/EditorContext.h"
#include "RBCEditorRuntime/components/DetailsPanel.h"
#include "RBCEditorRuntime/runtime/SceneSyncManager.h"
#include "RBCEditorRuntime/runtime/SceneSync.h"
#include "RBCEditorRuntime/EventBus.h"
#include <QDebug>

namespace rbc {

EntitySelectionHandler::EntitySelectionHandler(EditorContext *context, QObject *parent)
    : QObject(parent),
      context_(context),
      entitySelectedSubscriptionId_(-1) {
    Q_ASSERT(context_ != nullptr);
}

EntitySelectionHandler::~EntitySelectionHandler() {
    // 取消订阅事件总线
    if (entitySelectedSubscriptionId_ >= 0) {
        EventBus::instance().unsubscribe(entitySelectedSubscriptionId_);
    }
}

void EntitySelectionHandler::initialize() {
    // 订阅实体选择事件
    entitySelectedSubscriptionId_ = EventBus::instance().subscribe(
        EventType::EntitySelected,
        [this](const Event &event) {
            onEvent(event);
        });
}

void EntitySelectionHandler::handleEntitySelected(int entityId) {
    if (!context_->sceneSyncManager) {
        qWarning() << "EntitySelectionHandler: SceneSyncManager is not available";
        return;
    }

    const auto *sceneSync = context_->sceneSyncManager->sceneSync();
    if (!sceneSync) {
        qWarning() << "EntitySelectionHandler: SceneSync is not available";
        return;
    }

    // Get entity and resource data
    const auto *entity = sceneSync->getEntity(entityId);
    if (!entity) {
        qWarning() << "EntitySelectionHandler: Entity not found:" << entityId;
        return;
    }

    const EditorResourceMetadata *resource = nullptr;
    if (entity->has_render_component) {
        resource = sceneSync->getResource(entity->render_component.mesh_id);
    }

    // Update details panel
    if (context_->detailsPanel) {
        context_->detailsPanel->showEntity(entity, resource);
    } else {
        qWarning() << "EntitySelectionHandler: DetailsPanel is not available";
    }

    qDebug() << "EntitySelectionHandler: Entity selected:" << entityId;
    emit entitySelectionCompleted(entityId);
}

void EntitySelectionHandler::onEvent(const Event &event) {
    switch (event.type) {
        case EventType::EntitySelected: {
            // 从事件数据中获取实体ID
            bool ok = false;
            int entityId = event.data.toInt(&ok);
            if (ok) {
                handleEntitySelected(entityId);
            }
            break;
        }
        default:
            break;
    }
}

}// namespace rbc

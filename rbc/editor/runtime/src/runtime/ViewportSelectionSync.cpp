#include "RBCEditorRuntime/runtime/ViewportSelectionSync.h"
#include "RBCEditorRuntime/runtime/EditorContext.h"
#include "RBCEditorRuntime/runtime/EditorScene.h"
#include "RBCEditorRuntime/components/DetailsPanel.h"
#include "RBCEditorRuntime/engine/EditorEngine.h"
#include "RBCEditorRuntime/engine/visapp.h"
#include "RBCEditorRuntime/runtime/EventBus.h"
#include <QDebug>

ViewportSelectionSync::ViewportSelectionSync(rbc::EditorContext *context, QObject *parent)
    : QObject(parent),
      context_(context),
      checkTimer_(new QTimer(this)) {
    Q_ASSERT(context_ != nullptr);
    
    // Set timer interval to 50ms (20 Hz) for responsive selection updates
    checkTimer_->setInterval(50);
    connect(checkTimer_, &QTimer::timeout, this, &ViewportSelectionSync::checkSelection);
}

ViewportSelectionSync::~ViewportSelectionSync() {
    stop();
}

void ViewportSelectionSync::initialize() {
    if (!checkTimer_->isActive()) {
        checkTimer_->start();
    }
}

void ViewportSelectionSync::stop() {
    if (checkTimer_->isActive()) {
        checkTimer_->stop();
    }
}

void ViewportSelectionSync::checkSelection() {
    // Get VisApp instance from EditorEngine
    auto *renderApp = rbc::EditorEngine::instance().getRenderApp();
    if (!renderApp) {
        return;
    }

    // Cast to VisApp
    auto *visApp = dynamic_cast<rbc::VisApp *>(renderApp);
    if (!visApp) {
        return;
    }

    // Check if EditorScene is available
    if (!context_->editorScene) {
        return;
    }

    // Get current instance ids from VisApp
    const auto &currentInstanceIds = visApp->dragged_object_ids;

    // Check if selection has changed
    bool selectionChanged = false;
    if (currentInstanceIds.size() != lastInstanceIds_.size()) {
        selectionChanged = true;
    } else {
        for (size_t i = 0; i < currentInstanceIds.size(); ++i) {
            if (i >= lastInstanceIds_.size() || currentInstanceIds[i] != lastInstanceIds_[i]) {
                selectionChanged = true;
                break;
            }
        }
    }

    if (!selectionChanged) {
        return; // No change, skip update
    }

    // Update last instance ids
    lastInstanceIds_ = currentInstanceIds;

    // Convert instance ids to entity ids
    luisa::vector<int> entityIds = context_->editorScene->getEntityIdsFromInstanceIds(currentInstanceIds);

    // Emit selection events
    if (entityIds.empty()) {
        // No selection - clear details panel highlight
        qDebug() << "ViewportSelectionSync: Selection cleared";
        if (context_->detailsPanel) {
            context_->detailsPanel->clear();
        }
    } else {
        // Emit event for the first selected entity (or all if multi-select is supported)
        // For now, we'll use the first entity
        int selectedEntityId = entityIds[0];
        
        qDebug() << "ViewportSelectionSync: Entity selected:" << selectedEntityId 
                 << "from instance ids:" << currentInstanceIds.size();
        
        // Emit EntitySelected event through EventBus
        rbc::EventBus::instance().publish(
            rbc::EventType::EntitySelected,
            QVariant::fromValue(selectedEntityId)
        );
    }
}


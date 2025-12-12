#pragma once

#include "RBCEditorRuntime/config.h"

namespace rbc {
class HttpClient;
class SceneSyncManager;
class WorkflowManager;
class SceneHierarchyWidget;
class DetailsPanel;
class ViewportWidget;
class ResultPanel;
class AnimationPlayer;
class AnimationPlaybackManager;
class EditorScene;
class NodeEditor;
class ConnectionStatusView;
}// namespace rbc

class QDockWidget;

namespace rbc {
struct EditorContext {
    HttpClient *httpClient = nullptr;
    SceneSyncManager *sceneSyncManager = nullptr;
    WorkflowManager *workflowManager = nullptr;
    SceneHierarchyWidget *sceneHierarchy = nullptr;
    DetailsPanel *detailsPanel = nullptr;
    ViewportWidget *viewportWidget = nullptr;
    QDockWidget *viewportDock = nullptr;
    ResultPanel *resultPanel = nullptr;
    AnimationPlayer *animationPlayer = nullptr;
    AnimationPlaybackManager *playbackManager = nullptr;
    EditorScene *editorScene = nullptr;
    NodeEditor *nodeEditor = nullptr;
    QDockWidget *nodeDock = nullptr;
    ConnectionStatusView *connectionStatusView = nullptr;
    QDockWidget *connectionStatusDock = nullptr;
};
}// namespace rbc

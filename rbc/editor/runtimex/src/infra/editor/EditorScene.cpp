#include "RBCEditorRuntime/infra/editor/EditorScene.h"
#include <qDebug>

namespace rbc {

SceneSync::SceneSync() {}
SceneSync::~SceneSync() {}

EditorScene::EditorScene() {
    qDebug() << "EditorScene: Initialize with Runtime ECS";
}
EditorScene::~EditorScene() {
    qDebug() << "EditorScene: Destroying";
}

}// namespace rbc
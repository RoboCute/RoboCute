#pragma once
// 一个可以进行渲染的场景View，不会实际持有Entity，由Scene创建来对接渲染器
#include "RBCEditorRuntime/infra/editor/EditorScene.h"

namespace rbc {

struct RenderSceneView {

    EditorScene *scene_;
};

}// namespace rbc
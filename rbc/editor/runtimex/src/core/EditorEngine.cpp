#include "RBCEditorRuntime/core/EditorEngine.h"
#include "RBCEditorRuntime/plugins/PluginManager.h"
#include "RBCEditorRuntime/services/StyleManager.h"
#include <QDebug>

namespace rbc {

EditorEngine &EditorEngine::instance() {
    static EditorEngine engine;
    return engine;
}

void EditorEngine::init(int argc, char **argv) {
    // Register StyleManager service
    auto &pluginManager = EditorPluginManager::instance();
    StyleManager *styleManager = new StyleManager();
    styleManager->initialize(argc, argv);
    pluginManager.registerService(styleManager);
    qDebug() << "EditorEngine::init: StyleManager service registered";
}

void EditorEngine::shutdown() {
    // Services will be cleaned up by PluginManager
}

}// namespace rbc
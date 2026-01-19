#include "RBCEditorRuntime/core/EditorEngine.h"
#include "RBCEditorRuntime/plugins/PluginManager.h"

#include "RBCEditorRuntime/plugins/ViewportPlugin.h"

#include "RBCEditorRuntime/services/StyleManager.h"
#include "RBCEditorRuntime/services/SceneService.h"
#include "RBCEditorRuntime/services/ProjectService.h"
#include "RBCEditorRuntime/services/ConnectionService.h"

#include "RBCEditorRuntime/infra/render/app_base.h"
#include "RBCEditorRuntime/infra/render/visapp.h"

#include <QDebug>

namespace rbc {

EditorEngine &EditorEngine::instance() {
    static EditorEngine engine;
    return engine;
}

void EditorEngine::init(int argc, char **argv) {

    if (m_isInitialized) return;

    // TODO: parsed from arg or config
    m_backendName = "dx";
    m_programPath = argv[0];
    m_graphicsApi = QRhi::D3D12;

    // Register StyleManager service
    auto &pluginManager = EditorPluginManager::instance();
    qDebug() << "============= Start Register Services =================";
    // Register Services
    {
        auto *projectService = new ProjectService();
        pluginManager.registerService(projectService);
        qDebug() << "ProjectService registered";
    }
    {
        auto *connectionService = new ConnectionService();
        pluginManager.registerService(connectionService);
        qDebug() << "ConnectionService registered";
    }
    {
        StyleManager *styleManager = new StyleManager();
        styleManager->initialize(argc, argv);
        pluginManager.registerService(styleManager);
        qDebug() << "StyleManager service registered";
    }
    qDebug() << "============= Register Services Done =================";
    // register plugins
    qDebug() << "============= Start Register Built-in Plugins =================";
    pluginManager.registerPlugin<ViewportPlugin>();
    {
        if (!pluginManager.loadPlugin(ViewportPlugin::staticPluginId())) {
            qWarning() << "Failed to load ViewportPlugin";
        }
    }
    qDebug() << "============= Register Built-in Plugins Done =================";

    auto *viewportPlugin = static_cast<ViewportPlugin *>(pluginManager.getPlugin(ViewportPlugin::staticPluginId()));

    viewportPlugin->setRendererFactory([=](const ViewportConfig &config) {
        IRenderer *app;
        switch (config.type) {
            case ViewportType::Main: {
                app = static_cast<IRenderer *>(new VisApp());
                break;
            }
            default: {
                app = static_cast<IRenderer *>(new VisApp());
                break;
            }
        }
        app->init(m_programPath.c_str(), m_backendName.c_str());
        return app;
    });
    viewportPlugin->createDefaultViewports();

    m_isInitialized = true;
}

void EditorEngine::shutdown() {
    // Services will be cleaned up by PluginManager
}

}// namespace rbc
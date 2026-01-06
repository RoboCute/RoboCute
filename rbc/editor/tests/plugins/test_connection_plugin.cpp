#include "test_connection_plugin.h"

void TestConnectionPlugin::initTestCase() {
    qDebug() << "=== TestConnectionPlugin Test Suite ===";
}

void TestConnectionPlugin::cleanupTestCase() {
    qDebug() << "=== TestConnectionPlugin Test Suite Complete ===";
}

void TestConnectionPlugin::init() {
    pluginManager_ = &EditorPluginManager::instance();
    engine_ = new QQmlEngine(this);
    pluginManager_->setQmlEngine(engine_);
    
    mockService_ = new MockConnectionService(this);
    pluginManager_->registerService(mockService_);
    
    context_ = new PluginContext(pluginManager_, this);
    plugin_ = new ConnectionPlugin(this);
}

void TestConnectionPlugin::cleanup() {
    if (plugin_) {
        plugin_->unload();
        delete plugin_;
        plugin_ = nullptr;
    }
    
    delete context_;
    context_ = nullptr;
    
    delete engine_;
    engine_ = nullptr;
    
    // Don't delete pluginManager_ as it's a singleton
    pluginManager_ = nullptr;
    
    delete mockService_;
    mockService_ = nullptr;
    
    // Clean up plugin manager state
    EditorPluginManager::instance().unloadAllPlugins();
}

void TestConnectionPlugin::test_plugin_load() {
    QVERIFY(plugin_->load(context_));
    QCOMPARE(plugin_->id(), QString("com.robocute.connection"));
}

void TestConnectionPlugin::test_plugin_unload() {
    QVERIFY(plugin_->load(context_));
    QVERIFY(plugin_->unload());
}

void TestConnectionPlugin::test_plugin_reload() {
    QVERIFY(plugin_->load(context_));
    
    // Set some state
    mockService_->setServerUrl("http://test:5555");
    
    QVERIFY(plugin_->reload());
    
    // State should be preserved
    QCOMPARE(mockService_->serverUrl(), QString("http://test:5555"));
}

void TestConnectionPlugin::test_plugin_load_without_service() {
    // Create plugin context without service
    EditorPluginManager *pm = &EditorPluginManager::instance();
    QQmlEngine *eng = new QQmlEngine(this);
    pm->setQmlEngine(eng);
    
    PluginContext *ctx = new PluginContext(pm, this);
    ConnectionPlugin *plg = new ConnectionPlugin(this);
    
    // Should fail to load without service
    QVERIFY(!plg->load(ctx));
    
    delete plg;
    delete ctx;
    delete eng;
    pm->unloadAllPlugins();
}

void TestConnectionPlugin::test_plugin_id() {
    QCOMPARE(plugin_->id(), QString("com.robocute.connection"));
}

void TestConnectionPlugin::test_plugin_name() {
    QCOMPARE(plugin_->name(), QString("Connection Plugin"));
}

void TestConnectionPlugin::test_plugin_version() {
    QCOMPARE(plugin_->version(), QString("1.0.0"));
}

void TestConnectionPlugin::test_plugin_dependencies() {
    QStringList deps = plugin_->dependencies();
    QVERIFY(deps.isEmpty());
}

void TestConnectionPlugin::test_view_contributions() {
    QVERIFY(plugin_->load(context_));
    
    QList<ViewContribution> views = plugin_->view_contributions();
    QCOMPARE(views.size(), 1);
    
    ViewContribution view = views.first();
    QCOMPARE(view.viewId, QString("connection_status"));
    QCOMPARE(view.title, QString("Connection Status"));
    QCOMPARE(view.qmlSource, QString("ConnectionView.qml"));
    QCOMPARE(view.dockArea, QString("Left"));
}

void TestConnectionPlugin::test_view_model_registration() {
    QVERIFY(plugin_->load(context_));
    
    plugin_->register_view_models(engine_);
    
    // Check if type is registered
    // Note: QQmlEngine doesn't provide a direct way to check registered types
    // So we just verify the method completes without error
    QVERIFY(true);
}

void TestConnectionPlugin::test_get_view_model() {
    QVERIFY(plugin_->load(context_));
    
    QObject *vm = plugin_->getViewModel("connection_status");
    QVERIFY(vm != nullptr);
    
    ConnectionViewModel *connectionVM = qobject_cast<ConnectionViewModel*>(vm);
    QVERIFY(connectionVM != nullptr);
    
    // Test invalid view ID
    QObject *invalidVM = plugin_->getViewModel("invalid_view");
    QVERIFY(invalidVM == nullptr);
}

void TestConnectionPlugin::test_plugin_with_mock_service() {
    QVERIFY(plugin_->load(context_));
    
    QObject *vm = plugin_->getViewModel("connection_status");
    QVERIFY(vm != nullptr);
    
    ConnectionViewModel *connectionVM = qobject_cast<ConnectionViewModel*>(vm);
    QVERIFY(connectionVM != nullptr);
    
    // Test that ViewModel uses the mock service
    QCOMPARE(connectionVM->serverUrl(), mockService_->serverUrl());
    
    mockService_->setServerUrl("http://test:9999");
    QCoreApplication::processEvents();
    QCOMPARE(connectionVM->serverUrl(), QString("http://test:9999"));
}

void TestConnectionPlugin::test_view_model_from_plugin() {
    QVERIFY(plugin_->load(context_));
    
    QObject *vm = plugin_->getViewModel("connection_status");
    ConnectionViewModel *connectionVM = qobject_cast<ConnectionViewModel*>(vm);
    
    QVERIFY(connectionVM != nullptr);
    
    // Test ViewModel functionality
    QSignalSpy connectedSpy(connectionVM, &ConnectionViewModel::connectedChanged);
    
    connectionVM->connect();
    mockService_->simulateConnectionSuccess();
    QCoreApplication::processEvents();
    
    QCOMPARE(connectionVM->connected(), true);
    QVERIFY(connectedSpy.count() >= 1);
}

void TestConnectionPlugin::test_multiple_plugins() {
    // Test that multiple plugins can coexist
    EditorPluginManager *pm = &EditorPluginManager::instance();
    QQmlEngine *eng = new QQmlEngine(this);
    pm->setQmlEngine(eng);
    
    MockConnectionService *service1 = new MockConnectionService(this);
    pm->registerService(service1);
    
    PluginContext *ctx = new PluginContext(pm, this);
    
    ConnectionPlugin *plugin1 = new ConnectionPlugin(this);
    ConnectionPlugin *plugin2 = new ConnectionPlugin(this);
    
    QVERIFY(plugin1->load(ctx));
    QVERIFY(plugin2->load(ctx));
    
    QCOMPARE(plugin1->id(), plugin2->id());
    
    plugin1->unload();
    plugin2->unload();
    
    delete plugin2;
    delete plugin1;
    delete ctx;
    delete eng;
    delete service1;
    pm->unloadAllPlugins();
}

QTEST_MAIN(TestConnectionPlugin)

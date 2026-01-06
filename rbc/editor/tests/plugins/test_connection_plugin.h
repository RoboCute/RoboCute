#pragma once

#include <QtTest/QtTest>
#include <QtTest/QSignalSpy>
#include <QQmlEngine>
#include <QCoreApplication>

#include "RBCEditorRuntime/plugins/PluginManager.h"
#include "RBCEditorRuntime/plugins/PluginContext.h"
#include "RBCEditorRuntime/services/ConnectionService.h"
#include "../mocks/MockConnectionService.h"
#include "../../plugins/connection_plugin/ConnectionPlugin.h"

using namespace rbc;

/**
 * Integration tests for ConnectionPlugin
 * 
 * Tests plugin lifecycle, view contributions, and ViewModel creation
 */
class TestConnectionPlugin : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Plugin lifecycle tests
    void test_plugin_load();
    void test_plugin_unload();
    void test_plugin_reload();
    void test_plugin_load_without_service();

    // Plugin metadata tests
    void test_plugin_id();
    void test_plugin_name();
    void test_plugin_version();
    void test_plugin_dependencies();

    // View contribution tests
    void test_view_contributions();
    void test_view_model_registration();
    void test_get_view_model();

    // Integration tests
    void test_plugin_with_mock_service();
    void test_view_model_from_plugin();
    void test_multiple_plugins();

private:
    EditorPluginManager *pluginManager_;
    MockConnectionService *mockService_;
    ConnectionPlugin *plugin_;
    PluginContext *context_;
    QQmlEngine *engine_;
};


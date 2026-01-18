#pragma once
#include <QtTest/QtTest>
#include <QObject>
#include "RBCEditorRuntime/mvvm/ViewModelBase.h"
#include "../mocks/MockConnectionService.h"
#include "../../plugins/connection_plugin/ConnectionPlugin.h"

class TestConnectionViewModel : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Property tests
    void test_serverUrl_property();
    void test_connected_property();
    void test_statusText_property();
    void test_isBusy_property();

    // Method tests
    void test_setServerUrl();
    void test_connect();
    void test_disconnect();
    void test_testConnection();

    // Signal tests
    void test_serverUrlChanged_signal();
    void test_connectedChanged_signal();
    void test_statusTextChanged_signal();
    void test_isBusyChanged_signal();

    // Integration tests
    void test_connect_flow();
    void test_disconnect_flow();
    void test_testConnection_flow();
    void test_error_handling();

private:
    rbc::MockConnectionService *mockService_;
    rbc::IConnectionViewModel *viewModel_;
};

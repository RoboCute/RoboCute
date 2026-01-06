#include <QtTest/QtTest>
#include <QtTest/QSignalSpy>
#include <QTimer>
#include <QCoreApplication>
#include "test_connection_viewmodel.h"

using namespace rbc;

/**
 * Unit tests for ConnectionViewModel
 * 
 * Tests ViewModel behavior with mocked ConnectionService
 */

void TestConnectionViewModel::initTestCase() {
    qDebug() << "=== TestConnectionViewModel Test Suite ===";
}

void TestConnectionViewModel::cleanupTestCase() {
    qDebug() << "=== TestConnectionViewModel Test Suite Complete ===";
}

void TestConnectionViewModel::init() {
    mockService_ = new MockConnectionService(this);
    viewModel_ = new ConnectionViewModel(mockService_, this);
}

void TestConnectionViewModel::cleanup() {
    delete viewModel_;
    viewModel_ = nullptr;
    delete mockService_;
    mockService_ = nullptr;
}

void TestConnectionViewModel::test_serverUrl_property() {
    QCOMPARE(viewModel_->serverUrl(), QString("http://127.0.0.1:5555"));

    mockService_->setServerUrl("http://localhost:8080");
    QCOMPARE(viewModel_->serverUrl(), QString("http://localhost:8080"));
}

void TestConnectionViewModel::test_connected_property() {
    QCOMPARE(viewModel_->connected(), false);

    mockService_->simulateConnectionSuccess();
    QCOMPARE(viewModel_->connected(), true);

    mockService_->disconnect();
    QCOMPARE(viewModel_->connected(), false);
}

void TestConnectionViewModel::test_statusText_property() {
    QCOMPARE(viewModel_->statusText(), QString("Disconnected"));

    mockService_->simulateConnectionSuccess();
    QCOMPARE(viewModel_->statusText(), QString("Connected"));

    mockService_->simulateConnectionFailure("Network error");
    QCOMPARE(viewModel_->statusText(), QString("Network error"));
}

void TestConnectionViewModel::test_isBusy_property() {
    QCOMPARE(viewModel_->isBusy(), false);

    // When connecting, should be busy
    viewModel_->connect();
    QCOMPARE(viewModel_->isBusy(), true);

    // After connection completes, should not be busy
    mockService_->simulateConnectionSuccess();
    QCOMPARE(viewModel_->isBusy(), false);
}

void TestConnectionViewModel::test_setServerUrl() {
    QSignalSpy spy(viewModel_, &ConnectionViewModel::serverUrlChanged);

    viewModel_->setServerUrl("http://test:1234");
    QCOMPARE(viewModel_->serverUrl(), QString("http://test:1234"));
    QCOMPARE(spy.count(), 1);
}

void TestConnectionViewModel::test_connect() {
    QSignalSpy busySpy(viewModel_, &ConnectionViewModel::isBusyChanged);
    QSignalSpy connectedSpy(viewModel_, &ConnectionViewModel::connectedChanged);

    viewModel_->connect();
    
    // Should be busy immediately
    QCOMPARE(viewModel_->isBusy(), true);
    QVERIFY(busySpy.count() >= 1);

    // Simulate successful connection
    mockService_->simulateConnectionSuccess();
    
    // Wait for signals to propagate
    QCoreApplication::processEvents();
    
    QCOMPARE(viewModel_->connected(), true);
    QCOMPARE(viewModel_->isBusy(), false);
    QVERIFY(connectedSpy.count() >= 1);
}

void TestConnectionViewModel::test_disconnect() {
    // First connect
    mockService_->simulateConnectionSuccess();
    QCOMPARE(viewModel_->connected(), true);

    // Then disconnect
    QSignalSpy connectedSpy(viewModel_, &ConnectionViewModel::connectedChanged);
    viewModel_->disconnect();
    
    QCoreApplication::processEvents();
    
    QCOMPARE(viewModel_->connected(), false);
    QCOMPARE(viewModel_->isBusy(), false);
    QVERIFY(connectedSpy.count() >= 1);
}

void TestConnectionViewModel::test_testConnection() {
    QSignalSpy busySpy(viewModel_, &ConnectionViewModel::isBusyChanged);
    QSignalSpy testedSpy(mockService_, &MockConnectionService::connectionTested);

    viewModel_->testConnection();
    
    QCOMPARE(viewModel_->isBusy(), true);
    QVERIFY(busySpy.count() >= 1);

    mockService_->simulateTestSuccess();
    QCoreApplication::processEvents();
    
    QCOMPARE(viewModel_->isBusy(), false);
    QCOMPARE(testedSpy.count(), 1);
}

void TestConnectionViewModel::test_serverUrlChanged_signal() {
    QSignalSpy spy(viewModel_, &ConnectionViewModel::serverUrlChanged);

    mockService_->setServerUrl("http://new:9999");
    QCoreApplication::processEvents();
    
    QVERIFY(spy.count() >= 1);
}

void TestConnectionViewModel::test_connectedChanged_signal() {
    QSignalSpy spy(viewModel_, &ConnectionViewModel::connectedChanged);

    mockService_->simulateConnectionSuccess();
    QCoreApplication::processEvents();
    
    QVERIFY(spy.count() >= 1);
}

void TestConnectionViewModel::test_statusTextChanged_signal() {
    QSignalSpy spy(viewModel_, &ConnectionViewModel::statusTextChanged);

    mockService_->simulateConnectionSuccess();
    QCoreApplication::processEvents();
    
    QVERIFY(spy.count() >= 1);
}

void TestConnectionViewModel::test_isBusyChanged_signal() {
    QSignalSpy spy(viewModel_, &ConnectionViewModel::isBusyChanged);

    viewModel_->connect();
    QVERIFY(spy.count() >= 1);

    mockService_->simulateConnectionSuccess();
    QCoreApplication::processEvents();
    QVERIFY(spy.count() >= 2); // busy -> not busy
}

void TestConnectionViewModel::test_connect_flow() {
    // Test complete connect flow
    QCOMPARE(viewModel_->connected(), false);
    QCOMPARE(viewModel_->statusText(), QString("Disconnected"));

    viewModel_->setServerUrl("http://test:5555");
    viewModel_->connect();
    
    QCOMPARE(viewModel_->isBusy(), true);
    QCOMPARE(viewModel_->statusText(), QString("Connecting ..."));

    mockService_->simulateConnectionSuccess();
    QCoreApplication::processEvents();
    
    QCOMPARE(viewModel_->connected(), true);
    QCOMPARE(viewModel_->statusText(), QString("Connected"));
    QCOMPARE(viewModel_->isBusy(), false);
}

void TestConnectionViewModel::test_disconnect_flow() {
    // First connect
    mockService_->simulateConnectionSuccess();
    QCoreApplication::processEvents();
    QCOMPARE(viewModel_->connected(), true);

    // Then disconnect
    viewModel_->disconnect();
    QCoreApplication::processEvents();
    
    QCOMPARE(viewModel_->connected(), false);
    QCOMPARE(viewModel_->statusText(), QString("Disconnected"));
    QCOMPARE(viewModel_->isBusy(), false);
}

void TestConnectionViewModel::test_testConnection_flow() {
    viewModel_->setServerUrl("http://test:5555");
    viewModel_->testConnection();
    
    QCOMPARE(viewModel_->isBusy(), true);

    mockService_->simulateTestSuccess();
    QCoreApplication::processEvents();
    
    QCOMPARE(viewModel_->isBusy(), false);
    // Connection status should not change for test
    QCOMPARE(viewModel_->connected(), false);
}

void TestConnectionViewModel::test_error_handling() {
    // Test with null service (should not crash)
    ConnectionViewModel *vmWithNull = new ConnectionViewModel(nullptr, this);
    QVERIFY(vmWithNull != nullptr);
    
    // Should handle gracefully
    vmWithNull->setServerUrl("http://test");
    QCOMPARE(vmWithNull->serverUrl(), QString());
    
    vmWithNull->connect();
    QCOMPARE(vmWithNull->isBusy(), false);
    
    delete vmWithNull;
}

QTEST_MAIN(TestConnectionViewModel)

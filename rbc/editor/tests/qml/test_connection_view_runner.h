#pragma once

#include <QtQuickTest>
#include <QQmlEngine>
#include <QQmlContext>
#include <QObject>

#include "RBCEditorRuntime/mvvm/ViewModelBase.h"
#include "../mocks/MockConnectionService.h"
#include "../../plugins/connection_plugin/ConnectionPlugin.h"

using namespace rbc;

/**
 * QML Test Runner for ConnectionView
 * 
 * Sets up QML test environment with mocked services
 */
class ConnectionViewTestSetup : public QObject {
    Q_OBJECT

public:
    ConnectionViewTestSetup();

    static QObject *mockViewModelProvider(QQmlEngine *engine, QJSEngine *scriptEngine);

private:
    MockConnectionService *mockService_;
    ConnectionViewModel *viewModel_;
};


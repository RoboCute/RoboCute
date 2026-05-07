#pragma once

#include <QWidget>
#include <QQmlEngine>
#include <QQuickWidget>

namespace rbc {

class QmlViewContainer : public QWidget {
    Q_OBJECT

public:
    QmlViewContainer(const QString &qmlSource,
                     QObject *viewModel,
                     QQmlEngine *engine,
                     QWidget *parent = nullptr);
    ~QmlViewContainer();

    // reload support
    void reloadQml();

    // viewModel Query
    QObject *viewModel() const { return _view_model; }

signals:
    void loadError(const QString &error);

private:
    QString _qml_source;
    QObject *_view_model;
    QQmlEngine *_engine;
    QQuickWidget *_quick_widget;
    QQmlComponent *_component;
};

}// namespace rbc
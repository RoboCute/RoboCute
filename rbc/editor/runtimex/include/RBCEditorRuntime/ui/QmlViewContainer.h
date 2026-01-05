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
    QObject *viewModel() const { return viewModel_; }

signals:
    void loadError(const QString &error);

private:
    QString qmlSource_;
    QObject *viewModel_;
    QQmlEngine *engine_;
    QQuickWidget *quickWidget_;
    QQmlComponent *component_;
};

}// namespace rbc
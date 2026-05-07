#pragma once

#include <rbc_config.h>
#include <QObject>
#include "RBCEditorRuntime/infra/events/Event.h"
#include "RBCEditorRuntime/infra/events/EventType.h"

namespace rbc {

class RBC_EDITOR_RUNTIME_API ViewModelBase : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isBusy READ isBusy NOTIFY isBusyChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)

public:
    explicit ViewModelBase(QObject *parent = nullptr);

    bool isBusy() const { return _is_busy; }
    QString errorMessage() const { return _error_message; }

    // LifeCycle
    virtual void onActivate() {}  // on View show
    virtual void onDeactivate() {}// on View hide
    virtual void onReload() {}    // on Hot Reload

protected:
    void setBusy(bool busy);
    void setError(const QString &message);
    void clearError();

    // EventBus fast method
    void publish(const Event &event);
    void subscribe(EventType type, std::function<void(const Event &)> handler);
    void unsubscribe(EventType type);

signals:
    void isBusyChanged();
    void errorMessageChanged();

private:
    bool _is_busy;
    QString _error_message;
    QList<QMetaObject::Connection> _subscriptions;
};

}// namespace rbc
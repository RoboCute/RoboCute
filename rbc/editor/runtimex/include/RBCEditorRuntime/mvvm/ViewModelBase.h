#pragma once

#include <QObject>
#include "RBCEditorRuntime/infra/events/Event.h"
#include "RBCEditorRuntime/infra/events/EventType.h"

namespace rbc {

class ViewModelBase : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isBusy READ isBusy NOTIFY isBusyChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)

public:
    explicit ViewModelBase(QObject *parent = nullptr);

    bool isBusy() const { return isBusy_; }
    QString errorMessage() const { return errorMessage_; }

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
    bool isBusy_;
    QString errorMessage_;
    QList<QMetaObject::Connection> subscriptions_;
};

}// namespace rbc
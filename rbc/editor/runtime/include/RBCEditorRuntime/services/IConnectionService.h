#pragma once
#include <rbc_config.h>
#include <QObject>
#include "RBCEditorRuntime/services/IService.h"

namespace rbc {

class RBC_EDITOR_RUNTIME_API IConnectionService : public IService {
    Q_OBJECT
public:
    explicit IConnectionService(QObject *parent = nullptr) : IService(parent) {}
    virtual ~IConnectionService() = default;

    virtual QString serverUrl() const = 0;
    virtual void setServerUrl(const QString &url) = 0;
    virtual bool connected() const = 0;
    virtual QString statusText() const = 0;

signals:
    void serverUrlChanged();
    void connectedChanged();
    void statusTextChanged();
    void connectionTested(bool success);
};

}// namespace rbc
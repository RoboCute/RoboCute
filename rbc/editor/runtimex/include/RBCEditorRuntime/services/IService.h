#pragma once

#include <rbc_config.h>
#include <QObject>

namespace rbc {

/**
 * IService - Base interface for all editor services
 * 
 * All services should inherit from this interface and provide a unique service ID
 */
class RBC_EDITOR_RUNTIME_API IService : public QObject {
    Q_OBJECT
public:
    explicit IService(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~IService() = default;

    /**
     * Get the unique service ID
     * This ID is used for service registration and lookup
     */
    virtual QString serviceId() const = 0;
};

}// namespace rbc

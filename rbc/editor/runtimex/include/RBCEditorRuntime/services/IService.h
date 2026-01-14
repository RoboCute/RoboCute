#pragma once

#include <rbc_config.h>
#include <QObject>

namespace rbc {

/**
 * IService - Base interface for all editor services
 * 
 * All services should inherit from this interface and provide a unique service ID
 * 
 * 重要：作为接口类，使用内联析构器避免链接冲突
 * 具体实现类（如 ConnectionService）应在 .cpp 中定义析构器
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

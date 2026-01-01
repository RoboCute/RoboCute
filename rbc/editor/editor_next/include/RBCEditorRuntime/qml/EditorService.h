#pragma once

#include <QObject>
#include <QString>

namespace rbc {

/**
 * EditorService - QML 编辑器主服务
 * 提供编辑器级别的功能和服务管理
 */
class EditorService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString version READ version CONSTANT)

public:
    explicit EditorService(QObject *parent = nullptr);
    ~EditorService() override = default;

    QString version() const { return "0.3.0-next"; }

public slots:
    /**
     * 初始化编辑器
     */
    void initialize();

    /**
     * 关闭编辑器
     */
    void shutdown();

signals:
    void initialized();
    void shutdownCompleted();

private:
    bool m_initialized = false;
};

} // namespace rbc


#pragma once
#include <rbc_config.h>
#include <QObject>
#include <QDockWidget>
#include <QJsonObject>
namespace rbc {

class WindowManager;
class EditorPluginManager;

class ILayoutService : public QObject {
    Q_OBJECT
public:
    virtual ~ILayoutService() = default;
    
protected:
    // Constructor for subclasses
    explicit ILayoutService(QObject *parent = nullptr) : QObject(parent) {}
    
public:
    // Layout Management
    virtual QString currentLayoutId() const = 0;
    virtual QStringList availableLayouts() const = 0;
    virtual QJsonObject getLayoutMetadata(const QString &layoutId) const = 0;
    virtual bool hasLayout(const QString &layoutId) const = 0;
    virtual bool switchToLayout(const QString &layoutId, bool saveCurrentLayout = true) = 0;
    virtual bool saveCurrentLayout(const QString &layoutId = QString()) = 0;
    virtual void resetLayout(const QString &layoutId) = 0;
    virtual bool createLayout(const QString &layoutId, const QJsonObject &layoutConfig) = 0;
    virtual bool loadLayoutFromFile(const QString &filepath) = 0;
    virtual bool saveLayoutToFile(const QString &layoutId, const QString &filepath) = 0;
    virtual bool deleteLayout(const QString &layoutId) = 0;
    virtual bool cloneLayout(const QString &sourceId, const QString &newId, const QString &newName) = 0;

    // View Management
    virtual void setViewVisible(const QString &viewId, bool visible) = 0;
    virtual bool isViewVisible(const QString &viewId) const = 0;
    virtual void resizeView(const QString &viewId, int width, int height) = 0;
    virtual void moveViewToDockArea(const QString &viewId, const QString &dockArea) = 0;

public:
    // LifeCycle
    virtual void initialize(class WindowManager *windowManager, class EditorPluginManager *pluginManager) = 0;
    virtual void applyLayout(const QString &layoutId) = 0;

signals:
};

}// namespace rbc
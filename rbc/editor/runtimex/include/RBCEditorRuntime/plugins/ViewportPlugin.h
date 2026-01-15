#pragma once
#include <rbc_config.h>
#include <QObject>
#include <QHash>
#include <QtGui/rhi/qrhi.h>
#include <functional>
#include "RBCEditorRuntime/services/SceneService.h"
#include "RBCEditorRuntime/mvvm/ViewModelBase.h"
#include "RBCEditorRuntime/plugins/IEditorPlugin.h"
#include "RBCEditorRuntime/core/IRenderer.h"

namespace rbc {

// Forward declarations
class ViewportWidget;

// ============================================================================
// ViewportType - 视口类型枚举
// ============================================================================

enum class ViewportType {
    Main,         // Editor Main Viewport
    Preview,      // Preview Viewport for material/model/result...
    CameraPreview,// Preview for specific camera
    Custom
};

// ============================================================================
// ViewportConfig - 视口配置
// ============================================================================

struct ViewportConfig {
    QString viewportId;          // uid for this viewport
    ViewportType type = ViewportType::Main;
    QString rendererType;        // renderer type (support override)
    bool enableGizmos = true;
    bool enableGrid = true;
    bool enableSelection = true;
    QRhi::Implementation graphicsApi = QRhi::D3D12; // RHI 图形 API
};

// ============================================================================
// ViewportViewModel - 视口状态视图模型
// ============================================================================

/**
 * @brief ViewportViewModel - 视口状态视图模型
 * 
 * 管理单个视口的状态：
 * - Camera State
 * - Control Command State
 * - Show/Hide State
 * - RenderConfig
 */
class RBC_EDITOR_RUNTIME_API ViewportViewModel : public ViewModelBase {
    Q_OBJECT
    Q_PROPERTY(QString viewportId READ viewportId CONSTANT)
    Q_PROPERTY(bool gizmosEnabled READ gizmosEnabled WRITE setGizmosEnabled NOTIFY gizmosEnabledChanged)
    Q_PROPERTY(bool gridEnabled READ gridEnabled WRITE setGridEnabled NOTIFY gridEnabledChanged)
    Q_PROPERTY(QString cameraMode READ cameraMode WRITE setCameraMode NOTIFY cameraModeChanged)

public:
    explicit ViewportViewModel(
        const ViewportConfig &config,
        ISceneService *scenService,
        QObject *parent = nullptr);
    ~ViewportViewModel() override;

    // Properties
    QString viewportId() const { return config_.viewportId; }
    bool gizmosEnabled() const { return gizmosEnabled_; }
    void setGizmosEnabled(bool enabled);
    bool gridEnabled() const { return gridEnabled_; }
    void setGridEnabled(bool enabled);
    QString cameraMode() const { return cameraMode_; }
    void setCameraMode(const QString &mode);

    // Camera control
    Q_INVOKABLE void focusOnSelection();
    Q_INVOKABLE void resetCamera();
    Q_INVOKABLE void setCameraView(const QString &preset); // Top/Front/Side/Perspective

signals:
    void gizmosEnabledChanged();
    void gridEnabledChanged();
    void cameraModeChanged();

private:
    ViewportConfig config_;
    ISceneService *sceneService_ = nullptr;
    bool gizmosEnabled_ = true;
    bool gridEnabled_ = true;
    QString cameraMode_ = "Perspective";
};

// ============================================================================
// ViewportInstance - 视口实例（组合 Widget + ViewModel + Renderer）
// ============================================================================

/**
 * @brief ViewportInstance - 将 Widget、ViewModel 和 Renderer 组合在一起
 * 
 * 每个视口实例包含：
 * - config: 视口配置
 * - widget: ViewportWidget 实例
 * - viewModel: ViewportViewModel 实例
 * - renderer: IRenderer 实例
 */
struct ViewportInstance {
    ViewportConfig config;
    ViewportWidget *widget = nullptr;
    ViewportViewModel *viewModel = nullptr;
    IRenderer *renderer = nullptr;
    
    ~ViewportInstance() {
        // 注意：widget 的生命周期可能由 Qt parent-child 机制管理
        // 这里只负责清理 viewModel 和 renderer
        delete viewModel;
        viewModel = nullptr;
        // renderer 通常由外部工厂创建，需要外部销毁
        // delete renderer;
        renderer = nullptr;
    }
};

// ============================================================================
// RendererFactory - 渲染器工厂函数类型
// ============================================================================

/**
 * @brief 渲染器工厂函数类型
 * 
 * 用于创建 IRenderer 实例的函数类型。
 * ViewportPlugin 通过设置渲染器工厂来创建渲染器。
 * 
 * @param config 视口配置
 * @return IRenderer* 渲染器实例，调用者负责管理生命周期
 */
using RendererFactory = std::function<IRenderer*(const ViewportConfig& config)>;

// ============================================================================
// ViewportPlugin - 视口管理插件
// ============================================================================

/**
 * @brief ViewportPlugin - 视口管理插件
 * 
 * 职责：
 * 1. 管理所有 ViewportWidget 的生命周期
 * 2. 提供视口创建/销毁 API
 * 3. 通过 NativeViewContribution 向 WindowManager 注册视口
 * 4. 协调 ViewportViewModel 与 SceneService 的交互
 * 
 * 使用方式：
 * @code
 * // 设置渲染器工厂
 * viewportPlugin->setRendererFactory([](const ViewportConfig& config) {
 *     return new MyRenderer();
 * });
 * 
 * // 创建视口
 * ViewportConfig config;
 * config.viewportId = "my_viewport";
 * viewportPlugin->createViewport(config);
 * @endcode
 */
class RBC_EDITOR_RUNTIME_API ViewportPlugin : public IEditorPlugin {
    Q_OBJECT

public:
    explicit ViewportPlugin(QObject *parent = nullptr);
    ~ViewportPlugin() override;

    // === Static Methods for Factory ===
    static QString staticPluginId() { return "com.robocute.viewport"; }
    static QString staticPluginName() { return "Viewport Plugin"; }

    // === IEditorPlugin Interface ===
    bool load(PluginContext *context) override;
    bool unload() override;
    bool reload() override;

    QString id() const override { return staticPluginId(); }
    QString name() const override { return staticPluginName(); }
    QString version() const override { return "1.0.0"; }
    QStringList dependencies() const override { return {}; }
    
    // UI Contributions
    QList<ViewContribution> view_contributions() const override { return {}; }
    QList<MenuContribution> menu_contributions() const override;
    QList<ToolbarContribution> toolbar_contributions() const override;
    QList<NativeViewContribution> native_view_contributions() const override;

    void register_view_models(QQmlEngine *engine) override {}
    QObject *getViewModel(const QString &viewId) override;
    QWidget *getNativeWidget(const QString &viewId) override;

    // === Renderer Factory ===
    
    /**
     * @brief 设置渲染器工厂
     * @param factory 渲染器工厂函数
     * 
     * 必须在创建视口之前调用此方法设置渲染器工厂。
     */
    void setRendererFactory(RendererFactory factory) { rendererFactory_ = std::move(factory); }
    
    /**
     * @brief 获取渲染器工厂
     */
    RendererFactory rendererFactory() const { return rendererFactory_; }

    // === Viewport Management API ===
    
    /**
     * @brief 创建新视口
     * @param config 视口配置
     * @return 视口 ID，失败返回空字符串
     */
    QString createViewport(const ViewportConfig &config);
    
    /**
     * @brief 使用已有的渲染器创建视口
     * @param config 视口配置
     * @param renderer 渲染器实例（ViewportPlugin 不接管其生命周期）
     * @return 视口 ID，失败返回空字符串
     */
    QString createViewportWithRenderer(const ViewportConfig &config, IRenderer *renderer);
    
    /**
     * @brief 销毁视口
     * @param viewportId 视口 ID
     * @return 是否成功
     */
    bool destroyViewport(const QString &viewportId);
    
    /**
     * @brief 获取视口实例
     * @param viewportId 视口 ID
     * @return 视口实例，未找到返回 nullptr
     */
    ViewportInstance *getViewport(const QString &viewportId);
    
    /**
     * @brief 获取所有视口 ID
     */
    QStringList allViewportIds() const;
    
    /**
     * @brief 获取主视口实例
     */
    ViewportInstance *mainViewport() const;
    
    /**
     * @brief 设置默认图形 API
     * @param api RHI 图形 API 类型
     */
    void setDefaultGraphicsApi(QRhi::Implementation api) { defaultGraphicsApi_ = api; }
    QRhi::Implementation defaultGraphicsApi() const { return defaultGraphicsApi_; }

signals:
    void viewportCreated(const QString &viewportId);
    void viewportDestroyed(const QString &viewportId);

private:
    void createDefaultViewports();
    void destroyAllViewports();
    IRenderer *createRenderer(const ViewportConfig &config);

    PluginContext *context_ = nullptr;
    ISceneService *sceneService_ = nullptr;
    
    // 渲染器工厂
    RendererFactory rendererFactory_;
    
    // 默认图形 API
    QRhi::Implementation defaultGraphicsApi_ = QRhi::D3D12;
    
    // 视口实例管理
    QHash<QString, ViewportInstance *> viewports_;
    QString mainViewportId_;
    
    // 预注册的 Native View Contributions
    QList<NativeViewContribution> registeredContributions_;
};

} // namespace rbc
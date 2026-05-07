#pragma once
#include <rbc_config.h>
#include <QObject>
#include <QHash>
#include <QPointer>
#include <QtGui/rhi/qrhi.h>
#include <functional>
#include "RBCEditorRuntime/services/SceneService.h"
#include "RBCEditorRuntime/mvvm/ViewModelBase.h"
#include "RBCEditorRuntime/plugins/IEditorPlugin.h"
#include "RBCEditorRuntime/infra/render/app_base.h"

namespace rbc {

// Forward declarations
class ViewportWidget;

// ============================================================================
// ViewportType - Viewport type enum
// ============================================================================

enum class ViewportType {
    Main,         // Editor Main Viewport
    Preview,      // Preview Viewport for material/model/result...
    CameraPreview,// Preview for specific camera
    Custom
};

// ============================================================================
// ViewportConfig - Viewport configuration
// ============================================================================

struct ViewportConfig {
    QString viewportId;// uid for this viewport
    ViewportType type = ViewportType::Main;
    QString rendererType;// renderer type (support override)
    bool enableGizmos = true;
    bool enableGrid = true;
    bool enableSelection = true;
    QRhi::Implementation graphicsApi = QRhi::D3D12;// RHI graphics API
};

// ============================================================================
// ViewportViewModel - Viewport state view model
// ============================================================================

/**
 * @brief ViewportViewModel - Viewport state view model
 * 
 * Manages the state of a single viewport:
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
    QString viewportId() const { return _config.viewportId; }
    bool gizmosEnabled() const { return _gizmos_enabled; }
    void setGizmosEnabled(bool enabled);
    bool gridEnabled() const { return _grid_enabled; }
    void setGridEnabled(bool enabled);
    QString cameraMode() const { return _camera_mode; }
    void setCameraMode(const QString &mode);

    // Camera control
    Q_INVOKABLE void focusOnSelection();
    Q_INVOKABLE void resetCamera();
    Q_INVOKABLE void setCameraView(const QString &preset);// Top/Front/Side/Perspective

signals:
    void gizmosEnabledChanged();
    void gridEnabledChanged();
    void cameraModeChanged();

private:
    ViewportConfig _config;
    ISceneService *_scene_service = nullptr;
    bool _gizmos_enabled = true;
    bool _grid_enabled = true;
    QString _camera_mode = "Perspective";
};

// ============================================================================
// ViewportInstance - Viewport instance (combines Widget + ViewModel + Renderer)
// ============================================================================

/**
 * @brief ViewportInstance - Combines Widget, ViewModel, and Renderer
 * 
 * Each viewport instance contains:
 * - config: Viewport configuration
 * - widget: ViewportWidget instance (tracked with QPointer, auto-detects deletion)
 * - viewModel: ViewportViewModel instance
 * - renderer: IRenderer instance
 * 
 * Note: widget uses QPointer because it may be deleted by Qt's parent-child mechanism
 * (e.g., when WindowManager deletes main_window_)
 */
struct ViewportInstance {
    ViewportConfig config;
    QPointer<ViewportWidget> widget;// Tracked with QPointer, auto-detects deletion
    ViewportViewModel *viewModel = nullptr;
    IRenderer *renderer = nullptr;

    ~ViewportInstance() {
        // Note: widget uses QPointer, explicitly managed by destroyViewport or Qt parent-child mechanism
        // Only responsible for cleaning up viewModel and renderer here
        delete viewModel;
        viewModel = nullptr;
        // renderer is usually created by an external factory and needs external destruction
        renderer = nullptr;
    }
};

// ============================================================================
// RendererFactory - Renderer factory function type
// ============================================================================

/**
 * @brief Renderer factory function type
 * 
 * Function type for creating IRenderer instances.
 * ViewportPlugin creates renderers by setting a renderer factory.
 * 
 * @param config Viewport configuration
 * @return IRenderer* Renderer instance, caller manages lifetime
 */
using RendererFactory = std::function<IRenderer *(const ViewportConfig &config)>;

// ============================================================================
// ViewportPlugin - Viewport management plugin
// ============================================================================

/**
 * @brief ViewportPlugin - Viewport management plugin
 * 
 * Responsibilities:
 * 1. Manage all ViewportWidget lifecycles
 * 2. Provide viewport create/destroy API
 * 3. Register viewports with WindowManager via NativeViewContribution
 * 4. Coordinate ViewportViewModel and SceneService interaction
 * 
 * Usage:
 * @code
 * viewportPlugin->setRendererFactory([](const ViewportConfig& config) {
 *     return new MyRenderer();
 * });
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
     * @brief Set renderer factory
     * @param factory Renderer factory function
     * 
     * Must call this to set renderer factory before creating viewports.
     */
    void setRendererFactory(RendererFactory factory) { _renderer_factory = std::move(factory); }

    /**
     * @brief Get renderer factory
     */
    RendererFactory rendererFactory() const { return _renderer_factory; }

    // === Viewport Management API ===

    /**
     * @brief Create a new viewport
     * @param config Viewport configuration
     * @return Viewport ID, empty string on failure
     */
    QString createViewport(const ViewportConfig &config);

    /**
     * @brief Create a viewport with an existing renderer
     * @param config Viewport configuration
     * @param renderer Renderer instance (ViewportPlugin does not take ownership)
     * @return Viewport ID, empty string on failure
     */
    QString createViewportWithRenderer(const ViewportConfig &config, IRenderer *renderer);

    /**
     * @brief Destroy a viewport
     * @param viewportId Viewport ID
     * @return Whether successful
     */
    bool destroyViewport(const QString &viewportId);

    /**
     * @brief Get viewport instance
     * @param viewportId Viewport ID
     * @return Viewport instance, nullptr if not found
     */
    ViewportInstance *getViewport(const QString &viewportId);

    /**
     * @brief Get all viewport IDs
     */
    QStringList allViewportIds() const;

    /**
     * @brief Get main viewport instance
     */
    ViewportInstance *mainViewport() const;

    /**
     * @brief Set default graphics API
     * @param api RHI graphics API type
     */
    void setDefaultGraphicsApi(QRhi::Implementation api) { _default_graphics_api = api; }
    QRhi::Implementation defaultGraphicsApi() const { return _default_graphics_api; }

signals:
    void viewportCreated(const QString &viewportId);
    void viewportDestroyed(const QString &viewportId);

private:
    friend struct EditorEngine;// only supposed to create default viewport with EditorEngine
    void createDefaultViewports();
    void destroyAllViewports();
    IRenderer *createRenderer(const ViewportConfig &config);

    PluginContext *_context = nullptr;
    ISceneService *_scene_service = nullptr;

    // Renderer factory
    RendererFactory _renderer_factory;

    // Default graphics API
    QRhi::Implementation _default_graphics_api = QRhi::D3D12;

    // Viewport instance management
    QHash<QString, ViewportInstance *> _viewports;
    QString _main_viewport_id;

    // Pre-registered Native View Contributions
    QList<NativeViewContribution> _registered_contributions;
};

}// namespace rbc
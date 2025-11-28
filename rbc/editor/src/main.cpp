#include <QApplication>
#include <QFile>

#include "RBCEditor/MainWindow.h"

#include <QCommandLineParser>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QGroupBox>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QDebug>
#include "rhi/qrhi.h"
#include "QtGui/rhi/qrhi_platform.h"
#include "RBCEditor/components/rhiwindow.h"
#include "RBCEditor/dummyrt.h"
#include <luisa/gui/input.h>

// 用来给Qt窗口嵌入自定义Renderer
struct RendererImpl : public rbc::IRenderer {
    QRhi::Implementation backend = QRhi::Implementation::D3D12;
    rbc::App *render_app = nullptr;
    bool is_paused = false;
    void init(QRhiNativeHandles &handles_base) override {
        if (backend == QRhi::D3D12) {
            auto &handles = static_cast<QRhiD3D12NativeHandles &>(handles_base);
            handles.dev = render_app->device.native_handle();
            handles.minimumFeatureLevel = 0;
            handles.adapterLuidHigh = (qint32)render_app->dx_adaptor_luid.x;
            handles.adapterLuidLow = (qint32)render_app->dx_adaptor_luid.y;
            handles.commandQueue = render_app->stream.native_handle();
        } else {
            LUISA_ERROR("Backend unsupported.");
        }
    }
    void update() override {
        if (is_paused) return;
        render_app->update();
    }
    void pause() override { is_paused = true; }
    void resume() override { is_paused = false; }
    void handle_key(luisa::compute::Key key) override { render_app->handle_key(key); }
    uint64_t get_present_texture(luisa::uint2 resolution) override {
        return render_app->create_texture(resolution.x, resolution.y);
    }
};

int main(int argc, char **argv) {
    using namespace rbc;

    QApplication app(argc, argv);
    // Load Style
    QFile f(":/main.qss");
    QString styleSheet = R"(
        /* Global Colors and Fonts */
    )";
    if (f.open(QIODevice::ReadOnly)) {
        styleSheet = f.readAll();
    }
    // Load Graphics Backend

    app.setStyleSheet(styleSheet);
    luisa::compute::Context ctx{argv[0]};
    App render_app;
    luisa::string backend = "dx";
    QRhi::Implementation graphicsApi = QRhi::D3D12;
    render_app.init(ctx, backend.c_str());

    MainWindow window;

    QWidget mainWindow;
    // ============ 核心模块：创建LC-Driven Viewport ===============
    auto *renderWindow = new RhiWindow(graphicsApi);
    RendererImpl renderer_impl;
    renderer_impl.backend = graphicsApi;
    renderer_impl.render_app = &render_app;
    renderWindow->renderer = &renderer_impl;
    renderWindow->workspace_path = luisa::to_string(luisa::filesystem::path(argv[0]).parent_path());// set runtime workspace path

    // ============ 核心模块：创建LC-Driven Viewport ===============
    // 创建自定义容器Widget来转发事件
    auto *renderContainerWrapper = new RHIWindowContainerWidget(renderWindow, &mainWindow);
    renderContainerWrapper->setMinimumSize(800, 600);
    renderContainerWrapper->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Other Layout
    mainWindow.setWindowTitle("LuisaCompute Qt Sample");
    mainWindow.resize(1280, 800);
    // 创建主布局

    auto *mainLayout = new QVBoxLayout(&mainWindow);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    QHBoxLayout *topBarLayout = new QHBoxLayout();
    QLabel *titleLabel = new QLabel("LuisaCompute Real-time Renderer");
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(12);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    topBarLayout->addWidget(titleLabel);
    topBarLayout->addStretch();
    QLabel *statusLabel = new QLabel("Status: Running");
    statusLabel->setStyleSheet("QLabel { color: green; }");
    topBarLayout->addWidget(statusLabel);
    mainLayout->addLayout(topBarLayout);
    // 添加输入事件调试信息显示
    QLabel *inputDebugLabel = new QLabel("Input Events: Click on the render window and press keys (W/A/S/D)");
    inputDebugLabel->setStyleSheet("QLabel { background-color: #f0f0f0; padding: 5px; border: 1px solid #ccc; }");
    inputDebugLabel->setWordWrap(true);
    mainLayout->addWidget(inputDebugLabel);

    // 创建布局并将RhiWindow的容器嵌入其中
    QVBoxLayout *containerLayout = new QVBoxLayout(renderContainerWrapper);
    containerLayout->setContentsMargins(0, 0, 0, 0);

    QWidget *renderContainer = QWidget::createWindowContainer(renderWindow, renderContainerWrapper);
    renderContainer->setFocusPolicy(Qt::NoFocus);// 容器本身不接收焦点
    containerLayout->addWidget(renderContainer);

    // 设置初始焦点
    renderContainerWrapper->setFocus();
    // ============ 核心模块：创建LC-Driven Viewport ===============
    mainLayout->addWidget(renderContainerWrapper);

    // 底部控制面板
    QGroupBox *controlGroup = new QGroupBox("Render Controls");
    QHBoxLayout *controlLayout = new QHBoxLayout(controlGroup);

    // 左侧按钮组
    QVBoxLayout *buttonLayout = new QVBoxLayout();
    QPushButton *startButton = new QPushButton("Start Render");
    startButton->setEnabled(false);// 示例：默认已经在渲染
    QPushButton *pauseButton = new QPushButton("Pause Render");
    QObject::connect(pauseButton, &QPushButton::pressed, [renderWindow]() {
        renderWindow->renderer->pause();
    });
    QPushButton *resumeButton = new QPushButton("Resume Render");
    QObject::connect(resumeButton, &QPushButton::pressed, [renderWindow]() {
        renderWindow->renderer->resume();
    });
    buttonLayout->addWidget(startButton);
    buttonLayout->addWidget(pauseButton);
    buttonLayout->addWidget(resumeButton);
    buttonLayout->addStretch();

    controlLayout->addLayout(buttonLayout);

    // 中间参数控制
    QVBoxLayout *paramLayout = new QVBoxLayout();

    QHBoxLayout *sampleLayout = new QHBoxLayout();
    QLabel *sampleLabel = new QLabel("Samples:");
    QSlider *sampleSlider = new QSlider(Qt::Horizontal);
    sampleSlider->setRange(1, 100);
    sampleSlider->setValue(50);
    QLabel *sampleValueLabel = new QLabel("50");
    sampleLayout->addWidget(sampleLabel);
    sampleLayout->addWidget(sampleSlider);
    sampleLayout->addWidget(sampleValueLabel);

    QHBoxLayout *bounceLayout = new QHBoxLayout();
    QLabel *bounceLabel = new QLabel("Max Bounce:");
    QSlider *bounceSlider = new QSlider(Qt::Horizontal);
    bounceSlider->setRange(1, 10);
    bounceSlider->setValue(5);
    QLabel *bounceValueLabel = new QLabel("5");
    bounceLayout->addWidget(bounceLabel);
    bounceLayout->addWidget(bounceSlider);
    bounceLayout->addWidget(bounceValueLabel);

    paramLayout->addLayout(sampleLayout);
    paramLayout->addLayout(bounceLayout);

    controlLayout->addLayout(paramLayout);

    // 右侧统计信息
    QVBoxLayout *statsLayout = new QVBoxLayout();
    QLabel *fpsLabel = new QLabel("FPS: 60");
    QLabel *frameTimeLabel = new QLabel("Frame Time: 16.6ms");
    QLabel *resolutionLabel = new QLabel("Resolution: 1280x720");
    statsLayout->addWidget(fpsLabel);
    statsLayout->addWidget(frameTimeLabel);
    statsLayout->addWidget(resolutionLabel);
    statsLayout->addStretch();

    controlLayout->addLayout(statsLayout);

    mainLayout->addWidget(controlGroup);

    // 底部状态栏
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    QLabel *apiLabel = new QLabel(QString("API: %1").arg(renderWindow->graphicsApiName()));
    QLabel *deviceLabel = new QLabel("Device: GPU 0");
    bottomLayout->addWidget(apiLabel);
    bottomLayout->addStretch();
    bottomLayout->addWidget(deviceLabel);

    mainLayout->addLayout(bottomLayout);

    // 连接Slider和Label的信号槽
    QObject::connect(sampleSlider, &QSlider::valueChanged, [sampleValueLabel](int value) {
        sampleValueLabel->setText(QString::number(value));
    });
    QObject::connect(bounceSlider, &QSlider::valueChanged, [bounceValueLabel](int value) {
        bounceValueLabel->setText(QString::number(value));
    });
    QObject::connect(renderWindow, &RhiWindow::keyPressed, [inputDebugLabel](const QString &keyInfo) {
        inputDebugLabel->setText(QString("Input Events: %1").arg(keyInfo));
        inputDebugLabel->setStyleSheet("QLabel { background-color: #d4edda; padding: 5px; border: 1px solid #28a745; color: #155724; }");
    });

    QObject::connect(renderWindow, &RhiWindow::mouseClicked, [inputDebugLabel](const QString &mouseInfo) {
        inputDebugLabel->setText(QString("Input Events: %1").arg(mouseInfo));
        inputDebugLabel->setStyleSheet("QLabel { background-color: #d1ecf1; padding: 5px; border: 1px solid #17a2b8; color: #0c5460; }");
    });

    // 显示主窗口
    // mainWindow.show();
    window.setupUi(&mainWindow);
    window.show();
    int ret = QApplication::exec();
    if (renderWindow->handle())
        renderWindow->releaseSwapChain();
    return ret;
}

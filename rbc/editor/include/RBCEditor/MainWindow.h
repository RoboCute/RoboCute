#pragma once
#include <QMainWindow>

namespace rbc {
class HttpClient;
class SceneSyncManager;
class SceneHierarchyWidget;
class DetailsPanel;
class ViewportWidget;
}

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    void setupUi();
    ~MainWindow();

    // Access to HTTP client for scene sync
    rbc::HttpClient *httpClient() { return httpClient_; }
    rbc::SceneSyncManager *sceneSyncManager() { return sceneSyncManager_; }

    // Start scene synchronization
    void startSceneSync(const QString &serverUrl = "http://127.0.0.1:5555");

private:
    void setupMenuBar();
    void setupToolBar();
    void setupDocks();

private slots:
    void onSceneUpdated();
    void onConnectionStatusChanged(bool connected);
    void onEntitySelected(int entityId);

private:
    rbc::HttpClient *httpClient_;
    rbc::SceneSyncManager *sceneSyncManager_;
    rbc::SceneHierarchyWidget *sceneHierarchy_;
    rbc::DetailsPanel *detailsPanel_;
    rbc::ViewportWidget *viewportWidget_;
};

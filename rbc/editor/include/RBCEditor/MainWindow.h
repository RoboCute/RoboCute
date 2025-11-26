#pragma once
#include <QMainWindow>
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void setupUi();
    void setupMenuBar();
    void setupToolBar();
    void setupDocks();
};

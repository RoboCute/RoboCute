#pragma once
#include <QMainWindow>
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    void setupUi(QWidget *central_viewport);
    ~MainWindow();

private:

    void setupMenuBar();
    void setupToolBar();
    void setupDocks();
};

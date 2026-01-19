#pragma once

#include <QWidget>
#include <QTextEdit>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QTabWidget>
#include <QJsonObject>
#include <QPushButton>
#include <QLabel>

namespace rbc {

class ExecutionPanel : public QWidget {
    Q_OBJECT
public:
    explicit ExecutionPanel(QWidget *parent = nullptr);

    // console logging
    void logMessage(const QString &message);
    void logError(const QString &error);
    void logSuccess(const QString &message);
    void clearConsole();

    // Results display
    void displayExecutionResults(const QJsonObject &results);
    void clearResults();

    // Execution status
    void setExecutionStatus(const QString &status, bool isRunning);

signals:
    void cancelExecutionRequested();

private:
    void setupUI();
    // void addTreeItem(QTreeWidgetItem *parent, const QString &key, const QJsonValue &value);

    QLabel *m_statusLabel;
    QTabWidget *m_tabWidget;
    QTextEdit *m_consoleOutput;
    QTreeWidget *m_resultsTree;
    QPushButton *m_cancelButton;
};

}// namespace rbc
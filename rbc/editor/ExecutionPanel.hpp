#pragma once

#include <QWidget>
#include <QTextEdit>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QTabWidget>
#include <QJsonObject>
#include <QPushButton>
#include <QLabel>

/**
 * Panel for displaying execution results and console output
 */
class ExecutionPanel : public QWidget {
    Q_OBJECT

public:
    explicit ExecutionPanel(QWidget *parent = nullptr);

    // Console logging
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
    void addTreeItem(QTreeWidgetItem *parent, const QString &key, const QJsonValue &value);

    QTabWidget *m_tabWidget;

    // Console tab
    QTextEdit *m_consoleOutput;

    // Results tab
    QTreeWidget *m_resultsTree;

    // Status widgets
    QLabel *m_statusLabel;
    QPushButton *m_cancelButton;
};

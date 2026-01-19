#include "RBCEditorRuntime/ui/ExecutionPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>

namespace rbc {

ExecutionPanel::ExecutionPanel(QWidget *parent) : QWidget(parent) {
    setupUI();
}

void ExecutionPanel::setupUI() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Status bar
    auto statusWidget = new QWidget();
    auto statusLayout = new QHBoxLayout(statusWidget);
    statusLayout->setContentsMargins(5, 5, 5, 5);

    m_statusLabel = new QLabel("Ready");
    m_statusLabel->setStyleSheet("QLabel { font-weight: bold; }");
    statusLayout->addWidget(m_statusLabel);

    statusLayout->addStretch();

    m_cancelButton = new QPushButton("Cancel");
    m_cancelButton->setEnabled(false);
    connect(m_cancelButton, &QPushButton::clicked, this, &ExecutionPanel::cancelExecutionRequested);
    statusLayout->addWidget(m_cancelButton);

    mainLayout->addWidget(statusWidget);

    // Tab widget for console and results
    m_tabWidget = new QTabWidget();

    // Console tab
    m_consoleOutput = new QTextEdit();
    m_consoleOutput->setReadOnly(true);
    m_consoleOutput->setStyleSheet("QTextEdit { font-family: Consolas, Monaco, monospace; }");
    m_tabWidget->addTab(m_consoleOutput, "Console");

    // Results tab
    m_resultsTree = new QTreeWidget();
    m_resultsTree->setHeaderLabels({"Property", "Value"});
    m_resultsTree->setAlternatingRowColors(true);
    m_resultsTree->header()->setStretchLastSection(true);
    m_resultsTree->setColumnWidth(0, 200);
    m_tabWidget->addTab(m_resultsTree, "Results");

    mainLayout->addWidget(m_tabWidget);

    setLayout(mainLayout);
}

void ExecutionPanel::logMessage(const QString &message) {
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    m_consoleOutput->append(QString("[%1] %2").arg(timestamp, message));
}

void ExecutionPanel::logError(const QString &error) {
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    m_consoleOutput->append(QString("<span style='color: red;'>[%1] ERROR: %2</span>").arg(timestamp, error));
}

void ExecutionPanel::logSuccess(const QString &message) {
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    m_consoleOutput->append(QString("<span style='color: green;'>[%1] %2</span>").arg(timestamp, message));
}

void ExecutionPanel::clearConsole() {
    m_consoleOutput->clear();
}

void ExecutionPanel::displayExecutionResults(const QJsonObject &results) {
    m_resultsTree->clear();

    // Add overall status
}

void ExecutionPanel::clearResults() {
    m_resultsTree->clear();
}

void ExecutionPanel::setExecutionStatus(const QString &status, bool isRunning) {
    m_statusLabel->setText(status);
    m_cancelButton->setEnabled(isRunning);

    if (isRunning) {
        m_statusLabel->setStyleSheet("QLabel { font-weight: bold; color: orange; }");
    } else if (status.contains("Success") || status.contains("Completed")) {
        m_statusLabel->setStyleSheet("QLabel { font-weight: bold; color: green; }");
    } else if (status.contains("Error") || status.contains("Failed")) {
        m_statusLabel->setStyleSheet("QLabel { font-weight: bold; color: red; }");
    } else {
        m_statusLabel->setStyleSheet("QLabel { font-weight: bold;}");
    }
}

}// namespace rbc
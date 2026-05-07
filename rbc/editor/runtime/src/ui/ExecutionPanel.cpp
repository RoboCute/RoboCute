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

    _status_label = new QLabel("Ready");
    _status_label->setStyleSheet("QLabel { font-weight: bold; }");
    statusLayout->addWidget(_status_label);

    statusLayout->addStretch();

    _cancel_button = new QPushButton("Cancel");
    _cancel_button->setEnabled(false);
    connect(_cancel_button, &QPushButton::clicked, this, &ExecutionPanel::cancelExecutionRequested);
    statusLayout->addWidget(_cancel_button);

    mainLayout->addWidget(statusWidget);

    // Tab widget for console and results
    _tab_widget = new QTabWidget();

    // Console tab
    _console_output = new QTextEdit();
    _console_output->setReadOnly(true);
    _console_output->setStyleSheet("QTextEdit { font-family: Consolas, Monaco, monospace; }");
    _tab_widget->addTab(_console_output, "Console");

    // Results tab
    _results_tree = new QTreeWidget();
    _results_tree->setHeaderLabels({"Property", "Value"});
    _results_tree->setAlternatingRowColors(true);
    _results_tree->header()->setStretchLastSection(true);
    _results_tree->setColumnWidth(0, 200);
    _tab_widget->addTab(_results_tree, "Results");

    mainLayout->addWidget(_tab_widget);

    setLayout(mainLayout);
}

void ExecutionPanel::logMessage(const QString &message) {
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    _console_output->append(QString("[%1] %2").arg(timestamp, message));
}

void ExecutionPanel::logError(const QString &error) {
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    _console_output->append(QString("<span style='color: red;'>[%1] ERROR: %2</span>").arg(timestamp, error));
}

void ExecutionPanel::logSuccess(const QString &message) {
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    _console_output->append(QString("<span style='color: green;'>[%1] %2</span>").arg(timestamp, message));
}

void ExecutionPanel::clearConsole() {
    _console_output->clear();
}

void ExecutionPanel::displayExecutionResults(const QJsonObject &results) {
    _results_tree->clear();

    // Add overall status
}

void ExecutionPanel::clearResults() {
    _results_tree->clear();
}

void ExecutionPanel::setExecutionStatus(const QString &status, bool isRunning) {
    _status_label->setText(status);
    _cancel_button->setEnabled(isRunning);

    if (isRunning) {
        _status_label->setStyleSheet("QLabel { font-weight: bold; color: orange; }");
    } else if (status.contains("Success") || status.contains("Completed")) {
        _status_label->setStyleSheet("QLabel { font-weight: bold; color: green; }");
    } else if (status.contains("Error") || status.contains("Failed")) {
        _status_label->setStyleSheet("QLabel { font-weight: bold; color: red; }");
    } else {
        _status_label->setStyleSheet("QLabel { font-weight: bold;}");
    }
}

}// namespace rbc
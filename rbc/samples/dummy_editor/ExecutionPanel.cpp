#include "ExecutionPanel.hpp"
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QHBoxLayout>
#include <QHeaderView>

ExecutionPanel::ExecutionPanel(QWidget *parent)
    : QWidget(parent) {
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
    if (results.contains("status")) {
        auto statusItem = new QTreeWidgetItem(m_resultsTree);
        statusItem->setText(0, "Status");
        statusItem->setText(1, results["status"].toString());
        statusItem->setExpanded(true);
    }

    // Add graph_id
    if (results.contains("graph_id")) {
        auto idItem = new QTreeWidgetItem(m_resultsTree);
        idItem->setText(0, "Execution ID");
        idItem->setText(1, results["graph_id"].toString());
    }

    // Add outputs
    if (results.contains("outputs")) {
        auto outputsItem = new QTreeWidgetItem(m_resultsTree);
        outputsItem->setText(0, "Node Outputs");
        outputsItem->setExpanded(true);

        QJsonObject outputs = results["outputs"].toObject();
        for (auto it = outputs.begin(); it != outputs.end(); ++it) {
            auto nodeItem = new QTreeWidgetItem(outputsItem);
            nodeItem->setText(0, it.key());
            nodeItem->setExpanded(true);

            addTreeItem(nodeItem, "", it.value());
        }
    }

    // Switch to results tab
    m_tabWidget->setCurrentIndex(1);
}

void ExecutionPanel::addTreeItem(QTreeWidgetItem *parent, const QString &key, const QJsonValue &value) {
    if (value.isObject()) {
        QJsonObject obj = value.toObject();
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            auto item = new QTreeWidgetItem(parent);
            item->setText(0, it.key());

            if (it.value().isObject() || it.value().isArray()) {
                addTreeItem(item, it.key(), it.value());
            } else {
                item->setText(1, it.value().toVariant().toString());
            }
        }
    } else if (value.isArray()) {
        QJsonArray arr = value.toArray();
        for (int i = 0; i < arr.size(); ++i) {
            auto item = new QTreeWidgetItem(parent);
            item->setText(0, QString("[%1]").arg(i));

            if (arr[i].isObject() || arr[i].isArray()) {
                addTreeItem(item, QString::number(i), arr[i]);
            } else {
                item->setText(1, arr[i].toVariant().toString());
            }
        }
    } else {
        parent->setText(1, value.toVariant().toString());
    }
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
        m_statusLabel->setStyleSheet("QLabel { font-weight: bold; }");
    }
}

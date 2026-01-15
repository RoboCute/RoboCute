#include "RBCEditorRuntime/services/StyleManager.h"
#include <QApplication>
#include <QFile>
#include <QDebug>
#include <QDir>
#include <QUrl>

namespace rbc {

StyleManager::StyleManager(QObject *parent)
    : IStyleManager(parent)
    , engine_(nullptr)
    , watcher_(nullptr)
    , currentTheme_("dark") {
    loadDefaultPresets();
}

void StyleManager::initialize(int argc, char **argv) {
    Q_UNUSED(argc);
    Q_UNUSED(argv);
    
    // Load default dark theme
    loadGlobalStyleSheet(":/main.qss");
    setTheme("dark");
}

QUrl StyleManager::resolveUrl(const QString &relativePath) {
    return relativePath;
}

bool StyleManager::loadGlobalStyleSheet(const QString &qssPath) {
    QFile file(qssPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "StyleManager::loadGlobalStyleSheet: Failed to open" << qssPath;
        return false;
    }

    QString styleSheet = QString::fromUtf8(file.readAll());
    file.close();

    // Apply to QApplication
    QApplication *app = qobject_cast<QApplication *>(QApplication::instance());
    if (app) {
        app->setStyleSheet(styleSheet);
        globalStyleSheet_ = styleSheet;
        qDebug() << "StyleManager::loadGlobalStyleSheet: Loaded and applied" << qssPath;
        return true;
    }

    qWarning() << "StyleManager::loadGlobalStyleSheet: QApplication instance not found";
    return false;
}

bool StyleManager::applyStylePreset(QWidget *widget, const QString &presetName) {
    if (!widget) {
        qWarning() << "StyleManager::applyStylePreset: widget is null";
        return false;
    }

    QString qss = getStylePreset(presetName);
    if (qss.isEmpty()) {
        qWarning() << "StyleManager::applyStylePreset: Preset" << presetName << "not found";
        return false;
    }

    widget->setStyleSheet(qss);
    qDebug() << "StyleManager::applyStylePreset: Applied preset" << presetName << "to" << widget->metaObject()->className();
    return true;
}

QString StyleManager::getStylePreset(const QString &presetName) const {
    return stylePresets_.value(presetName, QString());
}

void StyleManager::registerStylePreset(const QString &presetName, const QString &qss) {
    stylePresets_[presetName] = qss;
    qDebug() << "StyleManager::registerStylePreset: Registered preset" << presetName;
}

void StyleManager::setTheme(const QString &themeName) {
    if (currentTheme_ == themeName) {
        return;
    }

    currentTheme_ = themeName;
    loadTheme(themeName);
    emit themeChanged(themeName);
    qDebug() << "StyleManager::setTheme: Changed to" << themeName;
}

void StyleManager::loadTheme(const QString &themeName) {
    // Load theme-specific stylesheet if exists
    QString themePath = QString(":/styles/%1.qss").arg(themeName);
    QFile file(themePath);
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QString styleSheet = QString::fromUtf8(file.readAll());
        file.close();
        
        QApplication *app = qobject_cast<QApplication *>(QApplication::instance());
        if (app) {
            app->setStyleSheet(styleSheet);
            globalStyleSheet_ = styleSheet;
            qDebug() << "StyleManager::loadTheme: Loaded theme" << themeName;
        }
    } else {
        // Fallback to default dark theme
        if (themeName == "dark") {
            loadGlobalStyleSheet(":/main.qss");
        }
    }
}

void StyleManager::loadDefaultPresets() {
    // FileTree preset - matches main.qss QTreeWidget style
    registerStylePreset("FileTree", R"(
        QTreeView {
            background-color: #1e1e1e;
            border: 1px solid #3e3e42;
            outline: none;
            alternate-background-color: #252526;
        }
        QTreeView::item {
            padding: 4px;
            border: none;
        }
        QTreeView::item:selected {
            background-color: #094771;
            color: white;
        }
        QTreeView::item:hover {
            background-color: #3e3e42;
        }
        QTreeView::branch {
            background-color: #1e1e1e;
        }
        QTreeView::branch:selected {
            background-color: #094771;
        }
        QHeaderView::section {
            background-color: #252526;
            border: none;
            padding: 4px;
            border-bottom: 1px solid #3e3e42;
        }
    )");

    // StatusLabel preset
    registerStylePreset("StatusLabel", R"(
        QLabel {
            color: #cccccc;
            font-weight: bold;
            padding: 5px;
        }
    )");

    // StatusLabel - Success
    registerStylePreset("StatusLabel.Success", R"(
        QLabel {
            color: #4ec9b0;
            font-weight: bold;
            padding: 5px;
        }
    )");

    // StatusLabel - Warning
    registerStylePreset("StatusLabel.Warning", R"(
        QLabel {
            color: #dcdcaa;
            font-weight: bold;
            padding: 5px;
        }
    )");

    // StatusLabel - Error
    registerStylePreset("StatusLabel.Error", R"(
        QLabel {
            color: #f48771;
            font-weight: bold;
            padding: 5px;
        }
    )");

    // StatusIndicator preset
    registerStylePreset("StatusIndicator", R"(
        QLabel {
            background-color: gray;
            border-radius: 8px;
            min-width: 16px;
            max-width: 16px;
            min-height: 16px;
            max-height: 16px;
        }
    )");

    // StatusIndicator - Success
    registerStylePreset("StatusIndicator.Success", R"(
        QLabel {
            background-color: green;
            border-radius: 8px;
            min-width: 16px;
            max-width: 16px;
            min-height: 16px;
            max-height: 16px;
        }
    )");

    // StatusIndicator - Warning
    registerStylePreset("StatusIndicator.Warning", R"(
        QLabel {
            background-color: yellow;
            border-radius: 8px;
            min-width: 16px;
            max-width: 16px;
            min-height: 16px;
            max-height: 16px;
        }
    )");

    // StatusIndicator - Error
    registerStylePreset("StatusIndicator.Error", R"(
        QLabel {
            background-color: red;
            border-radius: 8px;
            min-width: 16px;
            max-width: 16px;
            min-height: 16px;
            max-height: 16px;
        }
    )");

    // ConsoleOutput preset
    registerStylePreset("ConsoleOutput", R"(
        QTextEdit {
            font-family: Consolas, Monaco, monospace;
            background-color: #1e1e1e;
            color: #cccccc;
            border: 1px solid #3e3e42;
        }
    )");

    // InfoLabel preset
    registerStylePreset("InfoLabel", R"(
        QLabel {
            color: #666666;
            font-style: italic;
        }
    )");

    // Button preset
    registerStylePreset("Button", R"(
        QPushButton {
            background-color: #3e3e42;
            border: 1px solid #555555;
            border-radius: 3px;
            padding: 6px 12px;
            color: #cccccc;
        }
        QPushButton:hover {
            background-color: #4e4e52;
            border: 1px solid #007acc;
        }
        QPushButton:pressed {
            background-color: #007acc;
        }
        QPushButton:disabled {
            background-color: #2b2b2b;
            color: #666666;
        }
    )");

    // ListWidget preset
    registerStylePreset("ListWidget", R"(
        QListWidget {
            background-color: #1e1e1e;
            border: 1px solid #3e3e42;
            outline: none;
            font-size: 11pt;
        }
        QListWidget::item {
            padding: 4px;
        }
        QListWidget::item:selected {
            background-color: #094771;
            color: white;
        }
        QListWidget::item:hover {
            background-color: #3e3e42;
        }
    )");
}

}// namespace rbc
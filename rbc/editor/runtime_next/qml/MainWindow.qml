import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import RBCEditor 1.0

ApplicationWindow {
    id: mainWindow
    width: 1280
    height: 800
    visible: true
    title: "RoboCute Editor (QML)"

    // 编辑器服务
    EditorService {
        id: editorService
        onInitialized: {
            console.log("Editor initialized")
        }
    }

    // 连接服务
    ConnectionService {
        id: connectionService
        serverUrl: "http://127.0.0.1:5555"
        onConnectedChanged: {
            console.log("Connection status changed:", connected)
        }
    }

    // 主布局
    RowLayout {
        anchors.fill: parent
        spacing: 0

        // 左侧面板区域
        ColumnLayout {
            Layout.preferredWidth: 300
            Layout.fillHeight: true
            spacing: 0

            // 连接状态面板（迁移示例）
            ConnectionStatusPanel {
                Layout.fillWidth: true
                Layout.preferredHeight: 200
                connectionService: connectionService
            }

            // 其他面板可以在这里添加
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#2b2b2b"
                Text {
                    anchors.centerIn: parent
                    text: "Other Panels\n(To be implemented)"
                    color: "#888888"
                    font.pixelSize: 14
                }
            }
        }

        // 分隔线
        Rectangle {
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            color: "#404040"
        }

        // 中央内容区域
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#1e1e1e"
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 10

                Text {
                    text: "RoboCute Editor"
                    font.pixelSize: 24
                    font.bold: true
                    color: "#ffffff"
                }

                Text {
                    text: "QML Version - Hot Reload Enabled"
                    font.pixelSize: 14
                    color: "#888888"
                }

                Text {
                    Layout.topMargin: 20
                    text: "Version: " + editorService.version
                    font.pixelSize: 12
                    color: "#666666"
                }

                Text {
                    text: "Connection Status: " + (connectionService.connected ? "Connected" : "Disconnected")
                    font.pixelSize: 12
                    color: connectionService.connected ? "#4CAF50" : "#F44336"
                }
            }
        }
    }

    // 初始化编辑器
    Component.onCompleted: {
        editorService.initialize()
        connectionService.connect()
    }

    // 关闭时清理
    Component.onDestruction: {
        editorService.shutdown()
        connectionService.disconnect()
    }
}


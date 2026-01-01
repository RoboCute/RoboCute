import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import RBCEditor 1.0

/**
 * ConnectionStatusPanel - 连接状态面板
 * 
 * 这是从 Widget 版本迁移到 QML 的示例组件
 * 原始组件：rbc/editor/runtime/src/components/ConnectionStatusView.cpp
 * 
 * QML 版本的优势：
 * - 支持热重载（修改后 Ctrl+R 即可看到效果）
 * - 更简洁的代码
 * - 更好的动画支持
 * - 可以在 Qt Creator 中直接预览和调试
 */
Rectangle {
    id: root
    color: "#2b2b2b"
    border.color: "#404040"
    border.width: 1

    property ConnectionService connectionService: null

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        // 标题
        Text {
            text: "Connection Status"
            font.pixelSize: 14
            font.bold: true
            color: "#ffffff"
        }

        // 状态指示器
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            // 状态点
            Rectangle {
                id: statusIndicator
                width: 12
                height: 12
                radius: 6
                color: connectionService && connectionService.connected ? "#4CAF50" : "#F44336"
                
                // 连接动画
                SequentialAnimation on opacity {
                    running: connectionService && connectionService.connected
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.5; duration: 800 }
                    NumberAnimation { to: 1.0; duration: 800 }
                }
            }

            // 状态文本
            Text {
                Layout.fillWidth: true
                text: connectionService ? connectionService.statusText : "Unknown"
                font.pixelSize: 12
                color: connectionService && connectionService.connected ? "#4CAF50" : "#F44336"
            }
        }

        // 服务器 URL 输入
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Text {
                text: "Server URL:"
                font.pixelSize: 11
                color: "#aaaaaa"
            }

            TextField {
                id: urlInput
                Layout.fillWidth: true
                text: connectionService ? connectionService.serverUrl : ""
                placeholderText: "http://127.0.0.1:5555"
                selectByMouse: true
                background: Rectangle {
                    color: "#1e1e1e"
                    border.color: "#404040"
                    border.width: 1
                    radius: 2
                }
                color: "#ffffff"
                font.pixelSize: 11

                onTextChanged: {
                    if (connectionService) {
                        connectionService.serverUrl = text
                    }
                }
            }
        }

        // 按钮组
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                Layout.fillWidth: true
                text: "Test"
                enabled: connectionService && urlInput.text.length > 0
                
                background: Rectangle {
                    color: parent.pressed ? "#3a3a3a" : (parent.hovered ? "#353535" : "#2b2b2b")
                    border.color: "#404040"
                    border.width: 1
                    radius: 2
                }
                contentItem: Text {
                    text: parent.text
                    color: parent.enabled ? "#ffffff" : "#666666"
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: {
                    if (connectionService) {
                        connectionService.testConnection()
                    }
                }
            }

            Button {
                Layout.fillWidth: true
                text: connectionService && connectionService.connected ? "Disconnect" : "Connect"
                enabled: connectionService && urlInput.text.length > 0
                
                background: Rectangle {
                    color: parent.pressed ? "#3a3a3a" : (parent.hovered ? "#353535" : "#2b2b2b")
                    border.color: "#404040"
                    border.width: 1
                    radius: 2
                }
                contentItem: Text {
                    text: parent.text
                    color: parent.enabled ? "#ffffff" : "#666666"
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: {
                    if (connectionService) {
                        if (connectionService.connected) {
                            connectionService.disconnect()
                        } else {
                            connectionService.connect()
                        }
                    }
                }
            }
        }

        // 连接测试结果提示
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            visible: false // 可以通过信号显示测试结果
            color: "#1e1e1e"
            radius: 2
            
            Text {
                anchors.centerIn: parent
                text: "Test completed"
                font.pixelSize: 10
                color: "#888888"
            }
        }
    }
}


import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import RoboCute.Connection 1.0

/**
 * ConnectionView - 连接状态视图
 * 
 * 这是基于 MVVM 架构的连接状态面板
 * ViewModel 由 C++ 注入，QML 只负责 UI 展示
 */
Item {
    id: root

    // ViewModel 从上下文属性读取（由 C++ 通过 setContextProperty 注入）
    // 直接使用上下文属性 viewModel，无需声明

    Rectangle {
        id: container
        anchors.fill: parent
        color: "#2b2b2b"
        border.color: "#404040"
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 12

            // 标题
            Text {
                text: "Connection Status"
                font.pixelSize: 14
                font.bold: true
                color: '#b3ff00'
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
                    color: viewModel && viewModel.connected ? "#4CAF50" : "#F44336"
                    
                    // 连接动画
                    SequentialAnimation on opacity {
                        running: viewModel && viewModel.connected
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.5; duration: 800 }
                        NumberAnimation { to: 1.0; duration: 800 }
                    }
                }

                // 状态文本
                Text {
                    Layout.fillWidth: true
                    text: viewModel ? viewModel.statusText : "Unknown"
                    font.pixelSize: 12
                    color: viewModel && viewModel.connected ? "#4CAF50" : "#F44336"
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
                    text: viewModel ? viewModel.serverUrl : ""
                    placeholderText: "http://127.0.0.1:5555"
                    selectByMouse: true
                    enabled: !viewModel || !viewModel.isBusy
                    background: Rectangle {
                        color: "#1e1e1e"
                        border.color: "#404040"
                        border.width: 1
                        radius: 2
                    }
                    color: "#ffffff"
                    font.pixelSize: 11

                    onTextChanged: {
                        if (viewModel && text !== viewModel.serverUrl) {
                            viewModel.serverUrl = text
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
                    enabled: viewModel && urlInput.text.length > 0 && !viewModel.isBusy
                    
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
                        if (viewModel) {
                            viewModel.testConnection()
                        }
                    }
                }

                Button {
                    Layout.fillWidth: true
                    text: viewModel && viewModel.connected ? "Disconnect" : "Connect"
                    enabled: viewModel && urlInput.text.length > 0 && !viewModel.isBusy
                    
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
                        if (viewModel) {
                            if (viewModel.connected) {
                                viewModel.disconnect()
                            } else {
                                viewModel.connect()
                            }
                        }
                    }
                }
            }

            // 加载指示器
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 20
                visible: viewModel && viewModel.isBusy

                Text {
                    anchors.centerIn: parent
                    text: "Connecting..."
                    font.pixelSize: 10
                    color: "#888888"
                }
            }

            // 错误信息
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                visible: viewModel && viewModel.errorMessage.length > 0
                color: "#1e1e1e"
                radius: 2
                border.color: "#F44336"
                border.width: 1
                
                Text {
                    anchors.centerIn: parent
                    anchors.margins: 4
                    text: viewModel ? viewModel.errorMessage : ""
                    font.pixelSize: 10
                    color: "#F44336"
                    wrapMode: Text.Wrap
                }
            }
        }
    }
}


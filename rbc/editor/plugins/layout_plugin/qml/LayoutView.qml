import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import RoboCute.Layout 1.0

/**
 * LayoutView - Layout 管理视图
 * 
 * 提供 Layout 切换和视图可见性控制的 UI 面板
 * ViewModel 由 C++ 注入
 */
Item {
    id: root

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
                text: "Layout Manager"
                font.pixelSize: 14
                font.bold: true
                color: "#ffff00"
            }

            // === Layout Selection Section ===
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Text {
                    text: "Available Layouts"
                    font.pixelSize: 12
                    font.bold: true
                    color: "#cccccc"
                }

                // Layout List
                Repeater {
                    model: viewModel ? viewModel.availableLayouts : []

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 50
                        color: viewModel && viewModel.currentLayoutId === modelData ? "#3a5a3a" : "#1e1e1e"
                        border.color: viewModel && viewModel.currentLayoutId === modelData ? "#4CAF50" : "#404040"
                        border.width: 1
                        radius: 4

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onEntered: parent.color = viewModel && viewModel.currentLayoutId === modelData ? "#4a6a4a" : "#2a2a2a"
                            onExited: parent.color = viewModel && viewModel.currentLayoutId === modelData ? "#3a5a3a" : "#1e1e1e"
                            onClicked: {
                                if (viewModel) {
                                    viewModel.switchLayout(modelData)
                                }
                            }
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 2

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                // Active indicator
                                Rectangle {
                                    width: 8
                                    height: 8
                                    radius: 4
                                    color: viewModel && viewModel.currentLayoutId === modelData ? "#4CAF50" : "transparent"
                                    border.color: "#4CAF50"
                                    border.width: 1
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: viewModel ? viewModel.getLayoutName(modelData) : modelData
                                    font.pixelSize: 12
                                    font.bold: true
                                    color: "#ffffff"
                                    elide: Text.ElideRight
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: viewModel ? viewModel.getLayoutDescription(modelData) : ""
                                font.pixelSize: 10
                                color: "#888888"
                                elide: Text.ElideRight
                                wrapMode: Text.NoWrap
                            }
                        }
                    }
                }
            }

            // Separator
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: "#404040"
            }

            // === View Visibility Section ===
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 6

                Text {
                    text: "Panel Visibility"
                    font.pixelSize: 12
                    font.bold: true
                    color: "#cccccc"
                }

                // View List
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    ColumnLayout {
                        width: parent.width
                        spacing: 4

                        Repeater {
                            model: viewModel ? viewModel.viewStates : []

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 32
                                color: "#1e1e1e"
                                border.color: "#404040"
                                border.width: 1
                                radius: 2

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    spacing: 8

                                    // Visibility checkbox
                                    CheckBox {
                                        id: visibilityCheck
                                        checked: modelData.visible
                                        onCheckedChanged: {
                                            if (viewModel && checked !== modelData.visible) {
                                                viewModel.setViewVisible(modelData.viewId, checked)
                                            }
                                        }

                                        indicator: Rectangle {
                                            implicitWidth: 16
                                            implicitHeight: 16
                                            radius: 2
                                            border.color: "#606060"
                                            border.width: 1
                                            color: visibilityCheck.checked ? "#4CAF50" : "#2b2b2b"

                                            Text {
                                                anchors.centerIn: parent
                                                text: "\u2713"
                                                color: "#ffffff"
                                                font.pixelSize: 12
                                                visible: visibilityCheck.checked
                                            }
                                        }
                                    }

                                    // View name
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.displayName
                                        font.pixelSize: 11
                                        color: modelData.visible ? "#ffffff" : "#666666"
                                        elide: Text.ElideRight
                                    }

                                    // Show/Hide button
                                    Button {
                                        implicitWidth: 50
                                        implicitHeight: 24
                                        text: modelData.visible ? "Hide" : "Show"

                                        background: Rectangle {
                                            color: parent.pressed ? "#3a3a3a" : (parent.hovered ? "#353535" : "#2b2b2b")
                                            border.color: "#404040"
                                            border.width: 1
                                            radius: 2
                                        }
                                        contentItem: Text {
                                            text: parent.text
                                            color: "#ffffff"
                                            font.pixelSize: 10
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }

                                        onClicked: {
                                            if (viewModel) {
                                                viewModel.setViewVisible(modelData.viewId, !modelData.visible)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // === Actions Section ===
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Button {
                    Layout.fillWidth: true
                    text: "Reset Layout"

                    background: Rectangle {
                        color: parent.pressed ? "#3a3a3a" : (parent.hovered ? "#353535" : "#2b2b2b")
                        border.color: "#404040"
                        border.width: 1
                        radius: 2
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#ffffff"
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        if (viewModel) {
                            var currentId = viewModel.currentLayoutId
                            if (currentId.length > 0) {
                                viewModel.switchLayout(currentId)
                            }
                        }
                    }
                }
            }
        }
    }
}

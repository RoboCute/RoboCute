import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import RoboCute.ProjectPreviewer 1.0

/**
 * ProjectPreviewerView - 项目预览视图
 * 
 * 使用 QFileSystemModel 显示项目文件系统结构
 * 支持过滤、展开/折叠、双击打开文件等操作
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
            anchors.margins: 0
            spacing: 0

            // 标题栏
            Rectangle {
                id: headerBar
                Layout.fillWidth: true
                Layout.preferredHeight: 32
                color: "#1e1e1e"
                border.color: "#404040"
                border.width: 0
                border.bottomWidth: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 8

                    Text {
                        text: "Project Previewer"
                        font.pixelSize: 12
                        font.bold: true
                        color: "#ffffff"
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    // 返回上一级按钮
                    Button {
                        Layout.preferredWidth: 24
                        Layout.preferredHeight: 24
                        visible: viewModel && viewModel.canNavigateUp()
                        tooltip: "Navigate Up"
                        background: Rectangle {
                            color: parent.pressed ? "#3a3a3a" : (parent.hovered ? "#353535" : "transparent")
                            radius: 2
                        }
                        contentItem: Text {
                            text: "↑"
                            color: "#ffffff"
                            font.pixelSize: 14
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            if (viewModel) {
                                viewModel.navigateUp()
                            }
                        }
                    }

                    // 刷新按钮
                    Button {
                        Layout.preferredWidth: 24
                        Layout.preferredHeight: 24
                        tooltip: "Refresh"
                        background: Rectangle {
                            color: parent.pressed ? "#3a3a3a" : (parent.hovered ? "#353535" : "transparent")
                            radius: 2
                        }
                        contentItem: Text {
                            text: "↻"
                            color: "#ffffff"
                            font.pixelSize: 14
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            if (viewModel) {
                                viewModel.refresh()
                            }
                        }
                    }
                }
            }

            // 过滤栏
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 32
                color: "#252525"
                border.color: "#404040"
                border.width: 0
                border.bottomWidth: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 8

                    Text {
                        text: "Filter:"
                        font.pixelSize: 10
                        color: "#aaaaaa"
                    }

                    TextField {
                        id: filterInput
                        Layout.fillWidth: true
                        text: viewModel ? viewModel.filter : "*"
                        placeholderText: "* (all files)"
                        selectByMouse: true
                        background: Rectangle {
                            color: "#1e1e1e"
                            border.color: "#404040"
                            border.width: 1
                            radius: 2
                        }
                        color: "#ffffff"
                        font.pixelSize: 10

                        onTextChanged: {
                            if (viewModel && text !== viewModel.filter) {
                                viewModel.filter = text
                            }
                        }
                    }

                    // 快速过滤按钮
                    Row {
                        spacing: 4
                        Button {
                            text: "All"
                            width: 40
                            height: 20
                            background: Rectangle {
                                color: parent.pressed ? "#3a3a3a" : (parent.hovered ? "#353535" : "#2b2b2b")
                                border.color: "#404040"
                                border.width: 1
                                radius: 2
                            }
                            contentItem: Text {
                                text: parent.text
                                color: "#ffffff"
                                font.pixelSize: 9
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: filterInput.text = "*"
                        }
                        Button {
                            text: ".rbc"
                            width: 40
                            height: 20
                            background: Rectangle {
                                color: parent.pressed ? "#3a3a3a" : (parent.hovered ? "#353535" : "#2b2b2b")
                                border.color: "#404040"
                                border.width: 1
                                radius: 2
                            }
                            contentItem: Text {
                                text: parent.text
                                color: "#ffffff"
                                font.pixelSize: 9
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: filterInput.text = "*.rbcgraph,*.rbcscene"
                        }
                    }
                }
            }

            // 项目路径显示
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 24
                color: "#1e1e1e"
                visible: viewModel && viewModel.projectRoot.length > 0

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: viewModel ? viewModel.projectRoot : ""
                    font.pixelSize: 9
                    color: "#888888"
                    elide: Text.ElideMiddle
                }
            }

            // 文件树视图
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                background: Rectangle {
                    color: "#1e1e1e"
                }

                ListView {
                    id: fileListView
                    anchors.fill: parent
                    // QFileSystemModel is used as the model
                    // The rootIndex is set via ViewModel's rootIndex property
                    // ListView will display items from the root path set in the model
                    model: viewModel && viewModel.fileSystemModel ? viewModel.fileSystemModel : null

                    delegate: Rectangle {
                        id: fileItem
                        width: fileListView.width
                        height: 24
                        color: fileListView.currentIndex === index ? "#3a3a3a" : (mouseArea.containsMouse ? "#2b2b2b" : "transparent")

                        // 使用 ViewModel 的辅助方法获取文件信息
                        property string fileName: viewModel ? viewModel.getFileNameByRow(index) : ""
                        property bool isDir: viewModel ? viewModel.isDirectoryByRow(index) : false
                        property string filePath: viewModel ? viewModel.getFilePathByRow(index) : ""

                        Row {
                            anchors.left: parent.left
                            anchors.leftMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 8

                            // 文件/文件夹图标
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: fileItem.isDir ? "📁" : "📄"
                                font.pixelSize: 14
                                width: 20
                            }

                            // 文件名
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: fileItem.fileName || model.display || ""
                                color: fileItem.isDir ? "#4CAF50" : "#ffffff"
                                font.pixelSize: 11
                                elide: Text.ElideMiddle
                            }
                        }

                        MouseArea {
                            id: mouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onClicked: {
                                if (mouse.button === Qt.LeftButton) {
                                    fileListView.currentIndex = index
                                    if (viewModel && fileItem.isDir) {
                                        // 单击文件夹：进入文件夹
                                        viewModel.setRootPath(fileItem.filePath)
                                    }
                                } else if (mouse.button === Qt.RightButton) {
                                    // 右键菜单（未来可以添加）
                                }
                            }
                            onDoubleClicked: {
                                if (mouse.button === Qt.LeftButton) {
                                    if (viewModel) {
                                        if (fileItem.isDir) {
                                            // 双击文件夹：进入文件夹
                                            viewModel.setRootPath(fileItem.filePath)
                                        } else {
                                            // 双击文件：触发打开事件
                                            console.log("Double-clicked file:", fileItem.filePath)
                                            // TODO: 发送文件打开事件到 EventBus
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // 状态栏
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 20
                color: "#1e1e1e"
                border.color: "#404040"
                border.width: 0
                border.topWidth: 1
                visible: false  // 可以用于显示文件信息等

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: ""
                    font.pixelSize: 9
                    color: "#666666"
                }
            }
        }
    }
}

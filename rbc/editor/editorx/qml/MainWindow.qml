import QtQuick 2.6
import QtQuick.Window 2.2
import QtQuick.Controls 2.2

ApplicationWindow {
    id: mainWindow
    visible: true 
    width: 1920
    height: 1080
    title: qsTr("RoboCute Editor")

    RBCButton {
        windowRoot: mainWindow
    }
}
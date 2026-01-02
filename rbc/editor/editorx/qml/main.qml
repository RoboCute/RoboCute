import QtQuick 2.6
import QtQuick.Window 2.2
import QtQuick.Controls 2.2

Window {
    id: root
    visible: true 
    width: 1920
    height: 1080
    title: qsTr("RoboCute Editor")

    RBCButton {
        windowRoot: root
    }
}
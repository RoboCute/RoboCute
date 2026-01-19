import QtQuick 2.6
import QtQuick.Controls 2.2

Button {
    id: rbcButton
    text: "RBCButton"
    
    property var windowRoot: null
    
    onClicked: {
        if (windowRoot) {
            windowRoot.color = Qt.rgba(Math.random(), Math.random(), Math.random(), 1);
        }
    }
}


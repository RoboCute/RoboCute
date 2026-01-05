import QtQuick
import QtQuick.Controls
import RoboCute.Viewport 1.0

Item {
    id: root

    // plugin by C++
    require property ViewportViewModel viewModel

    // Render Surface 
    RenderSurface {
        id: renderSurface,
        anchors.fill: parent
        renderer: viewModel.renderer 

        // Mouse Interaction

        // Tool Bar

        // Status Bar
    }
}
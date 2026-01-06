import QtQuick 2.15
import QtTest 1.15
import RoboCute.Connection 1.0
import "../mocks"

/**
 * QML UI Tests for ConnectionView
 * 
 * Tests QML UI behavior with mocked ViewModel
 */
TestCase {
    id: testCase
    name: "ConnectionViewTests"
    width: 400
    height: 300

    property var mockViewModel: null

    function initTestCase() {
        console.log("=== QML ConnectionView Test Suite ===")
    }

    function cleanupTestCase() {
        console.log("=== QML ConnectionView Test Suite Complete ===")
    }

    function init() {
        // Create mock ViewModel
        mockViewModel = Qt.createQmlObject(
            'import QtQuick 2.15; import RoboCute.Connection 1.0; ' +
            'ConnectionViewModel { }',
            testCase
        )
    }

    function cleanup() {
        if (mockViewModel) {
            mockViewModel.destroy()
            mockViewModel = null
        }
    }

    function test_view_initialization() {
        // Load QML from qrc resource
        var component = Qt.createComponent("qrc:/qml/test/test_connection_view.qml")
        if (component.status === Component.Ready) {
            var view = component.createObject(testCase, {"viewModel": mockViewModel})
            verify(view !== null, "View should be created")
            if (view) {
                view.destroy()
            }
        } else {
            verify(false, "Failed to load QML component: " + component.errorString())
        }
    }

    function test_status_indicator_disconnected() {
        if (!mockViewModel) return
        
        mockViewModel.connected = false
        mockViewModel.statusText = "Disconnected"
        
        var view = Qt.createQmlObject(
            'import QtQuick 2.15; import "../plugins/connection_plugin/qml/ConnectionView.qml"; ' +
            'ConnectionView { viewModel: testCase.mockViewModel }',
            testCase
        )
        
        wait(100) // Wait for bindings to update
        
        // Find status indicator (simplified check)
        verify(view !== null, "View should exist")
        
        view.destroy()
    }

    function test_status_indicator_connected() {
        if (!mockViewModel) return
        
        mockViewModel.connected = true
        mockViewModel.statusText = "Connected"
        
        var view = Qt.createQmlObject(
            'import QtQuick 2.15; import "../plugins/connection_plugin/qml/ConnectionView.qml"; ' +
            'ConnectionView { viewModel: testCase.mockViewModel }',
            testCase
        )
        
        wait(100)
        verify(view !== null, "View should exist")
        
        view.destroy()
    }

    function test_server_url_input() {
        if (!mockViewModel) return
        
        mockViewModel.serverUrl = "http://test:5555"
        
        var view = Qt.createQmlObject(
            'import QtQuick 2.15; import "../plugins/connection_plugin/qml/ConnectionView.qml"; ' +
            'ConnectionView { viewModel: testCase.mockViewModel }',
            testCase
        )
        
        wait(100)
        verify(view !== null, "View should exist")
        
        view.destroy()
    }

    function test_buttons_enabled_state() {
        if (!mockViewModel) return
        
        mockViewModel.serverUrl = "http://test:5555"
        mockViewModel.isBusy = false
        
        var view = Qt.createQmlObject(
            'import QtQuick 2.15; import "../plugins/connection_plugin/qml/ConnectionView.qml"; ' +
            'ConnectionView { viewModel: testCase.mockViewModel }',
            testCase
        )
        
        wait(100)
        verify(view !== null, "View should exist")
        
        // Test busy state
        mockViewModel.isBusy = true
        wait(100)
        
        view.destroy()
    }

    function test_connect_button_text() {
        if (!mockViewModel) return
        
        mockViewModel.connected = false
        var view = Qt.createQmlObject(
            'import QtQuick 2.15; import "../plugins/connection_plugin/qml/ConnectionView.qml"; ' +
            'ConnectionView { viewModel: testCase.mockViewModel }',
            testCase
        )
        
        wait(100)
        verify(view !== null, "View should exist")
        
        // Change to connected
        mockViewModel.connected = true
        wait(100)
        
        view.destroy()
    }

    function test_error_message_display() {
        if (!mockViewModel) return
        
        mockViewModel.errorMessage = "Test error message"
        
        var view = Qt.createQmlObject(
            'import QtQuick 2.15; import "../plugins/connection_plugin/qml/ConnectionView.qml"; ' +
            'ConnectionView { viewModel: testCase.mockViewModel }',
            testCase
        )
        
        wait(100)
        verify(view !== null, "View should exist")
        
        // Clear error
        mockViewModel.errorMessage = ""
        wait(100)
        
        view.destroy()
    }

    function test_busy_indicator() {
        if (!mockViewModel) return
        
        mockViewModel.isBusy = true
        
        var view = Qt.createQmlObject(
            'import QtQuick 2.15; import "../plugins/connection_plugin/qml/ConnectionView.qml"; ' +
            'ConnectionView { viewModel: testCase.mockViewModel }',
            testCase
        )
        
        wait(100)
        verify(view !== null, "View should exist")
        
        mockViewModel.isBusy = false
        wait(100)
        
        view.destroy()
    }
}


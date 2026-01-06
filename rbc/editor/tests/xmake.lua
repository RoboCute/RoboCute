-- Editor Tests Configuration

-- Helper function to add Qt test targets
function add_qt_test(name, deps)
    deps = deps or {}
    
    target("test_editor_" .. name)
        set_kind("binary")
        set_group("02.tests")
        add_rules("qt.console")
        add_frameworks("QtCore", "QtTest", "QtGui", "QtWidgets", "QtQml", "QtQuick", "QtQuickTest")
        
        on_load(function (target)
            for k, v in pairs(opt) do
                target:set(k, v)
            end 
            target:set("exceptions", "cxx")
            for _, dep in ipairs(deps) do
                target:add("deps", dep)
            end
        end)
        
        -- Use specific path patterns instead of wildcards
        -- Note: name is like "connection_viewmodel" or "connection_plugin"
        -- Files are in mvvm/ or plugins/ subdirectories with "test_" prefix
        -- Q_OBJECT classes must be in header files and explicitly added
        if name == "connection_viewmodel" then
            add_files("mvvm/test_" .. name .. ".h")  -- Header with Q_OBJECT
            add_files("mvvm/test_" .. name .. ".cpp")
        elseif name == "connection_plugin" then
            add_files("plugins/test_" .. name .. ".h")  -- Header with Q_OBJECT
            add_files("plugins/test_" .. name .. ".cpp")
        else
            add_files(name .. "/*.cpp")
            add_files(name .. "/*.h")
        end
        add_includedirs(".")
        add_includedirs("../runtimex/include")
        add_includedirs("../plugins/connection_plugin")
        
        -- Import symbols from DLLs
        -- RBC_EDITOR_PLUGIN_API should be imported from RBCE_ConnectionPlugin DLL
        add_defines("RBC_EDITOR_PLUGIN_API=LUISA_DECLSPEC_DLL_IMPORT")
        add_defines("RBC_EDITOR_RUNTIME_API=LUISA_DECLSPEC_DLL_IMPORT")
        
        add_deps("rbc_editor_runtimex", "RBCE_ConnectionPlugin")
    target_end()
end

-- Mock objects
target("test_editor_mocks")
    set_kind("static")
    set_group("02.tests")
    add_rules("qt.static")
    add_frameworks("QtCore", "QtGui", "QtWidgets", "QtQml", "QtNetwork")
    
    add_files("mocks/*.cpp")
    add_files("mocks/*.h")
    add_headerfiles("mocks/*.h")
    add_includedirs(".")
    add_includedirs("../runtimex/include")
    
    add_deps("rbc_editor_runtimex")
target_end()

-- MVVM Tests
add_qt_test("connection_viewmodel", {"test_editor_mocks"})

-- Plugin Tests
add_qt_test("connection_plugin", {"test_editor_mocks"})

-- QML Tests
target("test_editor_connection_view_qml")
    set_kind("binary")
    set_group("02.tests")
    add_rules("qt.console")
    add_frameworks("QtCore", "QtTest", "QtGui", "QtWidgets", "QtQml", "QtQuick", "QtQuickTest")
    
    on_load(function (target)
        for k, v in pairs(opt) do
            target:set(k, v)
        end 
        target:set("exceptions", "cxx")
    end)
    
    -- Add source files
    -- Note: Do not include test_connection_viewmodel.cpp here as it has its own main() function
    add_files("qml/test_connection_view_runner.cpp")
    add_files("qml/test_connection_view_runner.h")  -- Header with Q_OBJECT for ConnectionViewTestSetup
    
    add_includedirs(".")
    add_includedirs("../runtimex/include")
    add_includedirs("../plugins/connection_plugin")
    
    -- Import symbols from DLLs
    -- RBC_EDITOR_PLUGIN_API should be imported from RBCE_ConnectionPlugin DLL
    add_defines("RBC_EDITOR_PLUGIN_API=LUISA_DECLSPEC_DLL_IMPORT")
    add_defines("RBC_EDITOR_RUNTIME_API=LUISA_DECLSPEC_DLL_IMPORT")
    
    add_deps("test_editor_mocks", "rbc_editor_runtimex", "RBCE_ConnectionPlugin")
    
    -- Set QML import paths
    add_defines("QT_QMLTEST_MAIN")
target_end()

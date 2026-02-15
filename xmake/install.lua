function main(mode, build_stubgen)
    if not mode then
        mode = 'release'
    end
    local ext_path = path.join(os.projectdir(), "src/robocute/rbc_ext/_C")
    os.mkdir(ext_path)
    local target_dir = path.join(os.projectdir(), 'build', os.host(), os.arch(), mode)
    -- copy targetdir to 
    os.cp(path.join(target_dir, '*.dll'), ext_path, {
        copy_if_different = true
    })
    os.cp(path.join(target_dir, '*.pyd'), ext_path, {
        copy_if_different = true
    })
    -- copy builtin-assets
    os.cp(path.join(target_dir, "../shader_build_dx"), ext_path, {
        copy_if_different = true
    })
    os.cp(path.join(target_dir, "../shader_build_vk"), ext_path, {
        copy_if_different = true
    })

    -- Do this manually
    if build_stubgen then
        os.setenv('PYTHONPATH', ext_path)
        if build_stubgen == 'uv' then
            os.runv('uvx', {'pybind11-stubgen', 'test_py_codegen', '--output-dir=' .. ext_path})
            os.runv('uvx', {'pybind11-stubgen', 'rbc_ext_c', '--output-dir=' .. ext_path})
            os.runv('uvx', {'pybind11-stubgen', 'lcapi_c', '--output-dir=' .. ext_path})
        else
            os.runv('pybind11-stubgen', {'test_py_codegen', '--output-dir=' .. ext_path})
            os.runv('pybind11-stubgen', {'rbc_ext_c', '--output-dir=' .. ext_path})
            -- os.runv('pybind11-stubgen', {'pyuipc', '--output-dir=' .. ext_path})
        end
    end
end

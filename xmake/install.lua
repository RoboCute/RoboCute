function main(mode)
    if not mode then
        mode = 'release'
    end
    local ext_path = path.join(os.projectdir(), "src/rbc_ext/_C")
    os.mkdir(ext_path)
    local target_dir = path.join(os.projectdir(), 'build', os.host(), os.arch(), mode)
    -- copy targetdir to 
    os.cp(path.join(target_dir, '*.dll'), ext_path, {
        copy_if_different = true,
        async = true
    })
    os.cp(path.join(target_dir, '*.pyd'), ext_path, {
        copy_if_different = true,
        async = true
    })
    -- Do this manually
    os.setenv('PYTHONPATH', target_dir)
    -- os.runv('uvx', {'pybind11-stubgen', 'test_py_codegen', '--output-dir=' .. ext_path})
end

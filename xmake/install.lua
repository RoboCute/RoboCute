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
        async = true,
        detach = true
    })
    os.cp(path.join(target_dir, '*.pyd'), ext_path, {
        copy_if_different = true,
        async = true,
        detach = true
    })

    -- generate "__init__.py"
    local file = io.open(path.join(ext_path, "__init__.py"), "w")
    file:write("# generated __init__.py")
    file:close()

    os.execv("uv run stubgen")
end

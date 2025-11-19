function main()
    import("async.jobgraph")
    import("async.runjobs")
    local ext_path = path.join(os.projectdir(), "src/rbc_ext/bin")
    os.mkdir(ext_path)
    local jobs = jobgraph.new()
    local target_dir = path.join(os.projectdir(), 'build', os.host(), os.arch(), 'release')
    -- copy targetdir to 
    jobs:add("rbc_pyext_copy_dll", function()
        os.cp(path.join(target_dir, '*.dll'), ext_path, {
            copy_if_different = true
        })
    end)
    jobs:add("rbc_pyext_copy_pyd", function()
        os.cp(path.join(target_dir, '*.pyd'), ext_path, {
            copy_if_different = true
        })
    end)
    runjobs("copy", jobs, {
        comax = 1000
    })
    os.execv("uv run generate_stub.py")
end

rule('py_codegen')
set_extensions('.py')
on_build_files(function(target, compile_jobs, sourcebatch, opt)
    import("async.jobgraph")
    import("async.runjobs")
    local py_bin = get_config('rbc_py_bin')
    if not py_bin then
        utils.error('Python binary path not setted.')
        return
    end
    local compile_path = target:extraconf('rules', 'py_codegen', 'compile_path')
    if not compile_path then
        utils.error('compile_path not set')
        return
    end
    if type(compile_path) ~= 'table' then
        compile_path = {compile_path}
    end
    local python_path = target:extraconf('rules', 'py_codegen', 'python_path')
    if type(python_path) ~= 'string' then
        python_path = 'scripts/py'
    end
    local job_prepare_name = target:name() .. '_pycodegen_prepare'
    local job_compile_name = target:name() .. '_pycodegen_compile'
    -- PYTHONPATH for import
    compile_jobs:add(job_prepare_name, function()
        if python_path then
            if type(python_path) == 'string' then
                os.addenv('PYTHONPATH', python_path)
            elseif type(python_path) == 'table' then
                for i, v in ipairs(python_path) do
                    os.addenv('PYTHONPATH', v)
                end
            end
        end

    end)
    -- Run python codegen
    local py_jobs = jobgraph.new()
    for i, sourcefile in ipairs(sourcebatch.sourcefiles) do
        local job_key = target:name() .. 'pycodegen' .. tostring(i)
        py_jobs:add(tostring(i), function()
            os.runv(path.join(py_bin, 'python'), {sourcefile})
        end)
    end
    runjobs(target:name() .. '_runpyjob', py_jobs, {
        comax = 1000,
        timeout = -1,
        timer = function(running_jobs_indices)
            utils.error("timeout.")
        end
    })
    -- Compile files
    local batchcxx = {
        rulename = 'c++.build',
        sourcekind = 'cxx',
        sourcefiles = {},
        objectfiles = {},
        dependfiles = {}
    }
    for _, path_rule in ipairs(compile_path) do
        for _, file in ipairs(os.files(path_rule)) do
            local objectfile = target:objectfile(file)
            local dependfile = target:dependfile(objectfile)
            table.insert(target:objectfiles(), objectfile)
            table.insert(batchcxx.objectfiles, objectfile)
            table.insert(batchcxx.dependfiles, dependfile)
            table.insert(batchcxx.sourcefiles, file)
        end
    end
    compile_jobs:group(job_compile_name, function()
        import('private.action.build.object')(target, compile_jobs, batchcxx, opt)
    end)
    if #batchcxx.sourcefiles > 0 then
        compile_jobs:add_orders(job_prepare_name, job_compile_name)
    end
end, {
    jobgraph = true
})
rule_end()
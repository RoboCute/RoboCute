rule('py_codegen')
set_extensions('.py')
on_build_files(function(target, jobgraph, sourcebatch, opt)
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
    jobgraph:add(job_prepare_name, function()
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
    for i, sourcefile in ipairs(sourcebatch.sourcefiles) do
        local job_key = target:name() .. 'pycodegen' .. tostring(i)
        os.runv('python', {sourcefile})
    end
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
    jobgraph:group(job_compile_name, function()
        import('private.action.build.object')(target, jobgraph, batchcxx, opt)
    end)
    if #batchcxx.sourcefiles > 0 then
        jobgraph:add_orders(job_prepare_name, job_compile_name)
    end
end, {
    jobgraph = true
})
rule_end()

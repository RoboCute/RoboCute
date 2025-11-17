rule('py_codegen')
set_extensions('.py')
on_build_files(function(target, jobgraph, sourcebatch, opt)
    local python_path = target:extraconf('rules', 'py_codegen', 'python_path')
    if type(python_path) ~= 'string' then
        python_path = 'scripts'
    end
    local root_path = target:extraconf('rules', 'py_codegen', 'root_path')
    if type(root_path) ~= 'string' then
        utils.error('root_path not setted.')
        return
    end
    local out_path = 'build/.codegen/' .. target:name()
    local job_prepare_name = target:name() .. '_pycodegen_prepare'
    local job_gen_name = target:name() .. '_pycodegen_gen'
    local job_compile_name = target:name() .. '_pycodegen_compile'
    local dest_files = {}
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
    jobgraph:group(job_gen_name, function()
        for i, sourcefile in ipairs(sourcebatch.sourcefiles) do
            local job_key = target:name() .. 'pycodegen' .. tostring(i)
            local relative_path = path.relative(path.directory(sourcefile), root_path)
            local file_parent_path = path.normalize(path.join(out_path, relative_path))
            local file_path = path.join(file_parent_path, path.basename(sourcefile) .. '.generated.cpp')
            table.insert(dest_files, file_path)
            jobgraph:add(job_key, function()
                os.mkdir(file_parent_path)
                os.runv('python', {sourcefile, file_path})
            end)
        end
    end)
    -- Compile files
    jobgraph:group(job_compile_name, function()
        local batchcxx = {
            rulename = 'c++.build',
            sourcekind = 'cxx',
            sourcefiles = {},
            objectfiles = {},
            dependfiles = {}
        }
        -- local dst_name = header_lib.dst_file()
        for _, v in ipairs(dest_files) do
            local objectfile = target:objectfile(v)
            local dependfile = target:dependfile(objectfile)
            table.insert(target:objectfiles(), objectfile)
            table.insert(batchcxx.objectfiles, objectfile)
            table.insert(batchcxx.dependfiles, dependfile)
            table.insert(batchcxx.sourcefiles, v)
        end
        import('private.action.build.object')(target, jobgraph, batchcxx, opt)
    end)
    jobgraph:add_orders(job_prepare_name, job_gen_name)
    if #dest_files > 0 then
        jobgraph:add_orders(job_gen_name, job_compile_name)
    end
end, {
    jobgraph = true
})
rule_end()

rule("nanobind_gen")
set_extensions(".lua")
on_load(function(target)
    target:add("deps", "nanogen", {
        inherit = false,
        public = false
    })
end)
on_build_files(function(target, jobgraph, sourcebatch, opt)
    local function to_clang_meta_path(target_name)
        return path.join(os.projectdir(), "build/.skr/codegen", target_name, "meta_database")
    end
    for _, sourcefile in ipairs(sourcebatch.sourcefiles) do
        local module_name = target:extraconf("rules", "nanobind_gen", "module_name")
        local filename = path.filename(sourcefile)
        local rootdir = path.directory(sourcefile)
        local header_lib = import(path.basename(filename), {
            rootdir = rootdir
        })
        local dst_targets = header_lib.dst_target()
        if type(dst_targets) == "string" then
            dst_targets = {dst_targets}
        end
        local dst_dir = header_lib.dst_dir()
        local process_job = sourcefile .. "/nanogen"
        if dst_targets and dst_dir then
            -- prepare files

            local main_module_file = path.absolute(path.join(dst_dir, module_name .. ".cpp"))
            local compile_dst_files = {}
            local compile_src_files = {}
            for _, tar_name in ipairs(dst_targets) do
                local meta_path = to_clang_meta_path(tar_name)
                for _, meta_file in ipairs(os.files(path.join(meta_path, "**.meta"))) do
                    local relative_path = path.relative(meta_file, meta_path)
                    local curr_dst_dir = path.join(dst_dir, path.directory(relative_path))
                    os.mkdir(curr_dst_dir)
                    table.insert(compile_src_files, meta_file)
                    table.insert(compile_dst_files,
                        path.join(curr_dst_dir, path.basename(path.basename(meta_file)) .. ".cpp"))
                end
            end
            jobgraph:add(process_job, function()
                local json_temp_path = path.join(os.projectdir(), "build/.gens", target:name(), os.host(), os.arch())
                os.mkdir(json_temp_path)
                json_temp_path = path.join(json_temp_path, module_name .. "_meta.json")
                local lib = import("lib", {
                    rootdir = get_config("_lc_script_path")
                })
                local sb = lib.StringBuilder()
                sb:add('[')
                local is_first = true
                for i, src_file in ipairs(compile_src_files) do
                    if not is_first then
                        sb:add(',')
                    else
                        is_first = false
                    end
                    sb:add('"'):add(path.absolute(src_file)):add('","'):add(path.absolute(compile_dst_files[i]))
                        :add('"')
                end
                sb:add(']')
                for i = 1, sb:size() do
                    if sb:get(i) == 92 then
                        sb:set(i, 47)
                    end
                end
                sb:write_to(json_temp_path)
                sb:dispose()
                local target_dir = path.join(target:targetdir(), "nanogen")
                local args = {json_temp_path, module_name, main_module_file}
                os.runv(target_dir, args)
            end)
            -- -- compile files
            jobgraph:group(sourcefile, function()

                local batchcxx = {
                    rulename = "c++.build",
                    sourcekind = "cxx",
                    sourcefiles = {},
                    objectfiles = {},
                    dependfiles = {}
                }
                local function add_file(dst_name)
                    local objectfile = target:objectfile(dst_name)
                    local dependfile = target:dependfile(objectfile)
                    table.insert(target:objectfiles(), objectfile)
                    table.insert(batchcxx.objectfiles, objectfile)
                    table.insert(batchcxx.dependfiles, dependfile)
                    table.insert(batchcxx.sourcefiles, dst_name)
                end
                for _, dst_name in ipairs(compile_dst_files) do
                    add_file(dst_name)
                end
                add_file(main_module_file)
                import("private.action.build.object")(target, jobgraph, batchcxx, opt)
            end)
            jobgraph:add_orders(process_job, sourcefile)
        end
    end
end, {
    jobgraph = true
})
rule_end()

rule("rbq_load_python")
on_load(function(target)
    local public = target:extraconf("rules", "rbq_load_python", "public") or false
    local function split_str(str, chr, func)
        local kv = str:split(chr, {
            plain = true
        })
        for _, v in ipairs(kv) do
            func(v)
        end
    end
    local lc_py_include = get_config("lc_py_include")
    local lc_py_linkdir = get_config("lc_py_linkdir")
    local lc_py_libs = get_config("lc_py_libs")
    if (not lc_py_include) or (not lc_py_linkdir) or (not lc_py_libs) then
        utils.error("Python not found, nanobind disabled.")
        target:set("enabled", false)
        return
    end
    split_str(lc_py_include, ';', function(v)
        target:add("includedirs", v, {
            public = public
        })
    end)
    if type(lc_py_linkdir) == "string" then
        split_str(lc_py_linkdir, ';', function(v)
            target:add("linkdirs", v, {
                public = public
            })
        end)
    end
    if type(lc_py_libs) == "string" then
        split_str(lc_py_libs, ';', function(v)
            target:add("links", v, {
                public = public
            })
        end)
    end
end)
rule_end()

import("core.base.scheduler")
import("async.jobgraph")
import("async.runjobs")

local function is_empty_folder(dir)
    if os.exists(dir) and not os.isfile(dir) then
        for _, v in ipairs(os.filedirs(path.join(dir, '*'))) do
            return false
        end
        return true
    else
        return true
    end
end
local function find_process_path(process)
    local path_str = os.getenv("PATH")
    if path_str then
        local paths = path.splitenv(path_str)
        for i, pth in ipairs(paths) do
            if os.isfile(path.join(pth, process)) then
                return pth
            end
        end
    end
    return nil
end
local function git_clone_or_pull(git_address, subdir, branch)
    local args
    if is_empty_folder(subdir) then
        args = {'clone', git_address}
        if branch then
            table.insert(args, '-b')
            table.insert(args, branch)
        end
        table.insert(args, path.translate(subdir))
    else
        args = {'-C', subdir, 'pull'}
        if branch then
            table.insert(args, 'origin')
            table.insert(args, branch)
        end
    end
    local done = false
    print("pulling " .. git_address)
    for i = 1, 4, 1 do
        try {function()
            os.execv('git', args)
            done = true
        end}
        if done then
            return
        end
    end
    utils.error("git clone " .. git_address .. " error.")
    os.exit(1)
end

function main(llvm_path)

    local lc_path = path.absolute("thirdparty/LuisaCompute")
    ------------------------------ git ------------------------------
    print("Clone submod? (y/n)")
    local clone_lc = io.read()
    if clone_lc == 'Y' or clone_lc == 'y' then
        print("git...")
        local jobs = jobgraph.new()
        local root_git_name = nil
        local function add_git_job(name, subdir, git_address, branch)
            jobs:add(name, function()
                git_clone_or_pull(git_address, subdir, branch)
            end)
            if root_git_name then
                jobs:add_orders(root_git_name, name)
            end
        end
        add_git_job("nanobind", "thirdparty/nanobind", "https://github.com/LuisaGroup/nanobind.git")
        add_git_job("robinmap", "thirdparty/nanobind/ext/robin_map", "https://github.com/Tessil/robin-map.git")
        jobs:add_orders("nanobind", "robinmap")
        add_git_job("lc", lc_path, "https://github.com/LuisaGroup/LuisaCompute.git", "next")

        -- lc submod
        do
            root_git_name = "lc"
            add_git_job("xxhash", "thirdparty/LuisaCompute/src/ext/xxhash", "https://github.com/Cyan4973/xxHash.git")
            add_git_job("spdlog", "thirdparty/LuisaCompute/src/ext/spdlog", "https://github.com/LuisaGroup/spdlog.git")
            add_git_job("EABase", "thirdparty/LuisaCompute/src/ext/EASTL/packages/EABase",
                "https://github.com/LuisaGroup/EABase.git")
            add_git_job("EASTL", "thirdparty/LuisaCompute/src/ext/EASTL", "https://github.com/LuisaGroup/EASTL.git", nil)
            jobs:add_orders("EASTL", "EABase")

            add_git_job("glfw", "thirdparty/LuisaCompute/src/ext/glfw", "https://github.com/glfw/glfw.git")
            add_git_job("magic_enum", "thirdparty/LuisaCompute/src/ext/magic_enum",
                "https://github.com/LuisaGroup/magic_enum")
            add_git_job("imgui", "thirdparty/LuisaCompute/src/ext/imgui", "https://github.com/ocornut/imgui.git", "docking")
            add_git_job("reproc", "thirdparty/LuisaCompute/src/ext/reproc", "https://github.com/LuisaGroup/reproc.git")
            add_git_job("stb", "thirdparty/LuisaCompute/src/ext/stb/stb", "https://github.com/nothings/stb.git")
            add_git_job("yyjson", "thirdparty/LuisaCompute/src/ext/yyjson", "https://github.com/ibireme/yyjson.git")
            root_git_name = nil
        end
        runjobs("git", jobs, {
            comax = 1000,
            timeout = -1,
            timer = function(running_jobs_indices)
                utils.error("git timeout.")
            end
        })
    end
    if is_empty_folder(lc_path) then
        utils.error("LuisaCompute not installed.")
        os.exit(1)
    end
    ------------------------------ llvm ------------------------------
    local builddir
    if os.is_host("windows") then
        builddir = path.absolute("build/windows/x64/release")
    elseif os.is_host("macosx") then
        builddir = path.absolute("build/macosx/x64/release")
    end
    local lib = import("lib", {
        rootdir = "thirdparty/LuisaCompute/scripts"
    })
    if llvm_path then
        if not llvm_path or type(llvm_path) ~= "string" then
            utils.error("Bad argument, should be 'xmake l setup.lua #LLVM_PATH#'")
            os.exit(1)
        elseif not os.isdir(llvm_path) then
            utils.error("LLVM path illegal")
            os.exit(1)
        end

        print("copying llvm...")

        local llvm_code_path = path.absolute("src/clangcxx/llvm", lc_path)
        os.tryrm(path.join(llvm_code_path, "include"))
        os.tryrm(path.join(llvm_code_path, "lib"))
        os.cp(path.join(llvm_path, "include"), path.join(llvm_code_path, "include"), {
            copy_if_different = true
        })
        os.cp(path.join(llvm_path, "lib"), path.join(llvm_code_path, "lib"), {
            copy_if_different = true
        })
        if builddir then
            lib.mkdirs(builddir)
            if os.is_host("windows") then
                os.cp(path.join(llvm_path, "bin", "clang.exe"), builddir, {
                    copy_if_different = true
                })
            elseif os.is_host("macosx") then
                os.cp(path.join(llvm_path, "bin", "clang"), builddir, {
                    copy_if_different = true
                })
            end
        else
            utils.error("build dir not set.")
            os.exit(1)
        end
    end
    -- python
    local sb = lib.StringBuilder()
    print("Write options.lua? (y/n)")
    local write_opt = io.read()
    if write_opt == 'Y' or write_opt == 'y' then
        local py_path = find_process_path("python.exe")
        sb:clear()
        sb:add('set_config("lc_toolchain", "clang-cl")\n')
        if py_path then
            sb:add('set_config("lc_py_include", "'):add(lib.string_replace(path.join(py_path, "include"), "\\", "/"))
                :add('")\nset_config("lc_py_linkdir", "')
            local lc_py_linkdir = path.join(py_path, "libs")
            sb:add(lib.string_replace(lc_py_linkdir, "\\", "/"))
            local py = "python"
            sb:add('")\n')
            local files = {}
            for _, filepath in ipairs(os.files(path.join(lc_py_linkdir, "*.lib"))) do
                local lib_name = path.basename(filepath)
                if #lib_name >= #py and lib_name:sub(1, #py):lower() == py then
                    table.insert(files, lib_name)
                end
            end
            if #files > 0 then
                sb:add('set_config("lc_py_libs", "')
                for i, v in ipairs(files) do
                    sb:add(v .. ";")
                end
                sb:add('")\n')
            end
        end
        sb:write_to(path.join(os.scriptdir(), "scripts/xmake/options_win.lua"))
    end
    -- shader command
    do
        sb:clear()
        print("preparing shader command...")
        local shader_out_dir = path.translate(path.join(path.directory(builddir), "shader_build"))
        local compiler_path = "clangcxx_compiler"
        if os.is_host("windows") then
            compiler_path = compiler_path .. ".exe"
        end
        compiler_path = path.join(os.projectdir(), "build/tool/clangcxx_compiler", compiler_path)
        local shader_dir = path.translate(path.absolute("rbc/runtime/render/shader/"))
        local compile_cmd_path = path.translate(path.join(shader_dir, 'compile.cmd'))
        local gen_json_cmd_path = path.translate(path.join(shader_dir, 'gen_json.cmd'))
        sb:add('@echo off\n"'):add(compiler_path):add('" --in="'):add(shader_dir):add('\\src" --out="'):add(
            shader_out_dir):add('" -hostgen="'):add(shader_dir):add('\\host'):add('" --include="'):add(shader_dir):add(
            '\\include"')
        sb:write_to(compile_cmd_path)
        compile_cmd_path = path.translate(path.join(shader_dir, 'clean_compile.cmd'))
        sb:add(' --rebuild')
        sb:write_to(compile_cmd_path)
        compile_cmd_path = path.translate(path.join(shader_dir, 'clean_compile_pack.cmd'))
        local lmdb_shader_out_dir = path.translate(path.join(path.directory(builddir), "shader_build_lmdb"))
        sb:add(' --pack_path="'):add(lmdb_shader_out_dir):add('"')
        sb:write_to(compile_cmd_path)
        sb:clear()
        sb:add('@echo off\n"'):add(compiler_path):add('" --in="'):add(shader_dir):add('\\src" --out="'):add(shader_dir)
            :add('\\.vscode\\compile_commands.json" --include="'):add(shader_dir):add('\\include"'):add(' --lsp')
        sb:write_to(gen_json_cmd_path)
    end
    sb:dispose()
end

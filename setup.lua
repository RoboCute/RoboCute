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

function main()
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
            add_git_job("mimalloc", "thirdparty/LuisaCompute/src/ext/EASTL/packages/mimalloc",
                "https://github.com/LuisaGroup/mimalloc.git")
            add_git_job("EASTL", "thirdparty/LuisaCompute/src/ext/EASTL", "https://github.com/LuisaGroup/EASTL.git", nil)
            jobs:add_orders("EASTL", "EABase")
            jobs:add_orders("EASTL", "mimalloc")

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
    local sb = lib.StringBuilder()
    -- options
    print("Write options.lua? (y/n)")
    local write_opt = io.read()
    if write_opt == 'Y' or write_opt == 'y' then
        local py_path = find_process_path("python.exe")
        sb:clear()
        sb:add('{\n')
        sb:add('"lc_toolchain": "clang-cl"\n')
        if py_path then
            sb:add(', "lc_py_include": "'):add(lib.string_replace(path.join(py_path, "include"), "\\", "/")):add('"\n')
            sb:add(', "rbc_py_bin": "'):add(lib.string_replace(py_path, "\\", "/")):add('"\n')
            sb:add(', "lc_py_linkdir": "')
            local lc_py_linkdir = path.join(py_path, "libs")
            sb:add(lib.string_replace(lc_py_linkdir, "\\", "/"))
            local py = "python"
            sb:add('"\n')
            local files = {}
            for _, filepath in ipairs(os.files(path.join(lc_py_linkdir, "*.lib"))) do
                local lib_name = path.basename(filepath)
                if #lib_name >= #py and lib_name:sub(1, #py):lower() == py then
                    table.insert(files, lib_name)
                end
            end
            if #files > 0 then
                sb:add(', "lc_py_libs": "')
                for i, v in ipairs(files) do
                    sb:add(v .. ";")
                end
                sb:add('"\n')
            end
        end
        sb:add('}')
        sb:write_to(path.join(os.scriptdir(), "scripts/options.json"))
    end
    sb:dispose()
end

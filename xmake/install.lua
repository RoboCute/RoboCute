import('async.jobgraph')
import('async.runjobs')

function main(mode, build_stubgen)
    if not mode then
        mode = 'release'
    end
    local ext_path = path.join(os.projectdir(), "src/robocute/rbc_ext/_C")
    os.mkdir(ext_path)
    local target_dir = path.join(os.projectdir(), 'build', os.host(), os.arch(), mode)
    -- copy targetdir to 
    local copy_options = {
        copy_if_different = true,
        async = true
    }
    local jobs = jobgraph.new()
    local function copy_ext(name)
        jobs:add(name .. '_copy', function()
            os.cp(path.join(target_dir, '*.' .. name), ext_path, copy_options)
        end)
    end
    local function copy_shader(name)
        jobs:add('shader_' .. name .. '_copy', function()
            os.cp(path.join(target_dir, '../shader_build_' .. name), ext_path, copy_options)
        end)
    end
    copy_ext('dll')
    copy_ext('pyd')
    copy_ext('bytes')
    copy_shader('dx')
    copy_shader('vk')
    runjobs('copy', jobs, {
        comax = 1000
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

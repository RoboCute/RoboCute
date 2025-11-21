target("install_clangcxx")
set_kind("phony")
add_rules('lc_install_sdk', {
    sdk_dir = 'build/download',
    libnames = {
        name = 'clangcxx_compiler-v2.0.0.7z',
        plat_spec = true,
        address = 'https://github.com/RoboCute/RoboCute.Resouces/releases/download/Release/',
        copy_dir = '',
        extract_dir = 'build/tool/clangcxx_compiler'

    }
})
target_end()

target("install_tools")
set_kind("phony")
add_rules('lc_install_sdk', {
    sdk_dir = 'build/download',
    libnames = {{
        name = 'clangd-v19.1.7.7z',
        plat_spec = true,
        address = 'https://github.com/RoboCute/RoboCute.Resouces/releases/download/Release/',
        copy_dir = '',
        extract_dir = 'build/tool/clangd'
    }
    -- , {
    --     name = 'render_resources.7z',
    --     address = 'https://github.com/RoboCute/RoboCute.Resouces/releases/download/Release/'
    -- }
}
})
target_end()

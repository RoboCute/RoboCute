target('Imath')
add_rules('lc_basic_settings', {
    project_kind = 'shared'
})

-- Source files
add_files('Imath/src/Imath/*.cpp')

-- Include directories (public)
add_includedirs('Imath/src', 'Imath/config', {
    public = true
})

on_load(function(target)
    target:add('defines', 'IMATH_EXPORTS', 'IMATH_INTERNAL_NAMESPACE=Imath', 'IMATH_NAMESPACE=Imath', 'IMATH_VERSION_STRING="3.2.0"',
        'IMATH_USE_NOEXCEPT=1')
    if not target:is_plat('windows') then
        target:add('syslinks', 'pthread', 'm')
    end
end)

target_end()

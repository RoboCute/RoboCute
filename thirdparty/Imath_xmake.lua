target('Imath')
add_rules('lc_basic_settings', {
    project_kind = 'static'
})

-- Source files
add_files('Imath/src/Imath/*.cpp')

-- Include directories (public)
add_includedirs('Imath/src', 'Imath/config', {
    public = true
})

-- Defines
add_defines('IMATH_INTERNAL_NAMESPACE=Imath', 'IMATH_NAMESPACE=Imath', 'IMATH_VERSION_STRING="3.2.0"',
    'IMATH_PACKAGE_STRING="Imath"', 'IMATH_VERSION_MAJOR=3', 'IMATH_VERSION_MINOR=2', 'IMATH_VERSION_PATCH=0',
    'IMATH_VERSION_RELEASE_TYPE=""', 'IMATH_LIB_VERSION_STRING="30.3.2.0"', 'IMATH_USE_NOEXCEPT=1')

-- Platform defines
if is_plat('windows') then
    add_defines('PLATFORM_WINDOWS', 'PLATFORM=WINDOWS')
elseif is_plat('macosx') then
    add_defines('PLATFORM_DARWIN', 'PLATFORM=DARWIN')
else
    add_defines('PLATFORM_LINUX', 'PLATFORM=LINUX')
end

-- Link math library on non-Windows platforms
if not is_plat('windows') then
    add_syslinks('m')
end
target_end()

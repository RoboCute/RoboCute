target('alembic')
add_rules('lc_basic_settings', {
    project_kind = 'static',
    enable_exception = true
})

-- Include directories (public)
add_includedirs('alembic/lib', {
    public = true
})

-- Source files (excluding HDF5 and tests)
add_files('alembic/lib/Alembic/Abc/*.cpp', 'alembic/lib/Alembic/AbcCollection/*.cpp',
    'alembic/lib/Alembic/AbcCoreAbstract/*.cpp', 'alembic/lib/Alembic/AbcCoreFactory/*.cpp',
    'alembic/lib/Alembic/AbcCoreLayer/*.cpp', 'alembic/lib/Alembic/AbcCoreOgawa/*.cpp',
    'alembic/lib/Alembic/AbcGeom/*.cpp', 'alembic/lib/Alembic/AbcMaterial/*.cpp', 'alembic/lib/Alembic/Ogawa/*.cpp',
    'alembic/lib/Alembic/Util/*.cpp')

-- Dependencies
add_deps('Imath')

-- Defines
add_defines('ALEMBIC_VERSION=\"1.8.10\"')

-- Platform defines
if is_plat('windows') then
    add_defines('PLATFORM_WINDOWS', 'PLATFORM=WINDOWS')
elseif is_plat('macosx') then
    add_defines('PLATFORM_DARWIN', 'PLATFORM=DARWIN')
else
    add_defines('PLATFORM_LINUX', 'PLATFORM=LINUX')
end

-- Thread support (only on non-Windows platforms)
if not is_plat('windows') then
    add_syslinks('pthread', {
        public = false
    })
end
target_end()

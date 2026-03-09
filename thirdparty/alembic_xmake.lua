target('alembic')
add_rules('lc_basic_settings', {
    project_kind = 'shared',
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

-- Platform defines
on_load(function(target)
    if target:is_plat('windows') then
        target:add('defines', 'ALEMBIC_DLL', {
            public = true
        })
    end
    target:add('defines', 'ALEMBIC_EXPORTS')
    if not target:is_plat('windows') then
        target:add('syslinks', 'pthread')
    end
end)

target_end()

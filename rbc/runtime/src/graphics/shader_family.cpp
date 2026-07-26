#include <rbc_graphics/shader_family.h>

#include <atomic>
#include <mutex>
#include <thread>
#include <utility>
#include <luisa/core/stl/vector.h>

#include <luisa/core/fiber.h>
#include <luisa/core/logging.h>
#include <luisa/vstl/common.h>

#include <rbc_graphics/shader_manager.h>

namespace rbc {

namespace {

enum class FamilyLoadState : uint8_t {
    Unloaded,
    Loading,
    Ready,
    Failed,
};

}// namespace

struct ShaderFamily::Impl {
    struct Entry {
        ShaderManager::VariantSelection selection;
        luisa::vector<ShaderBase const *> programs;
        FamilyLoadState state{FamilyLoadState::Unloaded};

        explicit Entry(ShaderManager::VariantSelection value)
            : selection{std::move(value)} {}
    };

    luisa::string name;
    luisa::vector<Program> program_bindings;
    Selector selector{};
    Resolver resolver{};
    luisa::vector<luisa::unique_ptr<Entry>> entries;
    std::mutex mutex;
    luisa::fiber::counter load_counter;
    std::atomic_uint64_t completion_epoch{};

    uint64_t feature_mask{};
    uint64_t observed_completion_epoch{};
    uint64_t revision{};
    ShaderManager::VariantSelection requested_selection;
    Entry *active{};
    bool requested{};
#ifndef NDEBUG
    std::thread::id owner_thread;
#endif

    Impl(
        luisa::string_view family_name,
        std::initializer_list<Program> programs,
        Selector family_selector,
        Resolver family_resolver)
        : name{family_name},
          program_bindings{programs},
          selector{family_selector},
          resolver{family_resolver} {
        LUISA_ASSERT(!name.empty(), "Shader family name cannot be empty.");
        LUISA_ASSERT(
            !program_bindings.empty(),
            "Shader family {} must contain at least one program.",
            name);
        LUISA_ASSERT(selector != nullptr, "Shader family {} has no selector.", name);
        LUISA_ASSERT(resolver != nullptr, "Shader family {} has no resolver.", name);
        for (size_t i = 0u; i < program_bindings.size(); ++i) {
            auto const &program = program_bindings[i];
            LUISA_ASSERT(
                !program.logical_name.empty() &&
                    program.slot != nullptr &&
                    ((program.load != nullptr) !=
                     (program.load_variant != nullptr)),
                "Shader family {} program {} has an invalid name, slot, or ABI loader.",
                name,
                i);
            for (size_t j = 0u; j < i; ++j) {
                LUISA_ASSERT(
                    program.slot != program_bindings[j].slot,
                    "Shader family {} binds the same shader slot more than once.",
                    name);
                LUISA_ASSERT(
                    program.logical_name != program_bindings[j].logical_name,
                    "Shader family {} binds logical program {} more than once.",
                    name,
                    program.logical_name);
            }
            *program.slot = nullptr;
        }
    }

    [[nodiscard]] Entry *find_entry_locked(
        ShaderManager::VariantSelection const &selection) const noexcept {
        for (auto const &entry : entries) {
            if (entry->selection == selection) {
                return entry.get();
            }
        }
        return nullptr;
    }

    [[nodiscard]] Entry &entry_locked(
        ShaderManager::VariantSelection const &selection) {
        if (auto entry = find_entry_locked(selection)) {
            return *entry;
        }
        auto entry = luisa::make_unique<Entry>(selection);
        auto result = entry.get();
        entries.emplace_back(std::move(entry));
        return *result;
    }

    [[nodiscard]] ShaderFamily::Status status() const noexcept {
        return {.ready = active != nullptr, .revision = revision};
    }

    void publish_locked(Entry &entry) noexcept {
        Entry *next = entry.state == FamilyLoadState::Ready ? &entry : nullptr;
        if (next != active) {
            active = next;
            for (size_t i = 0u; i < program_bindings.size(); ++i) {
                *program_bindings[i].slot =
                    active ? active->programs[i] : nullptr;
            }
            ++revision;
        }
    }

    void schedule_locked(Entry &entry) {
        entry.state = FamilyLoadState::Loading;
        load_counter.add();
        try {
            luisa::fiber::schedule([this, entry_ptr = &entry] {
                auto finish = vstd::scope_exit([this] { load_counter.done(); });
                ShaderManager::VariantFamilyResolution resolution;
                auto resolved = false;
                try {
                    resolved = resolver(name, entry_ptr->selection, resolution);
                } catch (...) {
                    resolved = false;
                }

                luisa::vector<ShaderBase const *> loaded_programs(
                    program_bindings.size(), nullptr);
                auto loaded = resolved &&
                              resolution.selection == entry_ptr->selection &&
                              resolution.programs.size() == program_bindings.size();
                luisa::vector<ShaderManager::VariantResolution const *> resolved_programs(
                    program_bindings.size(), nullptr);
                if (loaded) {
                    for (size_t i = 0u; i < program_bindings.size(); ++i) {
                        for (auto const &program : resolution.programs) {
                            if (program.logical_name ==
                                program_bindings[i].logical_name) {
                                if (resolved_programs[i] != nullptr ||
                                    !(program.selection == resolution.selection) ||
                                    program.build_id != resolution.build_id) {
                                    loaded = false;
                                    break;
                                }
                                resolved_programs[i] = &program;
                            }
                        }
                        if (!loaded || resolved_programs[i] == nullptr) {
                            loaded = false;
                            break;
                        }
                    }
                }
                if (loaded) {
                    std::atomic_bool all_loaded{true};
                    luisa::fiber::counter program_counter;
                    luisa::fiber::async_parallel(
                        program_counter,
                        program_bindings.size(),
                        [&](size_t i) noexcept {
                            try {
                                auto const &binding = program_bindings[i];
                                auto const &artifact =
                                    resolved_programs[i]->artifact;
                                ShaderBase const *shader{};
                                if (binding.load_variant) {
                                    shader = binding.load_variant(
                                        artifact,
                                        resolution.selection);
                                } else {
                                    shader = binding.load(artifact);
                                }
                                loaded_programs[i] = shader;
                                if (!shader) {
                                    all_loaded.store(
                                        false, std::memory_order_relaxed);
                                }
                            } catch (...) {
                                all_loaded.store(false, std::memory_order_relaxed);
                            }
                        });
                    program_counter.wait();
                    loaded = all_loaded.load(std::memory_order_relaxed);
                    if (loaded) {
                        for (auto shader : loaded_programs) {
                            if (!shader) {
                                loaded = false;
                                break;
                            }
                        }
                    }
                }

                {
                    std::lock_guard lock{mutex};
                    if (loaded) {
                        entry_ptr->programs = std::move(loaded_programs);
                        entry_ptr->state = FamilyLoadState::Ready;
                    } else {
                        entry_ptr->programs.clear();
                        entry_ptr->state = FamilyLoadState::Failed;
                    }
                    completion_epoch.fetch_add(1u, std::memory_order_release);
                }
                if (!loaded) {
                    LUISA_ERROR(
                        "Shader family {} failed to load its exact {}-program variant. "
                        "The shader build or installation is incomplete.",
                        name,
                        program_bindings.size());
                }
            });
        } catch (...) {
            entry.state = FamilyLoadState::Failed;
            load_counter.done();
            throw;
        }
    }

    [[nodiscard]] ShaderFamily::Status refresh(uint64_t new_feature_mask) {
        std::lock_guard lock{mutex};
        if (!requested || new_feature_mask != feature_mask) {
            ShaderManager::VariantSelection selected;
            if (!selector(name, new_feature_mask, selected)) {
                LUISA_ERROR(
                    "Shader family {} has no unique exact variant for scene feature mask {}.",
                    name,
                    new_feature_mask);
            }
            feature_mask = new_feature_mask;
            requested_selection = std::move(selected);
            requested = true;
        }

        auto &entry = entry_locked(requested_selection);
        publish_locked(entry);
        if (entry.state == FamilyLoadState::Unloaded) {
            schedule_locked(entry);
        } else if (entry.state == FamilyLoadState::Failed) {
            LUISA_ERROR(
                "Shader family {} exact variant previously failed to load.",
                name);
        }
        observed_completion_epoch =
            completion_epoch.load(std::memory_order_acquire);
        return status();
    }

    [[nodiscard]] ShaderFamily::Status acquire(uint64_t new_feature_mask) {
#ifndef NDEBUG
        auto const current_thread = std::this_thread::get_id();
        if (owner_thread == std::thread::id{}) {
            owner_thread = current_thread;
        } else {
            LUISA_DEBUG_ASSERT(
                owner_thread == current_thread,
                "Shader family {} must be acquired from one render thread.",
                name);
        }
#endif
        auto const epoch = completion_epoch.load(std::memory_order_acquire);
        if (requested &&
            new_feature_mask == feature_mask &&
            epoch == observed_completion_epoch) {
            return status();
        }
        return refresh(new_feature_mask);
    }
};

ShaderFamily::ShaderFamily(
    luisa::string_view family_name,
    std::initializer_list<Program> programs,
    Selector selector,
    Resolver resolver)
    : _impl{luisa::make_unique<Impl>(
          family_name,
          programs,
          selector,
          resolver)} {}

ShaderFamily::~ShaderFamily() {
    wait();
}

void ShaderFamily::prefetch(SceneShaderFeatureSnapshot features) {
    (void)acquire(features);
}

ShaderFamily::Status ShaderFamily::acquire(SceneShaderFeatureSnapshot features) {
    return _impl->acquire(features.mask);
}

void ShaderFamily::wait() {
    _impl->load_counter.wait();
}

bool ShaderFamily::select_with_shader_manager(
    luisa::string_view family_name,
    uint64_t scene_feature_mask,
    ShaderManager::VariantSelection &selection) {
    return ShaderManager::instance()->select_shader_family_variant(
        family_name, scene_feature_mask, selection);
}

bool ShaderFamily::resolve_with_shader_manager(
    luisa::string_view family_name,
    ShaderManager::VariantSelection const &selection,
    ShaderManager::VariantFamilyResolution &resolution) {
    return ShaderManager::instance()->resolve_shader_family(
        family_name, selection, resolution);
}

}// namespace rbc

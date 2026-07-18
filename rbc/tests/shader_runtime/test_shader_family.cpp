#include "test_util.h"

#include <atomic>
#include <cstdint>
#include <thread>

#include <luisa/core/fiber.h>
#include <rbc_graphics/shader_family.h>

namespace {

constexpr uint64_t complex_feature = 1u;
std::atomic_uint32_t selector_calls{};
std::atomic_uint32_t resolver_calls{};
std::atomic_uint32_t loader_calls{};
std::atomic_bool block_last_program{};
std::atomic_bool release_last_program{};
constexpr char const *program_names[]{"program_0", "program_1", "program_2"};
constexpr char const *simple_artifacts[]{
    "simple_0.bin", "simple_1.bin", "simple_2.bin"};
constexpr char const *complex_artifacts[]{
    "complex_0.bin", "complex_1.bin", "complex_2.bin"};

bool fake_select(
    luisa::string_view family_name,
    uint64_t feature_mask,
    rbc::ShaderManager::VariantSelection &selection) {
    if (family_name != "test_family") return false;
    selector_calls.fetch_add(1u, std::memory_order_relaxed);
    selection = {};
    selection.set(
        "material",
        (feature_mask & complex_feature) != 0u ? "complex" : "simple");
    return true;
}

bool fake_resolve(
    luisa::string_view family_name,
    rbc::ShaderManager::VariantSelection const &selection,
    rbc::ShaderManager::VariantFamilyResolution &resolution) {
    if (family_name != "test_family") return false;
    resolver_calls.fetch_add(1u, std::memory_order_relaxed);
    resolution = {};
    resolution.selection = selection;
    resolution.build_id = "test-build";
    auto const complex = selection.values.front().second == "complex";
    // Return manifest members in reverse order. ShaderFamily must bind by the
    // logical program name, not by JSON array position.
    for (size_t i = 0u; i < 3u; ++i) {
        auto const program_index = 2u - i;
        rbc::ShaderManager::VariantResolution program;
        program.logical_name = program_names[program_index];
        program.selection = selection;
        program.build_id = resolution.build_id;
        program.manifest_backed = true;
        program.artifact = complex
                               ? complex_artifacts[program_index]
                               : simple_artifacts[program_index];
        resolution.programs.emplace_back(std::move(program));
    }
    return true;
}

luisa::compute::ShaderBase const *fake_load(
    size_t program_index,
    luisa::filesystem::path const &artifact) {
    loader_calls.fetch_add(1u, std::memory_order_relaxed);
    if (program_index == 2u &&
        block_last_program.load(std::memory_order_acquire)) {
        while (!release_last_program.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
    }
    auto const complex =
        artifact == luisa::filesystem::path{complex_artifacts[program_index]};
    if (!complex &&
        artifact != luisa::filesystem::path{simple_artifacts[program_index]}) {
        return nullptr;
    }
    auto const address = uintptr_t{1u + program_index + (complex ? 16u : 0u)};
    return reinterpret_cast<luisa::compute::ShaderBase const *>(address);
}

luisa::compute::ShaderBase const *fake_load_0(
    luisa::filesystem::path const &artifact) {
    return fake_load(0u, artifact);
}

luisa::compute::ShaderBase const *fake_load_1(
    luisa::filesystem::path const &artifact) {
    return fake_load(1u, artifact);
}

luisa::compute::ShaderBase const *fake_load_2(
    luisa::filesystem::path const &artifact) {
    return fake_load(2u, artifact);
}

void reset_fakes() {
    selector_calls.store(0u, std::memory_order_relaxed);
    resolver_calls.store(0u, std::memory_order_relaxed);
    loader_calls.store(0u, std::memory_order_relaxed);
    block_last_program.store(false, std::memory_order_relaxed);
    release_last_program.store(false, std::memory_order_relaxed);
}

}// namespace

TEST_SUITE("shader family") {
    TEST_CASE("exact variants publish atomically and cache by selection") {
        reset_fakes();
        luisa::fiber::scheduler scheduler;
        luisa::compute::ShaderBase const *programs[3]{};
        rbc::ShaderFamily family{
            "test_family",
            {
                {program_names[0], &programs[0], fake_load_0},
                {program_names[1], &programs[1], fake_load_1},
                {program_names[2], &programs[2], fake_load_2},
            },
            fake_select,
            fake_resolve};

        auto loading_simple = family.acquire({.mask = 0u});
        CHECK_FALSE(loading_simple);
        CHECK(loading_simple.revision == 0u);
        family.wait();

        auto simple = family.acquire({.mask = 0u});
        REQUIRE(simple);
        CHECK(simple.revision == 1u);
        CHECK(programs[0] ==
              reinterpret_cast<luisa::compute::ShaderBase const *>(1u));

        auto stable = family.acquire({.mask = 0u});
        CHECK(stable.revision == simple.revision);
        CHECK(selector_calls.load(std::memory_order_relaxed) == 1u);

        auto loading_complex = family.acquire({.mask = complex_feature});
        CHECK_FALSE(loading_complex);
        CHECK(loading_complex.revision == 2u);
        CHECK(programs[0] == nullptr);
        family.wait();

        auto complex = family.acquire({.mask = complex_feature});
        REQUIRE(complex);
        CHECK(complex.revision == 3u);
        CHECK(programs[0] ==
              reinterpret_cast<luisa::compute::ShaderBase const *>(17u));

        // An unrelated scene bit maps to the already-loaded simple selection.
        auto simple_again = family.acquire({.mask = 2u});
        REQUIRE(simple_again);
        CHECK(simple_again.revision == 4u);
        CHECK(resolver_calls.load(std::memory_order_relaxed) == 2u);
        CHECK(loader_calls.load(std::memory_order_relaxed) == 6u);
    }

    TEST_CASE("a partially loaded family is never visible") {
        reset_fakes();
        luisa::fiber::scheduler scheduler;
        block_last_program.store(true, std::memory_order_release);
        luisa::compute::ShaderBase const *programs[3]{};
        rbc::ShaderFamily family{
            "test_family",
            {
                {program_names[0], &programs[0], fake_load_0},
                {program_names[1], &programs[1], fake_load_1},
                {program_names[2], &programs[2], fake_load_2},
            },
            fake_select,
            fake_resolve};

        auto first = family.acquire({.mask = 0u});
        CHECK_FALSE(first);
        auto still_loading = family.acquire({.mask = 0u});
        CHECK_FALSE(still_loading);
        CHECK(programs[0] == nullptr);
        CHECK(programs[1] == nullptr);
        CHECK(programs[2] == nullptr);

        release_last_program.store(true, std::memory_order_release);
        family.wait();
        auto ready = family.acquire({.mask = 0u});
        REQUIRE(ready);
        CHECK(ready.revision == 1u);
        CHECK(programs[0] != nullptr);
        CHECK(programs[1] != nullptr);
        CHECK(programs[2] != nullptr);
    }
}

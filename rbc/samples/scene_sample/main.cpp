#include <luisa/core/logging.h>
#include "rbc_world/ecs.h"
#include "rbc_world/scene.h"
#include "rbc_world/system.h"
#include <chrono>
#include <iostream>
#include <iomanip>

using namespace rbc;
using namespace luisa;

// Performance measurement utilities
class Timer {
public:
    Timer() : start_(std::chrono::high_resolution_clock::now()) {}

    double elapsed_ms() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start_).count();
    }

    void reset() {
        start_ = std::chrono::high_resolution_clock::now();
    }

private:
    std::chrono::high_resolution_clock::time_point start_;
};

// Test scenario types
enum class HierarchyType {
    Flat,// All entities at root level
    Deep,// Long chain of parent-child
    Wide // Many children per parent
};

// Test results structure
struct TestResults {
    size_t entity_count;
    HierarchyType hierarchy_type;
    double creation_time_ms;
    double avg_update_time_ms;
    double min_update_time_ms;
    double max_update_time_ms;
    bool correctness_passed;

    void print() const {
        const char *type_str = "";
        switch (hierarchy_type) {
            case HierarchyType::Flat: type_str = "Flat"; break;
            case HierarchyType::Deep: type_str = "Deep"; break;
            case HierarchyType::Wide: type_str = "Wide"; break;
        }

        std::cout << "  Entities: " << std::setw(6) << entity_count
                  << " | Type: " << std::setw(5) << type_str
                  << " | Creation: " << std::setw(8) << std::fixed << std::setprecision(2) << creation_time_ms << " ms"
                  << " | Update (avg): " << std::setw(8) << avg_update_time_ms << " ms"
                  << " | Update (min/max): " << min_update_time_ms << "/" << max_update_time_ms << " ms"
                  << " | Correctness: " << (correctness_passed ? "PASS" : "FAIL")
                  << std::endl;
    }
};

class SceneSampleTest {
public:
    SceneSampleTest() = default;

    void run_all_tests() {
        std::cout << "\n=== Transform System Performance Test ===" << std::endl;
        std::cout << "\nStarting comprehensive scene hierarchy tests...\n"
                  << std::endl;

        // Test configurations
        // std::vector<size_t> entity_counts = {100, 1000, 10000};
        std::vector<size_t> entity_counts = {100, 1000};
        std::vector<HierarchyType> hierarchy_types = {
            HierarchyType::Flat,
            HierarchyType::Deep,
            HierarchyType::Wide};

        for (auto count : entity_counts) {
            for (auto type : hierarchy_types) {
                run_single_test(count, type);
            }
        }

        std::cout << "\n=== All Tests Completed ===" << std::endl;
    }

private:
    void run_single_test(size_t entity_count, HierarchyType hierarchy_type) {
        EntityManager entity_manager;
        SystemScheduler scheduler;

        // Register transform system
        scheduler.register_system<TransformSystem>(&entity_manager);

        TestResults results;
        results.entity_count = entity_count;
        results.hierarchy_type = hierarchy_type;

        // Create entities
        Timer creation_timer;
        vector<EntityID> entities;
        create_entities(entity_manager, entity_count, hierarchy_type, entities);
        results.creation_time_ms = creation_timer.elapsed_ms();

        // Warm-up run
        scheduler.update_all_systems(0.016f);

        // Performance measurement
        const size_t num_frames = 1000;
        double total_time = 0.0;
        double min_time = std::numeric_limits<double>::max();
        double max_time = 0.0;

        for (size_t i = 0; i < num_frames; ++i) {
            Timer update_timer;
            scheduler.update_all_systems(0.016f);
            double frame_time = update_timer.elapsed_ms();

            total_time += frame_time;
            min_time = std::min(min_time, frame_time);
            max_time = std::max(max_time, frame_time);
        }

        results.avg_update_time_ms = total_time / num_frames;
        results.min_update_time_ms = min_time;
        results.max_update_time_ms = max_time;

        // Correctness check
        results.correctness_passed = verify_hierarchy_correctness(entity_manager, entities, hierarchy_type);

        // Print results
        results.print();
    }

    void create_entities(EntityManager &manager, size_t count,
                         HierarchyType type, vector<EntityID> &out_entities) {
        out_entities.clear();
        out_entities.reserve(count);

        switch (type) {
            case HierarchyType::Flat:
                create_flat_hierarchy(manager, count, out_entities);
                break;
            case HierarchyType::Deep:
                create_deep_hierarchy(manager, count, out_entities);
                break;
            case HierarchyType::Wide:
                create_wide_hierarchy(manager, count, out_entities);
                break;
        }
    }

    void create_flat_hierarchy(EntityManager &manager, size_t count, vector<EntityID> &out_entities) {
        // All entities at root level (no parent-child relationships)
        for (size_t i = 0; i < count; ++i) {
            EntityID entity = manager.create_entity();
            auto &transform = manager.add_component<TransformComponent>(entity);

            transform.position = make_float3(
                static_cast<float>(i % 100),
                static_cast<float>((i / 100) % 100),
                static_cast<float>(i / 10000));
            transform.rotation = make_float4(0.f, 0.f, 0.f, 1.f);
            transform.scaling = make_float3(1.f, 1.f, 1.f);

            out_entities.push_back(entity);
        }
    }

    void create_deep_hierarchy(EntityManager &manager, size_t count, vector<EntityID> &out_entities) {
        // Create a long chain: root -> child1 -> child2 -> ... -> childN
        EntityID parent = INVALID_ENTITY;

        for (size_t i = 0; i < count; ++i) {
            EntityID entity = manager.create_entity();
            auto &transform = manager.add_component<TransformComponent>(entity);

            transform.position = make_float3(0.f, static_cast<float>(i) * 0.1f, 0.f);
            transform.rotation = make_float4(0.f, 0.f, 0.f, 1.f);
            transform.scaling = make_float3(1.f, 1.f, 1.f);

            if (parent != INVALID_ENTITY) {
                transform.parent = parent;
                auto *parent_transform = manager.get_component<TransformComponent>(parent);
                if (parent_transform) {
                    parent_transform->children.push_back(entity);
                }
            }

            out_entities.push_back(entity);
            parent = entity;
        }
    }

    void create_wide_hierarchy(EntityManager &manager, size_t count, vector<EntityID> &out_entities) {
        // Create wide tree: one root with many children, each child has several children
        const size_t children_per_node = 10;

        // Create root
        EntityID root = manager.create_entity();
        auto &root_transform = manager.add_component<TransformComponent>(root);
        root_transform.position = make_float3(0.f, 0.f, 0.f);
        root_transform.rotation = make_float4(0.f, 0.f, 0.f, 1.f);
        root_transform.scaling = make_float3(2.f, 2.f, 2.f);
        out_entities.push_back(root);

        size_t created = 1;
        vector<EntityID> current_layer = {root};

        // Create layers
        while (created < count) {
            vector<EntityID> next_layer;

            for (EntityID parent : current_layer) {
                auto *parent_transform = manager.get_component<TransformComponent>(parent);
                if (!parent_transform) continue;

                for (size_t i = 0; i < children_per_node && created < count; ++i) {
                    EntityID child = manager.create_entity();
                    auto &child_transform = manager.add_component<TransformComponent>(child);

                    child_transform.position = make_float3(
                        static_cast<float>(i) - static_cast<float>(children_per_node) * 0.5f,
                        1.f,
                        0.f);
                    child_transform.rotation = make_float4(0.f, 0.f, 0.f, 1.f);
                    child_transform.scaling = make_float3(0.8f, 0.8f, 0.8f);
                    child_transform.parent = parent;

                    parent_transform->children.push_back(child);

                    out_entities.push_back(child);
                    next_layer.push_back(child);
                    ++created;
                }
            }

            current_layer = std::move(next_layer);
        }
    }

    bool verify_hierarchy_correctness(EntityManager &manager,
                                      const vector<EntityID> &entities,
                                      HierarchyType type) {
        if (entities.empty()) return false;

        switch (type) {
            case HierarchyType::Flat:
                return verify_flat_hierarchy(manager, entities);
            case HierarchyType::Deep:
                return verify_deep_hierarchy(manager, entities);
            case HierarchyType::Wide:
                return verify_wide_hierarchy(manager, entities);
        }

        return false;
    }

    bool verify_flat_hierarchy(EntityManager &manager, const vector<EntityID> &entities) {
        // All entities should have no parent and identity-scaled global transforms
        for (EntityID entity : entities) {
            auto *transform = manager.get_component<TransformComponent>(entity);
            if (!transform) return false;
            if (transform->parent != INVALID_ENTITY) return false;
            if (transform->global_dirty) return false;// Should be clean after update
        }
        return true;
    }

    bool verify_deep_hierarchy(EntityManager &manager, const vector<EntityID> &entities) {
        // Verify chain structure and accumulated transforms
        for (size_t i = 1; i < entities.size(); ++i) {
            auto *transform = manager.get_component<TransformComponent>(entities[i]);
            if (!transform) return false;

            // Each entity (except root) should have previous entity as parent
            if (transform->parent != entities[i - 1]) return false;
            if (transform->global_dirty) return false;

            // Global position should accumulate (y = 0.1 * i)
            float expected_y = static_cast<float>(i) * 0.1f;
            float actual_y = transform->global_transform[3][1];

            if (std::abs(actual_y - expected_y) > 0.01f) {
                LUISA_ERROR("Start Fail layer {}", i);
                return false;
            }
        }
        return true;
    }

    bool verify_wide_hierarchy(EntityManager &manager, const vector<EntityID> &entities) {
        // Verify tree structure
        for (EntityID entity : entities) {
            auto *transform = manager.get_component<TransformComponent>(entity);
            if (!transform) return false;
            if (transform->global_dirty) return false;

            // Verify children have correct parent reference
            for (EntityID child : transform->children) {
                auto *child_transform = manager.get_component<TransformComponent>(child);
                if (!child_transform) return false;
                if (child_transform->parent != entity) return false;
            }
        }
        return true;
    }
};

int main(int argc, char *argv[]) {
    std::cout << "Scene Sample Performance Test" << std::endl;
    std::cout << "Testing Transform System with ECS Architecture\n"
              << std::endl;

    SceneSampleTest test;
    test.run_all_tests();

    return 0;
}

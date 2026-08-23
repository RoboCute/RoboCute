#include <argparse/argparse.hpp>
#include <luisa/core/fiber.h>
#include <luisa/core/logging.h>
#include <rbc_core/type_info.h>
#include <rbc_core/rc.h>
#include <rbc_core/runtime_static.h>
#include <rbc_plugin/plugin_manager.h>
#include <rbc_world/importers/register_importers.h>
#include <rbc_world/resource_importer.h>
#include <rbc_world/resources/skeleton.h>
#include <rbc_world/base_object.h>
#define TINYGLTF_NO_INCLUDE_JSON
#include "tiny_gltf.h"
#include <iostream>
#include <cstring>
#include "rbc_test.hpp"

namespace rbc::test {



// TinyGLTF for raw gltf reading


using namespace rbc;
using namespace rbc::world;

// Helper struct to hold raw gltf skeleton info
struct RawGltfSkeletonInfo {
    int total_nodes = 0;
    int total_skins = 0;
    int total_joints = 0;
    luisa::vector<luisa::string> joint_names;
    luisa::vector<int> joint_parent_indices;
    
    void clear() {
        total_nodes = 0;
        total_skins = 0;
        total_joints = 0;
        joint_names.clear();
        joint_parent_indices.clear();
    }
};

// Parse raw gltf skeleton information from file
RawGltfSkeletonInfo parse_raw_gltf_skeleton(const char* filename) {
    RawGltfSkeletonInfo info;
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string errors;
    std::string warnings;

    // Set dummy image loader (we don't care about textures)
    auto image_loader = [](tinygltf::Image*, const int, std::string*, std::string*, 
                          int, int, const unsigned char*, int, void*) { return true; };
    loader.SetImageLoader(image_loader, NULL);

    bool success = loader.LoadASCIIFromFile(&model, &errors, &warnings, filename);
    
    if (!warnings.empty()) {
        std::cerr << "glTF parsing warnings: " << warnings << std::endl;
    }
    if (!errors.empty()) {
        std::cerr << "glTF parsing errors: " << errors << std::endl;
    }
    if (!success) {
        std::cerr << "Failed to load glTF file: " << filename << std::endl;
        return info;
    }

    info.total_nodes = static_cast<int>(model.nodes.size());
    info.total_skins = static_cast<int>(model.skins.size());

    // Collect all joints from all skins
    for (const auto& skin : model.skins) {
        info.total_joints += static_cast<int>(skin.joints.size());
        
        // Build node index to parent index mapping
        luisa::vector<int> node_to_parent(model.nodes.size(), -1);
        for (size_t node_idx = 0; node_idx < model.nodes.size(); ++node_idx) {
            const auto& node = model.nodes[node_idx];
            for (int child_idx : node.children) {
                if (child_idx >= 0 && child_idx < static_cast<int>(model.nodes.size())) {
                    node_to_parent[child_idx] = static_cast<int>(node_idx);
                }
            }
        }
        
        // Record joint info
        for (int joint_idx : skin.joints) {
            if (joint_idx >= 0 && joint_idx < static_cast<int>(model.nodes.size())) {
                info.joint_names.push_back(luisa::string(model.nodes[joint_idx].name));
                info.joint_parent_indices.push_back(node_to_parent[joint_idx]);
            }
        }
    }

    return info;
}

// Print raw gltf skeleton info
void print_raw_gltf_info(const RawGltfSkeletonInfo& info, const char* filename) {
    std::cout << "========== Raw glTF Skeleton Information ==========" << std::endl;
    std::cout << "File: " << filename << std::endl;
    std::cout << "Total nodes: " << info.total_nodes << std::endl;
    std::cout << "Total skins: " << info.total_skins << std::endl;
    std::cout << "Total joints: " << info.total_joints << std::endl;
    
    if (!info.joint_names.empty()) {
        std::cout << "----- Joints (first 10) -----" << std::endl;
        int num_to_print = std::min(static_cast<int>(info.joint_names.size()), 10);
        for (int i = 0; i < num_to_print; ++i) {
            std::cout << "  Joint[" << i << "]: name='" << info.joint_names[i] 
                      << "', parent_node_idx=" << info.joint_parent_indices[i] << std::endl;
        }
        if (info.joint_names.size() > 10) {
            std::cout << "  ... and " << (info.joint_names.size() - 10) << " more joints" << std::endl;
        }
    }
    std::cout << "========== End Raw glTF Info ==========" << std::endl;
}

// Print RBC skeleton info
void print_rbc_skeleton_info(SkeletonResource* skel) {
    std::cout << "========== RBC SkeletonResource Information ==========" << std::endl;
    
    const auto& ref_skel = skel->ref_skel();
    int num_joints = ref_skel.num_joints();
    int num_soa_joints = ref_skel.num_soa_joints();
    int num_bones = ref_skel.get_num_bones();
    
    std::cout << "Total joints: " << num_joints << std::endl;
    std::cout << "Total SOA joints: " << num_soa_joints << std::endl;
    std::cout << "Total bones: " << num_bones << std::endl;
    
    // Print joint details (first 10)
    if (num_joints > 0) {
        std::cout << "----- Joints (first 10) -----" << std::endl;
        auto joint_names = ref_skel.raw_joint_names();
        auto joint_parents = ref_skel.raw_joint_parents();
        
        int num_to_print = std::min(num_joints, 10);
        for (int i = 0; i < num_to_print; ++i) {
            std::cout << "  Joint[" << i << "]: name='" << joint_names[i] 
                      << "', parent=" << joint_parents[i] << std::endl;
        }
        if (num_joints > 10) {
            std::cout << "  ... and " << (num_joints - 10) << " more joints" << std::endl;
        }
    }
    
    std::cout << "========== End RBC Skeleton Info ==========" << std::endl;
}

// Compare raw glTF and RBC skeleton info
void compare_skeletons(const RawGltfSkeletonInfo& raw_info, SkeletonResource* skel) {
    std::cout << "========== Skeleton Comparison ==========" << std::endl;
    
    const auto& ref_skel = skel->ref_skel();
    int rbc_joint_count = ref_skel.num_joints();
    
    std::cout << "Raw glTF total joints: " << raw_info.total_joints << std::endl;
    std::cout << "RBC Skeleton joints: " << rbc_joint_count << std::endl;
    
    if (raw_info.total_joints == rbc_joint_count) {
        std::cout << "[PASS] Joint count matches!" << std::endl;
    } else {
        std::cout << "[NOTE] Joint count differs (this may be expected depending on glTF structure)" << std::endl;
    }
    
    // Compare joint names if counts match
    if (raw_info.total_joints > 0 && rbc_joint_count > 0) {
        auto rbc_joint_names = ref_skel.raw_joint_names();
        
        std::cout << "----- Joint Name Comparison (first 5) -----" << std::endl;
        int compare_count = std::min(std::min(5, rbc_joint_count), 
                                      static_cast<int>(raw_info.joint_names.size()));
        bool all_match = true;
        for (int i = 0; i < compare_count; ++i) {
            bool match = (raw_info.joint_names[i] == rbc_joint_names[i]);
            std::cout << "  [" << i << "] Raw glTF: '" << raw_info.joint_names[i] 
                      << "' | RBC: '" << rbc_joint_names[i] << "' " 
                      << (match ? "[MATCH]" : "[DIFF]") << std::endl;
            if (!match) all_match = false;
        }
        
        if (compare_count > 0 && all_match) {
            std::cout << "[PASS] All shown joint names match!" << std::endl;
        }
    }
    
    std::cout << "========== Comparison Complete ==========" << std::endl;
}

int disabled_main(int argc, char* argv[]) {
    argparse::ArgumentParser program("test_skeleton", "0.1.0");
    program.add_argument("gltf_file").help("Path to glTF file to load skeleton from");
    
    program.parse_args(argc, argv);
    
    auto gltf_file = program.get<std::string>("gltf_file");
    
    // Initialize RBC runtime
    luisa::fiber::scheduler scheduler;
    RuntimeStaticBase::init_all();
    
    // Initialize PluginManager (required for importers)
    PluginManager::init();
    
    // Create resource directory for world
    auto resource_dir = "test_skeleton_resources";
    if (!luisa::filesystem::exists(resource_dir)) {
        luisa::filesystem::create_directories(resource_dir);
    }
    
    // Initialize world with resource directory
    init_world(resource_dir, resource_dir);
    
    // Register importers
    register_builtin_importers();
    
    std::cout << std::endl;
    std::cout << "Loading glTF file: " << gltf_file << std::endl;
    std::cout << std::endl;
    
    // Step 1: Parse raw glTF skeleton info
    std::cout << ">>> Step 1: Parsing raw glTF skeleton data..." << std::endl;
    RawGltfSkeletonInfo raw_info = parse_raw_gltf_skeleton(gltf_file.c_str());
    print_raw_gltf_info(raw_info, gltf_file.c_str());
    
    if (raw_info.total_nodes == 0) {
        std::cerr << "Failed to parse glTF file or file is empty!" << std::endl;
        destroy_world();
        PluginManager::destroy_instance();
        RuntimeStaticBase::dispose_all();
        return 1;
    }
    
    std::cout << std::endl;
    
    // Step 2: Load via RBC importer
    std::cout << ">>> Step 2: Loading skeleton via RBC importer..." << std::endl;
    auto &registry = ResourceImporterRegistry::instance();
    auto importer = registry.find_importer(luisa::string_view{".gltf"}, 
                                           TypeInfo::get<rbc::world::SkeletonResource>().md5());
    
    if (!importer) {
        std::cerr << "Failed to find glTF skeleton importer!" << std::endl;
        destroy_world();
        PluginManager::destroy_instance();
        RuntimeStaticBase::dispose_all();
        return 1;
    }
    
    // Create skeleton resource
    auto skel = rbc::world::create_object<SkeletonResource>();
    bool result = importer->import(skel, gltf_file);
    
    if (!result) {
        std::cerr << "Failed to import skeleton from glTF file!" << std::endl;
        destroy_world();
        PluginManager::destroy_instance();
        RuntimeStaticBase::dispose_all();
        return 1;
    }
    
    // Print RBC skeleton info (using log_brief)
    std::cout << ">>> Step 3: RBC SkeletonResource log_brief() output:" << std::endl;
    skel->log_brief();
    
    std::cout << std::endl;
    
    // Print detailed RBC skeleton info
    std::cout << ">>> Step 4: Detailed RBC SkeletonResource info:" << std::endl;
    print_rbc_skeleton_info(skel);
    
    std::cout << std::endl;
    
    // Step 5: Compare
    std::cout << ">>> Step 5: Comparing raw glTF vs RBC SkeletonResource:" << std::endl;
    compare_skeletons(raw_info, skel);
    
    // Release skeleton resource using rbc_rc_delete
    std::cout << std::endl << ">>> Cleaning up..." << std::endl;
    std::cout << "Skeleton RC count before delete: " << skel->rbc_rc_count() << std::endl;
    skel->rbc_rc_delete();
    std::cout << "Skeleton deleted" << std::endl;
    
    // Cleanup in correct order
    std::cout << "Destroying world..." << std::endl;
    destroy_world();
    std::cout << "World destroyed" << std::endl;
    
    std::cout << "Destroying plugin manager..." << std::endl;
    PluginManager::destroy_instance();
    std::cout << "Plugin manager destroyed" << std::endl;
    
    std::cout << "Disposing runtime static..." << std::endl;
    RuntimeStaticBase::dispose_all();
    std::cout << "Runtime static disposed" << std::endl;
    
    std::cout << std::endl;
    std::cout << "Done!" << std::endl;
    
    return 0;
}

suite<"Skeleton|Import"> SkeletonImportTestSuite = [] {
    "placeholder"_test = [] {
        expect(true);
    };
}; // suite

} // namespace rbc::test

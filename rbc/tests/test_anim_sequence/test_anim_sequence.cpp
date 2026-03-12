#include <argparse/argparse.hpp>
#include <luisa/core/fiber.h>
#include <luisa/core/logging.h>

#include <rbc_core/type_info.h>
#include <rbc_core/rc.h>
#include <rbc_core/runtime_static.h>
#include <rbc_plugin/plugin_manager.h>
#include <rbc_world/importers/register_importers.h>
#include <rbc_world/resource_importer.h>
#include <rbc_world/resources/anim_sequence.h>
#include <rbc_world/resources/skeleton.h>
#include <rbc_world/base_object.h>

// TinyGLTF for raw gltf reading
#define TINYGLTF_NO_INCLUDE_JSON
#include "tiny_gltf.h"

#include <iostream>
#include <cstring>

using namespace rbc;
using namespace rbc::world;

// Helper struct to hold raw gltf animation info
struct RawGltfAnimInfo {
    int total_animations = 0;
    luisa::vector<luisa::string> anim_names;
    luisa::vector<float> anim_durations;
    
    void clear() {
        total_animations = 0;
        anim_names.clear();
        anim_durations.clear();
    }
};

// Parse raw gltf animation information from file
RawGltfAnimInfo parse_raw_gltf_anim(const char* filename) {
    RawGltfAnimInfo info;
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

    info.total_animations = static_cast<int>(model.animations.size());

    // Collect animation info
    for (const auto& anim : model.animations) {
        info.anim_names.push_back(luisa::string(anim.name));
        
        // Calculate duration from samplers
        float max_time = 0.0f;
        for (const auto& sampler : anim.samplers) {
            // Find input accessor for time values
            if (sampler.input >= 0 && sampler.input < static_cast<int>(model.accessors.size())) {
                const auto& accessor = model.accessors[sampler.input];
                const auto& bufferView = model.bufferViews[accessor.bufferView];
                const auto& buffer = model.buffers[bufferView.buffer];
                
                // Read time values (float array)
                const float* time_data = reinterpret_cast<const float*>(
                    buffer.data.data() + bufferView.byteOffset + accessor.byteOffset);
                
                // Find max time
                for (size_t i = 0; i < accessor.count; ++i) {
                    if (time_data[i] > max_time) {
                        max_time = time_data[i];
                    }
                }
            }
        }
        info.anim_durations.push_back(max_time);
    }

    return info;
}

// Print raw gltf animation info
void print_raw_gltf_info(const RawGltfAnimInfo& info, const char* filename) {
    std::cout << "========== Raw glTF Animation Information ==========" << std::endl;
    std::cout << "File: " << filename << std::endl;
    std::cout << "Total animations: " << info.total_animations << std::endl;
    
    for (size_t i = 0; i < info.anim_names.size(); ++i) {
        std::cout << "  Animation[" << i << "]: name='" << info.anim_names[i] 
                  << "', duration=" << info.anim_durations[i] << "s" << std::endl;
    }
    std::cout << "========== End Raw glTF Info ==========" << std::endl;
}

// Print RBC animation sequence info
void print_rbc_anim_info(AnimSequenceResource* anim) {
    std::cout << "========== RBC AnimSequenceResource Information ==========" << std::endl;
    
    const auto& seq = anim->ref_seq();
    int num_tracks = seq.GetNumTracks();
    int num_soa_tracks = seq.GetNumSoaTracks();
    
    std::cout << "Total tracks: " << num_tracks << std::endl;
    std::cout << "Total SOA tracks: " << num_soa_tracks << std::endl;
    
    // Print raw animation info
    const auto& raw_anim = seq.GetRawAnim();
    std::cout << "Animation name: " << raw_anim.name() << std::endl;
    std::cout << "Animation duration: " << raw_anim.duration() << "s" << std::endl;
    std::cout << "Animation num_tracks: " << raw_anim.num_tracks() << std::endl;
    
    std::cout << "========== End RBC AnimSequence Info ==========" << std::endl;
}

// Compare raw glTF and RBC animation info
void compare_animations(const RawGltfAnimInfo& raw_info, AnimSequenceResource* anim) {
    std::cout << "========== Animation Comparison ==========" << std::endl;
    
    const auto& seq = anim->ref_seq();
    const auto& raw_anim = seq.GetRawAnim();
    
    std::cout << "Raw glTF total animations: " << raw_info.total_animations << std::endl;
    std::cout << "RBC Animation duration: " << raw_anim.duration() << "s" << std::endl;
    
    if (raw_info.total_animations > 0) {
        std::cout << "Raw glTF first animation duration: " << raw_info.anim_durations[0] << "s" << std::endl;
        
        // Compare durations (with some tolerance for sampling differences)
        float diff = std::abs(raw_info.anim_durations[0] - raw_anim.duration());
        if (diff < 0.1f) {
            std::cout << "[PASS] Animation duration matches (within tolerance)!" << std::endl;
        } else {
            std::cout << "[NOTE] Animation duration differs by " << diff << "s" << std::endl;
        }
        
        // Compare names
        std::cout << "Raw glTF first animation name: '" << raw_info.anim_names[0] << "'" << std::endl;
        std::cout << "RBC Animation name: '" << raw_anim.name() << "'" << std::endl;
    }
    
    std::cout << "========== Comparison Complete ==========" << std::endl;
}

int main(int argc, char* argv[]) {
    argparse::ArgumentParser program("test_anim_sequence", "0.1.0");
    program.add_argument("gltf_file").help("Path to glTF file to load animation from");
    
    try {
        program.parse_args(argc, argv);
    } catch (const std::exception& err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        return 1;
    }
    
    auto gltf_file = program.get<std::string>("gltf_file");
    
    // Initialize RBC runtime
    luisa::fiber::scheduler scheduler;
    RuntimeStaticBase::init_all();
    
    // Initialize PluginManager (required for importers)
    PluginManager::init();
    
    // Create resource directory for world
    auto resource_dir = "test_anim_sequence_resources";
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
    
    // Step 1: Parse raw glTF animation info
    std::cout << ">>> Step 1: Parsing raw glTF animation data..." << std::endl;
    RawGltfAnimInfo raw_info = parse_raw_gltf_anim(gltf_file.c_str());
    print_raw_gltf_info(raw_info, gltf_file.c_str());
    
    if (raw_info.total_animations == 0) {
        std::cerr << "No animations found in glTF file!" << std::endl;
        destroy_world();
        PluginManager::destroy_instance();
        RuntimeStaticBase::dispose_all();
        return 1;
    }
    
    std::cout << std::endl;
    
    // Step 2: Load skeleton first (required for animation)
    std::cout << ">>> Step 2: Loading skeleton via RBC importer..." << std::endl;
    auto &registry = ResourceImporterRegistry::instance();
    auto skel_importer = registry.find_importer(luisa::string_view{".gltf"}, 
                                                TypeInfo::get<rbc::world::SkeletonResource>().md5());
    
    if (!skel_importer) {
        std::cerr << "Failed to find glTF skeleton importer!" << std::endl;
        destroy_world();
        PluginManager::destroy_instance();
        RuntimeStaticBase::dispose_all();
        return 1;
    }
    
    // Create skeleton resource
    auto skel = rbc::world::create_object<SkeletonResource>();
    bool skel_result = skel_importer->import(skel, gltf_file);
    
    if (!skel_result) {
        std::cerr << "Failed to import skeleton from glTF file!" << std::endl;
        destroy_world();
        PluginManager::destroy_instance();
        RuntimeStaticBase::dispose_all();
        return 1;
    }
    
    std::cout << "Skeleton loaded successfully!" << std::endl;
    std::cout << "Skeleton joints: " << skel->ref_skel().NumJoints() << std::endl;
    
    std::cout << std::endl;
    
    // Step 3: Load animation via RBC importer
    std::cout << ">>> Step 3: Loading animation via RBC importer..." << std::endl;
    auto anim_importer = registry.find_importer(luisa::string_view{".gltf"}, 
                                                TypeInfo::get<rbc::world::AnimSequenceResource>().md5());
    
    if (!anim_importer) {
        std::cerr << "Failed to find glTF animation importer!" << std::endl;
        skel->rbc_rc_delete();
        destroy_world();
        PluginManager::destroy_instance();
        RuntimeStaticBase::dispose_all();
        return 1;
    }
    
    // Create animation sequence resource
    auto anim = rbc::world::create_object<AnimSequenceResource>();
    
    // Set the reference skeleton (required for animation import)
    anim->ref_skel = skel;
    
    bool anim_result = anim_importer->import(anim, gltf_file);
    
    if (!anim_result) {
        std::cerr << "Failed to import animation from glTF file!" << std::endl;
        anim->rbc_rc_delete();
        skel->rbc_rc_delete();
        destroy_world();
        PluginManager::destroy_instance();
        RuntimeStaticBase::dispose_all();
        return 1;
    }
    
    // Print RBC animation info (using log_brief)
    std::cout << ">>> Step 4: RBC AnimSequenceResource log_brief() output:" << std::endl;
    anim->log_brief();
    
    std::cout << std::endl;
    
    // Print detailed RBC animation info
    std::cout << ">>> Step 5: Detailed RBC AnimSequenceResource info:" << std::endl;
    print_rbc_anim_info(anim);
    
    std::cout << std::endl;
    
    // Step 6: Compare
    std::cout << ">>> Step 6: Comparing raw glTF vs RBC AnimSequenceResource:" << std::endl;
    compare_animations(raw_info, anim);
    
    // Release resources using rbc_rc_delete
    std::cout << std::endl << ">>> Cleaning up..." << std::endl;
    std::cout << "Animation RC count before delete: " << anim->rbc_rc_count() << std::endl;
    anim->rbc_rc_delete();
    std::cout << "Animation deleted" << std::endl;
    
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

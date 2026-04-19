#pragma once
#define TINYGLTF_NO_INCLUDE_JSON

#include "ozz/animation/offline/raw_animation_utils.h"
#include "ozz/animation/offline/tools/import2ozz.h"
#include "tiny_gltf.h"

namespace rbc {

class GltfOzzImporter : public ozz::animation::offline::OzzImporter {
public:
    GltfOzzImporter();

private:
    bool Load(const char *_filename) override;

    // Find all unique root joints of skeletons used by given skins and add them
    // to `roots`
    void find_skin_root_joint_indices(const ozz::vector<tinygltf::Skin> &skins, ozz::vector<int> &roots);

    bool Import(ozz::animation::offline::RawSkeleton *_skeleton, const NodeType &_types) override;

    // Recursively import a node's children
    bool import_node(const tinygltf::Node &_node, ozz::animation::offline::RawSkeleton::Joint *_joint);

    // Returns all animations in the gltf document.
    AnimationNames GetAnimationNames() override;

    bool Import(const char *_animation_name, const ozz::animation::Skeleton &skeleton, float _sampling_rate, ozz::animation::offline::RawAnimation *_animation) override;

    bool sample_animation_channel(
        const tinygltf::Model &_model, const tinygltf::AnimationSampler &_sampler, const std::string &_target_path, float _sampling_rate, float *_duration, ozz::animation::offline::RawAnimation::JointTrack *_track);

    // Returns all skins belonging to a given gltf scene
    ozz::vector<tinygltf::Skin> get_skins_for_scene(
        const tinygltf::Scene &_scene) const;

    const tinygltf::Node *find_node_by_name(const std::string &_name) const;

    // no support for user-defined tracks
    NodeProperties GetNodeProperties(const char *) override {
        return NodeProperties();
    }
    bool Import(const char *, const char *, const char *, NodeProperty::Type, float, ozz::animation::offline::RawFloatTrack *) override {
        return false;
    }
    bool Import(const char *, const char *, const char *, NodeProperty::Type, float, ozz::animation::offline::RawFloat2Track *) override {
        return false;
    }
    bool Import(const char *, const char *, const char *, NodeProperty::Type, float, ozz::animation::offline::RawFloat3Track *) override {
        return false;
    }
    bool Import(const char *, const char *, const char *, NodeProperty::Type, float, ozz::animation::offline::RawFloat4Track *) override {
        return false;
    }

    tinygltf::TinyGLTF _loader;
    tinygltf::Model _model;
};

}// namespace rbc
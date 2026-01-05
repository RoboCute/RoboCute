#include "jolt_component.h"
#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <rbc_world/type_register.h>
#include <rbc_core/runtime_static.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Constraints/TwoBodyConstraint.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <iostream>
namespace JPH {
#include "Layers.h"
}// namespace JPH
namespace rbc::world {
struct JoltInstance {
    JPH::TempAllocator *mTempAllocator{};
    JPH::JobSystem *mJobSystem{};
    JPH::PhysicsSystem *mSystem{};
    JPH::BPLayerInterfaceImpl mBroadPhaseLayerInterface;
    JPH::ObjectVsBroadPhaseLayerFilterImpl mObjectVsBroadPhaseLayerFilter;
    JPH::ObjectLayerPairFilterImpl mObjectVsObjectLayerFilter;
    void init() {
        if (JPH::Factory::sInstance) return;
        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();

        mTempAllocator = new JPH::TempAllocatorImpl(4 * 1024 * 1024);
        mJobSystem = new JPH::JobSystemThreadPool(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers);
        mSystem = new JPH::PhysicsSystem();
        mSystem->Init(1024, 0, 4096, 1024, mBroadPhaseLayerInterface, mObjectVsBroadPhaseLayerFilter, mObjectVsObjectLayerFilter);
    }

    ~JoltInstance() {
        delete mSystem;
        delete mJobSystem;
        delete mTempAllocator;
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }
    JPH::Body &CreateBody(const JPH::BodyCreationSettings &inSettings, JPH::EActivation inActivation) {
        JPH::Body &body = *mSystem->GetBodyInterface().CreateBody(inSettings);
        mSystem->GetBodyInterface().AddBody(body.GetID(), inActivation);
        return body;
    }
    JPH::Body &CreateBody(const JPH::ShapeSettings *inShapeSettings, JPH::RVec3Arg inPosition, JPH::QuatArg inRotation, JPH::EMotionType inMotionType, JPH::EMotionQuality inMotionQuality, JPH::ObjectLayer inLayer, JPH::EActivation inActivation) {
        JPH::BodyCreationSettings settings;
        settings.SetShapeSettings(inShapeSettings);
        settings.mPosition = inPosition;
        settings.mRotation = inRotation;
        settings.mMotionType = inMotionType;
        settings.mMotionQuality = inMotionQuality;
        settings.mObjectLayer = inLayer;
        settings.mLinearDamping = 0.0f;
        settings.mAngularDamping = 0.0f;
        settings.mCollisionGroup.SetGroupID(0);
        return CreateBody(settings, inActivation);
    }
    JPH::Body &CreateBox(JPH::RVec3Arg inPosition, JPH::QuatArg inRotation, JPH::EMotionType inMotionType, JPH::EMotionQuality inMotionQuality, JPH::ObjectLayer inLayer, JPH::Vec3Arg inHalfExtent, JPH::EActivation inActivation) {
        return CreateBody(new JPH::BoxShapeSettings(inHalfExtent), inPosition, inRotation, inMotionType, inMotionQuality, inLayer, inActivation);
    }
    void test() {
        using namespace JPH;
        BoxShapeSettings floor_shape_settings(Vec3(100.0f, 1.0f, 100.0f));
        floor_shape_settings.SetEmbedded();
        ShapeSettings::ShapeResult floor_shape_result = floor_shape_settings.Create();
        ShapeRefC floor_shape = floor_shape_result.Get();
        BodyCreationSettings floor_settings(floor_shape, RVec3(0.0, -1.0, 0.0), Quat::sIdentity(), EMotionType::Static, Layers::NON_MOVING);

        // Create the actual rigid body
        Body &floor = CreateBody(floor_settings, EActivation::DontActivate);// Note that if we run out of bodies this can return nullptr
        BodyCreationSettings creation_settings{};
        creation_settings.mMotionType = EMotionType::Dynamic;
        creation_settings.mMotionQuality = EMotionQuality::Discrete;
        creation_settings.mObjectLayer = Layers::MOVING;
        creation_settings.mPosition = RVec3(0, 3, 0);
        creation_settings.mFriction = 0.5f;
        creation_settings.mRestitution = 0.6f;
        creation_settings.SetShape(new BoxShape(RVec3(0.5, 0.5, 0.5)));
        (new BoxShape(
             Vec3(0.5f, 0.75f, 1.0f)),
         EMotionType::Dynamic, Layers::MOVING);
        auto &body_interface = mSystem->GetBodyInterface();
        BodyID sphere_id = body_interface.CreateAndAddBody(creation_settings, EActivation::Activate);
        body_interface.SetLinearVelocity(sphere_id, Vec3(0.0f, -5.0f, 0.0f));
        mSystem->OptimizeBroadPhase();
        const float cDeltaTime = 1.0f / 60.0f;
        uint step = 0;
        while (body_interface.IsActive(sphere_id)) {
            // Next step
            ++step;

            // Output current position and velocity of the sphere
            RVec3 position = body_interface.GetCenterOfMassPosition(sphere_id);
            Vec3 velocity = body_interface.GetLinearVelocity(sphere_id);
            std::cout << "Step " << step << ": Position = (" << position.GetX() << ", " << position.GetY() << ", " << position.GetZ() << "), Velocity = (" << velocity.GetX() << ", " << velocity.GetY() << ", " << velocity.GetZ() << ")" << std::endl;

            // If you take larger steps than 1 / 60th of a second you need to do multiple collision steps in order to keep the simulation stable. Do 1 collision step per 1 / 60th of a second (round up).
            const int cCollisionSteps = 1;

            // Step the world
            mSystem->Update(cDeltaTime, cCollisionSteps, mTempAllocator, mJobSystem);
        }
    }
};
static RuntimeStatic<JoltInstance> jolt_instance;
void JoltComponent::test_jolt() {
    jolt_instance->init();
    jolt_instance->test();
}
JoltComponent::JoltComponent(Entity *entity)
    : ComponentDerive<JoltComponent>(entity) {}
JoltComponent::~JoltComponent() {}
void JoltComponent::on_awake() {
}
void JoltComponent::on_destroy() {}
void JoltComponent::serialize_meta(ObjSerialize const &ser) const {}
void JoltComponent::deserialize_meta(ObjDeSerialize const &ser) {}
DECLARE_WORLD_COMPONENT_REGISTER(JoltComponent);
}// namespace rbc::world
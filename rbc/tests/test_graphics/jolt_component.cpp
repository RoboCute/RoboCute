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
#include <rbc_world/entity.h>
#include <rbc_world/components/transform_component.h>
#include <iostream>
namespace JPH {
#include "Layers.h"
}// namespace JPH
namespace rbc::world {
struct JoltSingleton {
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

    ~JoltSingleton() {
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
        creation_settings.mPosition = RVec3(0, 3, 3);
        creation_settings.mFriction = 0.5f;
        creation_settings.mRestitution = 0.6f;
        auto boxShape = luisa::unique_ptr<BoxShape>(new BoxShape(RVec3(0.5, 0.5, 0.5)));
        creation_settings.SetShape(boxShape.get());
        auto &body_interface = mSystem->GetBodyInterface();
        BodyID box_id = body_interface.CreateAndAddBody(creation_settings, EActivation::Activate);
        body_interface.SetLinearVelocity(box_id, Vec3(0.0f, -5.0f, 0.0f));
        mSystem->OptimizeBroadPhase();
        const float cDeltaTime = 1.0f / 60.0f;
        uint step = 0;
        while (body_interface.IsActive(box_id)) {
            // Next step
            ++step;

            // Output current position and velocity of the sphere
            RVec3 position = body_interface.GetCenterOfMassPosition(box_id);
            Vec3 velocity = body_interface.GetLinearVelocity(box_id);
            std::cout << "Step " << step << ": Position = (" << position.GetX() << ", " << position.GetY() << ", " << position.GetZ() << "), Velocity = (" << velocity.GetX() << ", " << velocity.GetY() << ", " << velocity.GetZ() << ")" << std::endl;

            // If you take larger steps than 1 / 60th of a second you need to do multiple collision steps in order to keep the simulation stable. Do 1 collision step per 1 / 60th of a second (round up).
            const int cCollisionSteps = 1;

            // Step the world
            mSystem->Update(cDeltaTime, cCollisionSteps, mTempAllocator, mJobSystem);
        }
    }
};

static RuntimeStatic<JoltSingleton> jolt_singleton;
struct BoxCollider {
    luisa::unique_ptr<JPH::BoxShape> _box_shape;
    JPH::BodyID box_id;
};
struct JoltInstance : RBCStruct {
    vstd::variant<
        JPH::Body *, BoxCollider>
        body;
};
void JoltComponent::init(
    bool is_floor// for demo, floor or object
) {
    using namespace JPH;
    if (_physics_instance) return;
    jolt_singleton->init();
    auto inst = new JoltInstance{};
    _physics_instance = inst;
    auto transform = entity()->get_component<TransformComponent>();
    if (is_floor) {
        BoxShapeSettings floor_shape_settings(Vec3(10.0f, 0.3f, 10.0f));
        floor_shape_settings.SetEmbedded();
        ShapeSettings::ShapeResult floor_shape_result = floor_shape_settings.Create();
        ShapeRefC floor_shape = floor_shape_result.Get();
        BodyCreationSettings floor_settings(floor_shape, RVec3(0.0, -2.0, 3.0), Quat::sIdentity(), EMotionType::Static, Layers::NON_MOVING);
        inst->body = &jolt_singleton->CreateBody(floor_settings, EActivation::DontActivate);
        transform->set_pos(double3(0, -2, 3), false);
        transform->set_scale(double3(10.0, 0.6, 10.0), false);
    } else {
        BoxCollider collider;
        BodyCreationSettings creation_settings{};
        creation_settings.mMotionType = EMotionType::Dynamic;
        creation_settings.mMotionQuality = EMotionQuality::Discrete;
        creation_settings.mObjectLayer = Layers::MOVING;
        creation_settings.mPosition = RVec3(0, 3, 3);
        auto rot = quaternion(make_float3x3(luisa::rotation(float3(1, 0, 0), 0.1f) * luisa::rotation(float3(0, 0, 1), 0.2f)));
        creation_settings.mRotation = Quat{
            rot.v.x, rot.v.y, rot.v.z, rot.v.w};
        creation_settings.mFriction = 0.5f;
        creation_settings.mRestitution = 0.6f;
        collider._box_shape = luisa::unique_ptr<BoxShape>(new BoxShape(RVec3(0.5, 0.5, 0.5)));
        creation_settings.SetShape(collider._box_shape.get());
        auto &body_interface = jolt_singleton->mSystem->GetBodyInterface();
        collider.box_id = body_interface.CreateAndAddBody(creation_settings, EActivation::Activate);
        inst->body = std::move(collider);
        transform->set_pos(double3(0, 3, 3), false);
        body_interface.SetLinearVelocity(collider.box_id, Vec3(0.0f, -5.0f, 0.0f));
        jolt_singleton->mSystem->OptimizeBroadPhase();
    }
}
JoltComponent::JoltComponent(Entity *entity)
    : ComponentDerive<JoltComponent>(entity) {}
JoltComponent::~JoltComponent() {}
void JoltComponent::update_step(float delta_time) {
    jolt_singleton->mSystem->Update(delta_time, 8, jolt_singleton->mTempAllocator, jolt_singleton->mJobSystem);
}
void JoltComponent::update_pos() {
    using namespace JPH;
    if (!_physics_instance) return;
    auto &body = static_cast<JoltInstance *>(_physics_instance)->body;
    if (!body.is_type_of<BoxCollider>()) return;
    auto &collider = body.force_get<BoxCollider>();
    auto &body_interface = jolt_singleton->mSystem->GetBodyInterface();
    if (!body_interface.IsActive(collider.box_id)) return;
    auto transform = entity()->get_component<TransformComponent>();
    auto mat = body_interface.GetCenterOfMassTransform(collider.box_id);
    auto to_double4 = [&](JPH::Vec4 vec) {
        return double4(vec.GetX(), vec.GetY(), vec.GetZ(), vec.GetW());
    };
    transform->set_trs(
        double4x4(
            to_double4(mat.GetColumn4(0)),
            to_double4(mat.GetColumn4(1)),
            to_double4(mat.GetColumn4(2)),
            to_double4(mat.GetColumn4(3))),
        false);
}
void JoltComponent::on_awake() {
}
void JoltComponent::on_destroy() {
    delete static_cast<JoltInstance *>(_physics_instance);
    _physics_instance = nullptr;
}
void JoltComponent::serialize_meta(ObjSerialize const &ser) const {
    LUISA_NOT_IMPLEMENTED();
}
void JoltComponent::deserialize_meta(ObjDeSerialize const &ser) {
    LUISA_NOT_IMPLEMENTED();
}
DECLARE_WORLD_COMPONENT_REGISTER(JoltComponent);
}// namespace rbc::world
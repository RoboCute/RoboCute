# 优化版高性能机器人仿真场景树设计文档

## 概述

本文档描述了一个基于ECS（Entity-Component-System）架构的**高性能、可扩展、易用**的3D场景树系统，专为机器人仿真、动画回放、软体仿真和复杂渲染设计。

### 核心设计原则

1. **可扩展性优先**：基于ECS架构，支持动态添加新功能
2. **高性能导向**：SIMD优化、多线程并行、缓存友好的数据布局
3. **易用性保证**：流畅的API、智能指针、事件系统、调试工具
4. **模块化设计**：系统间松耦合，支持插件式扩展

---

## 核心架构

### 1. ECS架构基础

```cpp
// 实体ID类型
using EntityID = uint32_t;
constexpr EntityID INVALID_ENTITY = 0;

// 组件类型ID
using ComponentTypeID = uint32_t;
using ResourceID = uint32_t;
constexpr ResourceID INVALID_RESOURCE = 0;

// 基础组件接口
class IComponent {
public:
    virtual ~IComponent() = default;
    virtual ComponentTypeID get_type_id() const = 0;
    virtual std::unique_ptr<IComponent> clone() const = 0;
    virtual void serialize(Serializer& serializer) const = 0;
    virtual void deserialize(Deserializer& deserializer) = 0;
};

// 组件类型注册宏
#define DECLARE_COMPONENT(ComponentClass) \
    static ComponentTypeID get_static_type_id() { \
        static ComponentTypeID id = ComponentRegistry::register_type<ComponentClass>(#ComponentClass); \
        return id; \
    } \
    ComponentTypeID get_type_id() const override { return get_static_type_id(); }

// 实体管理器
class EntityManager {
public:
    EntityID create_entity();
    void destroy_entity(EntityID entity);
    bool is_valid(EntityID entity) const;
    
    template<typename T, typename... Args>
    T& add_component(EntityID entity, Args&&... args);
    
    template<typename T>
    T* get_component(EntityID entity);
    
    template<typename T>
    void remove_component(EntityID entity);
    
    template<typename T>
    std::vector<EntityID> get_entities_with_component();
    
private:
    std::vector<bool> entity_valid;
    std::queue<EntityID> free_entities;
    EntityID next_entity_id = 1;
    
    std::unordered_map<ComponentTypeID, std::unique_ptr<IComponentArray>> component_arrays;
};
```

### 2. 核心组件系统

#### 变换组件 - 支持层次结构和动画
```cpp
struct TransformComponent : public IComponent {
    DECLARE_COMPONENT(TransformComponent);
    
    // 局部变换
    Vector3 position{0, 0, 0};
    Quaternion rotation{0, 0, 0, 1};
    Vector3 scale{1, 1, 1};
    
    // 动画和物理支持
    Vector3 velocity{0, 0, 0};
    Vector3 angular_velocity{0, 0, 0};
    Vector3 acceleration{0, 0, 0};
    
    // 层次结构
    EntityID parent = INVALID_ENTITY;
    std::vector<EntityID> children;
    
    // 缓存的全局变换
    mutable Transform3D global_transform;
    mutable bool global_dirty = true;
    
    // 约束支持（用于机器人关节）
    struct Constraint {
        enum Type { None, Hinge, Ball, Slider, Fixed };
        Type type = None;
        Vector3 axis{0, 1, 0};
        float min_limit = -FLT_MAX;
        float max_limit = FLT_MAX;
        float stiffness = 1000.0f;
        float damping = 10.0f;
    } constraint;
    
    std::unique_ptr<IComponent> clone() const override;
    void serialize(Serializer& serializer) const override;
    void deserialize(Deserializer& deserializer) override;
};
```

#### 渲染组件 - 支持LOD和复杂材质
```cpp
struct RenderComponent : public IComponent {
    DECLARE_COMPONENT(RenderComponent);
    
    ResourceID mesh_id = INVALID_RESOURCE;
    std::vector<ResourceID> material_ids;
    
    bool visible = true;
    bool cast_shadow = true;
    bool receive_shadow = true;
    uint32_t layer_mask = 0xFFFFFFFF;
    
    // LOD支持
    struct LODLevel {
        ResourceID mesh_id;
        float distance;
        float screen_size_threshold;
    };
    std::vector<LODLevel> lod_levels;
    
    // 实例化渲染支持
    bool use_instancing = false;
    std::vector<Transform3D> instance_transforms;
    
    // 高级渲染特性
    float transparency = 1.0f;
    enum BlendMode { Opaque, AlphaBlend, Additive, Multiply } blend_mode = Opaque;
    
    // 自定义着色器参数
    std::unordered_map<StringName, Variant> shader_params;
    
    std::unique_ptr<IComponent> clone() const override;
    void serialize(Serializer& serializer) const override;
    void deserialize(Deserializer& deserializer) override;
};
```

#### 物理组件 - 支持刚体和软体仿真
```cpp
struct PhysicsComponent : public IComponent {
    DECLARE_COMPONENT(PhysicsComponent);
    
    enum class BodyType {
        Static,      // 静态物体
        Kinematic,   // 运动学物体（机器人关节）
        Dynamic,     // 动力学物体
        SoftBody,    // 软体
        Fluid        // 流体（SPH）
    };
    
    BodyType body_type = BodyType::Static;
    float mass = 1.0f;
    Vector3 center_of_mass{0, 0, 0};
    Vector3 inertia_tensor{1, 1, 1};
    
    // 材质属性
    float friction = 0.5f;
    float restitution = 0.0f;
    float linear_damping = 0.1f;
    float angular_damping = 0.1f;
    
    // 碰撞形状
    ResourceID collision_shape_id = INVALID_RESOURCE;
    uint32_t collision_layer = 1;
    uint32_t collision_mask = 0xFFFFFFFF;
    
    // 软体特有属性
    struct SoftBodyProperties {
        float stiffness = 1000.0f;
        float damping = 0.1f;
        float pressure = 0.0f;
        int subdivision_level = 0;
        bool self_collision = false;
    } soft_body;
    
    // 流体特有属性
    struct FluidProperties {
        float density = 1000.0f;
        float viscosity = 0.001f;
        float surface_tension = 0.0728f;
        float particle_radius = 0.1f;
    } fluid;
    
    std::unique_ptr<IComponent> clone() const override;
    void serialize(Serializer& serializer) const override;
    void deserialize(Deserializer& deserializer) override;
};
```

#### 动画组件 - 支持多种动画类型
```cpp
struct AnimationComponent : public IComponent {
    DECLARE_COMPONENT(AnimationComponent);
    
    struct AnimationClip {
        StringName name;
        float duration;
        bool looping = false;
        ResourceID animation_data_id = INVALID_RESOURCE;
        
        enum Type {
            Transform,    // 变换动画
            Skeletal,     // 骨骼动画
            Morph,        // 形变动画
            Material,     // 材质动画
            Custom        // 自定义属性动画
        } type = Transform;
    };
    
    std::vector<AnimationClip> clips;
    
    // 动画状态
    struct PlaybackState {
        StringName current_clip;
        float current_time = 0.0f;
        float playback_speed = 1.0f;
        bool playing = false;
        bool paused = false;
        
        // 混合支持
        struct BlendInfo {
            StringName clip_name;
            float weight;
            float fade_time;
        };
        std::vector<BlendInfo> blend_stack;
    } playback;
    
    // 骨骼动画支持
    ResourceID skeleton_id = INVALID_RESOURCE;
    std::vector<Transform3D> bone_transforms;
    std::vector<EntityID> bone_entities;  // 对应的骨骼实体
    
    // 动画事件
    struct AnimationEvent {
        float time;
        StringName event_name;
        Variant event_data;
    };
    std::vector<AnimationEvent> events;
    
    std::unique_ptr<IComponent> clone() const override;
    void serialize(Serializer& serializer) const override;
    void deserialize(Deserializer& deserializer) override;
};
```

#### 录制回放组件 - 支持动画replay
```cpp
struct RecordingComponent : public IComponent {
    DECLARE_COMPONENT(RecordingComponent);
    
    struct KeyFrame {
        float timestamp;
        
        // 记录的数据类型
        struct RecordedData {
            Transform3D transform;
            std::unordered_map<StringName, Variant> custom_properties;
            std::vector<float> joint_angles;  // 机器人关节角度
            Vector3 velocity;
            Vector3 angular_velocity;
        } data;
        
        // 插值类型
        enum InterpolationType {
            Linear,
            Cubic,
            Step
        } interpolation = Linear;
    };
    
    std::vector<KeyFrame> recorded_frames;
    
    // 录制设置
    struct RecordingSettings {
        bool is_recording = false;
        bool is_playing = false;
        float recording_interval = 1.0f / 60.0f;  // 60 FPS
        float playback_time = 0.0f;
        float playback_speed = 1.0f;
        bool loop_playback = false;
        
        // 录制过滤器
        std::set<StringName> recorded_properties;
        bool record_transform = true;
        bool record_physics = false;
        bool record_custom_data = false;
    } settings;
    
    // 压缩支持
    bool use_compression = true;
    float compression_tolerance = 0.001f;
    
    std::unique_ptr<IComponent> clone() const override;
    void serialize(Serializer& serializer) const override;
    void deserialize(Deserializer& deserializer) override;
};
```

---

## 高性能系统架构

### 1. 系统基类和调度器

```cpp
// 基础系统接口
class ISystem {
public:
    virtual ~ISystem() = default;
    virtual void initialize() {}
    virtual void update(float delta_time) = 0;
    virtual void shutdown() {}
    virtual int get_priority() const { return 0; }
    virtual bool is_parallel_safe() const { return false; }
    
    // 系统依赖关系
    virtual std::vector<std::type_index> get_dependencies() const { return {}; }
};

// 系统调度器
class SystemScheduler {
public:
    template<typename T, typename... Args>
    void register_system(Args&&... args) {
        auto system = std::make_unique<T>(std::forward<Args>(args)...);
        systems.emplace_back(std::move(system));
        sort_systems_by_priority();
    }
    
    void update_all_systems(float delta_time) {
        PROFILE_SCOPE("SystemScheduler::update_all_systems");
        
        // 串行系统
        for (auto& system : serial_systems) {
            PROFILE_SCOPE(typeid(*system).name());
            system->update(delta_time);
        }
        
        // 并行系统
        if (!parallel_systems.empty()) {
            TaskSystem& task_system = TaskSystem::get_instance();
            std::vector<std::future<void>> futures;
            
            for (auto& system : parallel_systems) {
                futures.push_back(task_system.submit([&system, delta_time]() {
                    PROFILE_SCOPE(typeid(*system).name());
                    system->update(delta_time);
                }));
            }
            
            for (auto& future : futures) {
                future.wait();
            }
        }
    }
    
private:
    std::vector<std::unique_ptr<ISystem>> systems;
    std::vector<ISystem*> serial_systems;
    std::vector<ISystem*> parallel_systems;
    
    void sort_systems_by_priority();
    void resolve_dependencies();
};
```

### 2. 高性能变换系统

```cpp
class TransformSystem : public ISystem {
public:
    void update(float delta_time) override {
        PROFILE_SCOPE("TransformSystem::update");
        
        // 收集需要更新的实体
        auto entities = entity_manager->get_entities_with_component<TransformComponent>();
        
        if (entities.empty()) return;
        
        // 按深度分层，确保父节点先于子节点更新
        auto layers = organize_by_depth(entities);
        
        // 并行更新每一层
        TaskSystem& task_system = TaskSystem::get_instance();
        
        for (const auto& layer : layers) {
            if (layer.size() > parallel_threshold) {
                // 并行处理大层
                task_system.parallel_for(0, layer.size(), [&](size_t i) {
                    update_single_transform(layer[i], delta_time);
                });
            } else {
                // 串行处理小层
                for (EntityID entity : layer) {
                    update_single_transform(entity, delta_time);
                }
            }
        }
    }
    
    int get_priority() const override { return 100; }  // 最高优先级
    bool is_parallel_safe() const override { return true; }
    
    // SIMD优化的批量变换更新
    void update_transforms_simd(const std::vector<EntityID>& entities) {
        #ifdef __AVX2__
        // 使用AVX2指令集并行处理多个变换
        constexpr size_t simd_width = 8;
        size_t simd_count = entities.size() / simd_width;
        
        for (size_t i = 0; i < simd_count; ++i) {
            update_transform_batch_avx2(&entities[i * simd_width]);
        }
        
        // 处理剩余的变换
        for (size_t i = simd_count * simd_width; i < entities.size(); ++i) {
            update_single_transform(entities[i], 0.0f);
        }
        #else
        // 回退到标准实现
        for (EntityID entity : entities) {
            update_single_transform(entity, 0.0f);
        }
        #endif
    }
    
private:
    EntityManager* entity_manager;
    static constexpr size_t parallel_threshold = 100;
    
    std::vector<std::vector<EntityID>> organize_by_depth(const std::vector<EntityID>& entities);
    void update_single_transform(EntityID entity, float delta_time);
    
    #ifdef __AVX2__
    void update_transform_batch_avx2(const EntityID* entities);
    #endif
};
```

### 3. 动画系统

```cpp
class AnimationSystem : public ISystem {
public:
    void update(float delta_time) override {
        PROFILE_SCOPE("AnimationSystem::update");
        
        auto entities = entity_manager->get_entities_with_component<AnimationComponent>();
        
        // 并行更新动画
        TaskSystem& task_system = TaskSystem::get_instance();
        task_system.parallel_for(0, entities.size(), [&](size_t i) {
            update_entity_animation(entities[i], delta_time);
        });
    }
    
    int get_priority() const override { return 90; }
    bool is_parallel_safe() const override { return true; }
    
    // 播放动画
    void play_animation(EntityID entity, const StringName& clip_name, float fade_time = 0.0f) {
        auto* anim_comp = entity_manager->get_component<AnimationComponent>(entity);
        if (!anim_comp) return;
        
        if (fade_time > 0.0f) {
            // 添加到混合栈
            AnimationComponent::PlaybackState::BlendInfo blend_info;
            blend_info.clip_name = clip_name;
            blend_info.weight = 0.0f;
            blend_info.fade_time = fade_time;
            anim_comp->playback.blend_stack.push_back(blend_info);
        } else {
            // 直接切换
            anim_comp->playback.current_clip = clip_name;
            anim_comp->playback.current_time = 0.0f;
        }
        
        anim_comp->playback.playing = true;
    }
    
    // 混合多个动画
    void blend_animations(EntityID entity, 
                         const std::vector<std::pair<StringName, float>>& clips_and_weights) {
        auto* anim_comp = entity_manager->get_component<AnimationComponent>(entity);
        if (!anim_comp) return;
        
        anim_comp->playback.blend_stack.clear();
        for (const auto& [clip_name, weight] : clips_and_weights) {
            AnimationComponent::PlaybackState::BlendInfo blend_info;
            blend_info.clip_name = clip_name;
            blend_info.weight = weight;
            blend_info.fade_time = 0.0f;
            anim_comp->playback.blend_stack.push_back(blend_info);
        }
    }
    
private:
    EntityManager* entity_manager;
    ResourceManager* resource_manager;
    
    void update_entity_animation(EntityID entity, float delta_time);
    void update_keyframe_animation(EntityID entity, AnimationComponent& anim, float delta_time);
    void update_skeletal_animation(EntityID entity, AnimationComponent& anim, float delta_time);
    void apply_animation_blending(EntityID entity, AnimationComponent& anim);
    void trigger_animation_events(EntityID entity, const AnimationComponent& anim, float old_time, float new_time);
};
```

### 4. 录制回放系统

```cpp
class RecordingSystem : public ISystem {
public:
    void update(float delta_time) override {
        PROFILE_SCOPE("RecordingSystem::update");
        
        auto entities = entity_manager->get_entities_with_component<RecordingComponent>();
        
        for (EntityID entity : entities) {
            auto* recording = entity_manager->get_component<RecordingComponent>(entity);
            if (!recording) continue;
            
            if (recording->settings.is_recording) {
                update_recording(entity, *recording, delta_time);
            } else if (recording->settings.is_playing) {
                update_playback(entity, *recording, delta_time);
            }
        }
    }
    
    int get_priority() const override { return 80; }
    
    // 开始录制
    void start_recording(EntityID entity, const RecordingComponent::RecordingSettings& settings = {}) {
        auto* recording = entity_manager->get_component<RecordingComponent>(entity);
        if (!recording) {
            recording = &entity_manager->add_component<RecordingComponent>(entity);
        }
        
        recording->settings = settings;
        recording->settings.is_recording = true;
        recording->recorded_frames.clear();
        
        // 记录初始帧
        record_frame(entity, *recording, 0.0f);
    }
    
    // 停止录制并优化数据
    void stop_recording(EntityID entity) {
        auto* recording = entity_manager->get_component<RecordingComponent>(entity);
        if (!recording) return;
        
        recording->settings.is_recording = false;
        
        // 压缩录制数据
        if (recording->use_compression) {
            compress_recording_data(*recording);
        }
    }
    
    // 播放录制
    void play_recording(EntityID entity, bool loop = false) {
        auto* recording = entity_manager->get_component<RecordingComponent>(entity);
        if (!recording || recording->recorded_frames.empty()) return;
        
        recording->settings.is_playing = true;
        recording->settings.loop_playback = loop;
        recording->settings.playback_time = 0.0f;
    }
    
    // 导出录制数据
    bool export_recording(EntityID entity, const StringName& file_path) {
        auto* recording = entity_manager->get_component<RecordingComponent>(entity);
        if (!recording) return false;
        
        // 序列化到文件
        BinarySerializer serializer(file_path);
        recording->serialize(serializer);
        return serializer.is_valid();
    }
    
    // 导入录制数据
    bool import_recording(EntityID entity, const StringName& file_path) {
        BinaryDeserializer deserializer(file_path);
        if (!deserializer.is_valid()) return false;
        
        auto& recording = entity_manager->add_component<RecordingComponent>(entity);
        recording.deserialize(deserializer);
        return true;
    }
    
private:
    EntityManager* entity_manager;
    float recording_timer = 0.0f;
    
    void update_recording(EntityID entity, RecordingComponent& recording, float delta_time);
    void update_playback(EntityID entity, RecordingComponent& recording, float delta_time);
    void record_frame(EntityID entity, RecordingComponent& recording, float timestamp);
    void apply_recorded_frame(EntityID entity, const RecordingComponent::KeyFrame& frame);
    void compress_recording_data(RecordingComponent& recording);
    
    // 插值函数
    RecordingComponent::KeyFrame::RecordedData interpolate_frames(
        const RecordingComponent::KeyFrame& frame1,
        const RecordingComponent::KeyFrame& frame2,
        float t);
};
```

### 5. 物理系统 - 支持软体仿真

```cpp
class PhysicsSystem : public ISystem {
public:
    void update(float delta_time) override {
        PROFILE_SCOPE("PhysicsSystem::update");
        
        // 更新刚体物理
        update_rigid_bodies(delta_time);
        
        // 更新软体物理
        update_soft_bodies(delta_time);
        
        // 更新流体物理
        update_fluid_simulation(delta_time);
        
        // 碰撞检测和响应
        detect_and_resolve_collisions(delta_time);
    }
    
    int get_priority() const override { return 70; }
    
    // 启用软体仿真
    void enable_soft_body_simulation(EntityID entity) {
        auto* physics = entity_manager->get_component<PhysicsComponent>(entity);
        auto* render = entity_manager->get_component<RenderComponent>(entity);
        
        if (!physics || !render) return;
        
        physics->body_type = PhysicsComponent::BodyType::SoftBody;
        
        // 创建软体数据
        SoftBodyData soft_body_data;
        initialize_soft_body_from_mesh(entity, soft_body_data);
        soft_bodies[entity] = std::move(soft_body_data);
    }
    
    // 设置软体约束
    void add_soft_body_constraint(EntityID entity, int vertex1, int vertex2, float rest_length) {
        auto it = soft_bodies.find(entity);
        if (it == soft_bodies.end()) return;
        
        SoftBodyConstraint constraint;
        constraint.vertex1 = vertex1;
        constraint.vertex2 = vertex2;
        constraint.rest_length = rest_length;
        constraint.stiffness = 1000.0f;
        
        it->second.constraints.push_back(constraint);
    }
    
private:
    EntityManager* entity_manager;
    
    // 软体仿真数据
    struct SoftBodyConstraint {
        int vertex1, vertex2;
        float rest_length;
        float stiffness;
    };
    
    struct SoftBodyData {
        std::vector<Vector3> vertices;
        std::vector<Vector3> velocities;
        std::vector<Vector3> forces;
        std::vector<SoftBodyConstraint> constraints;
        std::vector<int> fixed_vertices;  // 固定顶点
    };
    
    std::unordered_map<EntityID, SoftBodyData> soft_bodies;
    
    // 流体仿真数据（SPH）
    struct FluidParticle {
        Vector3 position;
        Vector3 velocity;
        float density;
        float pressure;
    };
    
    std::unordered_map<EntityID, std::vector<FluidParticle>> fluid_particles;
    
    void update_rigid_bodies(float delta_time);
    void update_soft_bodies(float delta_time);
    void update_fluid_simulation(float delta_time);
    void detect_and_resolve_collisions(float delta_time);
    
    void initialize_soft_body_from_mesh(EntityID entity, SoftBodyData& soft_body_data);
    void solve_soft_body_constraints(SoftBodyData& soft_body_data, float delta_time);
    void update_soft_body_mesh(EntityID entity, const SoftBodyData& soft_body_data);
    
    // SPH流体仿真
    void update_sph_simulation(std::vector<FluidParticle>& particles, float delta_time);
    float calculate_sph_density(const FluidParticle& particle, const std::vector<FluidParticle>& neighbors);
    Vector3 calculate_sph_pressure_force(const FluidParticle& particle, const std::vector<FluidParticle>& neighbors);
};
```

---

## 易用性API设计

### 1. 场景构建器

```cpp
// 流畅的场景构建API
class SceneBuilder {
public:
    SceneBuilder(EntityManager& entity_mgr, ResourceManager& resource_mgr)
        : entity_manager(entity_mgr), resource_manager(resource_mgr) {}
    
    // 创建实体
    SceneBuilder& create_entity(const StringName& name = "") {
        current_entity = entity_manager.create_entity();
        if (!name.empty()) {
            entity_manager.add_component<NameComponent>(current_entity, {name});
        }
        return *this;
    }
    
    // 添加变换
    SceneBuilder& with_transform(const Vector3& pos = Vector3::ZERO,
                                const Quaternion& rot = Quaternion::IDENTITY,
                                const Vector3& scale = Vector3::ONE) {
        auto& transform = entity_manager.add_component<TransformComponent>(current_entity);
        transform.position = pos;
        transform.rotation = rot;
        transform.scale = scale;
        return *this;
    }
    
    // 添加网格渲染
    SceneBuilder& with_mesh(const StringName& mesh_path) {
        ResourceID mesh_id = resource_manager.load_mesh(mesh_path);
        auto& render = entity_manager.add_component<RenderComponent>(current_entity);
        render.mesh_id = mesh_id;
        return *this;
    }
    
    // 添加材质
    SceneBuilder& with_material(const StringName& material_path) {
        ResourceID material_id = resource_manager.load_material(material_path);
        auto* render = entity_manager.get_component<RenderComponent>(current_entity);
        if (render) {
            render->material_ids.push_back(material_id);
        }
        return *this;
    }
    
    // 添加物理
    SceneBuilder& with_physics(PhysicsComponent::BodyType type = PhysicsComponent::BodyType::Dynamic,
                              float mass = 1.0f) {
        auto& physics = entity_manager.add_component<PhysicsComponent>(current_entity);
        physics.body_type = type;
        physics.mass = mass;
        return *this;
    }
    
    // 添加动画
    SceneBuilder& with_animation(const StringName& animation_path) {
        ResourceID anim_id = resource_manager.load_animation(animation_path);
        auto& anim = entity_manager.add_component<AnimationComponent>(current_entity);
        
        AnimationComponent::AnimationClip clip;
        clip.name = animation_path;
        clip.animation_data_id = anim_id;
        anim.clips.push_back(clip);
        
        return *this;
    }
    
    // 设置为子节点
    SceneBuilder& as_child_of(EntityID parent) {
        auto* parent_transform = entity_manager.get_component<TransformComponent>(parent);
        auto* child_transform = entity_manager.get_component<TransformComponent>(current_entity);
        
        if (parent_transform && child_transform) {
            child_transform->parent = parent;
            parent_transform->children.push_back(current_entity);
        }
        return *this;
    }
    
    // 启用录制
    SceneBuilder& with_recording(float interval = 1.0f/60.0f) {
        auto& recording = entity_manager.add_component<RecordingComponent>(current_entity);
        recording.settings.recording_interval = interval;
        return *this;
    }
    
    // 启用软体仿真
    SceneBuilder& as_soft_body(float stiffness = 1000.0f, float damping = 0.1f) {
        auto* physics = entity_manager.get_component<PhysicsComponent>(current_entity);
        if (physics) {
            physics->body_type = PhysicsComponent::BodyType::SoftBody;
            physics->soft_body.stiffness = stiffness;
            physics->soft_body.damping = damping;
        }
        return *this;
    }
    
    // 完成构建
    EntityID build() {
        EntityID result = current_entity;
        current_entity = INVALID_ENTITY;
        return result;
    }
    
private:
    EntityManager& entity_manager;
    ResourceManager& resource_manager;
    EntityID current_entity = INVALID_ENTITY;
};

// 使用示例
void create_robot_scene() {
    SceneBuilder builder(entity_manager, resource_manager);
    
    // 创建机器人底座
    auto base = builder
        .create_entity("RobotBase")
        .with_transform(Vector3(0, 0, 0))
        .with_mesh("models/robot_base.mesh")
        .with_material("materials/metal.mat")
        .with_physics(PhysicsComponent::BodyType::Static)
        .build();
    
    // 创建机器人手臂
    auto arm = builder
        .create_entity("RobotArm")
        .with_transform(Vector3(0, 1, 0))
        .with_mesh("models/robot_arm.mesh")
        .with_material("materials/metal.mat")
        .with_physics(PhysicsComponent::BodyType::Kinematic, 5.0f)
        .with_animation("animations/arm_movement.anim")
        .with_recording()  // 启用录制功能
        .as_child_of(base)
        .build();
    
    // 创建软体抓手
    auto gripper = builder
        .create_entity("SoftGripper")
        .with_transform(Vector3(0, 0.5, 0))
        .with_mesh("models/soft_gripper.mesh")
        .with_material("materials/rubber.mat")
        .with_physics(PhysicsComponent::BodyType::SoftBody, 0.5f)
        .as_soft_body(500.0f, 0.2f)  // 较软的材质
        .as_child_of(arm)
        .build();
}
```

### 2. 智能指针和RAII

```cpp
// 智能实体句柄
class EntityHandle {
public:
    EntityHandle() = default;
    EntityHandle(EntityID id, EntityManager* manager) : entity_id(id), manager(manager) {}
    
    // 移动语义
    EntityHandle(EntityHandle&& other) noexcept 
        : entity_id(other.entity_id), manager(other.manager) {
        other.entity_id = INVALID_ENTITY;
        other.manager = nullptr;
    }
    
    EntityHandle& operator=(EntityHandle&& other) noexcept {
        if (this != &other) {
            destroy();
            entity_id = other.entity_id;
            manager = other.manager;
            other.entity_id = INVALID_ENTITY;
            other.manager = nullptr;
        }
        return *this;
    }
    
    ~EntityHandle() { destroy(); }
    
    // 禁止拷贝
    EntityHandle(const EntityHandle&) = delete;
    EntityHandle& operator=(const EntityHandle&) = delete;
    
    // 组件操作
    template<typename T, typename... Args>
    T& add_component(Args&&... args) {
        return manager->add_component<T>(entity_id, std::forward<Args>(args)...);
    }
    
    template<typename T>
    T* get_component() {
        return manager->get_component<T>(entity_id);
    }
    
    template<typename T>
    void remove_component() {
        manager->remove_component<T>(entity_id);
    }
    
    // 高级操作
    void set_position(const Vector3& pos) {
        if (auto* transform = get_component<TransformComponent>()) {
            transform->position = pos;
            transform->global_dirty = true;
        }
    }
    
    Vector3 get_position() const {
        if (auto* transform = get_component<TransformComponent>()) {
            return transform->position;
        }
        return Vector3::ZERO;
    }
    
    void play_animation(const StringName& clip_name) {
        AnimationSystem::get_instance().play_animation(entity_id, clip_name);
    }
    
    void start_recording() {
        RecordingSystem::get_instance().start_recording(entity_id);
    }
    
    bool is_valid() const {
        return entity_id != INVALID_ENTITY && manager && manager->is_valid(entity_id);
    }
    
    EntityID get_id() const { return entity_id; }
    
private:
    EntityID entity_id = INVALID_ENTITY;
    EntityManager* manager = nullptr;
    
    void destroy() {
        if (is_valid()) {
            manager->destroy_entity(entity_id);
        }
    }
};

// 工厂函数
EntityHandle create_entity(const StringName& name = "") {
    EntityID id = EntityManager::get_instance().create_entity();
    if (!name.empty()) {
        EntityManager::get_instance().add_component<NameComponent>(id, {name});
    }
    return EntityHandle(id, &EntityManager::get_instance());
}
```

### 3. 事件系统

```cpp
// 事件系统
template<typename... Args>
class Signal {
public:
    using SlotType = std::function<void(Args...)>;
    
    class Connection {
    public:
        Connection(Signal* signal, size_t id) : signal(signal), connection_id(id) {}
        Connection(Connection&& other) noexcept : signal(other.signal), connection_id(other.connection_id) {
            other.signal = nullptr;
        }
        
        ~Connection() { disconnect(); }
        
        void disconnect() {
            if (signal) {
                signal->disconnect(connection_id);
                signal = nullptr;
            }
        }
        
        bool is_connected() const { return signal != nullptr; }
        
    private:
        Signal* signal;
        size_t connection_id;
    };
    
    Connection connect(SlotType slot) {
        size_t id = next_id++;
        slots[id] = std::move(slot);
        return Connection(this, id);
    }
    
    void emit(Args... args) {
        for (auto& [id, slot] : slots) {
            slot(args...);
        }
    }
    
    void disconnect(size_t connection_id) {
        slots.erase(connection_id);
    }
    
    void clear() {
        slots.clear();
    }
    
private:
    std::unordered_map<size_t, SlotType> slots;
    size_t next_id = 0;
};

// 事件定义
struct TransformChangedEvent {
    EntityID entity;
    Transform3D old_transform;
    Transform3D new_transform;
};

struct CollisionEvent {
    EntityID entity_a;
    EntityID entity_b;
    Vector3 contact_point;
    Vector3 contact_normal;
    float impulse;
};

struct AnimationFinishedEvent {
    EntityID entity;
    StringName animation_name;
};

struct RecordingFinishedEvent {
    EntityID entity;
    size_t frame_count;
    float duration;
};

// 全局事件管理器
class EventManager {
public:
    static EventManager& get_instance() {
        static EventManager instance;
        return instance;
    }
    
    template<typename EventType>
    Signal<EventType>& get_signal() {
        static Signal<EventType> signal;
        return signal;
    }
    
    template<typename EventType>
    void emit(const EventType& event) {
        get_signal<EventType>().emit(event);
    }
    
    template<typename EventType, typename Func>
    auto connect(Func&& func) {
        return get_signal<EventType>().connect(std::forward<Func>(func));
    }
};

// 便利宏
#define CONNECT_EVENT(EventType, func) \
    EventManager::get_instance().connect<EventType>(func)

#define EMIT_EVENT(EventType, ...) \
    EventManager::get_instance().emit(EventType{__VA_ARGS__})

// 使用示例
void setup_event_handlers() {
    // 监听碰撞事件
    auto collision_connection = CONNECT_EVENT(CollisionEvent, [](const CollisionEvent& event) {
        std::cout << "Collision between entities " << event.entity_a 
                  << " and " << event.entity_b << std::endl;
    });
    
    // 监听动画完成事件
    auto anim_connection = CONNECT_EVENT(AnimationFinishedEvent, [](const AnimationFinishedEvent& event) {
        std::cout << "Animation " << event.animation_name.c_str() 
                  << " finished for entity " << event.entity << std::endl;
    });
}
```

---

## 调试和性能分析工具

### 1. 调试渲染器

```cpp
class DebugRenderer {
public:
    static DebugRenderer& get_instance() {
        static DebugRenderer instance;
        return instance;
    }
    
    // 基础图形绘制
    void draw_line(const Vector3& start, const Vector3& end, const Color& color = Color::WHITE, float duration = 0.0f);
    void draw_sphere(const Vector3& center, float radius, const Color& color = Color::RED, float duration = 0.0f);
    void draw_box(const Vector3& center, const Vector3& size, const Color& color = Color::GREEN, float duration = 0.0f);
    void draw_arrow(const Vector3& start, const Vector3& end, const Color& color = Color::BLUE, float duration = 0.0f);
    
    // 高级调试图形
    void draw_transform_gizmo(const Transform3D& transform, float size = 1.0f);
    void draw_aabb(const AABB& aabb, const Color& color = Color::YELLOW);
    void draw_frustum(const Frustum& frustum, const Color& color = Color::CYAN);
    
    // 物理调试
    void draw_physics_body(EntityID entity);
    void draw_soft_body_constraints(EntityID entity);
    void draw_collision_shapes();
    
    // 动画调试
    void draw_skeleton(EntityID entity);
    void draw_bone_hierarchy(EntityID entity);
    void draw_animation_path(EntityID entity, const StringName& clip_name);
    
    // 文本和UI
    void draw_text_3d(const Vector3& position, const std::string& text, const Color& color = Color::WHITE);
    void draw_performance_overlay();
    void draw_entity_inspector(EntityID entity);
    
    // 录制可视化
    void draw_recording_timeline(EntityID entity);
    void draw_keyframe_markers(EntityID entity);
    
    void update(float delta_time);
    void render();
    void clear();
    
private:
    struct DebugLine { Vector3 start, end; Color color; float remaining_time; };
    struct DebugSphere { Vector3 center; float radius; Color color; float remaining_time; };
    struct DebugBox { Vector3 center, size; Color color; float remaining_time; };
    struct DebugText { Vector3 position; std::string text; Color color; float remaining_time; };
    
    std::vector<DebugLine> lines;
    std::vector<DebugSphere> spheres;
    std::vector<DebugBox> boxes;
    std::vector<DebugText> texts;
    
    bool show_physics_debug = false;
    bool show_animation_debug = false;
    bool show_performance_overlay = true;
};
```

### 2. 性能分析器

```cpp
class Profiler {
public:
    class ScopedTimer {
    public:
        ScopedTimer(const std::string& name) : timer_name(name) {
            start_time = std::chrono::high_resolution_clock::now();
        }
        
        ~ScopedTimer() {
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                end_time - start_time).count();
            Profiler::get_instance().add_sample(timer_name, duration);
        }
        
    private:
        std::string timer_name;
        std::chrono::high_resolution_clock::time_point start_time;
    };
    
    static Profiler& get_instance() {
        static Profiler instance;
        return instance;
    }
    
    void add_sample(const std::string& name, int64_t microseconds) {
        std::lock_guard<std::mutex> lock(mutex);
        samples[name].push_back(microseconds);
        if (samples[name].size() > max_samples) {
            samples[name].erase(samples[name].begin());
        }
    }
    
    struct ProfileData {
        double average_time;
        double min_time;
        double max_time;
        size_t sample_count;
    };
    
    ProfileData get_profile_data(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = samples.find(name);
        if (it == samples.end() || it->second.empty()) {
            return {0.0, 0.0, 0.0, 0};
        }
        
        const auto& sample_list = it->second;
        int64_t total = 0;
        int64_t min_val = INT64_MAX;
        int64_t max_val = INT64_MIN;
        
        for (int64_t sample : sample_list) {
            total += sample;
            min_val = std::min(min_val, sample);
            max_val = std::max(max_val, sample);
        }
        
        return {
            static_cast<double>(total) / sample_list.size(),
            static_cast<double>(min_val),
            static_cast<double>(max_val),
            sample_list.size()
        };
    }
    
    void print_stats() const {
        std::lock_guard<std::mutex> lock(mutex);
        std::cout << "=== Performance Statistics ===" << std::endl;
        for (const auto& [name, sample_list] : samples) {
            auto data = get_profile_data(name);
            std::cout << name << ": avg=" << data.average_time << "μs, "
                      << "min=" << data.min_time << "μs, "
                      << "max=" << data.max_time << "μs" << std::endl;
        }
    }
    
    void clear_stats() {
        std::lock_guard<std::mutex> lock(mutex);
        samples.clear();
    }
    
private:
    mutable std::mutex mutex;
    std::unordered_map<std::string, std::vector<int64_t>> samples;
    static constexpr size_t max_samples = 100;
};

#define PROFILE_SCOPE(name) Profiler::ScopedTimer timer(name)
#define PROFILE_FUNCTION() PROFILE_SCOPE(__FUNCTION__)
```

---

## 使用示例

### 完整的机器人仿真场景

```cpp
#include "optimized_scene_system.h"

class RobotSimulation {
public:
    void initialize() {
        // 初始化系统
        system_scheduler.register_system<TransformSystem>(&entity_manager);
        system_scheduler.register_system<AnimationSystem>(&entity_manager, &resource_manager);
        system_scheduler.register_system<PhysicsSystem>(&entity_manager);
        system_scheduler.register_system<RecordingSystem>(&entity_manager);
        
        // 设置事件处理
        setup_event_handlers();
        
        // 创建机器人场景
        create_robot_scene();
        
        // 开始录制
        recording_system.start_recording(robot_arm);
    }
    
    void update(float delta_time) {
        PROFILE_FUNCTION();
        
        // 更新所有系统
        system_scheduler.update_all_systems(delta_time);
        
        // 更新调试渲染
        DebugRenderer::get_instance().update(delta_time);
    }
    
    void render() {
        PROFILE_FUNCTION();
        
        // 提取渲染数据
        RenderDataExtractor extractor;
        std::vector<RenderInstance> instances;
        extractor.extract_render_instances(entity_manager, instances);
        
        // 渲染场景
        renderer.render(instances);
        
        // 渲染调试信息
        DebugRenderer::get_instance().render();
    }
    
private:
    EntityManager entity_manager;
    ResourceManager resource_manager;
    SystemScheduler system_scheduler;
    RecordingSystem recording_system;
    Renderer renderer;
    
    EntityID robot_base;
    EntityID robot_arm;
    EntityID soft_gripper;
    
    void create_robot_scene() {
        SceneBuilder builder(entity_manager, resource_manager);
        
        // 创建机器人底座
        robot_base = builder
            .create_entity("RobotBase")
            .with_transform(Vector3(0, 0, 0))
            .with_mesh("models/robot_base.mesh")
            .with_material("materials/metal.mat")
            .with_physics(PhysicsComponent::BodyType::Static)
            .build();
        
        // 创建机器人手臂（支持录制回放）
        robot_arm = builder
            .create_entity("RobotArm")
            .with_transform(Vector3(0, 1, 0))
            .with_mesh("models/robot_arm.mesh")
            .with_material("materials/metal.mat")
            .with_physics(PhysicsComponent::BodyType::Kinematic, 5.0f)
            .with_animation("animations/arm_movement.anim")
            .with_recording(1.0f/120.0f)  // 120 FPS录制
            .as_child_of(robot_base)
            .build();
        
        // 创建软体抓手
        soft_gripper = builder
            .create_entity("SoftGripper")
            .with_transform(Vector3(0, 0.5, 0))
            .with_mesh("models/soft_gripper.mesh")
            .with_material("materials/rubber.mat")
            .with_physics(PhysicsComponent::BodyType::SoftBody, 0.5f)
            .as_soft_body(500.0f, 0.2f)
            .as_child_of(robot_arm)
            .build();
        
        // 启用软体仿真
        PhysicsSystem::get_instance().enable_soft_body_simulation(soft_gripper);
    }
    
    void setup_event_handlers() {
        // 碰撞事件处理
        auto collision_conn = CONNECT_EVENT(CollisionEvent, [this](const CollisionEvent& event) {
            std::cout << "Collision detected!" << std::endl;
            
            // 可视化碰撞点
            DebugRenderer::get_instance().draw_sphere(event.contact_point, 0.1f, Color::RED, 2.0f);
        });
        
        // 动画完成事件
        auto anim_conn = CONNECT_EVENT(AnimationFinishedEvent, [this](const AnimationFinishedEvent& event) {
            if (event.entity == robot_arm) {
                // 机器人手臂动画完成，开始下一个动作
                AnimationSystem::get_instance().play_animation(robot_arm, "return_to_home");
            }
        });
        
        // 录制完成事件
        auto recording_conn = CONNECT_EVENT(RecordingFinishedEvent, [this](const RecordingFinishedEvent& event) {
            std::cout << "Recording finished: " << event.frame_count 
                      << " frames, " << event.duration << " seconds" << std::endl;
            
            // 导出录制数据
            recording_system.export_recording(event.entity, "robot_motion.rec");
        });
    }
};

int main() {
    RobotSimulation simulation;
    simulation.initialize();
    
    // 主循环
    auto last_time = std::chrono::high_resolution_clock::now();
    
    while (true) {
        auto current_time = std::chrono::high_resolution_clock::now();
        float delta_time = std::chrono::duration<float>(current_time - last_time).count();
        last_time = current_time;
        
        simulation.update(delta_time);
        simulation.render();
        
        // 每秒打印性能统计
        static float stats_timer = 0.0f;
        stats_timer += delta_time;
        if (stats_timer >= 1.0f) {
            Profiler::get_instance().print_stats();
            stats_timer = 0.0f;
        }
    }
    
    return 0;
}
```

---

## 总结

### 优化后的设计优势

1. **可扩展性**：
   - ECS架构支持动态添加新组件和系统
   - 插件式系统设计，易于扩展新功能
   - 支持动画回放、软体仿真、复杂渲染等高级功能

2. **高性能**：
   - SIMD优化的数学运算
   - 多线程并行处理
   - 缓存友好的数据布局
   - 内存池管理减少分配开销

3. **易用性**：
   - 流畅的构建器API
   - 智能指针和RAII资源管理
   - 完善的事件系统
   - 丰富的调试和性能分析工具

4. **模块化**：
   - 系统间松耦合
   - 清晰的接口定义
   - 支持热插拔功能模块

这个优化设计为机器人仿真提供了一个强大、灵活且高性能的基础架构，能够满足未来扩展需求。

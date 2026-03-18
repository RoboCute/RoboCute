# Skeletal Animation System

RoboCute的骨骼动画系统是一个基于节点图的可扩展动画框架，支持从资源导入、动画采样到蒙皮渲染的完整管线。

## 架构概览

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        Skeletal Animation System                        │
├─────────────────────────────────────────────────────────────────────────┤
│  Resource Layer    │  Runtime Layer     │  Rendering Layer             │
├────────────────────┼────────────────────┼──────────────────────────────┤
│  SkeletonResource  │  AnimInstance      │  SkeletalMeshRenderObject    │
│  SkinResource      │  AnimInstanceProxy │  SkelMeshRenderDataLOD       │
│  AnimSequence      │  SkeletalMesh      │  SkinPrimitive               │
│  AnimGraphResource │  Animation Graph   │  CPU/GPU Skinning            │
│  SkelMeshResource  │  Pose System       │  Transform Upload            │
└────────────────────┴────────────────────┴──────────────────────────────┘
```

## 核心概念

### 1. 资源系统 (Resource Layer)

动画资源采用RoboCute统一的资源管理系统，所有动画相关资源继承自 `world::Resource`。

#### SkeletonResource (`rbc_world/resources/skeleton.h`)
骨骼资源，存储角色的骨骼层次结构。

- **核心数据**: `ReferenceSkeleton` - 基于OZZ Animation的骨架运行时数据
- **功能**: 存储骨骼名称、父子关系、参考姿势（T-Pose）
- **依赖**: 无（最基础的动画资源）

```cpp
struct SkeletonResource : ResourceBaseImpl<SkeletonResource> {
    ReferenceSkeleton skeleton;  // OZZ skeleton runtime asset
};
```

#### SkinResource (`rbc_world/resources/skin.h`)
蒙皮资源，定义网格顶点如何受骨骼影响。

- **核心数据**:
  - `joint_remaps`: 关节名称映射列表
  - `inverse_bind_poses`: 绑定姿势的逆矩阵（用于将顶点从模型空间转换到骨骼局部空间）
  - `joint_remaps_LUT`: 查找表（joint_remaps[i] → 骨骼索引）
- **依赖**: SkeletonResource, MeshResource

```cpp
struct SkinResource : ResourceBaseImpl<SkinResource> {
    RC<SkeletonResource> ref_skel;
    RC<MeshResource> ref_mesh;
    luisa::vector<luisa::string> joint_remaps;
    luisa::vector<AnimFloat4x4> inverse_bind_poses;
    luisa::vector<BoneIndexType> joint_remaps_LUT;
};
```

#### AnimSequenceResource (`rbc_world/resources/anim_sequence.h`)
动画序列资源，存储关键帧动画数据。

- **核心数据**: `AnimSequence` - 基于OZZ Animation的动画运行时数据
- **配置参数**:
  - `anim_name`: 动画名称（导入时使用）
  - `sampling_rate`: 采样率（默认30fps）
- **依赖**: SkeletonResource

```cpp
struct AnimSequenceResource : ResourceBaseImpl<AnimSequenceResource> {
    AnimSequence anim_sequence;  // OZZ animation runtime asset
    RC<SkeletonResource> ref_skel;
};
```

#### AnimGraphResource (`rbc_world/resources/anim_graph.h`)
动画图资源，存储可执行的动画节点图。

- **核心数据**: `AnimGraph` - 节点容器
- **特点**: 支持序列化和反序列化节点连接关系
- **依赖**: 运行时动态解析

```cpp
struct AnimGraphResource : ResourceBaseImpl<AnimGraphResource> {
    AnimGraph graph;  // Node graph container
};
```

#### SkelMeshResource (`rbc_world/resources/skelmesh.h`)
骨骼网格资源，整合所有动画相关资源的复合资源。

- **作用**: 将 Skeleton + Skin + AnimGraph 绑定为一个可运行的骨骼网格
- **特点**: 资源层的"入口点"，SkelMeshComponent直接引用此资源

```cpp
struct SkelMeshResource : ResourceBaseImpl<SkelMeshResource> {
    RC<SkinResource> ref_skin;
    RC<SkeletonResource> ref_skeleton;
    RC<AnimGraphResource> ref_anim_graph;
};
```

### 2. 运行时系统 (Runtime Layer)

#### AnimInstance (`rbc_anim/anim_instance.h`)
动画实例，AnimGraph资源的运行时动态数据结构。

- **核心职责**:
  - 管理动画图的生命周期（Initialize → Update → Evaluate）
  - 提供跨线程访问代理（AnimInstanceProxy）
  - 控制动画更新开关（`bUpdateAnimationEnabled`）

```cpp
struct AnimInstance : RCBase {
    void InitAnimInstance(RC<AnimGraphResource> &InAnimGraph);
    void UpdateAnimation(float DeltaSeconds, bool bNeedsValidRootMotion, EUpdateAnimationFlag UpdateFlag);
    void ParallelEvaluateAnimation(bool bForceRefPose, const SkeletalMesh *InSkeletalMesh, ParallelEvaluationData &OutData);
    
    RC<world::AnimGraphResource> anim_graph;
    SkeletalMesh *skel_mesh;
};
```

#### AnimInstanceProxy (`rbc_anim/anim_instance.h`)
动画实例代理，支持游戏线程和渲染线程的并发访问。

- **核心职责**:
  - 线程安全的动画状态访问
  - 骨骼需求计算（`RecalcRequiredBones`）
  - 动画更新入口（`UpdateAnimation`）
  - 动画评估入口（`EvaluateAnimation`）

```cpp
struct AnimInstanceProxy {
    void UpdateAnimation();           // Update Phase Entry
    void EvaluateAnimation(PoseContext &Output);  // Evaluate Phase Entry
    void CacheBones();
    
    AnimNode *root_node;              // Graph entry point
    luisa::shared_ptr<BoneContainer> required_bones;
};
```

#### SkeletalMesh (`rbc_anim/skeletal_mesh.h`)
骨骼网格运行时对象，完整的动画计算控制器。

**动画计算流程**:

```
Skeleton Asset → Rest Pose → BoneSpaceTransforms
                        ↓
              Sampling Job (AnimSequence + time)
                        ↓
              Runtime BoneSpaceTransform (SOA格式)
                        ↓
              Local To Model Job
                        ↓
              Runtime ComponentSpaceTransforms (矩阵)
                        ↓
              CPU/GPU Skinning
                        ↓
              Component Space Triangles (渲染)
```

**核心功能**:
- 管理动画评估上下文（`AnimationEvaluationContext`）
- 双缓冲骨骼变换（`ComponentSpaceTransformsArray[2]`）
- 支持并行动画评估（`DispatchParallelEvaluationTasks`）
- 渲染状态管理（`CreateRenderState_Concurrent`）

```cpp
struct SkeletalMesh : RCBase {
    // Game Thread Loop
    void Tick(float InDeltaTime_s);
    void TickAnimation(float InDeltaTime_s, bool bNeedsValidRootMotion);
    
    // Render Thread Loop
    void CreateRenderState_Concurrent(RenderDevice *device);
    void SendRenderDynamicData_Concurrent(AnimRenderState &state);
    
    // Resources
    RC<world::SkeletonResource> ref_skeleton;
    RC<world::SkinResource> ref_skin;
    RC<AnimInstance> anim_instance;
    
    // Transform Data (Double Buffered)
    luisa::vector<AnimFloat4x4> ComponentSpaceTransformsArray[2];
};
```

### 3. 动画图系统 (Animation Graph)

#### AnimGraph (`rbc_anim/graph/AnimGraph.h`)
动画图容器，存储节点集合。

```cpp
struct AnimGraph : RCBase {
    luisa::vector<RC<AnimNode>> nodes;
    AnimNode *GetRootNode();
};
```

#### AnimNode (`rbc_anim/graph/AnimNode.h`)
动画节点基类，所有动画节点的抽象接口。

**节点生命周期**:
1. `Initialize_AnyThread` - 初始化节点
2. `Update_AnyThread` - 更新节点状态（时间推进、权重计算等）
3. `Evaluate_AnyThread` - 评估输出姿势

```cpp
struct AnimNode : public RCBase {
    virtual void Initialize_AnyThread(const AnimationInitializationContext &InContext) = 0;
    virtual void Update_AnyThread(const AnimationUpdateContext &InContext) = 0;
    virtual void Evaluate_AnyThread(PoseContext &Output) = 0;
    
    IndexType NodeID = INVALID_INDEX;
};
```

#### PoseLink (`rbc_anim/graph/AnimNode.h`)
姿势链接，用于连接动画节点的输出和输入。

```cpp
struct PoseLink : RCBase {
    IndexType LinkedNodeID = INVALID_INDEX;  // Serialized ID
    AnimNode *LinkedNode = nullptr;          // Runtime pointer
    
    void AttemptRelink(const AnimationBaseContext &InContext);
    void Evaluate(PoseContext &Output);
};
```

#### 内置节点类型

**AnimNode_Root** (`rbc_anim/graph/AnimNode_Root.h`)
- 动画图的根节点，整个图的入口点
- 包含一个 `PoseLink result` 连接子节点

**AnimNode_SequencePlayer** (`rbc_anim/graph/AnimNode_SequencePlayer.h`)
- 动画序列播放器
- 播放单个 `AnimSequenceResource`
- 支持循环播放和时间累加

```cpp
struct AnimNode_SequencePlayer : public AnimNode {
    RC<world::AnimSequenceResource> anim_seq_resource;
    bool is_looping = true;
    float blend_weight = 0.0f;
    float internal_time_accumulator = 0.0f;
};
```

### 4. 姿势系统 (Pose System)

#### BoneContainer (`rbc_anim/bone_container.h`)
骨骼容器，管理运行时所需的骨骼子集。

- **功能**: 将完整骨骼映射到紧凑姿势（Compact Pose）
- **查找表**:
  - `CompactPoseToSkeletonIndex`: 紧凑索引 → 骨架索引
  - `SkeletonToCompactPose`: 骨架索引 → 紧凑索引

```cpp
struct BoneContainer {
    void InitializeTo(luisa::span<const BoneIndexType> InRequiredBoneIndices, SkeletalMesh *InSkelMesh);
    
    luisa::vector<BoneIndexType> bone_indices;          // Required bones
    luisa::vector<int32_t> CompactPoseToSkeletonIndex; // LUT
    ReferenceSkeleton *ref_skeleton;
};
```

#### CompactPose (`rbc_anim/bone_pose.h`)
紧凑姿势，SOA（Structure of Arrays）格式的骨骼变换。

```cpp
struct CompactPose : BaseCompactPose {
    void ResetToRefPose();
    void ResetToAdditiveIdentity();
    void NormalizeRotation();
    
    // SOA format storage
    luisa::vector<AnimSOATransform> bones;
};
```

**SOA格式优势**:
- SIMD友好，支持批量运算
- 每个属性（translation, rotation, scale）连续存储
- 适合现代CPU缓存结构

### 5. 渲染系统 (Rendering Layer)

#### SkeletalMeshRenderObject (`rbc_anim/render/skelmesh_render.h`)
骨骼网格渲染对象，负责蒙皮和GPU数据上传。

```cpp
struct SkeletalMeshRenderObject {
    virtual void Update(AnimRenderState &state, int32_t LODIndex, 
                       const SkeletalMeshSceneProxyDynamicData &InDynamicData, 
                       const world::SkinResource *InRefSkin) = 0;
    
    virtual bool IsCPUSkinned() { return true; }
    virtual bool IsGPUSkinned() { return false; }
};
```

**蒙皮流程**:
1. 从 `ComponentSpaceTransforms` 和 `inverse_bind_poses` 计算 `ReferenceToLocal` 矩阵
2. 执行CPU或GPU蒙皮（顶点位置/法线/切线变换）
3. 更新动态顶点缓冲区
4. 上传GPU供渲染器使用

#### SkelMeshRenderDataLOD (`rbc_anim/render/render_data.h`)
LOD渲染数据，包含蒙皮所需的顶点缓冲区。

```cpp
struct SkelMeshRenderDataLOD {
    luisa::vector<AnimFloat4x4> skin_matrices;
    luisa::vector<SkinPrimitive> skin_primitives;
    RC<world::MeshResource> morph_mesh;  // Deformed mesh instance
};
```

## 使用示例

### 从GLTF加载动画角色

```cpp
// 1. 注册导入器
world::register_builtin_importers();

// 2. 加载各个资源
auto skel = world::create_object<world::SkeletonResource>();
skel_importer->import(skel.get(), gltf_path);
skel->unsafe_set_loaded();

auto anim = world::create_object<world::AnimSequenceResource>();
anim->ref_skel = skel;
anim_importer->import(anim.get(), gltf_path);
anim->unsafe_set_loaded();

auto skin = world::create_object<world::SkinResource>();
skin->ref_skel = skel;
skin->ref_mesh = mesh;
skin_importer->import(skin.get(), gltf_path);
skin->generate_LUT();  // 生成查找表
skin->unsafe_set_loaded();

// 3. 创建动画图
auto anim_graph = world::create_object<world::AnimGraphResource>();
auto root = RC<rbc::AnimNode_Root>::New();
auto seq_player = RC<rbc::AnimNode_SequencePlayer>::New();
seq_player->anim_seq_resource = anim;
anim_graph->graph.nodes.emplace_back(root);
anim_graph->graph.nodes.emplace_back(seq_player);
root->result.LinkedNodeID = 1;  // 连接根节点到序列播放器
anim_graph->unsafe_set_loaded();

// 4. 创建骨骼网格资源
auto skel_mesh = world::create_object<world::SkelMeshResource>();
skel_mesh->ref_skin = skin;
skel_mesh->ref_skeleton = skel;
skel_mesh->ref_anim_graph = anim_graph;
skel_mesh->unsafe_set_loaded();

// 5. 创建实体并附加组件
auto entity = world::create_object<world::Entity>();
auto skelmesh_comp = entity->add_component<world::SkelMeshComponent>();
skelmesh_comp->SetRefSkelMesh(skel_mesh);

// 6. 每帧更新
skelmesh_comp->tick(delta_time);
skelmesh_comp->update_render();
```

### 自定义动画节点

```cpp
struct AnimNode_MyCustom : public AnimNode {
    void Initialize_AnyThread(const AnimationInitializationContext &ctx) override {
        // 初始化节点资源
    }
    
    void Update_AnyThread(const AnimationUpdateContext &ctx) override {
        // 更新时间、权重等
        internal_time += ctx.GetDeltaTime();
    }
    
    void Evaluate_AnyThread(PoseContext &output) override {
        // 输出姿势到output.Pose
        output.Pose.ResetToRefPose();
        // ... 自定义姿势计算
    }
    
    void Serialize(rbc::ArchiveWrite &w) override {
        // 序列化节点数据
    }
    
    void Deserialize(rbc::ArchiveRead &r) override {
        // 反序列化节点数据
    }
};
```

## 设计评估与优化建议

### 当前设计的优点

1. **清晰的资源分层**
   - 资源层（Resource Layer）与运行时层（Runtime Layer）分离
   - 资源可独立加载、保存、序列化
   - 支持异步加载和依赖管理

2. **灵活的动画图系统**
   - 基于节点图的可扩展架构
   - 支持运行时动态连接节点
   - PoseLink机制解耦节点依赖

3. **高效的SOA数据布局**
   - 使用OZZ Animation的SOA格式
   - SIMD友好，缓存局部性好
   - 支持并行评估

4. **双缓冲机制**
   - ComponentSpaceTransformsArray[2]支持读写分离
   - 避免游戏线程和渲染线程的竞争

5. **骨骼子集支持**
   - BoneContainer支持只计算所需骨骼
   - 减少不必要的计算开销

### 架构层面的优化建议

#### 1. 动画图编译期优化

**现状**: 动画图在运行时每帧遍历节点，存在虚函数调用开销。

**建议**: 实现动画图编译系统
```cpp
struct CompiledAnimGraph {
    // 将节点图编译为线性指令序列
    luisa::vector<AnimInstruction> instructions;
    
    // 连续内存存储所有节点状态
    luisa::vector<std::byte> node_states;
    
    void ExecuteUpdate(const AnimationUpdateContext &ctx);
    void ExecuteEvaluate(PoseContext &output);
};
```

**收益**:
- 消除虚函数调用
- 更好的指令缓存局部性
- 支持向量化节点执行

#### 2. 动画数据流批处理

**现状**: 每个SkeletalMesh独立计算动画，缓存不共享。

**建议**: 实现动画计算调度器
```cpp
struct AnimationScheduler {
    // 收集所有需要更新的动画实例
    void CollectTickRequests(luisa::span<SkeletalMesh *> meshes);
    
    // 批量执行相同类型的动画节点
    void BatchUpdateNodes(luisa::span<AnimNode_SequencePlayer *> players);
    
    // 并行批处理采样任务
    void ParallelBatchSampling(luisa::span<SamplingTask> tasks);
};
```

**收益**:
- 相同动画的实例共享采样结果
- 批量矩阵运算可利用SIMD
- 减少缓存抖动

#### 3. GPU蒙皮与计算着色器

**现状**: 当前主要支持CPU蒙皮（`IsCPUSkinned() == true`）。

**建议**: 完整实现GPU蒙皮管线
```cpp
struct GPUSkinningRenderObject : SkeletalMeshRenderObject {
    // 上传骨骼矩阵到GPU Buffer
    void UploadBoneMatrices(luisa::span<const AnimFloat4x4> matrices);
    
    // 计算着色器执行蒙皮
    void DispatchSkinningComputeShader(AnimRenderState &state);
    
    // 直接使用GPU顶点缓冲区渲染
    bool IsGPUSkinned() override { return true; }
};
```

**收益**:
- 解放CPU计算资源
- 支持更多骨骼网格实例
- 蒙皮与渲染零拷贝

#### 4. 动画LOD系统

**现状**: `PredictedLODLevel` 字段存在但未实现功能。

**建议**: 实现完整的动画LOD
```cpp
struct AnimLODConfig {
    float distance_threshold;
    int32_t max_bone_count;      // 限制骨骼数量
    float sample_rate_scale;      // 降低采样率
    bool disable_facial_bones;    // 禁用面部骨骼
};

struct SkeletalMesh {
    void UpdateLOD(float distance_to_camera);
    void ComputeRequiredBonesForLOD(int32_t LODIndex, luisa::vector<BoneIndexType> &out_bones);
};
```

**收益**:
- 远处角色降低动画精度
- 根据重要性分配计算资源
- 支持大规模角色场景

#### 5. 动画事件系统

**现状**: 缺乏动画事件触发机制。

**建议**: 添加事件轨道支持
```cpp
struct AnimEvent {
    float trigger_time;
    luisa::string event_name;
    luisa::vector<std::byte> payload;
};

struct AnimSequenceResource {
    luisa::vector<AnimEvent> events;
};

struct AnimInstance {
    // 在每帧更新时检查并触发事件
    void ProcessAnimEvents(float prev_time, float curr_time);
};
```

**收益**:
- 支持 footsteps、攻击判定帧等游戏逻辑
- 动画与Gameplay解耦
- 可视化编辑事件

#### 6. 混合空间 (Blend Space)

**现状**: 缺乏基于参数的动画混合（如8方向移动）。

**建议**: 实现BlendSpace资源
```cpp
struct BlendSpace2D : AnimNode {
    luisa::string x_param;  // "Speed"
    luisa::string y_param;  // "Direction"
    
    struct SamplePoint {
        float2 coord;
        RC<AnimSequenceResource> animation;
    };
    luisa::vector<SamplePoint> samples;
    
    void Evaluate_AnyThread(PoseContext &output) override {
        // 根据当前参数值插值多个动画
        float x = GetParamValue(x_param);
        float y = GetParamValue(y_param);
        // 三角剖分插值...
    }
};
```

**收益**:
- 自然流畅的角色移动
- 减少美术制作动画数量
- 参数化动画控制

#### 7. 根运动 (Root Motion)

**现状**: 接口存在（`bNeedsValidRootMotion`）但未完整实现。

**建议**: 完整实现根运动提取和应用
```cpp
struct RootMotionExtractor {
    // 从动画提取根骨骼位移/旋转
    Transform ExtractRootMotion(AnimSequenceResource *anim, float start_time, float end_time);
};

struct AnimInstance {
    Transform ConsumeRootMotion();
    void ApplyRootMotionToTransform(TransformComponent *transform);
};
```

**收益**:
- 动画驱动角色移动
- 精准的 foot locking
- 自然的上下坡适应

#### 8. 动画压缩

**现状**: 依赖OZZ Animation的基础压缩。

**建议**: 实现更激进的压缩选项
```cpp
struct AnimCompressionSettings {
    float position_error_threshold;
    float rotation_error_threshold;
    float scale_error_threshold;
    bool remove_linear_keys;      // 移除可线性插值的关键帧
    bool use_fcurve_compression;  // 曲线拟合压缩
};

struct AnimSequence {
    void Compress(const AnimCompressionSettings &settings);
};
```

**收益**:
- 减少内存占用
- 更快的缓存命中
- 支持更多动画资源

#### 9. 动画IK/FK系统

**现状**: 缺乏运行时IK解算。

**建议**: 添加IK/FK节点
```cpp
struct AnimNode_TwoBoneIK : public AnimNode {
    BoneIndexType upper_bone;
    BoneIndexType lower_bone;
    BoneIndexType end_bone;
    float3 target_position;
    
    void Evaluate_AnyThread(PoseContext &output) override {
        // 解算IK并修改输出姿势
    }
};
```

**收益**:
- Foot IK适应地形
- LookAt IK注视目标
- Hand IK抓取物体

#### 10. 动画蓝图可视化编辑

**现状**: 动画图需要手写代码创建。

**建议**: 实现可视化编辑器支持
```cpp
// 扩展AnimGraph序列化格式支持编辑器元数据
struct AnimGraphEditorData {
    luisa::vector<NodePosition> node_positions;
    luisa::vector<CommentBox> comments;
    luisa::hash_map<luisa::string, VariableDefinition> variables;
};
```

**收益**:
- 美术人员可独立制作动画逻辑
- 实时预览和调试
- 更快的迭代速度

### 总结

RoboCute的骨骼动画系统已经具备了工业级引擎的基础架构，资源分层清晰，运行时性能考虑周到。主要的优化方向集中在：

1. **性能优化**: 动画图编译、批处理、GPU蒙皮
2. **功能扩展**: 事件系统、混合空间、根运动、IK
3. **工具链**: 可视化编辑器、动画压缩
4. **可扩展性**: 更灵活的节点系统、LOD支持

建议按照优先级逐步实现：
- **高优先级**: GPU蒙皮、动画LOD、事件系统
- **中优先级**: 混合空间、根运动、动画压缩
- **低优先级**: IK系统、可视化编辑器

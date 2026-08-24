# Shader 变体系统

本文档描述 RoboCute 当前 Shader 变体系统的完整设计：如何声明编译变体、如何从材质推导场景 feature、如何在运行时选择一组相互兼容的 Shader，以及如何构建、验证和发布这些产物。

当前实现只使用 `schema_version: 2`。没有旧 schema 兼容、变体 fallback 或缺失产物降级；描述、产物或代际不一致都应尽早报错。

## 1. 目标与边界

系统解决的核心问题是：

> 根据场景中**正在被渲染对象使用的材质**自动选择 Shader 变体，同时不让渲染 Pass 自己维护材质规则、变体枚举和文件路径。

当前 `offline_pt` family 有一个 `pbr_material` 维度：

| 场景状态 | 选择结果 | 编译行为 |
| --- | --- | --- |
| 没有复杂 PBR 材质 | `pbr_material=lite` | 定义 `RBC_LITE_PBR_MATERIAL=1` |
| 有复杂 PBR 材质但没有 FSD | `pbr_material=full` | 不定义 Lite/FSD 宏 |
| 至少有一个 FSD 材质 | `pbr_material=fsd` | 定义 `RBC_ENABLE_FREE_SPACE_DIFFRACTION=1` |

`lite`、`full` 和 `fsd` 只是当前维度的 value，不是写死在 `OfflinePTPass` 中的通用枚举。以后可以增加其他 feature、dimension、value 和 family，运行时仍使用相同的选择机制。

设计边界如下：

- source manifest 是构建输入，由开发者维护。
- runtime manifest 是构建输出，由工具自动生成，不能手工维护。
- C++ 材质分类代码负责回答“当前材质需要哪些能力”。
- `SceneManager` 负责回答“当前场景正在使用哪些能力”。
- `ShaderManager` 负责把场景 feature 精确映射到 artifact。
- `ShaderFamily` 负责异步加载并整组发布必须同步切换的 Shader。
- 渲染 Pass 只绑定原有 program loader 和 slot，不描述 Lite/Full 规则。
- 稳定帧不扫描材质、不解析 JSON、不查询文件，也不等待 Shader 加载。

## 2. 术语与职责

### 2.1 术语

| 术语 | 含义 |
| --- | --- |
| source manifest | 手写的 `rbc/shader/shader_variants.json`，描述编译空间和场景选择规则 |
| scene feature | 从场景数据推导出的能力位，例如 `complex_pbr_material` |
| dimension | 一个编译维度，例如 `pbr_material` |
| value | 维度的一个取值，例如 `lite`、`full`、`fsd` |
| permutation | 一组明确的 dimension selection；只有显式声明的 permutation 才会构建 |
| variant set | source manifest 中的一组 permutation 及其 program 成员；构建后投影为 runtime family |
| program | 一个逻辑 Shader 程序，例如 `path_tracer/offline_pt` |
| selection | 运行时精确选择键，例如 `pbr_material=full` |
| artifact | 某个 program 在某个 backend 和 selection 下的 `.bin` 文件 |
| runtime manifest | 自动生成的 `shader_manifest.json`，记录实际 artifact、完整性信息和 family 规则 |
| Shader family | 必须使用同一 selection、同一代构建并整组切换的一组 program |

### 2.2 三份描述为什么不重复

这套系统有三类信息，它们看起来相关，但各自描述不同事实：

| 位置 | 描述的事实 | 为什么不能由另一层代替 |
| --- | --- | --- |
| `shader_variants.json` | 允许构建哪些维度、宏、permutation 和 program 组合 | Shader 源码本身无法声明构建矩阵、场景选择规则和发布完整性 |
| `shader_features.h` 等 C++ 代码 | 具体运行时对象如何从真实材质数据计算 feature | JSON 不知道 C++ 材质结构、生命周期和哪些材质正在被场景使用 |
| `shader_manifest.json` | 本次构建实际生成了哪些文件，它们的路径、大小、哈希和 build id | source manifest 不能证明编译产物确实存在，也不能记录 backend 特定结果 |

渲染 Pass 中还需要少量绑定代码，因为生成的 host ABI loader 是 C++ 类型信息。它只绑定：

```text
logical program name -> 原有 Shader 指针 slot -> 原有 ABI load_shader()
```

它不重复描述宏、selection、required feature 或 artifact 路径。

## 3. 端到端流程

### 3.1 构建期

```text
shader_variants.json
Shader 源码和 include
clangcxx compiler 及其模板/DLL
        |
        v
rbcxx --variant=build（C++ 驱动，严格校验 schema、路径、规则覆盖、宏冲突和 program 归属）
        |
        v
展开显式 permutations，并为每个 backend 编译（自调用单文件/目录编译模式）
        |
        +--> 默认/variant .bin
        +--> 自动生成 shader_manifest.json
        +--> stable program 对所有 define 组合执行 hostgen 并验证 ABI
        +--> per_variant program 为每个 permutation 生成独立 host ABI
        +--> rbc/shader/host/*.inl、host/variants/**/*.inl + .shader_input_id
        |
        v
rbcxx --variant=verify-coherence（DX/VK/host/render plugin input_id 一致）
        |
        v
artifact_publish 原子发布完整一代产物
```

### 3.2 运行期

```text
材质 JSON 或参数变化
        |
        v
MaterialResource::_install()
  更新 GPU 材质后计算该材质的 feature mask
        |
        v
RenderComponent/ObjectStub bind 或 unbind 材质 source
        |
        v
SceneManager 对 active source 做逐 feature 引用计数
  发布一个 atomic scene feature mask
        |
        v
OfflinePTPass::early_update()
  scene.shader_features()
        |
        v
ShaderFamily::acquire()
        |
        +--> ShaderManager 根据 runtime manifest 选唯一 selection
        +--> 后台并行加载 family 的全部 program
        +--> 全部成功后由渲染线程整组发布 slot
        |
        v
revision 变化 -> 清零 PT 累积 -> 使用新变体 dispatch
```

这条链路中，示例、材质或场景代码从不直接指定 `lite`/`full` Shader。它们只修改真实材质数据；scene feature 和 Shader selection 都由后续层自动推导。

## 4. Source Manifest

唯一手写入口是 `rbc/shader/shader_variants.json`。当前配置的核心内容如下：

```json
{
  "schema_version": 2,
  "backends": ["dx", "vk"],
  "compile": {
    "source_root": "src",
    "include_dirs": ["include"],
    "optimization": "on",
    "defines": {}
  },
  "scene_features": [
    "complex_pbr_material",
    "free_space_diffraction"
  ],
  "dimensions": {
    "pbr_material": {
      "default": "lite",
      "values": {
        "lite": {
          "defines": {"RBC_LITE_PBR_MATERIAL": "1"},
          "scene_features": {
            "forbidden": [
              "complex_pbr_material",
              "free_space_diffraction"
            ]
          }
        },
        "full": {
          "defines": {},
          "scene_features": {
            "required": ["complex_pbr_material"],
            "forbidden": ["free_space_diffraction"]
          }
        },
        "fsd": {
          "defines": {"RBC_ENABLE_FREE_SPACE_DIFFRACTION": "1"},
          "scene_features": {
            "required": ["free_space_diffraction"]
          }
        }
      }
    }
  },
  "variant_sets": {
    "offline_pt": {
      "default": "lite",
      "permutations": [
        {"id": "lite", "select": {"pbr_material": "lite"}},
        {"id": "full", "select": {"pbr_material": "full"}},
        {"id": "fsd", "select": {"pbr_material": "fsd"}}
      ]
    }
  },
  "programs": [
    {
      "id": "path_tracer/offline_pt",
      "source": "src/path_tracer/offline_pt.cpp",
      "variant_set": "offline_pt",
      "host_abi": "per_variant"
    },
    {
      "id": "path_tracer/offline_pt_denoise",
      "source": "src/path_tracer/offline_pt.cpp",
      "defines": {"RBC_OFFLINE_PT_DENOISE": null},
      "variant_set": "offline_pt",
      "host_abi": "per_variant"
    },
    {
      "id": "path_tracer/pt_multi_bounce_offline",
      "source": "src/path_tracer/pt_multi_bounce_offline.cpp",
      "variant_set": "offline_pt",
      "host_abi": "per_variant"
    }
  ]
}
```

### 4.1 顶层字段

| 字段 | 规则 |
| --- | --- |
| `schema_version` | 当前必须严格等于 `2`，没有版本兼容分支 |
| `backends` | 非空、唯一的 backend 列表；当前为 `dx` 和 `vk` |
| `compile.source_root` | 相对 `rbc/shader` 的 Shader `.cpp` 根目录 |
| `compile.include_dirs` | 相对 `rbc/shader` 的 include 根目录列表，至少一个 |
| `compile.optimization` | 只能是 `on` 或 `off` |
| `compile.defines` | 所有编译共同使用的宏，值是字符串或 `null` |
| `scene_features` | runtime manifest 规则允许引用的 feature 全集 |
| `dimensions` | 编译维度及其 value、宏和场景规则 |
| `variant_sets` | 显式 permutation 集合及默认 permutation |
| `programs` | 需要参与变体构建的 Shader program |

未知字段会直接报错，避免拼写错误被静默忽略。

### 4.2 Dimension 与 value

每个 dimension 必须有：

- 一个 `default`，且必须存在于 `values`。
- 至少一个 value。
- 每个 value 可增加 `defines`。
- 每个 value 可声明 `scene_features.required` 和 `scene_features.forbidden`。

`required` 表示这些 feature 必须全部存在；`forbidden` 表示这些 feature 必须全部不存在。两者不能包含同一个 feature，也不能引用顶层未声明的 feature。

公共 `compile.defines` 与有效维度组合不能重复定义同名宏。这样可以防止宏来源不明确，或者不同编译入口得到不同值。

### 4.3 Variant set 与 permutation

`variant_sets.<name>.default` 引用的是一个 permutation `id`。`id` 只用于配置内部标识和默认引用，真正进入运行时 selection 的是 `permutations[].select`。

例如：

```json
{
  "id": "full",
  "select": {"pbr_material": "full"}
}
```

会得到 selection：

```text
pbr_material=full
```

多维 selection 会按 dimension 名排序，并用 `+` 连接：

```text
quality=high+transport=spectral
```

同一个 variant set 的所有 permutation 必须选择完全相同的一组 dimension。默认 permutation 必须选择这些 dimension 各自的全局 default value。

生成器会穷举 family 涉及的 scene feature 布尔组合，并要求每种组合**恰好命中一条**规则：

- 没有规则命中是 coverage gap。
- 多条规则命中是 ambiguity。
- 两者都会在构建前报错。
- 当前每个 family 最多允许 16 个参与选择的 scene feature。

这项校验针对规则空间，不会隐式生成 dimension 的笛卡尔积。要构建的 permutation 仍必须在 JSON 中显式列出。

### 4.4 Program

`programs[]` 只列出需要变体的 program：

- `id` 是唯一逻辑名；多个 program 可以共享一个物理 `source`。
- `source` 必须存在、位于 source root 内并以 `.cpp` 结尾。
- `defines` 是该 logical program 独有的宏，不能覆盖公共或维度宏。
- `variant_set` 必须存在。
- 一个 program 只能属于一个 variant set。
- 每个 variant set 至少要被一个 program 使用。
- `host_abi` 必须是 `stable` 或 `per_variant`。

source root 下没有列入 `programs[]` 的 `.cpp` 仍会被发现和编译，生成默认 backend 产物和默认 host interface，但不属于任何 runtime family。

### 4.5 `host_abi: stable`

`stable` 不是注释。构建系统会对该 program 的所有实际 define 组合运行 hostgen，规范化生成的 `.inl`，然后比较接口。

如果宏改变了 host 侧参数类型、数量或布局，构建会报：

```text
Stable host ABI changed
```

这样使用 stable ABI 的 Pass 可以继续使用原有 namespace、`load_shader()` 和 `dispatch_shader()`，不需要为每个 selection 生成另一套 C++ 名字。

### 4.6 `host_abi: per_variant`

当宏有意改变 program 的参数列表时，使用 `per_variant`。这类 program 不参加 stable ABI 比较；hostgen 会使用 program 所属 variant set 的每个 permutation 对应的完整 define 集分别生成接口，并发布到：

```text
rbc/shader/host/variants/<variant-set>/<permutation-id>/<program-id>.inl
```

例如 `offline_pt/fsd` 的接口为：

```text
rbc/shader/host/variants/offline_pt/fsd/path_tracer/offline_pt.inl
```

每个 permutation 有独立缓存键，包含编译器指纹、Shader 输入摘要、selection、维度 define、program define 和 program 列表。`variants/` 是生成器管理的目录；重新生成时会整体替换，目录外的手写 host helper 保持不变。共享 source 的 logical program 由 program define 区分，不需要 wrapper entrypoint。

hostgen 对每个 permutation 生成一次完整 source tree，以刷新普通 Shader 接口；同源且带 program define 的 logical program 再做一次隔离 hostgen，避免它的宏污染同一物理 source 的其他 program。最后从根目录移除 `per_variant` program 接口并发布到 `variants/<family>/<permutation>/`。因此普通 Shader 不会留下陈旧 `.inl`，同源变体也保持独立 ABI 和缓存键。

## 5. 材质如何变成 Scene Feature

当前 feature 注册表位于 `rbc/runtime/include/rbc_graphics/shader_features.h`：

```cpp
enum class SceneShaderFeature : uint64_t {
    ComplexPbrMaterial = 1ull << 0u,
    FreeSpaceDiffraction = 1ull << 1u,
};
```

同一文件还维护 runtime manifest 名字到 bit 的映射：

```text
"complex_pbr_material" -> SceneShaderFeature::ComplexPbrMaterial
"free_space_diffraction" -> SceneShaderFeature::FreeSpaceDiffraction
```

这是 source JSON 和 C++ runtime 之间必须保持一致的契约。runtime manifest 中出现未注册 feature 会被拒绝。

Feature identity bit 必须彼此独立，manifest 的 `required` 和 `forbidden` 始终针对这些 identity bit 做精确匹配。材质分类直接返回该材质需要的完整能力集合；FSD 材质返回：

```text
free_space_diffraction | complex_pbr_material
```

`SceneManager` 只聚合 source 已经发布的 bit，不包含 FSD 特判。`fsd` rule 只需 require FSD，而普通 `full` rule require Complex 并 forbid FSD。这样 complex-only 场景仍选 `full`，FSD 场景唯一选择 `fsd`。

### 5.1 当前 Lite 判定

材质只有同时满足以下条件才兼容 Lite PBR：

| 字段 | Lite 条件 |
| --- | --- |
| `diffuse_roughness` | 等于 `0` |
| `coat` | 等于 `0` |
| `transmission` | 等于 `0`；或者等于 `1` 且 `metallic == 0` |
| `subsurface` | 字段存在时必须等于 `0` |
| `thin_film` | 字段存在时必须等于 `0` |
| `fuzz` | 字段存在时必须等于 `0` |
| `diffraction` | 字段存在时必须等于 `0` |

不满足任意一条就设置 `ComplexPbrMaterial` bit。

`OpenPBRParticle` 复用同一个结构化检查；不存在于该材质类型中的字段不会被访问。

纹理 handle 当前不参与分类。约束是：权重纹理只能调制一个已经启用的 lobe，不能把 host 权重为 `0` 的不支持 lobe 激活。如果以后纹理可以突破这个约束，分类函数也必须同步修改。

### 5.2 分类时机

`MaterialResource::_install()` 的顺序是：

1. 安装或更新 GPU 材质。
2. 从已经安装的数据计算 feature mask。
3. 原子保存 material mask。
4. 通知 `SceneManager::update_shader_feature_source()`。

所以场景 feature 对应的是已经对 GPU 生效的材质，不会提前按一份尚未安装的 JSON 切换 Shader。

对已安装材质调用 `load_from_json()` 时，更新会排入渲染线程并按 generation 合并。连续修改不会为每次中间状态重复安排无意义的安装；安装期间再次变化时，会再安排最新一代。

### 5.3 只有正在使用的材质才影响场景

加载一个复杂材质并不会自动把场景切到 Full。材质 source 只有被 `RenderComponent` 或 `ObjectStub` 绑定后才 active。

每个 source 在 `SceneManager` 中记录：

```text
mask + binding_count + directly_active
```

- 普通材质使用 `update/bind/unbind`。
- 同一材质被多个对象使用时，`binding_count` 保证移除一个对象不会错误清除 feature。
- 绑定列表变化时先 bind 新列表，再 unbind 旧列表，避免仍在使用的 feature 瞬间消失。
- Voxel 等没有普通材质绑定生命周期的入口可以使用 direct source，在 `set()` 到 `remove()` 之间保持 active。
- source 销毁时必须从场景移除。

`SceneManager` 对 64 个 feature bit 分别维护引用计数。只要任意 active source 含某个 bit，聚合 mask 就保留该 bit；最后一个 source 移除后才清除。

## 6. Runtime Manifest 与构建产物

每个 backend 会生成一个完整的 Shader root。Windows x64 默认位置为：

```text
build/windows/x64/shader_build_dx/
build/windows/x64/shader_build_vk/
```

典型目录结构：

```text
shader_build_dx/
|-- shader_manifest.json
|-- path_tracer/
|   |-- offline_pt.bin
|   |-- offline_pt_denoise.bin
|   `-- pt_multi_bounce_offline.bin
`-- variants/
    `-- pbr_material=full/
        `-- path_tracer/
            |-- offline_pt.bin
            |-- offline_pt_denoise.bin
            `-- pt_multi_bounce_offline.bin
```

默认 selection 复用原有普通路径，所以现有 program 名和 loader 不需要改名。非默认 selection 放在：

```text
variants/<canonical-selection>/<program>.bin
```

### 6.1 `shader_manifest.json` 是自动生成的

runtime manifest 由 source manifest 和实际编译结果生成。它包含：

- `schema_version` 和 `backend`。
- compiler fingerprint。
- `input_id` 和 backend 特定的 `build_id`。
- 每个 program 的 `default_selection`。
- 每个 variant 的 selection、artifact、size、SHA-256 和 compile key。
- 从 `variant_sets` 与 `programs[].variant_set` 投影得到的 `families`。
- 每个 family 的成员 program 和 required/forbidden feature 规则。

简化后的 family 部分如下：

```json
{
  "families": {
    "offline_pt": {
      "programs": [
        "path_tracer/offline_pt",
        "path_tracer/offline_pt_denoise",
        "path_tracer/pt_multi_bounce_offline"
      ],
      "rules": [
        {
          "selection": {"pbr_material": "lite"},
          "required_features": [],
          "forbidden_features": [
            "complex_pbr_material",
            "free_space_diffraction"
          ]
        },
        {
          "selection": {"pbr_material": "full"},
          "required_features": ["complex_pbr_material"],
          "forbidden_features": ["free_space_diffraction"]
        },
        {
          "selection": {"pbr_material": "fsd"},
          "required_features": ["free_space_diffraction"],
          "forbidden_features": []
        }
      ]
    }
  }
}
```

它的功能不是重复 source JSON，而是把抽象声明冻结成某个 backend 的可验证运行时索引。运行时不需要重新读取 source tree、展开 permutation 或猜测文件名。

### 6.2 `input_id` 与 `build_id`

`input_id` 表示一代共同输入，由以下信息决定：

- source manifest。
- source root 和 include dirs 下的全部输入文件。
- clangcxx compiler 及相邻 DLL/模板的整体 fingerprint。
- source schema 版本。

DX、VK、hostgen 和 render plugin 必须具有相同 `input_id`，否则它们不是同一代输入。

`build_id` 还包含 backend、program/variant 记录、compile key，以及实际 artifact 的路径、SHA-256 和 size。因此 DX 与 VK 的 `build_id` 通常不同，不要求相等。

### 6.3 完整性检查分层

| 阶段 | 检查内容 |
| --- | --- |
| 构建/verify | manifest 结构、规则、所有 artifact 存在、size 和 SHA-256 |
| 发布 | 再次检查 backend、host、plugin 的代际一致性和 staging 完整性 |
| runtime 启动 | schema、backend、program、family、selection、路径和元数据格式 |
| runtime resolve | 精确 artifact 存在且实际 size 与 manifest 相同 |

运行时不会每次启动重新计算大文件 SHA-256；内容哈希由构建和发布阶段保证。这样保留完整性保障，同时避免运行时启动成本扩大。

## 7. Runtime 选择与整组切换

### 7.1 `ShaderManager`

`SceneManager` 创建时会用当前 backend 的 Shader root 创建 `ShaderManager`。Manager 只在启动时读取一次 `shader_manifest.json`，并严格解析：

- schema 与 backend。
- `input_id`、`build_id` 基本格式。
- program、default selection 和 variant 唯一性。
- artifact 安全相对路径与 size。
- family 成员存在且不重复。
- required/forbidden feature 合法且不相交。
- 每个 family program 都具有规则所指向的精确 selection。

Family rule 的匹配条件是：

```cpp
required_match =
    (scene_mask & required_features) == required_features;
forbidden_match =
    (scene_mask & forbidden_features) == 0;
```

必须恰好匹配一条 rule。

`resolve_shader_variant()` 只接受 selection 完全相等的记录。`resolve_shader_family()` 还会保证所有成员使用同一 selection 和同一 build id。

普通、不属于 family 的直接 Shader 加载仍保留原有缓存和 reload 行为；family artifact 则不能被单独 unload/reload，否则 `ShaderFamily` 缓存的整组指针会失去一致性。

### 7.2 `ShaderFamily`

`ShaderFamily` 是一组 Shader 的运行时状态机。每个 selection 有一个缓存 entry：

```text
Unloaded -> Loading -> Ready
                    `-> Failed
```

加载流程如下：

1. `ShaderManager` 解析 family 的全部 program。
2. `ShaderFamily` 按 logical name 对齐 program，不依赖 JSON 数组顺序。
3. 校验成员 selection 和 build id 一致。
4. 在后台 fiber 中并行调用各自的 ABI loader。
5. 任何一个成员失败，整个 entry 进入 `Failed`。
6. 只有全部成员 ready，渲染线程下一次 `acquire()` 才一次写入所有 slot。

这里的“整组原子发布”是渲染线程边界上的逻辑原子性，不是把多个指针包装成多个 atomic。`prefetch()`、`acquire()` 和 dispatch 必须位于同一个渲染线程；Debug 构建会检查 `acquire()` 的 owner thread。

切换到尚未加载的 selection 时：

- 旧 family slot 会一起清空。
- family `revision` 增加。
- 不会在新变体未 ready 时继续误用旧变体。
- 后台加载完成后，slot 一起发布，`revision` 再次增加。

已成功加载的 entry 会缓存。`Lite -> Full -> Lite` 的第二次 Lite 切换不会重新读文件或创建 Shader。

`wait()` 只用于析构，确保后台任务不能晚于 family 生命周期；渲染帧不会等待首次加载。

### 7.3 `OfflinePTPass` 集成

`OfflinePTPass` 保留原有成员和生成的 ABI 名称：

```text
_pt_shader          -> offline_pt_shader::load_shader
_pt_shader_denoise  -> offline_pt_shader_denoise::load_shader
_multi_bounce       -> offline_multibounce::load_shader
```

构造函数只把这三个 binding 注册到 `offline_pt` family。Pass 内没有 Lite/Full enum、宏规则、manifest 路径或 fallback 分支。

`on_enable()` 非阻塞地 `prefetch()` 当前 selection。`early_update()` 每帧 acquire 当前 scene mask；当 family revision 变化时，PT 累积帧归零，避免两个不同 Shader 变体共享历史结果。

Family 尚未 ready 时，三个 slot 都为空。非 AO 路径会暂时只绘制 sky，保证下游资源仍然有效，并且不提交错误的 PT Shader。加载失败不是长期 sky-only 降级，而是 fatal error。

## 8. 性能模型

### 8.1 稳定帧

材质和 scene feature 没有变化时，每帧成本为：

| 操作 | 成本 |
| --- | --- |
| `SceneManager::shader_features()` | 一次 atomic acquire load |
| `ShaderFamily::acquire()` | 一次 completion epoch atomic acquire，加 mask/epoch 比较 |
| `OfflinePTPass` | 一次 revision 比较 |

稳定帧没有：

- 材质遍历。
- JSON 或 manifest 解析。
- 文件存在性/size 查询。
- mutex/spin mutex 获取。
- Shader 加载等待。

### 8.2 变化时

| 事件 | 工作量 |
| --- | --- |
| 材质 install/reinstall | 固定数量的浮点比较，计算一个 feature mask |
| bind/unbind/update | 一次 scene feature 锁和固定 64 bit 扫描 |
| 第一次遇到 selection | 规则选择、artifact resolve 和后台并行加载 |
| 切回已加载 selection | 从缓存整组发布，不重新加载 |

因此这套自动选择不会把材质复杂度检查放进逐帧热路径。真正较重的工作只发生在材质或 selection 改变时。

## 9. 构建与发布

### 9.1 Compiler 路径

`uv run prepare -y` 会准备所需工具和资源，Shader 构建入口按约定路径使用它，所以通常不需要手工在 `PATH` 中查找 compiler。需要覆盖时可以显式传 `--compiler`。

### 9.2 常用命令

先同步完整开发环境并准备工具：

```powershell
uv sync --extra=all
uv run prepare -y
```

只校验 source manifest 和输入：

```powershell
build/tool/rbcxx/rbcxx.exe --variant=validate `
  --project-root=. `
  --manifest=rbc/shader/shader_variants.json
```

通过 Xmake 构建 manifest 中声明的全部 backend，并生成 host ABI：

```powershell
xmake build compile_shaders_hostgen
```

分别验证 backend Shader root：

```powershell
build/tool/rbcxx/rbcxx.exe --variant=verify `
  --project-root=. `
  --build-root=build/windows/x64 `
  --backend=dx

build/tool/rbcxx/rbcxx.exe --variant=verify `
  --project-root=. `
  --build-root=build/windows/x64 `
  --backend=vk
```

检查 backend、host 和 render plugin 是否来自同一代输入：

```powershell
build/tool/rbcxx/rbcxx.exe --variant=verify-coherence `
  --project-root=. `
  --build-root=build/windows/x64 `
  --host-out=rbc/shader/host `
  --plugin-marker=build/windows/x64/releasedbg/rbc_render_plugin.input_id
```

完整构建并安装 Python extension 产物：

```powershell
uv run scripts/build_and_copy.py releasedbg uv
```

生成供 Shader IDE/LSP 使用的编译数据库：

```powershell
build/tool/rbcxx/rbcxx.exe --variant=lsp `
  --project-root=. `
  --out=rbc/shader/compile_commands.json
```

`--variant=build` 还支持 repeatable `--backend`、`--build-root`、`--cache-root`、`--hostgen`、`--hostgen-only`、`--host-out`、`--rebuild` 和 `--quiet`；`--variant=backends` 打印声明的 backend 列表（每行一个）。

### 9.3 缓存与原子构建

默认变体缓存位于：

```text
build/.shader_cache/variants-v1/
```

缓存不是发布产物，可以删除后重建。构建基于输入快照工作，并使用进程锁和临时目录；如果构建期间输入发生变化，会重试一次。输入持续变化或编译失败时保留上一份完整 Shader root，不发布半套结果。

### 9.4 `artifact_publish.py` 的职责

`src/rbc_build/artifact_publish.py` 只参与构建/安装，不参与运行时选择，也没有逐帧性能成本。

它负责：

1. 完整验证每个 backend manifest、artifact size 和 SHA-256。
2. 检查 DX、VK、hostgen 和 render plugin 的 `input_id` 一致。
3. 把二进制和全部 `shader_build_<backend>` 放入临时 staging root。
4. 再次验证 staging 和构建源没有在发布期间变化。
5. 原子替换 `src/robocute/rbc_ext/_C` 中受管理的整套产物。
6. 保留该目录中不受管理的文件，例如已有 `.pyi`。

任何一步失败，旧发布目录保持不变。

## 10. 如何扩展

### 10.1 增加新的 Scene Feature

假设要增加 `spectral_material`：

1. 在 `SceneShaderFeature` 中分配新的唯一 bit。
2. 在字符串映射中注册 `"spectral_material"`。
3. 扩展对应材质类型的 `scene_shader_feature_mask(material)` 分类逻辑。
4. 确认所有相关资源和对象入口正确 update/bind/unbind source。
5. 在 source manifest 顶层 `scene_features` 声明该名字。
6. 在 dimension value 中添加 required/forbidden 规则。
7. 补齐 family 涉及的所有 feature 组合，保证唯一覆盖。
8. 增加分类、规则和 runtime family 测试。

!!! warning "分类必须保守"
    当前模板分类函数对不满足 OpenPBR 结构要求的未知类型返回 `0`。如果一种新材质可能需要复杂 Shader，必须显式扩展分类；否则它会被当成 Lite compatible。

### 10.2 给现有 family 增加 value 或 dimension

1. 在 `dimensions` 中增加 value 或新 dimension。
2. 为 value 描述宏和 scene feature 条件。
3. 在 `variant_sets.offline_pt.permutations` 中显式增加所有有效 selection。
4. 保证所有 permutation 选择相同的 dimension key 集合。
5. 保证默认 permutation 使用各 dimension 的全局 default。
6. 在 Shader 源码中实现对应宏分支。
7. 运行 validate、host ABI 检查和全部测试。

不要在 Pass 中添加 selection 枚举或拼接 `variants/...` 路径。Pass 继续只向 `ShaderFamily` 提供 logical name、slot 和 loader。

### 10.3 增加新的 family

1. 在 source manifest 新建 `variant_sets.<family>`。
2. 把必须同步切换的 program 放入该 variant set。
3. 为每个 program 保持稳定 host ABI。
4. 在消费方构造一个 `ShaderFamily`，绑定原有 logical program 名和 ABI loader。
5. 每帧使用 scene snapshot 调用 `acquire()`，并用 revision 使该 Pass 的历史状态失效。
6. 定义 family 未 ready 时的瞬时输出行为，不能继续 dispatch 旧 selection。

一个 family 的所有成员会得到相同的 permutation 集。成员数量、logical name、selection 或 build id 不一致时，整个 family 加载失败。

### 10.4 增加 family 成员

同时修改两处：

- source manifest 的 `programs[]`，让新 program 属于目标 `variant_set`。
- 消费方 `ShaderFamily` binding，提供新 program 的原有 slot 和 loader。

运行时按 logical name 对齐，不依赖 program 数组顺序。

## 11. 光碟示例

交互式验证入口是：

```powershell
uv run python samples/diffraction_disc.py `
  --backend dx `
  --window `
  --material-mode auto
```

控制窗口中的选项含义：

| UI | 实际操作 |
| --- | --- |
| `Simple PBR` | 把光碟材质更新为不启用 diffraction/coat 的简单参数 |
| `Diffraction PBR` | 把同一个材质更新为启用 diffraction/coat 的复杂参数 |
| `Auto Cycle` | 按时间在上述两份材质参数之间切换 |
| `Cycle interval` | Auto Cycle 的材质切换间隔，不是 Shader 参数 |

示例调用 `MaterialResource.load_from_json()` 和 `install()`，没有直接设置 scene feature，也没有调用 Shader selection API。材质安装后，scene mask 和 family selection 自动跟随。

`Simple PBR` 表示使用 Lite 能力集合，不表示画面必须无色。金属反射、base color、灯光和环境贴图仍然可以产生颜色；被关闭的是 Lite Shader 不支持的复杂 lobe，例如 diffraction。

交互相机需要先点击渲染窗口，然后保持鼠标右键：

```text
RMB + mouse drag    旋转
RMB + W/A/S/D       前后左右移动
RMB + Q/E           上下移动
```

环境贴图默认开启；使用 `--no-envmap` 才会只使用内置 studio lights。`auto` 需要 `--window` 且不能配合 `--no-controls`。

Headless Full 渲染示例：

```powershell
uv run python samples/diffraction_disc.py `
  --backend dx `
  --material-mode full `
  --resolution 640x360 `
  --spp 8 `
  --max-frames 200 `
  --output build/diffraction-test.png
```

## 12. 错误与排查

| 现象 | 含义与处理 |
| --- | --- |
| `Shader compiler not found` | 先运行 `uv run prepare -y`；自定义位置用 `--compiler` |
| source schema 不是 `2` 或出现未知字段 | 修正手写 manifest；当前没有版本兼容 |
| feature 未声明或从未使用 | 同步顶层声明和 value 规则 |
| rule gap/ambiguity | 补齐 required/forbidden 规则，使每种 feature 组合唯一命中 |
| default permutation 不匹配全局 default | 修正 `variant_sets.<name>.default` 或其 selection |
| `Stable host ABI changed` | 宏改变了 host 接口；恢复稳定 ABI，或重新设计 family/consumer 接口 |
| runtime manifest 缺失或无效 | 重新构建并安装完整 Shader root，不要单独复制 `.bin` |
| artifact 缺失、size/hash 不符 | 清理对应构建产物后完整重建；不会 fallback |
| device backend 与 manifest backend 不同 | 使用对应 backend 的 Shader root |
| exact selection 不存在 | source permutations、runtime family rules 和实际 artifact 不完整 |
| DX/VK/host/plugin `input_id` 不一致 | 重新执行 hostgen、plugin 构建和原子发布，不要手改 marker |
| 切换瞬间只看到 sky | 新 selection 首次后台加载的正常瞬时状态 |
| 切换后持续失败 | 查看 fatal/error log；系统不会把加载失败当作可持续降级 |
| Auto 模式启动失败 | 必须同时使用 `--window`，且不能使用 `--no-controls` |
| Headless 未达到 SPP 就退出 | 增加 `--max-frames`，或不设置它让示例使用默认余量 |

缺文件时正确处理是修复构建/安装，而不是增加 Lite/Full 兼容分支。

## 13. 测试

Source schema、构建、verify、coherence 和原子发布测试：

```powershell
uv run pytest `
  test/test_rbcxx_variant_build.py `
  -q
```

> 集成测试需要先构建 `rbcxx` 目标（`build/tool/rbcxx/rbcxx.exe`）；未构建时跳过。
> 真实 Shader 构建测试设置 `RBCXX_RUN_BUILD_TESTS=1` 后运行。

Runtime family 测试：

```powershell
xmake build test_shader_runtime
xmake run test_shader_runtime
```

当前覆盖包括：

- Lite/complex 材质分类的 compile-time `static_assert`。
- source manifest 严格校验和 rule coverage。
- runtime manifest 生成与 artifact 完整性。
- backend/host/plugin coherence。
- exact selection、selection cache 和 revision。
- 按 logical name 绑定，不依赖 manifest 顺序。
- family 部分加载时绝不发布部分 slot。
- 原子构建/发布失败时保留旧产物。

当前仍应注意的测试空白：

- feature 名到 C++ bit 仍是显式注册表，不是代码生成。
- `SceneManager` bind/unbind 动态聚合目前缺少独立的 C++ 单元测试。
- 材质分类主要由 compile-time assertions 和光碟集成示例覆盖。
- Xmake 的自动 Shader build target 当前主要面向 Windows 工具链。

## 14. 关键文件索引

| 文件 | 职责 |
| --- | --- |
| `rbc/shader/shader_variants.json` | 唯一手写的变体构建描述 |
| `rbc/rbcxx/variant_config.cpp` | source schema 校验与规则覆盖 |
| `rbc/rbcxx/variant_build.cpp` | 构建、hostgen、缓存与增量 no-op |
| `rbc/rbcxx/variant_verify.cpp` | runtime manifest、verify、coherence |
| `rbc/rbcxx/variant_util.cpp` | 锁、原子目录、SHA-256、canonical JSON |
| `src/rbc_build/shader_common.py` | 安装路径共享的通用 FS/锁/hash 原语 |
| `src/rbc_build/artifact_publish.py` | 一致性验证（经 rbcxx）和原子发布 |
| `rbc/runtime/include/rbc_graphics/shader_features.h` | feature bit 注册与材质分类 |
| `rbc/runtime/include/rbc_graphics/scene_manager.h` | scene source 和聚合状态声明 |
| `rbc/runtime/src/graphics/scene_manager.cpp` | feature 引用计数与 atomic snapshot |
| `rbc/runtime/include/rbc_graphics/shader_manager.h` | selection/resolution 数据结构和 Manager API |
| `rbc/runtime/src/graphics/shader_manager.cpp` | runtime manifest 解析与精确解析 |
| `rbc/runtime/include/rbc_graphics/shader_family.h` | family 消费接口 |
| `rbc/runtime/src/graphics/shader_family.cpp` | 异步加载、缓存和整组发布状态机 |
| `rbc/runtime/src/world/resources/material.cpp` | 材质安装、重装和 feature 发布 |
| `rbc/runtime/src/world/components/render_component.cpp` | 场景材质 source 绑定 |
| `rbc/render_plugin/src/offline_pt_pass.cpp` | `offline_pt` family 消费方 |
| `samples/diffraction_disc.py` | 材质驱动自动切换的交互示例 |
| `test/test_rbcxx_variant_build.py` | rbcxx 集成与 shader_common 单元测试 |
| `rbc/tests/shader_runtime/test_shader_family.cpp` | C++ family runtime 测试 |

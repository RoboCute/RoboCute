# RBCWorld Python Binding

RoboCute的World系统通过`rbc_ext`模块暴露给Python，提供Entity-Component-Resource架构的完整访问能力。

## 架构概览

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    RBCWorld Python Binding Architecture                 │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   Interface Definition        Code Generation        Python Runtime     │
│   ┌──────────────────┐       ┌──────────────┐       ┌──────────────┐   │
│   │ world_interface.py│──────▶│  C++ Bindings │──────▶│ rbc_ext._C   │   │
│   │ (reflect decorators)│    │ (world.cpp)   │       │ (pybind11)   │   │
│   └──────────────────┘       └──────────────┘       └──────────────┘   │
│          │                            │                    │           │
│          │                            ▼                    ▼           │
│          │                   ┌──────────────────┐    ┌──────────────┐  │
│          └──────────────────▶│ Python Wrappers  │───▶│ robocute.    │  │
│                              │ (world_v2.py)    │    │ rbc_ext.world│  │
│                              └──────────────────┘    └──────────────┘  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

## 核心组件

### 1. 接口定义层 (`src/rbc_meta/types/world_interface.py`)

使用Python注解和装饰器定义C++类的Python接口。

```python
from rbc_meta.utils.reflect import reflect

@reflect(
    pybind=True,
    cpp_namespace="rbc",
    create_instance=False
)
class Entity(Object):
    def add_component(name: str) -> VoidPtr: ...
    def get_component(name: str) -> VoidPtr: ...
    def remove_component(name: str) -> bool: ...
    def name() -> str: ...
    def set_name(name: str) -> None: ...
```

**关键装饰器参数**:
- `pybind=True`: 启用pybind11代码生成
- `cpp_namespace`: C++命名空间
- `create_instance=False`: 不生成默认构造函数（需要通过已有handle创建）
- `cpp_prefix`: C++ API前缀（如`TEST_GRAPHICS_API`）

### 2. 代码生成系统

#### C++ Bindings (`rbc/extensions/ext_c/src/generated/world.cpp`)

自动生成pybind11绑定代码，使用handle-based设计：

```cpp
// 方法绑定为独立的C函数
m.def("Entity__add_component__", [](void* ptr, luisa::string_view name) {
    py::gil_scoped_release gil_released;  // 释放GIL
    return rbc::Entity::add_component(ptr, name);
});

m.def("Entity__set_name__", [](void* ptr, luisa::string_view name) {
    py::gil_scoped_release gil_released;
    rbc::Entity::set_name(ptr, name);
});
```

**设计特点**:
- 使用`void*`作为对象handle（不透明指针）
- 自动释放GIL避免阻塞Python
- 函数命名格式：`ClassName__method_name__`

#### Python Wrappers (`src/robocute/rbc_ext/generated/world_v2.py`)

生成Python类包装handle和自动内存管理：

```python
class Entity(Object):
    def __init__(self, handle):
        self._handle = handle  # C++对象指针

    def __bool__(self):
        return self._handle is not None

    def add_component(self, name: str):
        return Entity__add_component__(self._handle, name)
    
    def dispose(self):
        hhfae4d451 = self._handle
        self._handle = None
        Entity__dispose__(hhfae4d451)  # 手动释放
```

### 3. 引用计数与内存管理

#### C++侧 (`export_base_object.cpp`)

```cpp
void export_base_obj(py::module &m) {
    // 手动引用计数管理
    m.def("rbc_add_ref", [](void *ptr) {
        manually_add_ref(static_cast<RCBase *>(ptr));
    });
    m.def("rbc_release", [](void *ptr) {
        manually_release_ref(static_cast<RCBase *>(ptr));
    });
    
    // 动态创建资源
    m.def("_create_resource", [&](luisa::string_view type_info) -> void * {
        auto ptr = world::create_object(type);
        return ptr;
    });
}
```

#### Python侧生命周期管理

```python
class TextureResource(Resource):
    def __init__(self, *args):
        if len(args) == 0:
            self._handle = create__TextureResource__()  # 创建新对象
        else:
            self._handle = args[0]  # 包装已有handle

    def __del__(self):
        if self._handle:
            rbc_release(self._handle)  # 析构时释放引用
    
    def dispose(self):
        if self._handle:
            rbc_release(self._handle)
            self._handle = None  # 防止重复释放
```

### 4. 模块初始化系统

#### 注册机制 (`module_register.h`)

使用静态初始化器自动注册绑定函数：

```cpp
struct ModuleRegister {
    static ModuleRegister *header;
    ModuleRegister *next;
    void (*_callback)(py::module &);
    
    explicit ModuleRegister(void (*callback)(py::module &));
    static void init(py::module &m);  // 遍历链表初始化
};

// 使用示例
static ModuleRegister module_register_export_base_obj(export_base_obj);
```

#### 主模块 (`ext.cpp`)

```cpp
PYBIND11_MODULE(rbc_ext_c, m) {
    ModuleRegister::init(m);  // 调用所有注册的绑定函数
}
```

## 核心类绑定详解

### BaseObject 层次结构

```
Object (base)
├── Entity
│   └── 通过add_component()附加组件
├── Component (abstract)
│   ├── TransformComponent
│   ├── RenderComponent
│   ├── LightComponent
│   ├── CameraComponent
│   └── DataComponent
└── Resource (abstract)
    ├── TextureResource
    ├── MeshResource
    ├── MaterialResource
    ├── BufferResource
    └── Scene
```

### 1. Object 基类

所有World对象的基类，提供类型信息。

```python
class Object:
    def guid(self) -> GUID: ...           # 唯一标识符
    def type_name(self) -> str: ...       # 类型名称
    def type_id(self) -> GUID: ...        # 类型ID
    def is_type(name: str) -> bool: ...   # 类型检查
    def base_type(self) -> BaseObjectType: ...
```

### 2. Entity 实体

场景中的基本对象容器。

```python
class Entity(Object):
    # 组件管理
    def add_component(self, name: str) -> VoidPtr: ...
    def get_component(self, name: str) -> VoidPtr: ...
    def remove_component(self, name: str) -> bool: ...
    
    # 命名
    def name(self) -> str: ...
    def set_name(self, name: str) -> None: ...
    
    # 手动释放（可选）
    def dispose(self) -> None: ...
```

**使用示例**:
```python
import robocute.rbc_ext as re

# 创建实体
entity = scene.add_entity()
entity.set_name("my_entity")

# 添加组件
transform = re.world.TransformComponent(
    entity.add_component("TransformComponent")
)
transform.set_pos(lc.double3(0, 0, 0), False)
```

### 3. Component 组件

附着在Entity上的功能单元。

#### TransformComponent

```python
class TransformComponent(Component):
    # 获取变换
    def position(self) -> double3: ...
    def rotation(self) -> float4: ...  # 四元数
    def scale(self) -> double3: ...
    def trs(self) -> double4x4: ...    # 变换矩阵
    
    # 设置变换
    def set_pos(self, pos: double3, recursive: bool) -> None: ...
    def set_rotation(self, rotation: float4, recursive: bool) -> None: ...
    def set_scale(self, scale: double3, recursive: bool) -> None: ...
    def set_trs(self, pos: double3, rotation: float4, 
                scale: double3, recursive: bool) -> None: ...
```

#### RenderComponent

```python
class RenderComponent(Component):
    def mesh(self) -> MeshResource: ...
    def mat_count(self) -> int: ...
    def get_material(self, idx: int) -> MaterialResource: ...
    
    def update_mesh(self, mesh: MeshResource) -> None: ...
    def update_material(self, mat_vector) -> None: ...
    def update_object(self, mat_vector, mesh: MeshResource) -> None: ...
    def remove_object(self) -> None: ...
```

#### CameraComponent

```python
class CameraComponent(Component):
    # 相机参数
    def fov(self) -> float: ...
    def set_fov(self, value: float) -> None: ...
    def near_plane(self) -> float: ...
    def far_plane(self) -> float: ...
    def aspect_ratio(self) -> float: ...
    
    # 渲染控制
    def render_image(self) -> LCPYImage2D: ...
    def save_image_to(self, path: str) -> None: ...
    def render_settings(self) -> RenderSettings: ...
```

#### LightComponent

```python
class LightComponent(Component):
    def add_point_light(self, luminance: float3, visible: bool) -> None: ...
    def add_spot_light(self, luminance: float3, angle_radians: float,
                       small_angle_radians: float, angle_atten_pow: float,
                       visible: bool) -> None: ...
    def add_area_light(self, luminance: float3, visible: bool) -> None: ...
    def add_disk_light(self, luminance: float3, visible: bool) -> None: ...
```

### 4. Resource 资源

可序列化的数据资产。

#### 通用Resource接口

```python
class Resource(Object):
    def load(self) -> None: ...              # 异步加载
    def wait_loading(self) -> None: ...      # 等待加载完成
    def load_status(self) -> ResourceLoadStatus: ...
    def install(self) -> bool: ...           # 安装到GPU
    def path(self) -> str: ...               # 资源路径
    def save_to_path(self) -> bool: ...      # 保存到磁盘
```

#### TextureResource

```python
class TextureResource(Resource):
    def create_empty(self, pixel_storage: LCPixelStorage, 
                     size: uint2, mip_level: int, 
                     is_virtual_texture: bool) -> None: ...
    def data_buffer(self) -> DataBuffer: ...     # 获取CPU缓冲区
    def device_texture(self) -> LCPYImage2D: ... # 获取GPU纹理
    def size(self) -> uint2: ...
    def mip_level(self) -> int: ...
    def set_skybox(self) -> None: ...
    def pack_to_tile(self) -> bool: ...  # VT相关
```

**使用示例**:
```python
# 从项目导入纹理
tex = project.import_texture('test_grid.png', mip_level=1, to_vt=False)

# 创建空白纹理
tex = re.world.TextureResource()
tex.create_empty(
    pixel_storage=re.world.LCPixelStorage.FLOAT4,
    size=lc.uint2(1024, 1024),
    mip_level=1,
    is_virtual_texture=False
)
```

#### MeshResource

```python
class MeshResource(Resource):
    def create_empty(self, submesh_offsets, vertex_count: int,
                     triangle_count: int, uv_count: int,
                     contained_normal: bool, contained_tangent: bool) -> None: ...
    def data_buffer(self) -> DataBuffer: ...
    def device_data_buffer(self) -> LCPYBuffer: ...
    def pos_buffer(self) -> DataBuffer: ...
    def normal_buffer(self) -> DataBuffer: ...
    def uv_buffer(self, uv_count: int) -> DataBuffer: ...
    
    # 网格信息
    def vertex_count(self) -> int: ...
    def triangle_count(self) -> int: ...
    def submesh_count(self) -> int: ...
    
    # 变形网格
    def create_as_morphing_instance(self, origin_mesh) -> None: ...
    def build_before_tick(self) -> None: ...
```

**使用示例**:
```python
# 创建动态网格
mesh = re.world.MeshResource()
submesh_offsets = np.array([0, 12], dtype=np.uint32)  # 两个子网格
mesh.create_empty(
    submesh_offsets=submesh_offsets,
    vertex_count=16,
    triangle_count=24,
    uv_count=1,
    contained_normal=False,
    contained_normal=False
)

# 获取缓冲区并填充数据
mesh_array = np.ndarray(
    vertex_count * 4 + vertex_count * 2 + triangle_count * 3,
    dtype=np.float32,
    buffer=mesh.data_buffer()
)
# ... 填充数据 ...
mesh.install()  # 安装到GPU
```

#### MaterialResource

```python
class MaterialResource(Resource):
    def load_from_json(self, json: str) -> None: ...
    def dump_json(self) -> str: ...
    def mat_code(self) -> int: ...  # 材质类型代码
```

**使用示例**:
```python
# 使用OpenPBR接口创建材质
mat = re.world.MaterialResource()
mat_json = mat_builtin.OpenPBRInterface(project)
mat_json.set_specular_roughness(0.8)
mat_json.set_weight_metallic(0.3)
mat_json.set_base_albedo((0.8, 0.8, 0.8))
mat_json.set_base_albedo_tex(texture)
mat.load_from_json(mat_json.dump_to_json())
```

#### Scene 场景资源

```python
class Scene(Resource):
    def add_entity(self) -> Entity: ...
    def get_entity(self, guid: GUID) -> Entity: ...
    def get_entity_by_name(self, name: str) -> Entity: ...
    def get_entities_by_name(self, name: str) -> EntitiesCollection: ...
    def remove_entity(self, guid: GUID) -> None: ...
    def update_data(self) -> None: ...
```

## 类型映射

### 基本类型映射

| Python类型 | C++类型 | 说明 |
|-----------|---------|------|
| `int` | `int32_t` / `int64_t` | 整型 |
| `float` | `float` / `double` | 浮点 |
| `bool` | `bool` | 布尔 |
| `str` | `luisa::string_view` | 字符串 |
| `np.ndarray` | `luisa::span<T>` | 数组视图 |

### 向量/矩阵类型

| Python类型 | C++类型 | 说明 |
|-----------|---------|------|
| `float2` | `luisa::float2` | 2D向量 |
| `float3` | `luisa::float3` | 3D向量 |
| `float4` | `luisa::float4` | 4D向量/四元数 |
| `double3` | `luisa::double3` | 双精度3D向量 |
| `uint2` | `luisa::uint2` | 2D整数向量 |
| `float4x4` | `luisa::float4x4` | 4x4矩阵 |

**导入路径**:
```python
from robocute.rbc_ext._C.lcapi_c import float2, float3, float4, double3, uint2
# 或使用luisa包装模块
import robocute.rbc_ext.luisa as lc
lc.float3(1.0, 2.0, 3.0)
```

### 特殊类型映射

#### DataBuffer (numpy数组视图)

```python
# C++侧返回DataBuffer
DataBuffer = Type[luisa.span<std::byte>]

# Python侧作为numpy数组使用
mesh_array = np.ndarray(
    size,
    dtype=np.float32,
    buffer=mesh.data_buffer()
)
```

#### GUID

```python
from robocute.rbc_ext._C.rbc_ext_c import GUID

# 创建GUID
guid = GUID("xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx")
```

#### VoidPtr

```python
# C++对象的void* handle
# 通常需要包装成对应类型
component_handle = entity.get_component("TransformComponent")
transform = re.world.TransformComponent(component_handle)
```

## 完整使用示例

### 创建带材质和网格的实体

```python
import numpy as np
import robocute.rbc_ext as re
import robocute.rbc_ext.luisa as lc

def create_cube_entity(scene, project):
    # 1. 创建材质
    mat = re.world.MaterialResource()
    mat_json = mat_builtin.OpenPBRInterface(project)
    mat_json.set_base_albedo((0.8, 0.8, 0.8))
    mat_json.set_specular_roughness(0.5)
    mat.load_from_json(mat_json.dump_to_json())
    
    # 2. 创建网格
    mesh = re.world.MeshResource()
    mesh.create_empty(
        submesh_offsets=np.array([0], dtype=np.uint32),
        vertex_count=8,
        triangle_count=12,
        uv_count=1,
        contained_normal=False,
        contained_tangent=False
    )
    
    # 3. 填充网格数据
    mesh_data = np.ndarray(
        8*4 + 8*2 + 12*3,  # pos(4) + uv(2) + indices(3)
        dtype=np.float32,
        buffer=mesh.data_buffer()
    )
    # ... 填充顶点、UV、索引数据 ...
    mesh.install()
    
    # 4. 创建实体并附加组件
    entity = scene.add_entity()
    entity.set_name("cube")
    
    # 添加Transform组件
    transform = re.world.TransformComponent(
        entity.add_component("TransformComponent")
    )
    transform.set_pos(lc.double3(0, 0, 0), False)
    
    # 添加Render组件
    render = re.world.RenderComponent(
        entity.add_component("RenderComponent")
    )
    
    # 创建材质vector
    mat_vector = lc.capsule_vector()
    mat_vector.emplace_back(mat._handle)
    render.update_object(mat_vector, mesh)
    
    return entity
```

### 资源导入

```python
# 导入纹理
texture = project.import_texture(
    path='assets/texture.png',
    mip_level=4,
    to_vt=False  # 是否生成Virtual Texture
)

# 导入网格
mesh = project.import_mesh('assets/model.obj')

# 导入材质
material = project.import_material('assets/mat.json')

# 导入场景
scene = project.import_scene('assets/scene.gltf', extra_meta='{}')
```

### 相机控制

```python
# 获取显示相机
cam = ctx.create_display_cam()

# 设置相机参数
cam.set_fov(math.radians(60.0))
cam.set_near_plane(0.01)
cam.set_far_plane(1000.0)

# 渲染设置
settings = cam.render_settings()
settings.set_offline_spp(128)  # 离线渲染采样数
settings.set_enable_ao_mode(True)

# 保存图像
cam.save_image_to("screenshot.png")
```

## 设计特点与优势

### 1. Handle-Based设计

- 所有C++对象通过`void*` handle访问
- 避免复杂的对象生命周期管理
- 支持跨线程安全访问（配合GIL管理）

### 2. 自动内存管理

- Python wrapper自动管理C++对象引用计数
- 支持`dispose()`显式释放
- 析构函数自动调用`rbc_release`

### 3. GIL管理

```cpp
// C++绑定自动释放GIL
m.def("method", [](void* ptr) {
    py::gil_scoped_release gil_released;  // 允许其他Python线程运行
    return cpp_function(ptr);
});
```

### 4. 类型安全

- 强类型的handle传递
- 枚举类型自动转换
- 向量/矩阵类型编译期检查

### 5. 代码生成工作流

```
修改 world_interface.py 接口定义
           │
           ▼
    运行代码生成工具
           │
           ▼
    生成 world.cpp (C++绑定)
    生成 world_v2.py (Python包装)
           │
           ▼
    重新编译 ext_c 模块
```

## 扩展自定义类型

如需添加新的Resource或Component类型：

1. **定义接口** (`world_interface.py`):
```python
@reflect(pybind=True, cpp_namespace="rbc")
class MyCustomResource(Resource):
    def custom_method(self, arg: float) -> float: ...
```

2. **实现C++类**:
```cpp
struct MyCustomResource : ResourceBaseImpl<MyCustomResource> {
    float custom_method(float arg);
};
```

3. **注册反射**:
```cpp
RBC_RTTI(rbc::world::MyCustomResource)
```

4. **重新生成并编译**:
```bash
# 生成代码
python -m rbc_meta.generate

# 编译模块
python setup.py build_ext --inplace
```

## 注意事项

1. **生命周期管理**: 通过handle传递的对象需确保在C++端保持存活
2. **线程安全**: C++调用自动释放GIL，但Python对象操作需要持有GIL
3. **资源安装**: Resource需要调用`install()`后才能被渲染系统使用
4. **handle有效性**: 使用`if obj:`检查handle是否有效
5. **dispose调用**: 手动调用`dispose()`会立即释放资源，后续访问会导致崩溃

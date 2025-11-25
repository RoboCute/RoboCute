# Python 端定义 (src/rbc_ext/resource.py)
from enum import IntEnum
from dataclasses import dataclass
from typing import Optional, Dict, Any, Callable, List
from enum import IntEnum
from dataclasses import dataclass, field
import time
import threading
import queue
from pathlib import Path
import weakref
import rbc_ext._C.rbc_ext_c as _C


class ResourceType(IntEnum):
    Unknown = 0
    Mesh = 1
    Texture = 2
    Material = 3
    Shader = 4
    Animation = 5
    Skeleton = 6
    PhysicsShape = 7
    Audio = 8
    Custom = 1000


class ResourceState(IntEnum):
    Unloaded = 0
    Pending = 1
    Loading = 2
    Loaded = 3
    Failed = 4
    Unloading = 5


@dataclass
class ResourceMetadata:
    id: int  # ResourceID
    type: ResourceType
    state: ResourceState
    path: str
    name: str
    memory_size: int = 0
    reference_count: int = 0
    last_access_time: float = 0.0
    type_specific_data: Optional[Dict[str, Any]] = None


class LoadPriority(IntEnum):
    Critical = 0
    High = 1
    Normal = 2
    Low = 3
    Background = 4


@dataclass
class LoadOptions:
    generate_mipmaps: bool = True
    compress_gpu: bool = True
    compute_normals: bool = False
    compute_tangents: bool = False
    lod_level: int = 0
    custom_options: Optional[Dict[str, Any]] = None


@dataclass(order=True)
class ResourceRequest:
    priority: LoadPriority = field(compare=True)
    # timestamp: float = field(default_factory=time.time, compare=True)
    id: int = field(compare=False)
    type: ResourceType = field(compare=False)
    path: str = field(compare=False)
    options: LoadOptions = field(default_factory=LoadOptions, compare=False)
    on_complete: Optional[Callable[[int, bool], None]] = field(
        default=None, compare=False
    )


class ResourceHandle:
    """
    资源句柄 (智能指针)
    - 自动管理引用计数
    - 提供便利的访问接口
    """

    def __init__(self, manager, resource_id: int):
        self._manager = manager
        self._resource_id = resource_id

    def __del__(self):
        """析构时释放引用"""
        self._manager.release_handle(self._resource_id)

    @property
    def id(self) -> int:
        return self._resource_id

    @property
    def metadata(self) -> Optional[ResourceMetadata]:
        return self._manager.get_metadata(self._resource_id)

    @property
    def is_loaded(self) -> bool:
        return self._manager.is_loaded(self._resource_id)

    def wait_until_loaded(self, timeout: float = 30.0) -> bool:
        """等待资源加载完成"""
        import time

        start = time.time()
        while time.time() - start < timeout:
            if self.is_loaded:
                return True
            time.sleep(0.01)
        return False

    def get_data(self):
        """获取资源数据 (通过C++接口)"""
        if not self.is_loaded:
            return None
        return self._manager._cpp_loader.get_resource_data(self._resource_id)


class ResourceManager:
    """
    Python端资源管理器
    - 管理资源的生命周期
    - 维护资源注册表
    - 调度异步加载请求
    """

    def __init__(self, cache_budget_mb: int = 2048):
        self._resources: Dict[int, ResourceMetadata] = {}
        self._path_to_id: Dict[str, int] = {}
        self._next_resource_id: int = 1
        self._resource_lock = threading.RLock()
        # 请求队列 (优先级队列)
        self._request_queue = queue.PriorityQueue()

        # C++ 异步加载器
        self._cpp_loader = _C.AsyncResourceLoader()
        self._cpp_loader.set_cache_budget(cache_budget_mb * 1024 * 1024)

        # 加载线程
        self._loader_thread: Optional[threading.Thread] = None
        self._running = False
        # 回调管理
        self._completion_callbacks: Dict[int, List[Callable]] = {}
        # 引用追踪 (弱引用)
        self._handles: weakref.WeakValueDictionary = weakref.WeakValueDictionary()

    # === 启动/停止 ===
    def start(self):
        """启动资源管理器 (在app.is_running()时调用)"""
        if self._running:
            return

        self._running = True
        self._loader_thread = threading.Thread(
            target=self._loader_worker, name="ResourceLoader", daemon=True
        )
        self._loader_thread.start()
        print("[ResourceManager] Started")

    def stop(self):
        """停止资源管理器"""
        if not self._running:
            return

        self._running = False
        self._request_queue.put(None)  # 发送终止信号

        if self._loader_thread:
            self._loader_thread.join(timeout=5.0)

        self._cpp_loader.shutdown()
        print("[ResourceManager] Stopped")

    # === 资源注册 ===
    def register_resource(
        self, path: str, resource_type: ResourceType, name: Optional[str] = None
    ) -> int:
        """
        注册资源 (不立即加载)

        Args:
            path: 资源文件路径
            resource_type: 资源类型
            name: 资源名称 (可选)

        Returns:
            ResourceID
        """
        path = str(Path(path).resolve())

        with self._resource_lock:
            # 已注册
            if path in self._path_to_id:
                return self._path_to_id[path]

            # 分配新ID
            resource_id = self._allocate_resource_id(resource_type)

            metadata = ResourceMetadata(
                id=resource_id,
                type=resource_type,
                state=ResourceState.Unloaded,
                path=path,
                name=name or Path(path).stem,
            )

            self._resources[resource_id] = metadata
            self._path_to_id[path] = resource_id

            return resource_id

    def _allocate_resource_id(self, resource_type: ResourceType) -> int:
        """分配资源ID (高32位=类型, 低32位=序列号)"""
        type_bits = int(resource_type) << 32
        serial = self._next_resource_id
        self._next_resource_id += 1
        return type_bits | serial

    # === 资源加载 ===
    def load_resource(
        self,
        resource_id: int,
        priority: LoadPriority = LoadPriority.Normal,
        options: Optional[LoadOptions] = None,
        on_complete: Optional[Callable[[int, bool], None]] = None,
    ) -> bool:
        """
        异步加载资源

        Args:
            resource_id: 资源ID
            priority: 加载优先级
            options: 加载选项
            on_complete: 完成回调

        Returns:
            是否成功提交加载请求
        """
        with self._resource_lock:
            # 检查是否注册
            if resource_id not in self._resources:
                print(f"[ResourceManager] Resource {resource_id} not found")
                return False

            metadata = self._resources[resource_id]

            # 已加载,直接返回
            if metadata.state == ResourceState.Loaded:
                if on_complete:
                    on_complete(resource_id, True)
                return True

            # 正在加载,添加回调
            if metadata.state in (ResourceState.Pending, ResourceState.Loading):
                if on_complete:
                    self._add_completion_callback(resource_id, on_complete)
                return True

            # 更新状态
            metadata.state = ResourceState.Pending
            # 创建请求
            request = ResourceRequest(
                priority=priority,
                id=resource_id,
                type=metadata.type,
                path=metadata.path,
                options=options or LoadOptions(),
                on_complete=on_complete,
            )
            # 添加到回调列表
            if on_complete:
                self._add_completion_callback(resource_id, on_complete)
            # 提交到队列
            self._request_queue.put(request)
            return True

    def load_resource_sync(
        self, resource_id: int, options: Optional[LoadOptions] = None
    ) -> bool:
        """
        同步加载资源 (阻塞直到加载完成)

        注意: 尽量避免使用,除非是Critical资源
        """
        event = threading.Event()
        success = [False]

        def on_complete(rid: int, s: bool):
            success[0] = s
            event.set()

        if not self.load_resource(
            resource_id, LoadPriority.Critical, options, on_complete
        ):
            return False

        event.wait(timeout=30.0)  # 最多等待30秒
        return success[0]

    # === 便利函数 ===

    def load_mesh(
        self,
        path: str,
        priority: LoadPriority = LoadPriority.Normal,
        on_complete: Optional[Callable[[int, bool], None]] = None,
    ) -> int:
        """加载网格资源"""
        resource_id = self.register_resource(path, ResourceType.Mesh)
        self.load_resource(resource_id, priority, on_complete=on_complete)
        return resource_id

    def load_texture(
        self,
        path: str,
        priority: LoadPriority = LoadPriority.Normal,
        on_complete: Optional[Callable[[int, bool], None]] = None,
    ) -> int:
        """加载纹理资源"""
        resource_id = self.register_resource(path, ResourceType.Texture)
        self.load_resource(resource_id, priority, on_complete=on_complete)
        return resource_id

    def load_material(
        self,
        path: str,
        priority: LoadPriority = LoadPriority.Normal,
        on_complete: Optional[Callable[[int, bool], None]] = None,
    ) -> int:
        """加载材质资源"""
        resource_id = self.register_resource(path, ResourceType.Material)
        self.load_resource(resource_id, priority, on_complete=on_complete)
        return resource_id

    # === 资源句柄 ===

    def get_handle(self, resource_id: int) -> Optional["ResourceHandle"]:
        """
        获取资源句柄 (智能指针)
        - 自动管理引用计数
        - 句柄销毁时自动减少引用
        """
        with self._resource_lock:
            if resource_id not in self._resources:
                return None

            # 检查是否已存在句柄
            if resource_id in self._handles:
                return self._handles[resource_id]

            # 创建新句柄
            handle = ResourceHandle(self, resource_id)
            self._handles[resource_id] = handle

            # 增加引用计数
            self._resources[resource_id].reference_count += 1

            return handle

    def release_handle(self, resource_id: int):
        """释放资源句柄 (减少引用计数)"""
        with self._resource_lock:
            if resource_id not in self._resources:
                return

            metadata = self._resources[resource_id]
            metadata.reference_count = max(0, metadata.reference_count - 1)

            # 引用计数为0时,标记为可卸载
            if metadata.reference_count == 0:
                self._schedule_unload(resource_id)

    # === 资源查询 ===

    def get_metadata(self, resource_id: int) -> Optional[ResourceMetadata]:
        """获取资源元数据"""
        with self._resource_lock:
            return self._resources.get(resource_id)

    def get_state(self, resource_id: int) -> ResourceState:
        """获取资源状态"""
        with self._resource_lock:
            metadata = self._resources.get(resource_id)
            return metadata.state if metadata else ResourceState.Unloaded

    def is_loaded(self, resource_id: int) -> bool:
        """资源是否已加载"""
        return self.get_state(resource_id) == ResourceState.Loaded

    # === 资源卸载 ===

    def unload_resource(self, resource_id: int):
        """卸载资源"""
        with self._resource_lock:
            if resource_id not in self._resources:
                return

            metadata = self._resources[resource_id]

            # 仍有引用,不能卸载
            if metadata.reference_count > 0:
                print(
                    f"[ResourceManager] Cannot unload {resource_id}: still referenced"
                )
                return

            # 更新状态
            metadata.state = ResourceState.Unloading

            # 通知C++端卸载
            self._cpp_loader.unload_resource(resource_id)

            # 更新状态
            metadata.state = ResourceState.Unloaded
            metadata.memory_size = 0

    def _schedule_unload(self, resource_id: int):
        """调度延迟卸载 (避免立即卸载后又加载)"""
        # TODO: 实现延迟卸载策略
        pass

    def clear_unused(self):
        """清理未使用的资源"""
        with self._resource_lock:
            to_unload = [
                rid
                for rid, meta in self._resources.items()
                if meta.reference_count == 0 and meta.state == ResourceState.Loaded
            ]

            for rid in to_unload:
                self.unload_resource(rid)

            print(f"[ResourceManager] Unloaded {len(to_unload)} unused resources")

    # === 内部方法 ===

    def _loader_worker(self):
        """资源加载工作线程"""
        print("[ResourceLoader] Worker thread started")

        while self._running:
            try:
                # 获取请求 (阻塞)
                request = self._request_queue.get(timeout=1.0)

                if request is None:  # 终止信号
                    break

                # 处理请求
                self._process_request(request)

            except queue.Empty:
                continue
            except Exception as e:
                print(f"[ResourceLoader] Error: {e}")

        print("[ResourceLoader] Worker thread stopped")

    def _process_request(self, request: ResourceRequest):
        """处理资源加载请求"""
        resource_id = request.id
        # 更新状态
        with self._resource_lock:
            if resource_id not in self._resources:
                return
            self._resources[resource_id].state = ResourceState.Loading
        # 调用C++加载器
        try:
            success = self._cpp_loader.load_resource(
                resource_id, request.type.value, request.path, ""
            )

            # 更新状态
            with self._resource_lock:
                if resource_id in self._resources:
                    metadata = self._resources[resource_id]
                    metadata.state = (
                        ResourceState.Loaded if success else ResourceState.Failed
                    )

                    if success:
                        # 更新内存大小
                        metadata.memory_size = self._cpp_loader.get_resource_size(
                            resource_id
                        )
                        metadata.last_access_time = time.time()
            # 调用回调
            self._invoke_completion_callbacks(resource_id, success)

        except Exception as e:
            print(f"[ResourceLoader] Failed to load {request.path}: {e}")

            with self._resource_lock:
                if resource_id in self._resources:
                    self._resources[resource_id].state = ResourceState.Failed

            self._invoke_completion_callbacks(resource_id, False)

    def _add_completion_callback(self, resource_id: int, callback: Callable):
        """添加完成回调"""
        if resource_id not in self._completion_callbacks:
            self._completion_callbacks[resource_id] = []
        self._completion_callbacks[resource_id].append(callback)

    def _invoke_completion_callbacks(self, resource_id: int, success: bool):
        """调用完成回调"""
        callbacks = self._completion_callbacks.pop(resource_id, [])
        for callback in callbacks:
            try:
                callback(resource_id, success)
            except Exception as e:
                print(f"[ResourceManager] Callback error: {e}")

    # === Editor同步 ===
    def get_all_metadata(self) -> List[ResourceMetadata]:
        """获取所有资源元数据 (用于Editor同步)"""
        with self._resource_lock:
            return list(self._resources.values())

    def serialize_metadata(self, resource_id: int) -> Optional[dict]:
        """序列化资源元数据 (用于Editor同步)"""
        with self._resource_lock:
            metadata = self._resources.get(resource_id)
            if not metadata:
                return None

            return {
                "id": metadata.id,
                "type": metadata.type.value,
                "state": metadata.state.value,
                "path": metadata.path,
                "name": metadata.name,
                "memory_size": metadata.memory_size,
                "reference_count": metadata.reference_count,
            }

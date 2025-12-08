# Text2Image Nodes for RoboCute

基于 Stable Diffusion 的文本生成图像节点实现，参考 [ComfyUI](https://github.com/comfyanonymous/ComfyUI) 的设计。

## 功能特性

本实现提供了两种使用方式：

1. **模块化方式**（类似 ComfyUI）：使用多个独立节点构建完整的工作流
2. **简化方式**：使用单个 `Text2Image` 节点快速生成图像

## 安装依赖

```bash
# 安装 PyTorch (根据你的 CUDA 版本选择)
pip install torch torchvision

# 安装 diffusers 和相关库
pip install diffusers transformers accelerate
```

## 可用节点

### 1. CLIPTextEncode
- **类型**: `clip_text_encode`
- **功能**: 将文本提示编码为 CLIP 嵌入
- **输入**:
  - `text` (string): 要编码的文本提示
- **输出**:
  - `conditioning` (conditioning): CLIP 文本条件嵌入

### 2. EmptyLatentImage
- **类型**: `empty_latent_image`
- **功能**: 生成指定尺寸的空潜在张量
- **输入**:
  - `width` (integer, 默认 512): 图像宽度
  - `height` (integer, 默认 512): 图像高度
  - `batch_size` (integer, 默认 1): 批次大小
- **输出**:
  - `latent` (latent): 空潜在张量

### 3. KSampler
- **类型**: `ksampler`
- **功能**: 使用扩散模型从文本生成图像
- **输入**:
  - `conditioning` (conditioning, 必需): 来自 CLIPTextEncode 的文本条件
  - `latent` (latent, 必需): 来自 EmptyLatentImage 的空潜在张量
  - `seed` (integer, 默认 -1): 随机种子（-1 表示随机）
  - `steps` (integer, 默认 20): 扩散步数
  - `cfg_scale` (number, 默认 7.0): 分类器自由引导比例
  - `model_name` (string, 默认 "runwayml/stable-diffusion-v1-5"): Stable Diffusion 模型名称
- **输出**:
  - `latent` (latent): 生成的潜在张量（包含图像）

### 4. VAEDecode
- **类型**: `vae_decode`
- **功能**: 将潜在张量解码为图像（简化版本中为透传）
- **输入**:
  - `latent` (latent, 必需): 来自 KSampler 的潜在张量
- **输出**:
  - `image` (image): 解码后的图像

### 5. SaveImage
- **类型**: `save_image`
- **功能**: 保存生成的图像到文件
- **输入**:
  - `image` (image, 必需): 来自 VAEDecode 的图像
  - `filename_prefix` (string, 默认 "ComfyUI"): 文件名前缀
- **输出**:
  - `image_path` (string): 保存的文件路径
  - `image_url` (string): 图像的 data URI（用于预览）

### 6. Text2Image（简化节点）
- **类型**: `text2image`
- **功能**: 完整的文本到图像生成流程（单节点）
- **输入**:
  - `prompt` (string, 必需): 文本提示
  - `negative_prompt` (string, 默认 ""): 负面提示
  - `width` (integer, 默认 512): 图像宽度
  - `height` (integer, 默认 512): 图像高度
  - `seed` (integer, 默认 -1): 随机种子
  - `steps` (integer, 默认 20): 扩散步数
  - `cfg_scale` (number, 默认 7.0): 引导比例
  - `model_name` (string, 默认 "runwayml/stable-diffusion-v1-5"): 模型名称
- **输出**:
  - `image` (image): 生成的图像（base64 PNG）
  - `image_url` (string): 图像的 data URI
  - `seed` (integer): 使用的随机种子

## 使用示例

### 示例 1: 使用简化节点

```python
import robocute as rbc
from robocute.graph import GraphDefinition, NodeDefinition

graph_def = GraphDefinition(
    nodes=[
        NodeDefinition(
            node_id="text2image",
            node_type="text2image",
            inputs={
                "prompt": "a beautiful sunset over mountains, highly detailed, 4k",
                "width": 512,
                "height": 512,
                "steps": 20,
                "cfg_scale": 7.0,
                "seed": 42,
            },
        ),
    ],
    connections=[],
)

graph = rbc.NodeGraph.from_definition(graph_def)
executor = rbc.GraphExecutor(graph)
result = executor.execute()

if result.status == rbc.ExecutionStatus.COMPLETED:
    outputs = result.get_node_outputs("text2image")
    print(f"Image generated! Seed: {outputs.get('seed')}")
```

### 示例 2: 使用模块化节点（ComfyUI 风格）

```python
import robocute as rbc
from robocute.graph import GraphDefinition, NodeDefinition, NodeConnection

graph_def = GraphDefinition(
    nodes=[
        NodeDefinition(
            node_id="clip_encode",
            node_type="clip_text_encode",
            inputs={"text": "a futuristic cityscape at night"},
        ),
        NodeDefinition(
            node_id="empty_latent",
            node_type="empty_latent_image",
            inputs={"width": 768, "height": 512},
        ),
        NodeDefinition(
            node_id="sampler",
            node_type="ksampler",
            inputs={"steps": 25, "cfg_scale": 8.0, "seed": 123},
        ),
        NodeDefinition(
            node_id="vae_decode",
            node_type="vae_decode",
        ),
        NodeDefinition(
            node_id="save",
            node_type="save_image",
            inputs={"filename_prefix": "ComfyUI"},
        ),
    ],
    connections=[
        NodeConnection(
            from_node="clip_encode",
            from_output="conditioning",
            to_node="sampler",
            to_input="conditioning",
        ),
        NodeConnection(
            from_node="empty_latent",
            from_output="latent",
            to_node="sampler",
            to_input="latent",
        ),
        NodeConnection(
            from_node="sampler",
            from_output="latent",
            to_node="vae_decode",
            to_input="latent",
        ),
        NodeConnection(
            from_node="vae_decode",
            from_output="image",
            to_node="save",
            to_input="image",
        ),
    ],
)

graph = rbc.NodeGraph.from_definition(graph_def)
executor = rbc.GraphExecutor(graph)
result = executor.execute()
```

## 在编辑器中使用

1. 启动后端服务器：
   ```bash
   python main.py
   ```

2. 在编辑器中切换到 **Text2Image** 工作流（菜单栏或工具栏）

3. 在 NodeGraph 中：
   - 从节点面板拖拽 `Text2Image` 节点
   - 设置提示词和其他参数
   - 点击 "Execute" 执行
   - 生成的图像会在节点内部显示预览

## 支持的模型

默认使用 `runwayml/stable-diffusion-v1-5`，你也可以使用其他兼容的 Stable Diffusion 模型：

- `runwayml/stable-diffusion-v1-5`
- `stabilityai/stable-diffusion-2-1`
- `stabilityai/stable-diffusion-xl-base-1.0`
- 其他 Hugging Face 上的兼容模型

## 注意事项

1. **首次运行**：首次使用某个模型时，会自动从 Hugging Face 下载，可能需要一些时间
2. **GPU 要求**：建议使用支持 CUDA 的 GPU，CPU 模式会很慢
3. **内存要求**：Stable Diffusion 模型需要约 4-8GB 显存
4. **图像输出**：生成的图像保存在 `output/` 目录下

## 与 ComfyUI 的对比

| 特性 | ComfyUI | RoboCute Text2Image |
|------|---------|---------------------|
| 节点系统 | ✅ | ✅ |
| 模块化设计 | ✅ | ✅ |
| 图像预览 | ✅ | ✅ (节点内预览) |
| 工作流保存 | ✅ | ✅ |
| 自定义节点 | ✅ | ✅ |
| 模型管理 | ✅ | ⚠️ (简化) |
| 高级采样器 | ✅ | ⚠️ (DPM++) |

## 参考

- [ComfyUI GitHub](https://github.com/comfyanonymous/ComfyUI)
- [Hugging Face Diffusers](https://huggingface.co/docs/diffusers)
- [Stable Diffusion Models](https://huggingface.co/models?pipeline_tag=text-to-image)


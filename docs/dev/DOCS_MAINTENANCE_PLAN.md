# RoboCute 文档维护计划 / Documentation Maintenance Plan

**创建日期**: 2026-02-01  
**最后更新**: 2026-02-01  
**维护者**: RoboCute Team

---

## 1. 文档维护目标

### 1.1 核心目标

为 LLM（大语言模型）和开发者提供：

1. **及时性**: 文档内容与代码实现保持同步
2. **完整性**: 覆盖架构、API、开发流程、版本变更等各方面
3. **结构化**: 层次清晰，便于 LLM 理解和检索
4. **可追溯性**: 记录变更历史和决策理由

### 1.2 LLM 友好性要求

- 每个文档开头包含**摘要**和**关键词**
- 代码示例包含完整的上下文
- 使用一致的术语和命名
- 提供清晰的交叉引用链接
- 保持中英双语支持

---

## 2. 文档更新周期

### 2.1 更新频率规划

| 文档类别 | 更新频率 | 触发条件 | 责任人 |
|---------|---------|---------|--------|
| **版本日志 (devlog)** | 每周 | 每周五下班前 | 主开发者 |
| **API 文档** | 每次 API 变更 | PR 合并时 | 功能开发者 |
| **架构文档** | 每月/重大变更 | 架构调整时 | 架构负责人 |
| **开发指南** | 每两周 | 新功能/流程变更 | 主开发者 |
| **Roadmap** | 每月 | 月初规划会议后 | 项目负责人 |
| **快速开始** | 每版本发布 | 发布前检查 | 文档维护者 |

### 2.2 定期检查清单

#### 每周检查 (周五)

- [ ] 更新 `docs/devlog/version/v0X.md` 本周进展
- [ ] 检查是否有新增/废弃的 API 需要文档更新
- [ ] 确认示例代码仍然可运行
- [ ] 更新 `docs/devlog/README.md` 的开发状态

#### 每月检查 (月初)

- [ ] 审核 `docs/dev/Roadmap.md` 进度
- [ ] 更新 `docs/design/Architecture.md` 如有变更
- [ ] 检查所有文档中的死链接
- [ ] 更新版本相关信息
- [ ] 审核 `docs/design/ProjectStructure.md` 与实际目录结构一致性

#### 每版本发布检查

- [ ] 更新 `docs/index.md` 版本状态
- [ ] 确认 `docs/getting-started/` 系列完整可用
- [ ] 生成/更新 API 参考文档
- [ ] 添加版本迁移指南（如有 breaking changes）
- [ ] 更新 `docs/BUILD.md` 构建说明

---

## 3. 文档结构与职责

### 3.1 当前文档目录结构

```
docs/
├── index.md                    # 项目首页 ← 版本发布时更新
├── BUILD.md                    # 构建指南 ← 依赖变更时更新
├── DeveloperGuide.md           # 开发者指南入口
├── UserGuide.md                # 用户指南入口
├── PUBLISH.md                  # 发布流程
│
├── api/                        # API 文档 ← API 变更时更新
│   ├── python-api.md
│   └── rest-api.md
│
├── design/                     # 设计文档 ← 架构变更时更新
│   ├── Overview.md             # 设计总览
│   ├── Architecture.md         # 系统架构
│   ├── Editor.md               # 编辑器设计
│   ├── Pipeline.md             # 渲染管线
│   ├── PRD.md                  # 产品需求
│   ├── ProjectStructure.md     # 项目结构规范
│   ├── editor/                 # 编辑器详细设计
│   ├── pipeline/               # 管线详细设计
│   ├── rbc_cmdline/            # 命令行工具
│   ├── rbc_ext/                # 扩展系统
│   └── runtime/                # 运行时设计
│
├── dev/                        # 开发文档 ← 每周更新
│   ├── AppDev.md               # 应用开发
│   ├── Codebase.md             # 代码库说明
│   ├── EditorDev.md            # 编辑器开发
│   ├── Roadmap.md              # 路线图 ← 每月更新
│   ├── implementation_checklist.md  # 实现检查清单
│   ├── project_initialization.md
│   ├── resource_management.md
│   ├── app/                    # 应用开发细节
│   ├── codebase/               # 代码库细节
│   └── editor/                 # 编辑器开发细节
│
├── devlog/                     # 开发日志 ← 每周更新
│   ├── README.md               # 开发日志索引
│   ├── version/                # 版本里程碑
│   │   ├── v01.md              # v0.1 记录
│   │   ├── v02.md              # v0.2 记录
│   │   ├── v03.md              # v0.3 进行中
│   │   └── v04.md              # v0.4 计划中
│   └── editor/                 # 编辑器开发日志
│
├── getting-started/            # 快速开始 ← 版本发布时更新
│   ├── overview.md
│   ├── installation.md
│   └── quickstart.md
│
├── user-guide/                 # 用户指南
│   ├── animation.md
│   ├── editor.md
│   ├── node-system.md
│   └── scene-management.md
│
└── images/                     # 文档图片
```

### 3.2 各文档更新职责

| 文档路径 | 主要内容 | 更新触发 |
|---------|---------|---------|
| `devlog/version/v03.md` | 当前版本开发进度 | 每周/重大功能完成 |
| `dev/Roadmap.md` | 整体规划 | 每月/规划变更 |
| `design/Overview.md` | 设计原则 | 架构决策变更 |
| `api/python-api.md` | Python API | API 变更 |
| `dev/codebase/codegen.md` | 代码生成 | codegen 逻辑变更 |

---

## 4. 最近提交总结 (2026-01-25 ~ 2026-02-01)

### 4.1 功能开发

#### 渲染与图形
- **天空盒系统**: 添加默认天空盒生成，简化天空生成流程，美化天空效果
- **编辑线修复**: 修复编辑器中的线条渲染问题
- **CUDA Headless 模式**: 支持无界面 CUDA 计算设备

#### 项目与资源管理
- **项目双阶段加载**: 改进项目加载流程，支持分阶段加载
- **rbc-lcpy 简化**: 添加嵌套支持，简化资源复制逻辑
- **Entity 数据处理**: 改进实体序列化/反序列化

#### Python 绑定与代码生成
- **Python codegen 重构**: 完成 "丑陋重构"，改进代码生成架构
- **Meta codegen**: 添加元数据代码生成功能
- **内置图像 API**: 添加 built-in image API 支持
- **LuisaCompute Python 绑定**: 添加 `rbc_ext.luisa` 模块

#### 核心改进
- **RBC_RTTI_WITH_NAME**: 添加带名称的 RTTI 宏
- **安全锁**: 添加资源访问安全锁
- **渲染组件限制移除**: 解除渲染组件的限制
- **场景加载修复**: 修复场景加载问题

### 4.2 文档更新

本周文档变更较少，仅涉及：
- `docs/dev/codebase/pytest.md` - pytest 使用说明
- `docs/dev/editor/editor_test.md` - 编辑器测试
- `docs/dev/editor/nodegraph.md` - 节点图开发
- `docs/devlog/version/v04.md` - v0.4 规划

### 4.3 代码质量

- 清理头文件包含关系
- 移除 LC 绑定（使用官方版本）
- 修复 MSVC 编译问题

---

## 5. 需要完善的文档方向

### 5.1 急需更新 (Priority: High)

#### 5.1.1 Roadmap 需要充实
**文件**: `docs/dev/Roadmap.md`
**现状**: 几乎为空，只有标题
**需要**:
- [ ] 补充 v0.3 具体任务和进度
- [ ] 添加 v0.4 规划概要
- [ ] 添加长期规划展望
- [ ] 与 `devlog/version/` 保持同步

#### 5.1.2 Python API 文档更新
**文件**: `docs/api/python-api.md`
**需要**:
- [ ] 更新 `rbc_ext.luisa` 模块文档
- [ ] 添加 built-in image API 说明
- [ ] 更新代码生成相关 API
- [ ] 添加最新示例代码

#### 5.1.3 开发日志同步
**文件**: `docs/devlog/README.md`
**需要**:
- [ ] 更新 Last Updated 日期
- [ ] 确认 v0.3 状态 (应为 "实现中")
- [ ] 添加最近一周进展摘要

### 5.2 需要补充 (Priority: Medium)

#### 5.2.1 天空盒系统文档
**建议位置**: `docs/design/runtime/` 或 `docs/dev/`
**内容**:
- [ ] 天空大气渲染原理
- [ ] API 使用说明
- [ ] 自定义天空盒指南

#### 5.2.2 Compute Device 文档
**建议位置**: `docs/design/runtime/Graphics.md`
**内容**:
- [ ] ComputeDevice 与 RenderDevice 区别
- [ ] Headless 模式使用
- [ ] CUDA 后端说明

#### 5.2.3 项目加载流程文档
**建议位置**: `docs/dev/project_initialization.md`
**内容**:
- [ ] 双阶段加载机制
- [ ] 资源导入流程
- [ ] 错误处理

#### 5.2.4 代码生成文档更新
**文件**: `docs/dev/codebase/codegen.md`
**内容**:
- [ ] 最新 codegen 架构
- [ ] `rbc_meta` 模块说明
- [ ] 生成代码位置和用途

### 5.3 长期完善 (Priority: Low)

#### 5.3.1 编辑器插件开发指南
**位置**: `docs/design/editor/plugins/`
**需要**:
- [ ] 补充所有插件文档内容
- [ ] 添加插件开发教程
- [ ] 插件 API 参考

#### 5.3.2 用户指南完善
**位置**: `docs/user-guide/`
**需要**:
- [ ] 添加实际可运行示例
- [ ] 截图和录屏
- [ ] FAQ 部分

#### 5.3.3 架构决策记录 (ADR)
**建议添加**: `docs/design/adr/`
**用途**: 记录重要架构决策的背景和理由

---

## 6. 文档模板

### 6.1 版本日志模板

```markdown
# RoboCute vX.Y 开发日志

**计划开始**: YYYY年M月  
**预计完成**: YYYY年M月  
**状态**: 🚧 实现中 / ✅ 已完成 / 🎯 计划中

## 概述

[本版本的主要目标和理念]

## 主要任务

### 1. [任务类别]
- [ ] 子任务 1
- [ ] 子任务 2

## 本周进展 (YYYY-MM-DD ~ YYYY-MM-DD)

### 完成
- [功能1]: 简要描述

### 进行中
- [功能2]: 当前进度

### 遇到的问题
- [问题描述和解决方案]

## 风险与挑战

## 完成情况
```

### 6.2 API 文档模板

```markdown
# [模块名] API 参考

**模块**: `robocute.xxx`  
**版本**: v0.X.Y  
**最后更新**: YYYY-MM-DD

## 概述

[模块功能简介]

## 快速示例

```python
import robocute as rbc
# 示例代码
```

## 类/函数列表

### `ClassName`

**描述**: ...

**参数**:
- `param1` (type): 描述

**返回**: ...

**示例**:
```python
# 使用示例
```

## 变更历史

- v0.X.Y: 添加 xxx 功能
```

---

## 7. 自动化建议

### 7.1 Git Hooks

建议添加 pre-commit hook 检查：
- 如果修改了 `src/` 下的 Python API，提醒更新 `docs/api/`
- 如果修改了核心架构文件，提醒更新 `docs/design/`

### 7.2 CI 检查

- 检查文档中的代码示例是否可执行
- 检查文档链接有效性
- 自动生成 API 文档（基于 docstring）

### 7.3 文档生成

考虑使用：
- **MkDocs**: 当前文档结构适合
- **Sphinx**: 如需自动 API 文档生成
- **pydoc-markdown**: Python API 文档自动化

---

## 8. 下周文档任务

### 本周末 (2026-02-01)
- [ ] 更新 `docs/dev/Roadmap.md` 填充内容
- [ ] 更新 `docs/devlog/README.md` 日期和状态
- [ ] 在 `docs/devlog/version/v03.md` 添加本周进展

### 下周
- [ ] 添加天空盒相关文档
- [ ] 更新 Python API 文档
- [ ] 补充 compute device 说明

---

**注意**: 本计划应每月审核一次，根据项目实际情况调整。


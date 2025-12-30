# Documentation Pipeline / 文档撰写与发布流程

我们基于[mkdocs](https://www.mkdocs.org/)搭建了文档整理和发布流程：

- `mkdocs.yml`: Main configuration file
- `docs/`: Documentation source directory
- `.github/workflows/docs.yml`: GitHub Actions workflow for automatic deployment
- `doc/DEPLOYMENT.md`: Comprehensive deployment guide

## Documentation Deployment / 文档部署

This guide explains how to deploy the RoboCute documentation to GitHub Pages.

本指南说明如何将 RoboCute 文档部署到 GitHub Pages。

### 1. Prerequisites / 前置要求

我们使用 [MkDocs](https://www.mkdocs.org/) 和 [Material 主题](https://squidfunk.github.io/mkdocs-material/) 来生成文档站点。

使用`uv sync --extra dev`来同步安装文档相关的依赖工具

### 2. Configure Repository / 配置仓库

1. 进入您的 GitHub 仓库
2. 导航到 **Settings** → **Pages**
3. 在 **Source** 下，选择：
   - Branch: `gh-pages`
   - Folder: `/ (root)`
4. 点击 **Save**

编辑 `mkdocs.yml` 并更新以下字段：

```yaml
site_url: https://your-username.github.io/robocute
repo_name: robocute
repo_url: https://github.com/your-username/robocute
```

### 2. Deployment Methods / 部署方法

```bash
mkdocs gh-deploy
```

#### 方法 1：GitHub Actions（推荐）

仓库包含一个 GitHub Actions 工作流（`.github/workflows/docs.yml`），当更改推送到 `main` 分支时会自动构建和部署文档。

**步骤:**

1. 确保工作流文件存在：`.github/workflows/docs.yml`
2. 将更改推送到 `main` 分支
3. 工作流将自动：
   - 构建文档
   - 部署到 `gh-pages` 分支
   - 在 `https://your-username.github.io/robocute` 提供访问

#### 方法 2：手动部署

对于手动部署，使用 `mkdocs gh-deploy` 命令：

```bash
mkdocs gh-deploy
```

此命令将：
1. 构建文档
2. 创建/更新 `gh-pages` 分支
3. 推送到 GitHub

### 3. 本地预览

要在不部署的情况下本地预览文档：

```bash
mkdocs serve
```

然后在浏览器中打开 http://127.0.0.1:8000。

### 4. Custom Domain / 自定义域名

要使用自定义域名（例如 `docs.robocute.dev`）：

1. 在 `docs` 目录中添加一个 `CNAME` 文件，包含您的域名：
   ```
   docs.robocute.dev
   ```

2. 更新 `mkdocs.yml`：
   ```yaml
   site_url: https://docs.robocute.dev
   ```

3. 配置 DNS：
   - 添加一个 CNAME 记录指向 `your-username.github.io`

### 5. Troubleshooting / 故障排除

#### Build Errors / 构建错误

如果遇到构建错误：

1. 检查 Python 版本：`python --version`（应为 3.12+）
2. 验证依赖：`pip list | grep mkdocs`
3. 检查 `mkdocs.yml` 语法
4. 查看构建输出：`mkdocs build --verbose`

#### GitHub Pages 未更新

如果 GitHub Pages 未更新：

1. 检查 GitHub Actions 工作流状态
2. 验证 `gh-pages` 分支存在且有内容
3. 确保 Pages 源设置为 `gh-pages` 分支
4. 等待几分钟让更改传播

## Resources / 资源

- [MkDocs 文档](https://www.mkdocs.org/)
- [Material for MkDocs](https://squidfunk.github.io/mkdocs-material/)
- [GitHub Pages 文档](https://docs.github.com/en/pages)


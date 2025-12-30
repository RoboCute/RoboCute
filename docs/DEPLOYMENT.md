# Documentation Deployment Guide / 文档部署指南

This guide explains how to deploy the RoboCute documentation to GitHub Pages.

本指南说明如何将 RoboCute 文档部署到 GitHub Pages。


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

## Deployment Methods / 部署方法

```bash
mkdocs gh-deploy
```

### 方法 1：GitHub Actions（推荐）

仓库包含一个 GitHub Actions 工作流（`.github/workflows/docs.yml`），当更改推送到 `main` 分支时会自动构建和部署文档。

**步骤:**

1. 确保工作流文件存在：`.github/workflows/docs.yml`
2. 将更改推送到 `main` 分支
3. 工作流将自动：
   - 构建文档
   - 部署到 `gh-pages` 分支
   - 在 `https://your-username.github.io/robocute` 提供访问

### 方法 2：手动部署

对于手动部署，使用 `mkdocs gh-deploy` 命令：

```bash
mkdocs gh-deploy
```

此命令将：
1. 构建文档
2. 创建/更新 `gh-pages` 分支
3. 推送到 GitHub

### 方法 3：本地预览

**中文:**

要在不部署的情况下本地预览文档：

```bash
mkdocs serve
```

然后在浏览器中打开 http://127.0.0.1:8000。

## Custom Domain / 自定义域名

**English:**

To use a custom domain (e.g., `docs.robocute.dev`):

1. Add a `CNAME` file in the `docs` directory with your domain:
   ```
   docs.robocute.dev
   ```

2. Update `mkdocs.yml`:
   ```yaml
   site_url: https://docs.robocute.dev
   ```

3. Configure DNS:
   - Add a CNAME record pointing to `your-username.github.io`

**中文:**

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

## Troubleshooting / 故障排除

### Build Errors / 构建错误

**English:**

If you encounter build errors:

1. Check Python version: `python --version` (should be 3.12+)
2. Verify dependencies: `pip list | grep mkdocs`
3. Check `mkdocs.yml` syntax
4. Review build output: `mkdocs build --verbose`

**中文:**

如果遇到构建错误：

1. 检查 Python 版本：`python --version`（应为 3.12+）
2. 验证依赖：`pip list | grep mkdocs`
3. 检查 `mkdocs.yml` 语法
4. 查看构建输出：`mkdocs build --verbose`

### GitHub Pages Not Updating / GitHub Pages 未更新

**English:**

If GitHub Pages is not updating:

1. Check GitHub Actions workflow status
2. Verify `gh-pages` branch exists and has content
3. Ensure Pages source is set to `gh-pages` branch
4. Wait a few minutes for changes to propagate

**中文:**

如果 GitHub Pages 未更新：

1. 检查 GitHub Actions 工作流状态
2. 验证 `gh-pages` 分支存在且有内容
3. 确保 Pages 源设置为 `gh-pages` 分支
4. 等待几分钟让更改传播

## Alternative Documentation Tools / 替代文档工具

**English:**

While we recommend MkDocs, here are other options:

1. **Docusaurus** - React-based documentation framework
2. **VuePress** - Vue-powered static site generator
3. **Sphinx** - Python documentation generator
4. **GitBook** - Modern documentation platform

**中文:**

虽然我们推荐 MkDocs，但以下是其他选项：

1. **Docusaurus** - 基于 React 的文档框架
2. **VuePress** - Vue 驱动的静态站点生成器
3. **Sphinx** - Python 文档生成器
4. **GitBook** - 现代文档平台

## Resources / 资源

**English:**

- [MkDocs Documentation](https://www.mkdocs.org/)
- [Material for MkDocs](https://squidfunk.github.io/mkdocs-material/)
- [GitHub Pages Documentation](https://docs.github.com/en/pages)

**中文:**

- [MkDocs 文档](https://www.mkdocs.org/)
- [Material for MkDocs](https://squidfunk.github.io/mkdocs-material/)
- [GitHub Pages 文档](https://docs.github.com/en/pages)


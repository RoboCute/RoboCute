# RoboCute Documentation

This directory contains the documentation source files for building the documentation site.

此目录包含用于构建文档站点的文档源文件。

## Building Documentation / 构建文档

We use [MkDocs](https://www.mkdocs.org/) with the [Material theme](https://squidfunk.github.io/mkdocs-material/) to generate our documentation site.

我们使用 [MkDocs](https://www.mkdocs.org/) 和 [Material 主题](https://squidfunk.github.io/mkdocs-material/) 来生成文档站点。

### Prerequisites / 前置要求

```bash
pip install mkdocs mkdocs-material
```

### Build Locally / 本地构建

```bash
mkdocs build
```

### Serve Locally / 本地预览

```bash
mkdocs serve
```

Then open http://127.0.0.1:8000 in your browser.

然后在浏览器中打开 http://127.0.0.1:8000。

## Deployment / 部署

### GitHub Pages / GitHub Pages 部署

The documentation is automatically deployed to GitHub Pages when changes are pushed to the `main` branch.

当更改推送到 `main` 分支时，文档会自动部署到 GitHub Pages。

To enable GitHub Pages:

1. Go to repository Settings → Pages
2. Select source: "Deploy from a branch"
3. Select branch: `gh-pages` (or `main` / `docs`)
4. Select folder: `/ (root)` or `/docs`

启用 GitHub Pages：

1. 进入仓库设置 → Pages
2. 选择源："从分支部署"
3. 选择分支：`gh-pages`（或 `main` / `docs`）
4. 选择文件夹：`/ (root)` 或 `/docs`

### Manual Deployment / 手动部署

```bash
mkdocs gh-deploy
```

This will build the documentation and push it to the `gh-pages` branch.

这将构建文档并将其推送到 `gh-pages` 分支。


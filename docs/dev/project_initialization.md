# Project Initialization and Management

## 概述 / Overview

本文档说明 RoboCute 项目的初始化、加载、配置和管理工具的实现。

This document describes the implementation of RoboCute project initialization, loading, configuration, and management tools.

---

## 1. 项目初始化工具 / Project Initialization Tool

### 1.1 项目创建流程 / Project Creation Flow

```
User Request: Create New Project
         ↓
    [Validate Project Name]
    - Check for valid characters
    - Check for existing project
         ↓
    [Create Project Directory Structure]
    - Create root directory
    - Create subdirectories
         ↓
    [Generate rbc_project.json]
    - Set default values
    - Set user-provided metadata
         ↓
    [Create .rbcignore]
    - Add default ignore patterns
         ↓
    [Initialize .rbc/ Directory]
    - Create subdirectories
    - Initialize resource registry database
    - Create log database
         ↓
    [Create Default Files]
    - Create README.md
    - Create default scene
    - Create startup graph (optional)
         ↓
    [Initialize Git (Optional)]
    - git init
    - Create .gitignore
    - Create .gitattributes
    - Initial commit
         ↓
    [Project Created Successfully]
```

---

## 2. 项目管理 API / Project Management API

### 2.1 C++ API

```cpp
// rbc/runtime/include/rbc_project/project.h

namespace rbc::project {

struct ProjectConfig {
    luisa::string name;
    luisa::string version = "0.1.0";
    luisa::string rbc_version;
    luisa::string author;
    luisa::string description;
    luisa::string license = "Apache-2.0";
    
    luisa::unordered_map<luisa::string, luisa::string> paths;
    luisa::unordered_map<luisa::string, luisa::string> config;
    luisa::unordered_map<luisa::string, luisa::string> metadata;
};

struct RBC_RUNTIME_API Project {
public:
    // Create new project
    static std::unique_ptr<Project> create(
        luisa::filesystem::path const& path,
        ProjectConfig const& config);
    
    // Load existing project
    static std::unique_ptr<Project> load(
        luisa::filesystem::path const& path);
    
    // Save project configuration
    bool save();
    
    // Get project paths
    luisa::filesystem::path root_path() const;
    luisa::filesystem::path assets_path() const;
    luisa::filesystem::path scenes_path() const;
    luisa::filesystem::path graphs_path() const;
    luisa::filesystem::path scripts_path() const;
    luisa::filesystem::path intermediate_path() const;
    luisa::filesystem::path resources_path() const;
    luisa::filesystem::path cache_path() const;
    luisa::filesystem::path logs_path() const;
    luisa::filesystem::path output_path() const;
    
    // Get project configuration
    ProjectConfig const& config() const;
    void set_config(ProjectConfig const& config);
    
    // Resource management
    world::ResourceImportManager* import_manager();
    world::ResourceRegistryDB* resource_registry();
    
    // Scene management
    bool load_default_scene();
    bool save_current_scene();
    
    ~Project();

private:
    Project(luisa::filesystem::path const& path);
    
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

// Project creation options
struct ProjectCreationOptions {
    bool create_git_repo = false;
    bool create_default_scene = true;
    bool create_startup_graph = false;
    bool create_example_scripts = false;
};

// Create project with options
RBC_RUNTIME_API std::unique_ptr<Project> create_project(
    luisa::filesystem::path const& path,
    ProjectConfig const& config,
    ProjectCreationOptions const& options = {});

} // namespace rbc::project
```

### 2.2 项目类实现 / Project Class Implementation

```cpp
// rbc/runtime/src/project/project.cpp

namespace rbc::project {

struct Project::Impl {
    luisa::filesystem::path root_path;
    ProjectConfig config;
    
    std::unique_ptr<world::ResourceImportManager> import_manager;
    std::unique_ptr<world::ResourceRegistryDB> resource_registry;
    
    bool is_loaded = false;
};

Project::Project(luisa::filesystem::path const& path)
    : _impl(std::make_unique<Impl>()) {
    _impl->root_path = path;
}

Project::~Project() = default;

std::unique_ptr<Project> Project::create(
    luisa::filesystem::path const& path,
    ProjectConfig const& config) {
    
    // Validate project name
    if (config.name.empty()) {
        LUISA_ERROR("Project name cannot be empty");
        return nullptr;
    }
    
    // Check if project already exists
    auto project_file = path / "rbc_project.json";
    if (luisa::filesystem::exists(project_file)) {
        LUISA_ERROR("Project already exists at {}", path.string());
        return nullptr;
    }
    
    // Create project directory
    std::error_code ec;
    luisa::filesystem::create_directories(path, ec);
    if (ec) {
        LUISA_ERROR("Failed to create project directory: {}", ec.message());
        return nullptr;
    }
    
    // Create project instance
    auto project = std::unique_ptr<Project>(new Project(path));
    project->_impl->config = config;
    
    // Set default paths if not provided
    auto& paths = project->_impl->config.paths;
    if (paths.find("assets") == paths.end()) paths["assets"] = "assets";
    if (paths.find("scenes") == paths.end()) paths["scenes"] = "scenes";
    if (paths.find("graphs") == paths.end()) paths["graphs"] = "graphs";
    if (paths.find("scripts") == paths.end()) paths["scripts"] = "scripts";
    if (paths.find("docs") == paths.end()) paths["docs"] = "docs";
    if (paths.find("datasets") == paths.end()) paths["datasets"] = "datasets";
    if (paths.find("pretrained") == paths.end()) paths["pretrained"] = "pretrained";
    if (paths.find("intermediate") == paths.end()) paths["intermediate"] = ".rbc";
    
    // Create directory structure
    for (auto const& [key, rel_path] : paths) {
        auto dir_path = path / rel_path;
        luisa::filesystem::create_directories(dir_path, ec);
        if (ec) {
            LUISA_WARNING("Failed to create directory {}: {}", 
                         dir_path.string(), ec.message());
        }
    }
    
    // Create subdirectories in .rbc/
    auto rbc_dir = project->intermediate_path();
    luisa::filesystem::create_directories(rbc_dir / "resources" / "meshes", ec);
    luisa::filesystem::create_directories(rbc_dir / "resources" / "textures", ec);
    luisa::filesystem::create_directories(rbc_dir / "resources" / "materials", ec);
    luisa::filesystem::create_directories(rbc_dir / "resources" / "skeletons", ec);
    luisa::filesystem::create_directories(rbc_dir / "resources" / "skins", ec);
    luisa::filesystem::create_directories(rbc_dir / "resources" / "animations", ec);
    luisa::filesystem::create_directories(rbc_dir / "cache" / "shaders" / "dx", ec);
    luisa::filesystem::create_directories(rbc_dir / "cache" / "shaders" / "vk", ec);
    luisa::filesystem::create_directories(rbc_dir / "cache" / "thumbnails", ec);
    luisa::filesystem::create_directories(rbc_dir / "cache" / "temp", ec);
    luisa::filesystem::create_directories(rbc_dir / "logs", ec);
    luisa::filesystem::create_directories(rbc_dir / "out" / "renders", ec);
    luisa::filesystem::create_directories(rbc_dir / "out" / "exports", ec);
    
    // Initialize resource management
    project->_impl->import_manager = std::make_unique<world::ResourceImportManager>(
        path, rbc_dir);
    
    // Save project configuration
    if (!project->save()) {
        LUISA_ERROR("Failed to save project configuration");
        return nullptr;
    }
    
    // Create .rbcignore
    create_rbcignore_file(path / ".rbcignore");
    
    project->_impl->is_loaded = true;
    return project;
}

std::unique_ptr<Project> Project::load(luisa::filesystem::path const& path) {
    auto project_file = path / "rbc_project.json";
    
    if (!luisa::filesystem::exists(project_file)) {
        LUISA_ERROR("Project file not found: {}", project_file.string());
        return nullptr;
    }
    
    // Load project configuration
    auto project = std::unique_ptr<Project>(new Project(path));
    
    // Parse JSON
    std::ifstream file(project_file);
    if (!file.is_open()) {
        LUISA_ERROR("Failed to open project file: {}", project_file.string());
        return nullptr;
    }
    
    nlohmann::json json;
    try {
        file >> json;
    } catch (std::exception const& e) {
        LUISA_ERROR("Failed to parse project file: {}", e.what());
        return nullptr;
    }
    
    // Load configuration
    project->_impl->config.name = json["name"];
    project->_impl->config.version = json.value("version", "0.1.0");
    project->_impl->config.rbc_version = json.value("rbc_version", RBC_VERSION);
    project->_impl->config.author = json.value("author", "");
    project->_impl->config.description = json.value("description", "");
    project->_impl->config.license = json.value("license", "Apache-2.0");
    
    if (json.contains("paths")) {
        for (auto& [key, value] : json["paths"].items()) {
            project->_impl->config.paths[key] = value;
        }
    }
    
    if (json.contains("config")) {
        for (auto& [key, value] : json["config"].items()) {
            project->_impl->config.config[key] = value;
        }
    }
    
    if (json.contains("metadata")) {
        for (auto& [key, value] : json["metadata"].items()) {
            project->_impl->config.metadata[key] = value;
        }
    }
    
    // Initialize resource management
    auto rbc_dir = project->intermediate_path();
    project->_impl->import_manager = std::make_unique<world::ResourceImportManager>(
        path, rbc_dir);
    
    project->_impl->is_loaded = true;
    
    LUISA_INFO("Loaded project: {}", project->_impl->config.name);
    return project;
}

bool Project::save() {
    auto project_file = _impl->root_path / "rbc_project.json";
    
    // Create JSON
    nlohmann::json json;
    json["name"] = _impl->config.name;
    json["version"] = _impl->config.version;
    json["rbc_version"] = _impl->config.rbc_version;
    json["author"] = _impl->config.author;
    json["description"] = _impl->config.description;
    
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    
    if (!_impl->config.metadata.contains("created_at")) {
        json["created_at"] = ss.str();
    } else {
        json["created_at"] = _impl->config.metadata["created_at"];
    }
    json["modified_at"] = ss.str();
    
    json["paths"] = _impl->config.paths;
    json["config"] = _impl->config.config;
    json["metadata"] = _impl->config.metadata;
    
    // Write to file
    std::ofstream file(project_file);
    if (!file.is_open()) {
        LUISA_ERROR("Failed to open project file for writing: {}", project_file.string());
        return false;
    }
    
    file << json.dump(2);
    return true;
}

luisa::filesystem::path Project::root_path() const {
    return _impl->root_path;
}

luisa::filesystem::path Project::assets_path() const {
    return _impl->root_path / _impl->config.paths["assets"];
}

luisa::filesystem::path Project::intermediate_path() const {
    return _impl->root_path / _impl->config.paths["intermediate"];
}

luisa::filesystem::path Project::resources_path() const {
    return intermediate_path() / "resources";
}

world::ResourceImportManager* Project::import_manager() {
    return _impl->import_manager.get();
}

} // namespace rbc::project
```

### 2.3 辅助函数 / Helper Functions

```cpp
// rbc/runtime/src/project/project_helpers.cpp

namespace rbc::project {

void create_rbcignore_file(luisa::filesystem::path const& path) {
    std::ofstream file(path);
    if (!file.is_open()) {
        LUISA_WARNING("Failed to create .rbcignore file");
        return;
    }
    
    file << R"(# RoboCute intermediate files
.rbc/
*.rbcb.tmp
*.log

# OS files
.DS_Store
Thumbs.db
desktop.ini

# IDE files
.vscode/
.idea/
*.swp
*.swo
*~

# Large temporary files
*.tmp
*.cache
*.bak

# Python cache
__pycache__/
*.pyc
*.pyo
*.pyd
.Python
*.egg-info/

# Build artifacts
build/
dist/
*.so
*.dll
*.dylib
)";
}

void create_gitignore_file(luisa::filesystem::path const& path) {
    std::ofstream file(path);
    if (!file.is_open()) {
        LUISA_WARNING("Failed to create .gitignore file");
        return;
    }
    
    file << R"(# RoboCute intermediate files
.rbc/

# OS files
.DS_Store
Thumbs.db
desktop.ini

# IDE files
.vscode/
.idea/
*.swp
*.swo
*~

# Python
__pycache__/
*.py[cod]
*$py.class
*.so
.Python
*.egg-info/
dist/
build/
venv/
env/

# Logs
*.log
)";
}

void create_gitattributes_file(luisa::filesystem::path const& path) {
    std::ofstream file(path);
    if (!file.is_open()) {
        LUISA_WARNING("Failed to create .gitattributes file");
        return;
    }
    
    file << R"(# 3D Models
*.gltf filter=lfs diff=lfs merge=lfs -text
*.glb filter=lfs diff=lfs merge=lfs -text
*.fbx filter=lfs diff=lfs merge=lfs -text
*.obj filter=lfs diff=lfs merge=lfs -text
*.usd filter=lfs diff=lfs merge=lfs -text
*.usda filter=lfs diff=lfs merge=lfs -text
*.usdc filter=lfs diff=lfs merge=lfs -text

# Textures and Images
*.png filter=lfs diff=lfs merge=lfs -text
*.jpg filter=lfs diff=lfs merge=lfs -text
*.jpeg filter=lfs diff=lfs merge=lfs -text
*.exr filter=lfs diff=lfs merge=lfs -text
*.hdr filter=lfs diff=lfs merge=lfs -text
*.tiff filter=lfs diff=lfs merge=lfs -text
*.tga filter=lfs diff=lfs merge=lfs -text
*.bmp filter=lfs diff=lfs merge=lfs -text

# Audio
*.wav filter=lfs diff=lfs merge=lfs -text
*.mp3 filter=lfs diff=lfs merge=lfs -text
*.ogg filter=lfs diff=lfs merge=lfs -text

# Video
*.mp4 filter=lfs diff=lfs merge=lfs -text
*.mov filter=lfs diff=lfs merge=lfs -text
*.avi filter=lfs diff=lfs merge=lfs -text

# ML Models
*.safetensors filter=lfs diff=lfs merge=lfs -text
*.pt filter=lfs diff=lfs merge=lfs -text
*.pth filter=lfs diff=lfs merge=lfs -text
*.onnx filter=lfs diff=lfs merge=lfs -text
*.pb filter=lfs diff=lfs merge=lfs -text

# Archives
*.zip filter=lfs diff=lfs merge=lfs -text
*.7z filter=lfs diff=lfs merge=lfs -text
*.tar.gz filter=lfs diff=lfs merge=lfs -text
)";
}

void create_readme_file(luisa::filesystem::path const& path, ProjectConfig const& config) {
    std::ofstream file(path);
    if (!file.is_open()) {
        LUISA_WARNING("Failed to create README.md file");
        return;
    }
    
    file << fmt::format(R"(# {}

{}

## Project Information

- **Version**: {}
- **RoboCute Version**: {}
- **Author**: {}
- **License**: {}

## Quick Start

### Run the Project

```bash
# Start the server
python main.py
```

### Connect with Editor

```bash
# Start the C++ editor (if available)
rbc_editor --connect localhost:5555
```

## Project Structure

- `assets/` - Raw assets (GLTF, textures, etc.)
- `scenes/` - Scene files
- `graphs/` - Node graph definitions
- `scripts/` - Custom Python scripts and nodes
- `docs/` - Project documentation
- `.rbc/` - Intermediate files (do not commit to git)

## Documentation

For more information, see the [RoboCute Documentation](https://robocute.github.io/RoboCute/).

## License

{}
)",
        config.name,
        config.description,
        config.version,
        config.rbc_version,
        config.author,
        config.license,
        config.license
    );
}

void create_default_main_py(luisa::filesystem::path const& path, ProjectConfig const& config) {
    std::ofstream file(path);
    if (!file.is_open()) {
        LUISA_WARNING("Failed to create main.py file");
        return;
    }
    
    file << fmt::format(R"(#!/usr/bin/env python3
"""
{} - Main Entry Point

{}
"""

import robocute as rbc
from pathlib import Path

def main():
    # Initialize project
    project_root = Path(__file__).parent
    project = rbc.Project.load(project_root)
    
    # Initialize world system
    rbc.init_world()
    
    # Create scene
    scene = rbc.Scene()
    scene.start()
    
    # Load default scene if exists
    default_scene_path = project.scenes_path() / "main.rbcscene"
    if default_scene_path.exists():
        scene.load(default_scene_path)
    else:
        # Create a simple default scene
        entity = scene.create_entity("DefaultEntity")
        transform = entity.add_component(rbc.TransformComponent)
        transform.position = [0.0, 0.0, 0.0]
    
    # Start server
    server = rbc.Server(title="{} Server", version="{}")
    editor_service = rbc.EditorService(scene)
    server.register_service(editor_service)
    
    print("Starting server on port 5555...")
    print("Connect with the RoboCute editor to visualize the scene")
    
    server.start(port=5555)
    
    # Cleanup
    rbc.destroy_world()

if __name__ == "__main__":
    main()
)",
        config.name,
        config.description,
        config.name,
        config.version
    );
}

} // namespace rbc::project
```

---

## 3. Python API

```python
# src/robocute/project.py

from typing import Optional, Dict
from pathlib import Path
import robocute_ext as _rbc_ext

class Project:
    """RoboCute project manager"""
    
    def __init__(self, impl):
        self._impl = impl
    
    @staticmethod
    def create(path: str, 
              name: str,
              author: str = "",
              description: str = "",
              version: str = "0.1.0",
              **kwargs) -> 'Project':
        """Create a new RoboCute project
        
        Args:
            path: Project root directory path
            name: Project name
            author: Project author
            description: Project description
            version: Project version
            **kwargs: Additional configuration options
            
        Returns:
            Project instance
        """
        config = {
            'name': name,
            'version': version,
            'author': author,
            'description': description,
            **kwargs
        }
        impl = _rbc_ext.Project.create(path, config)
        return Project(impl)
    
    @staticmethod
    def load(path: str) -> 'Project':
        """Load an existing project
        
        Args:
            path: Project root directory path
            
        Returns:
            Project instance
        """
        impl = _rbc_ext.Project.load(path)
        return Project(impl)
    
    def save(self) -> bool:
        """Save project configuration"""
        return self._impl.save()
    
    def root_path(self) -> Path:
        """Get project root path"""
        return Path(self._impl.root_path())
    
    def assets_path(self) -> Path:
        """Get assets directory path"""
        return Path(self._impl.assets_path())
    
    def scenes_path(self) -> Path:
        """Get scenes directory path"""
        return Path(self._impl.scenes_path())
    
    def graphs_path(self) -> Path:
        """Get graphs directory path"""
        return Path(self._impl.graphs_path())
    
    def scripts_path(self) -> Path:
        """Get scripts directory path"""
        return Path(self._impl.scripts_path())
    
    def intermediate_path(self) -> Path:
        """Get intermediate directory path"""
        return Path(self._impl.intermediate_path())
    
    def resources_path(self) -> Path:
        """Get resources directory path"""
        return Path(self._impl.resources_path())
    
    def import_manager(self):
        """Get resource import manager"""
        return self._impl.import_manager()
    
    @property
    def config(self) -> Dict:
        """Get project configuration"""
        return self._impl.config()
```

---

## 4. 命令行工具 / Command Line Tool

```python
# src/robocute/cli/project.py

import click
import robocute as rbc
from pathlib import Path

@click.group()
def cli():
    """RoboCute project management tool"""
    pass

@cli.command()
@click.argument('path')
@click.option('--name', required=True, help='Project name')
@click.option('--author', default='', help='Project author')
@click.option('--description', default='', help='Project description')
@click.option('--version', default='0.1.0', help='Project version')
@click.option('--git/--no-git', default=False, help='Initialize git repository')
@click.option('--default-scene/--no-default-scene', default=True, help='Create default scene')
def create(path, name, author, description, version, git, default_scene):
    """Create a new RoboCute project"""
    
    click.echo(f"Creating project '{name}' at {path}...")
    
    try:
        project = rbc.Project.create(
            path,
            name=name,
            author=author,
            description=description,
            version=version
        )
        
        # Create default files
        if default_scene:
            create_default_scene(project)
        
        # Initialize git
        if git:
            init_git_repo(project.root_path())
        
        click.echo(f"✓ Project created successfully at {path}")
        click.echo(f"  Run 'cd {path} && python main.py' to start the server")
        
    except Exception as e:
        click.echo(f"✗ Failed to create project: {e}", err=True)
        return 1

@cli.command()
@click.argument('path')
def info(path):
    """Show project information"""
    try:
        project = rbc.Project.load(path)
        config = project.config
        
        click.echo(f"Project: {config['name']}")
        click.echo(f"Version: {config['version']}")
        click.echo(f"Author: {config.get('author', 'N/A')}")
        click.echo(f"Description: {config.get('description', 'N/A')}")
        click.echo(f"RoboCute Version: {config.get('rbc_version', 'N/A')}")
        click.echo(f"Root Path: {project.root_path()}")
        
    except Exception as e:
        click.echo(f"✗ Failed to load project: {e}", err=True)
        return 1

@cli.command()
@click.argument('path')
def validate(path):
    """Validate project structure"""
    try:
        project = rbc.Project.load(path)
        
        errors = []
        warnings = []
        
        # Check required directories
        required_dirs = ['assets', 'scenes', 'graphs', 'scripts']
        for dir_name in required_dirs:
            dir_path = project.root_path() / dir_name
            if not dir_path.exists():
                errors.append(f"Missing required directory: {dir_name}/")
        
        # Check rbc_project.json
        project_file = project.root_path() / "rbc_project.json"
        if not project_file.exists():
            errors.append("Missing rbc_project.json")
        
        # Check .rbc/ directory
        rbc_dir = project.intermediate_path()
        if not rbc_dir.exists():
            warnings.append("Missing .rbc/ directory (will be created on first run)")
        
        # Report results
        if errors:
            click.echo("✗ Validation failed:")
            for error in errors:
                click.echo(f"  ERROR: {error}")
        
        if warnings:
            click.echo("⚠ Warnings:")
            for warning in warnings:
                click.echo(f"  WARNING: {warning}")
        
        if not errors and not warnings:
            click.echo("✓ Project structure is valid")
        
        return 1 if errors else 0
        
    except Exception as e:
        click.echo(f"✗ Failed to validate project: {e}", err=True)
        return 1

def create_default_scene(project):
    """Create a default scene"""
    # TODO: Implement default scene creation
    pass

def init_git_repo(path: Path):
    """Initialize git repository"""
    import subprocess
    
    try:
        subprocess.run(['git', 'init'], cwd=path, check=True)
        subprocess.run(['git', 'add', '.'], cwd=path, check=True)
        subprocess.run(['git', 'commit', '-m', 'Initial commit'], cwd=path, check=True)
        click.echo("✓ Initialized git repository")
    except subprocess.CalledProcessError as e:
        click.echo(f"⚠ Failed to initialize git: {e}", err=True)

if __name__ == '__main__':
    cli()
```

使用示例：

```bash
# Create new project
python -m robocute.cli.project create MyRobotProject \
    --name "My Robot Project" \
    --author "John Doe" \
    --description "A robot simulation project" \
    --git

# Show project info
python -m robocute.cli.project info MyRobotProject

# Validate project structure
python -m robocute.cli.project validate MyRobotProject
```

---

## 5. 项目模板 / Project Templates

### 5.1 模板系统 / Template System

```python
# src/robocute/templates/template_manager.py

from typing import Dict, List
from pathlib import Path
import shutil
import json

class ProjectTemplate:
    """Project template"""
    
    def __init__(self, name: str, path: Path):
        self.name = name
        self.path = path
        self.config = self.load_config()
    
    def load_config(self) -> Dict:
        """Load template configuration"""
        config_file = self.path / "template.json"
        if not config_file.exists():
            return {}
        
        with open(config_file, 'r') as f:
            return json.load(f)
    
    def apply(self, target_path: Path, project_config: Dict):
        """Apply template to target path"""
        # Copy template files
        shutil.copytree(self.path, target_path, 
                       ignore=shutil.ignore_patterns('template.json'))
        
        # Replace placeholders in files
        self.replace_placeholders(target_path, project_config)
    
    def replace_placeholders(self, path: Path, config: Dict):
        """Replace placeholders in template files"""
        placeholders = {
            '{{PROJECT_NAME}}': config.get('name', 'MyProject'),
            '{{PROJECT_VERSION}}': config.get('version', '0.1.0'),
            '{{PROJECT_AUTHOR}}': config.get('author', ''),
            '{{PROJECT_DESCRIPTION}}': config.get('description', ''),
        }
        
        for file_path in path.rglob('*'):
            if file_path.is_file() and file_path.suffix in ['.py', '.json', '.md']:
                content = file_path.read_text()
                for placeholder, value in placeholders.items():
                    content = content.replace(placeholder, value)
                file_path.write_text(content)

class TemplateManager:
    """Manage project templates"""
    
    def __init__(self, templates_dir: Path):
        self.templates_dir = templates_dir
        self.templates = self.discover_templates()
    
    def discover_templates(self) -> Dict[str, ProjectTemplate]:
        """Discover all available templates"""
        templates = {}
        
        if not self.templates_dir.exists():
            return templates
        
        for template_dir in self.templates_dir.iterdir():
            if template_dir.is_dir():
                template = ProjectTemplate(template_dir.name, template_dir)
                templates[template.name] = template
        
        return templates
    
    def get_template(self, name: str) -> ProjectTemplate:
        """Get template by name"""
        return self.templates.get(name)
    
    def list_templates(self) -> List[str]:
        """List all available templates"""
        return list(self.templates.keys())
    
    def create_project_from_template(self, 
                                    template_name: str,
                                    target_path: Path,
                                    project_config: Dict):
        """Create project from template"""
        template = self.get_template(template_name)
        if not template:
            raise ValueError(f"Template '{template_name}' not found")
        
        template.apply(target_path, project_config)
```

### 5.2 内置模板 / Built-in Templates

**模板目录结构：**

```
robocute/templates/
├── empty/                     # 空项目模板
│   └── template.json
├── basic_simulation/          # 基础仿真模板
│   ├── template.json
│   ├── main.py
│   ├── scenes/
│   │   └── main.rbcscene
│   └── graphs/
│       └── startup.rbcgraph
├── robot_chassis/             # 机器人底盘模板
│   ├── template.json
│   ├── main.py
│   ├── scripts/
│   │   └── robot_control.py
│   └── assets/
│       └── models/
└── aigc_workflow/             # AIGC工作流模板
    ├── template.json
    ├── main.py
    └── graphs/
        └── text2model.rbcgraph
```

**模板配置示例 `basic_simulation/template.json`:**

```json
{
  "name": "Basic Simulation",
  "description": "A basic physics simulation template",
  "author": "RoboCute Team",
  "version": "0.1.0",
  "dependencies": {
    "robocute": ">=0.2.0"
  },
  "features": [
    "physics_simulation",
    "basic_rendering",
    "default_scene"
  ]
}
```

---

## 6. 配置管理 / Configuration Management

### 6.1 应用级配置 / Application-Level Config

```python
# src/robocute/config.py

from pathlib import Path
import json
import os

class AppConfig:
    """Application-level configuration"""
    
    @staticmethod
    def get_config_path() -> Path:
        """Get application config file path"""
        if os.name == 'nt':  # Windows
            config_dir = Path(os.environ['APPDATA']) / 'RoboCute'
        else:  # Linux/macOS
            config_dir = Path.home() / '.config' / 'robocute'
        
        config_dir.mkdir(parents=True, exist_ok=True)
        return config_dir / 'config.json'
    
    @staticmethod
    def load() -> dict:
        """Load application configuration"""
        config_file = AppConfig.get_config_path()
        
        if not config_file.exists():
            return AppConfig.get_default_config()
        
        with open(config_file, 'r') as f:
            return json.load(f)
    
    @staticmethod
    def save(config: dict):
        """Save application configuration"""
        config_file = AppConfig.get_config_path()
        
        with open(config_file, 'w') as f:
            json.dump(config, f, indent=2)
    
    @staticmethod
    def get_default_config() -> dict:
        """Get default configuration"""
        return {
            'version': '0.2.0',
            'runtime': {
                'executable_path': '',
                'shader_search_paths': []
            },
            'editor': {
                'theme': 'dark',
                'font_size': 12,
                'auto_save_interval': 300,
                'recent_projects': [],
                'default_project_path': str(Path.home() / 'RoboCuteProjects')
            },
            'rendering': {
                'default_backend': 'dx',
                'vsync': True,
                'max_fps': 60
            },
            'python': {
                'interpreter_path': '',
                'site_packages': []
            }
        }
```

---

## 7. 项目迁移和升级 / Project Migration and Upgrade

```python
# src/robocute/cli/migrate.py

import click
import robocute as rbc
from pathlib import Path

@click.group()
def cli():
    """Project migration and upgrade tool"""
    pass

@cli.command()
@click.argument('project_path')
def check(project_path):
    """Check project compatibility with current RoboCute version"""
    try:
        project = rbc.Project.load(project_path)
        config = project.config
        
        project_rbc_version = config.get('rbc_version', '0.0.0')
        current_rbc_version = rbc.__version__
        
        click.echo(f"Project RoboCute Version: {project_rbc_version}")
        click.echo(f"Current RoboCute Version: {current_rbc_version}")
        
        if project_rbc_version == current_rbc_version:
            click.echo("✓ Project is compatible")
        else:
            click.echo("⚠ Project may need migration")
            
    except Exception as e:
        click.echo(f"✗ Failed to check project: {e}", err=True)

@cli.command()
@click.argument('project_path')
@click.option('--backup/--no-backup', default=True, help='Create backup before migration')
def migrate(project_path, backup):
    """Migrate project to current RoboCute version"""
    try:
        if backup:
            backup_path = Path(project_path).parent / (Path(project_path).name + '_backup')
            click.echo(f"Creating backup at {backup_path}...")
            # TODO: Implement backup
        
        click.echo("Migrating project...")
        # TODO: Implement migration logic
        
        click.echo("✓ Project migrated successfully")
        
    except Exception as e:
        click.echo(f"✗ Migration failed: {e}", err=True)

if __name__ == '__main__':
    cli()
```

---

**文档版本**: v1.0.0  
**最后更新**: 2025-12-31  
**作者**: RoboCute Team


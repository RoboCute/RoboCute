GIT_TASKS = {
    "lc": {
        "subdir": "thirdparty/LuisaCompute",
        "url": "https://github.com/LuisaGroup/LuisaCompute.git",
        "branch": "next",
        "deps": [],
    },
    "xxhash": {
        "subdir": "thirdparty/LuisaCompute/src/ext/xxhash",
        "url": "https://github.com/Cyan4973/xxHash.git",
        "branch": None,
        "deps": ["lc"],
    },
    "spdlog": {
        "subdir": "thirdparty/LuisaCompute/src/ext/spdlog",
        "url": "https://github.com/LuisaGroup/spdlog.git",
        "branch": None,
        "deps": ["lc"],
    },
    "EASTL": {
        "subdir": "thirdparty/LuisaCompute/src/ext/EASTL",
        "url": "https://github.com/LuisaGroup/EASTL.git",
        "branch": None,
        "deps": ["lc"],
    },
    "EABase": {
        "subdir": "thirdparty/LuisaCompute/src/ext/EASTL/packages/EABase",
        "url": "https://github.com/LuisaGroup/EABase.git",
        "branch": None,
        "deps": ["EASTL"],
    },
    "mimalloc": {
        "subdir": "thirdparty/LuisaCompute/src/ext/EASTL/packages/mimalloc",
        "url": "https://github.com/LuisaGroup/mimalloc.git",
        "branch": None,
        "deps": ["EASTL"],
    },
    "glfw": {
        "subdir": "thirdparty/LuisaCompute/src/ext/glfw",
        "url": "https://github.com/glfw/glfw.git",
        "branch": None,
        "deps": ["lc"],
    },
    "magic_enum": {
        "subdir": "thirdparty/LuisaCompute/src/ext/magic_enum",
        "url": "https://github.com/LuisaGroup/magic_enum",
        "branch": None,
        "deps": ["lc"],
    },
    "marl": {
        "subdir": "thirdparty/LuisaCompute/src/ext/marl",
        "url": "https://github.com/LuisaGroup/marl.git",
        "branch": None,
        "deps": ["lc"],
    },
    "imgui": {
        "subdir": "thirdparty/LuisaCompute/src/ext/imgui",
        "url": "https://github.com/ocornut/imgui.git",
        "branch": "docking",
        "deps": ["lc"],
    },
    "reproc": {
        "subdir": "thirdparty/LuisaCompute/src/ext/reproc",
        "url": "https://github.com/LuisaGroup/reproc.git",
        "branch": None,
        "deps": ["lc"],
    },
    "stb": {
        "subdir": "thirdparty/LuisaCompute/src/ext/stb/stb",
        "url": "https://github.com/nothings/stb.git",
        "branch": None,
        "deps": ["lc"],
    },
    "yyjson": {
        "subdir": "thirdparty/LuisaCompute/src/ext/yyjson",
        "url": "https://github.com/ibireme/yyjson.git",
        "branch": None,
        "deps": ["lc"],
    },
    "pybind11": {
        "subdir": "thirdparty/LuisaCompute/src/ext/pybind11",
        "url": "https://github.com/LuisaGroup/pybind11.git",
        "branch": None,
        "deps": ["lc"],
    },
    "qt_node_editor": {
        "subdir": "thirdparty/qt_node_editor",
        "url": "https://github.com/RoboCute/nodeeditor.git",
        "branch": None,
        "deps": [],
    },
}
CLANGCXX_NAME = 'clangcxx_compiler-v2.0.1'
CLANGCXX_PATH = "build/tool/clangcxx_compiler/clangcxx_compiler.exe"
SHADER_PATH = "rbc/shader"
CLANGD_NAME = 'clangd-v19.1.7'
LC_SDK_ADDRESS = "https://github.com/LuisaGroup/SDKs/releases/download/sdk/"
RBC_SDK_ADDRESS = "https://github.com/RoboCute/RoboCute.Resouces/releases/download/Release/"
LC_DX_SDK = "dx_sdk_20250816.zip"
RENDER_RESOURCE_NAME = "render_resources.7z"
PLATFORM = 'windows'
ARCH = 'x64'
LC_TOOLCHAIN = "llvm"
PYD_TOOLCHAIN = "clang-cl"
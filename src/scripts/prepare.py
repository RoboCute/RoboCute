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
    "ozz_animation": {
        "subdir": "thirdparty/ozz_animation",
        "url": "https://github.com/RoboCute/ozz-animation.git",
        "branch": None,
        "deps": [],
    },
    "jolt_physics": {
        "subdir": "thirdparty/jolt_physics",
        "url": "https://github.com/RoboCute/JoltPhysics.git",
        "branch": None,
        "deps": [],
    },
    # "acl": {
    #     "subdir": "thirdparty/acl",
    #     "url": "https://github.com/RoboCute/acl.git",
    #     "branch": None,
    #     "deps": [],
    # },
    # "cpptrace": {
    #     "subdir": "thirdparty/cpptrace",
    #     "url": "https://github.com/RoboCute/cpptrace.git",
    #     "branch": None,
    #     "deps": [],
    # },
    # "libigl": {
    #     "subdir": "thirdparty/libigl",
    #     "url": "https://github.com/RoboCute/libigl.git",
    #     "branch": None,
    #     "deps": [],
    # },
    # "eigen": {
    #     "subdir": "thirdparty/eigen",
    #     "url": "https://github.com/RoboCute/eigen.git",
    #     "branch": "3.4",
    #     "deps": [],
    # },
    # "cppitertools": {
    #     "subdir": "thirdparty/cppitertools",
    #     "url": "https://github.com/RoboCute/cppitertools.git",
    #     "branch": "3.4",
    #     "deps": [],
    # },
    # "dylib": {
    #     "subdir": "thirdparty/dylib",
    #     "url": "https://github.com/RoboCute/dylib.git",
    #     "branch": "3.4",
    #     "deps": [],
    # },
}
OIDN_NAME = "oidn-2.3.3"
CLANGCXX_NAME = "clangcxx_compiler-v2.0.6"
CLANGCXX_PATH = "build/tool/clangcxx_compiler/clangcxx_compiler.exe"
SHADER_PATH = "rbc/shader"
CLANGD_NAME = "clangd-v19.1.7"
LC_SDK_ADDRESS = "https://github.com/LuisaGroup/SDKs/releases/download/sdk/"
RBC_SDK_ADDRESS = (
    "https://github.com/RoboCute/RoboCute.Resouces/releases/download/Release/"
)
LC_DX_SDK = "dx_sdk_20250816.zip"
RENDER_RESOURCE_NAME = "render_resources-v1.0.0.7z"
XMAKE_GLOBAL_TOOLCHAIN = "clang-cl"
PLATFORM = "windows"
ARCH = "x64"


# TODO: platform
def _to_platform_spec(name):
    return f"{name}-{PLATFORM}-{ARCH}.7z"


OIDN_NAME = _to_platform_spec(OIDN_NAME)
CLANGCXX_NAME = _to_platform_spec(CLANGCXX_NAME)
CLANGD_NAME = _to_platform_spec(CLANGD_NAME)
# PYD_TOOLCHAIN = "msvc"

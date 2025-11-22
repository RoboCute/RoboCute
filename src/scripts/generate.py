from pathlib import Path

GENERATION_TASKS = [
    (
        "meta.test_codegen",
        "codegen_module",
        Path("rbc/tests/test_py_codegen/generated").resolve(),
        Path("src/rbc_ext/generated").resolve(),
    ),
    (
        "meta.test_serde",
        "codegen_header",
        Path("rbc/tests/test_serde/generated/generated.hpp").resolve(),
    ),
    (
        "meta.pipeline_settings",
        "codegen_header",
        Path(
            "rbc/render_plugin/include/rbc_render/generated/pipeline_settings.hpp"
        ).resolve(),
        Path("rbc/render_plugin/src/generated/pipeline_settings.cpp").resolve(),
    ),
]

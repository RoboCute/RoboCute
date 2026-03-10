#include <rbc_core/version.h>
#include <stdint.h>
#include <luisa/core/dll_export.h>

LUISA_EXPORT_API uint64_t rbc_version() {
    return RBC_VERSION;
}

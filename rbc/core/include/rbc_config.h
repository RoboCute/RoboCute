#pragma once
#include <luisa/core/dll_export.h>


#ifdef _MSC_VER
#define RBC_UNREACHABLE() __assume(false)
#define RBC_UNIMPLEMENTED() __assume(false)
#else
#define RBC_UNREACHABLE() __builtin_unreachable()
#define RBC_UNIMPLEMENTED() __builtin_unreachable()
#endif

#define RBC_EXTERN_C extern "C"
#define RBC_NOEXCEPT noexcept
#define RBC_FORCEINLINE inline
#define RBC_RUNTIME_USE_MIMALLOC true

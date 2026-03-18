#pragma once
#include <luisa/core/dll_export.h>

// Suppress MSVC warnings for STL containers in DLL interfaces
// These warnings are false positives for modern usage patterns
#ifdef _MSC_VER
#pragma warning(disable : 4251)  // class needs to have dll-interface to be used by clients
#pragma warning(disable : 4275)  // non-DLL interface class base used with DLL interface class
#pragma warning(disable : 4305)  // truncation from 'type1' to 'type2'
#pragma warning(disable : 4099)  // struct/class mismatch in forward declaration
#endif

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

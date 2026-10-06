// Delay-load hook for the node.exe import library — required so that this
// addon loads inside Electron, where there is no "node.exe" module (the N-API
// symbols are exported by electron.exe itself).
//
// Same mechanism as node-gyp's deps/win-delay-load-hook: node.lib references
// "node.exe"; we delay-load it and redirect the load to the host process.

#include <windows.h>

#include <delayimp.h>

#include <cstring>

// Emit /delayload:node.exe straight into the link step (more reliable than
// piping this flag through build systems; this is what allows the hook below
// to redirect node.exe to the host process in Electron).
#if defined(_MSC_VER)
#pragma comment(linker, "/delayload:node.exe")
#endif

static FARPROC WINAPI rbcDelayLoadHook(unsigned int notify, PDelayLoadInfo pdli) {
    if (notify == dliNotePreLoadLibrary && pdli != nullptr &&
        _stricmp(pdli->szDll, "node.exe") == 0) {
        HMODULE host = GetModuleHandleW(nullptr);
        return reinterpret_cast<FARPROC>(host);
    }
    return nullptr;
}

extern "C" {

// The Visual C++ delay-load runtime queries this symbol before each
// delay-loaded import; it must match the declaration in delayimp.h exactly
// (modern MSVC declares it as `const PfnDliHook`).
const PfnDliHook __pfnDliNotifyHook2 = rbcDelayLoadHook;

}

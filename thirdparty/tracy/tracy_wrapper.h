#pragma once
#pragma once

#ifdef __OPTIMIZE__
// Some platforms define NDEBUG for Release builds
#ifndef NDEBUG
#define NDEBUG
#endif
#elif !defined(_MSC_VER)
#define _DEBUG 1
#endif

#pragma region tracy

#if !defined(RBC_PROFILE_ENABLE) && !defined(RBC_PROFILE_OVERRIDE_DISABLE) && !defined(RBC_PROFILE_OVERRIDE_ENABLE)
#ifdef _DEBUG
#define RBC_PROFILE_ENABLE
#else
#endif
#endif

#if !defined(RBC_PROFILE_ENABLE) && defined(RBC_PROFILE_OVERRIDE_ENABLE)
#define RBC_PROFILE_ENABLE
#endif

#if defined(RBC_PROFILE_ENABLE)
#ifndef TRACY_ENABLE
#define TRACY_ENABLE
#endif

#ifndef TRACY_ON_DEMAND
#define TRACY_ON_DEMAND
#endif

#ifndef TRACY_FIBERS
#define TRACY_FIBERS
#endif

#ifndef TRACY_TRACE_ALLOCATION
#define TRACY_TRACE_ALLOCATION
#endif
#endif

#include "./tracy/tracy/TracyC.h"

typedef TracyCZoneCtx RBCCZoneCtx;

#define RBCCZone(ctx, active) TracyCZone(ctx, active)
#define RBCCZoneN(ctx, name, active) TracyCZoneN(ctx, name, active)
#define RBCCZoneC(ctx, color, active) TracyCZoneC(ctx, color, active)
#define RBCCZoneNC(ctx, name, color, active) TracyCZoneNC(ctx, name, color, active)

#define RBCCZoneEnd(ctx) TracyCZoneEnd(ctx)

#define RBCCZoneText(ctx, txt, size) TracyCZoneText(ctx, txt, size)
#define RBCCZoneName(ctx, txt, size) TracyCZoneName(ctx, txt, size)
#define RBCCZoneColor(ctx, color) TracyCZoneColor(ctx, color)
#define RBCCZoneValue(ctx, value) TracyCZoneValue(ctx, value)

#define RBCCAlloc(ptr, size) TracyCAlloc(ptr, size)
#define RBCCFree(ptr) TracyCFree(ptr)
#define RBCCSecureAlloc(ptr, size) TracyCSecureAlloc(ptr, size)
#define RBCCSecureFree(ptr) TracyCSecureFree(ptr)

#define RBCCAllocN(ptr, size, name) TracyCAllocN(ptr, size, name)
#define RBCCFreeN(ptr, name) TracyCFreeN(ptr, name)
#define RBCCSecureAllocN(ptr, size, name) TracyCSecureAllocN(ptr, size, name)
#define RBCCSecureFreeN(ptr, name) TracyCSecureFreeN(ptr, name)

#define RBCCMessage(txt, size) TracyCMessage(txt, size)
#define RBCCMessageL(txt) TracyCMessageL(txt)
#define RBCCMessageC(txt, size, color) TracyCMessageC(txt, size, color)
#define RBCCMessageLC(txt, color) TracyCMessageLC(txt, color)

#define RBCCFrameMark TracyCFrameMark
#define RBCCFrameMarkNamed(name) TracyCFrameMarkNamed(name)
#define RBCCFrameMarkStart(name) TracyCFrameMarkStart(name)
#define RBCCFrameMarkEnd(name) TracyCFrameMarkEnd(name)
#define RBCCFrameImage(image, width, height, offset, flip) TracyCFrameImage(image, width, height, offset, flip)

#define RBCCPlot(name, val) TracyCPlot(name, val)
#define RBCCPlotF(name, val) TracyCPlotF(name, val)
#define RBCCPlotI(name, val) TracyCPlotI(name, val)
#define RBCCPlotConfig(name, type, step, fill, color) TracyCPlotConfig(name, type, step, fill, color)
#define RBCCAppInfo(txt, size) TracyCAppInfo(txt, size)

#define RBCCZoneS(ctx, depth, active) TracyCZoneS(ctx, depth, active)
#define RBCCZoneNS(ctx, name, depth, active) TracyCZoneNS(ctx, name, depth, active)
#define RBCCZoneCS(ctx, color, depth, active) TracyCZoneCS(ctx, color, depth, active)
#define RBCCZoneNCS(ctx, name, color, depth, active) TracyCZoneNCS(ctx, name, color, depth, active)

#define RBCCAllocS(ptr, size, depth) TracyCAllocS(ptr, size, depth)
#define RBCCFreeS(ptr, depth) TracyCFreeS(ptr, depth)
#define RBCCSecureAllocS(ptr, size, depth) TracyCSecureAllocS(ptr, size, depth)
#define RBCCSecureFreeS(ptr, depth) TracyCSecureFreeS(ptr, depth)

#define RBCCAllocNS(ptr, size, depth, name) TracyCAllocNS(ptr, size, depth, name)
#define RBCCFreeNS(ptr, depth, name) TracyCFreeNS(ptr, depth, name)
#define RBCCSecureAllocNS(ptr, size, depth, name) TracyCSecureAllocNS(ptr, size, depth, name)
#define RBCCSecureFreeNS(ptr, depth, name) TracyCSecureFreeNS(ptr, depth, name)

#define RBCCMessageS(txt, size, depth) TracyCMessageS(txt, size, depth)
#define RBCCMessageLS(txt, depth) TracyCMessageLS(txt, depth)
#define RBCCMessageCS(txt, size, color, depth) TracyCMessageCS(txt, size, color, depth)
#define RBCCMessageLCS(txt, color, depth) TracyCMessageLCS(txt, color, depth)

#define RBCCIsConnected TracyCIsConnected

#define RBCCFiberEnter(fiber) TracyCFiberEnter(fiber)
#define RBCCFiberLeave TracyCFiberLeave

#ifdef __cplusplus
#include "./tracy/tracy/Tracy.hpp"

#define RBCZoneNamed(x, y) ZoneNamed(x, y)
#define RBCZoneNamedN(x, y, z) ZoneNamedN(x, y, z)
#define RBCZoneNamedC(x, y, z) ZoneNamedC(x, y, z)
#define RBCZoneNamedNC(x, y, z, w) ZoneNamedNC(x, y, z, w)

#define RBCZoneTransient(x, y) ZoneTransient(x, y)
#define RBCZoneTransientN(x, y, z) ZoneTransientN(x, y, z)

#define RBCZoneScoped ZoneScoped
#define RBCZoneScopedN(x) ZoneScopedN(x)
#define RBCZoneScopedC(x) ZoneScopedC(x)
#define RBCZoneScopedNC(x, y) ZoneScopedNC(x, y)

#define RBCZoneText(x, y) ZoneText(x, y)
#define RBCZoneTextV(x, y, z) ZoneTextV(x, y, z)
#define RBCZoneName(x, y) ZoneName(x, y)
#define RBCZoneNameV(x, y, z) ZoneNameV(x, y, z)
#define RBCZoneColor(x) ZoneColor(x)
#define RBCZoneColorV(x, y) ZoneColorV(x, y)
#define RBCZoneValue(x) ZoneValue(x)
#define RBCZoneValueV(x, y) ZoneValueV(x, y)
#define RBCZoneIsActive ZoneIsActive
#define RBCZoneIsActiveV(x) ZoneIsActiveV(x)

#define RBCFrameMark FrameMark
#define RBCFrameMarkNamed(x) FrameMarkNamed(x)
#define RBCFrameMarkStart(x) FrameMarkStart(x)
#define RBCFrameMarkEnd(x) FrameMarkEnd(x)

#define RBCFrameImage(x, y, z, w, a) FrameImage(x, y, z, w, a)

#define RBCLockable(type, varname) TracyLockable(type, varname)
#define RBCLockableN(type, varname, desc) TracyLockableN(type, varname, desc)
#define RBCSharedLockable(type, varname) TracySharedLockable(type, varname)
#define RBCSharedLockableN(type, varname, desc) TracySharedLockableN(type, varname, desc)
#define RBCLockableBase(type) LockableBase(type)
#define RBCSharedLockableBase(type) SharedLockableBase(type)
#define RBCLockMark(x) LockMark(x)
#define RBCLockableName(x, y, z) LockableName(x, y, z)

#define RBCPlot(x, y) TracyPlot(x, y)
#define RBCPlotConfig(x, y, z, w, a) TracyPlotConfig(x, y, z, w, a)

#define RBCMessage(x, y) TracyMessage(x, y)
#define RBCMessageL(x) TracyMessageL(x)
#define RBCMessageC(x, y, z) TracyMessageC(x, y, z)
#define RBCMessageLC(x, y) TracyMessageLC(x, y)
#define RBCAppInfo(x, y) TracyAppInfo(x, y)

#define RBCAlloc(x, y) TracyAlloc(x, y)
#define RBCFree(x) TracyFree(x)
#define RBCSecureAlloc(x, y) TracySecureAlloc(x, y)
#define RBCSecureFree(x) TracySecureFree(x)

#define RBCAllocN(x, y, z) TracyAllocN(x, y, z)
#define RBCFreeN(x, y) TracyFreeN(x, y)
#define RBCSecureAllocN(x, y, z) TracySecureAllocN(x, y, z)
#define RBCSecureFreeN(x, y) TracySecureFreeN(x, y)

#define RBCZoneNamedS(x, y, z) ZoneNamedS(x, y, z)
#define RBCZoneNamedNS(x, y, z, w) ZoneNamedNS(x, y, z, w)
#define RBCZoneNamedCS(x, y, z, w) ZoneNamedCS(x, y, z, w)
#define RBCZoneNamedNCS(x, y, z, w, a) ZoneNamedNCS(x, y, z, w, a)

#define RBCZoneTransientS(x, y, z) ZoneTransientS(x, y, z)
#define RBCZoneTransientNS(x, y, z, w) ZoneTransientNS(x, y, z, w)

#define RBCZoneScopedS(x) ZoneScopedS(x)
#define RBCZoneScopedNS(x, y) ZoneScopedNS(x, y)
#define RBCZoneScopedCS(x, y) ZoneScopedCS(x, y)
#define RBCZoneScopedNCS(x, y, z) ZoneScopedNCS(x, y, z)

#define RBCAllocS(x, y, z) TracyAllocS(x, y, z)
#define RBCFreeS(x, y) TracyFreeS(x, y)
#define RBCSecureAllocS(x, y, z) TracySecureAllocS(x, y, z)
#define RBCSecureFreeS(x, y) TracySecureFreeS(x, y)

#define RBCAllocNS(x, y, z, w) TracyAllocNS(x, y, z, w)
#define RBCFreeNS(x, y, z) TracyFreeNS(x, y, z)
#define RBCSecureAllocNS(x, y, z, w) TracySecureAllocNS(x, y, z, w)
#define RBCSecureFreeNS(x, y, z) TracySecureFreeNS(x, y, z)

#define RBCMessageS(x, y, z) TracyMessageS(x, y, z)
#define RBCMessageLS(x, y) TracyMessageLS(x, y)
#define RBCMessageCS(x, y, z, w) TracyMessageCS(x, y, z, w)
#define RBCMessageLCS(x, y, z) TracyMessageLCS(x, y, z)

#define RBCSourceCallbackRegister(x, y) TracySourceCallbackRegister(x, y)
#define RBCParameterRegister(x, y) TracyParameterRegister(x, y)
#define RBCParameterSetup(x, y, z, w) TracyParameterSetup(x, y, z, w)
#define RBCIsConnected TracyIsConnected
#define RBCSetProgramName(x) TracySetProgramName(x)

#define RBCFiberEnter(x) TracyFiberEnter(x)
#define RBCFiberLeave TracyFiberLeave

#endif

#pragma endregion
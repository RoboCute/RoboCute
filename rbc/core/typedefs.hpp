/**************************************************************************/
/*  typedefs.h                                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

/**
 * Basic definitions and simple functions to be used everywhere.
 */

// IWYU pragma: begin_exports

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

// IWYU pragma: end_exports

// Turn argument to string constant:
// https://gcc.gnu.org/onlinedocs/cpp/Stringizing.html#Stringizing
#ifndef _STR
#define _STR(m_x) #m_x
#define _MKSTR(m_x) _STR(m_x)
#endif

// Should always inline no matter what.
#ifndef _ALWAYS_INLINE_
#if defined(__GNUC__)
#define _ALWAYS_INLINE_ __attribute__((always_inline)) inline
#elif defined(_MSC_VER)
#define _ALWAYS_INLINE_ __forceinline
#else
#define _ALWAYS_INLINE_ inline
#endif
#endif

// Should always inline, except in dev builds because it makes debugging harder,
// or `size_enabled` builds where inlining is actively avoided.
#ifndef _FORCE_INLINE_
#if defined(DEV_ENABLED) || defined(SIZE_EXTRA)
#define _FORCE_INLINE_ inline
#else
#define _FORCE_INLINE_ _ALWAYS_INLINE_
#endif
#endif

// Should never inline.
#ifndef _NO_INLINE_
#if defined(__GNUC__)
#define _NO_INLINE_ __attribute__((noinline))
#elif defined(_MSC_VER)
#define _NO_INLINE_ __declspec(noinline)
#else
#define _NO_INLINE_
#endif
#endif

// In some cases [[nodiscard]] will get false positives,
// we can prevent the warning in specific cases by preceding the call with a
// cast.
#ifndef _ALLOW_DISCARD_
#define _ALLOW_DISCARD_ (void)
#endif

// Windows badly defines a lot of stuff we'll never use. Undefine it.
#ifdef _WIN32
#undef min    // override standard definition
#undef max    // override standard definition
#undef ERROR  // override (really stupid) wingdi.h standard definition
#undef DELETE // override (another really stupid) winnt.h standard definition
#undef MessageBox // override winuser.h standard definition
#undef Error
#undef OK
#undef CONNECT_DEFERRED // override from Windows SDK, clashes with Object enum
#undef MONO_FONT
#endif

// Make room for our constexpr's below by overriding potential system-specific
// macros.
#undef SIGN
#undef MIN
#undef MAX
#undef CLAMP

template <typename T> constexpr const T SIGN(const T m_v) {
  return m_v > 0 ? +1.0f : (m_v < 0 ? -1.0f : 0.0f);
}

template <typename T, typename T2>
constexpr auto MIN(const T m_a, const T2 m_b) {
  return m_a < m_b ? m_a : m_b;
}

template <typename T, typename T2>
constexpr auto MAX(const T m_a, const T2 m_b) {
  return m_a > m_b ? m_a : m_b;
}

template <typename T, typename T2, typename T3>
constexpr auto CLAMP(const T m_a, const T2 m_min, const T3 m_max) {
  return m_a < m_min ? m_min : (m_a > m_max ? m_max : m_a);
}

// Generic swap template.
#ifndef SWAP
#define SWAP(m_x, m_y) std::swap((m_x), (m_y))
#endif // SWAP

// Like std::size, but without requiring any additional includes.
template <typename T, size_t SIZE>
constexpr size_t std_size(const T (&)[SIZE]) {
  return SIZE;
}

/* Functions to handle powers of 2 and shifting. */

// Returns `true` if a positive integer is a power of 2, `false` otherwise.
template <typename T> inline bool is_power_of_2(const T x) {
  return x && ((x & (x - 1)) == 0);
}

// Function to find the next power of 2 to an integer.
constexpr uint64_t next_power_of_2(uint64_t p_number) {
  if (p_number == 0) {
    return 0;
  }

  --p_number;
  p_number |= p_number >> 1;
  p_number |= p_number >> 2;
  p_number |= p_number >> 4;
  p_number |= p_number >> 8;
  p_number |= p_number >> 16;
  p_number |= p_number >> 32;

  return ++p_number;
}

constexpr uint32_t next_power_of_2(uint32_t p_number) {
  if (p_number == 0) {
    return 0;
  }

  --p_number;
  p_number |= p_number >> 1;
  p_number |= p_number >> 2;
  p_number |= p_number >> 4;
  p_number |= p_number >> 8;
  p_number |= p_number >> 16;

  return ++p_number;
}

// Function to find the previous power of 2 to an integer.
constexpr uint64_t previous_power_of_2(uint64_t p_number) {
  p_number |= p_number >> 1;
  p_number |= p_number >> 2;
  p_number |= p_number >> 4;
  p_number |= p_number >> 8;
  p_number |= p_number >> 16;
  p_number |= p_number >> 32;
  return p_number - (p_number >> 1);
}

constexpr uint32_t previous_power_of_2(uint32_t p_number) {
  p_number |= p_number >> 1;
  p_number |= p_number >> 2;
  p_number |= p_number >> 4;
  p_number |= p_number >> 8;
  p_number |= p_number >> 16;
  return p_number - (p_number >> 1);
}

// Function to find the closest power of 2 to an integer.
constexpr uint64_t closest_power_of_2(uint64_t p_number) {
  uint64_t nx = next_power_of_2(p_number);
  uint64_t px = previous_power_of_2(p_number);
  return (nx - p_number) > (p_number - px) ? px : nx;
}

constexpr uint32_t closest_power_of_2(uint32_t p_number) {
  uint32_t nx = next_power_of_2(p_number);
  uint32_t px = previous_power_of_2(p_number);
  return (nx - p_number) > (p_number - px) ? px : nx;
}

// Get a shift value from a power of 2.
constexpr int32_t get_shift_from_power_of_2(uint64_t p_bits) {
  for (uint64_t i = 0; i < (uint64_t)64; i++) {
    if (p_bits == (uint64_t)((uint64_t)1 << i)) {
      return i;
    }
  }

  return -1;
}

constexpr int32_t get_shift_from_power_of_2(uint32_t p_bits) {
  for (uint32_t i = 0; i < (uint32_t)32; i++) {
    if (p_bits == (uint32_t)((uint32_t)1 << i)) {
      return i;
    }
  }

  return -1;
}

template <typename T>
static _FORCE_INLINE_ T nearest_power_of_2_templated(T p_number) {
  --p_number;

  // The number of operations on x is the base two logarithm
  // of the number of bits in the type. Add three to account
  // for sizeof(T) being in bytes.
  constexpr size_t shift_steps =
      get_shift_from_power_of_2((uint64_t)sizeof(T)) + 3;

  // If the compiler is smart, it unrolls this loop.
  // If it's dumb, this is a bit slow.
  for (size_t i = 0; i < shift_steps; i++) {
    p_number |= p_number >> (1 << i);
  }

  return ++p_number;
}

// Function to find the nearest (bigger) power of 2 to an integer.
constexpr uint64_t nearest_shift(uint64_t p_number) {
  uint64_t i = 63;
  do {
    i--;
    if (p_number & ((uint64_t)1 << i)) {
      return i + (uint64_t)1;
    }
  } while (i != 0);

  return 0;
}

constexpr uint32_t nearest_shift(uint32_t p_number) {
  uint32_t i = 31;
  do {
    i--;
    if (p_number & ((uint32_t)1 << i)) {
      return i + (uint32_t)1;
    }
  } while (i != 0);

  return 0;
}

// constexpr function to find the floored log2 of a number
template <typename T> constexpr T floor_log2(T x) {
  return x < 2 ? x : 1 + floor_log2(x >> 1);
}

// Get the number of bits needed to represent the number.
// IE, if you pass in 8, you will get 4.
// If you want to know how many bits are needed to store 8 values however, pass
// in (8 - 1).
template <typename T> constexpr T get_num_bits(T x) { return floor_log2(x); }

// Swap 16, 32 and 64 bits value for endianness.
#if defined(__GNUC__)
#define BSWAP16(x) __builtin_bswap16(x)
#define BSWAP32(x) __builtin_bswap32(x)
#define BSWAP64(x) __builtin_bswap64(x)
#elif defined(_MSC_VER)
#define BSWAP16(x) _byteswap_ushort(x)
#define BSWAP32(x) _byteswap_ulong(x)
#define BSWAP64(x) _byteswap_uint64(x)
#else
static inline uint16_t BSWAP16(uint16_t x) { return (x >> 8) | (x << 8); }

static inline uint32_t BSWAP32(uint32_t x) {
  return ((x << 24) | ((x << 8) & 0x00FF0000) | ((x >> 8) & 0x0000FF00) |
          (x >> 24));
}

static inline uint64_t BSWAP64(uint64_t x) {
  x = (x & 0x00000000FFFFFFFF) << 32 | (x & 0xFFFFFFFF00000000) >> 32;
  x = (x & 0x0000FFFF0000FFFF) << 16 | (x & 0xFFFF0000FFFF0000) >> 16;
  x = (x & 0x00FF00FF00FF00FF) << 8 | (x & 0xFF00FF00FF00FF00) >> 8;
  return x;
}
#endif

// Generic comparator used in Map, List, etc.
template <typename T> struct Comparator {
  _ALWAYS_INLINE_ bool operator()(const T &p_a, const T &p_b) const {
    return (p_a < p_b);
  }
};

// Global lock macro, relies on the static Mutex::_global_mutex.
void _global_lock();
void _global_unlock();

struct _GlobalLock {
  _GlobalLock() { _global_lock(); }
  ~_GlobalLock() { _global_unlock(); }
};

#define GLOBAL_LOCK_FUNCTION _GlobalLock _global_lock_;

#if defined(__GNUC__)
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x) x
#define unlikely(x) x
#endif

#if defined(__GNUC__)
#define _PRINTF_FORMAT_ATTRIBUTE_2_0 __attribute__((format(printf, 2, 0)))
#define _PRINTF_FORMAT_ATTRIBUTE_2_3 __attribute__((format(printf, 2, 3)))
#else
#define _PRINTF_FORMAT_ATTRIBUTE_2_0
#define _PRINTF_FORMAT_ATTRIBUTE_2_3
#endif

// This is needed due to a strange OpenGL API that expects a pointer
// type for an argument that is actually an offset.
#define CAST_INT_TO_UCHAR_PTR(ptr) ((uint8_t *)(uintptr_t)(ptr))

// Home-made index sequence trick, so it can be used everywhere without the
// costly include of std::tuple.
// https://stackoverflow.com/questions/15014096/c-index-of-type-during-variadic-template-expansion
template <size_t... Is> struct IndexSequence {};

template <size_t N, size_t... Is>
struct BuildIndexSequence : BuildIndexSequence<N - 1, N - 1, Is...> {};

template <size_t... Is>
struct BuildIndexSequence<0, Is...> : IndexSequence<Is...> {};

// Limit the depth of recursive algorithms when dealing with Array/Dictionary
#define MAX_RECURSION 100

#pragma once

#include "typedefs.hpp"

namespace rbc {
template <typename T> class TypedArray;

template <typename T> class Ref;

enum class EPropertyHint {
  PROPERTY_HINT_NONE,
  PROPERTY_HINT_FILE,
  PROPERTY_HINT_DIR,
  PROPERTY_HINT_MAX
};

enum class EPropertyUsageFlags {
  PROPERTY_USAGE_NONE = 0,
  PROPERTY_USAGE_STORAGE,
  PROPERTY_USAGE_EDITOR
};

#define RBSOFTCLASS(m_class, m_inherits)                                       \
public:                                                                        \
  using self_type = m_class;                                                   \
  using super_type = m_inherits;                                               \
  static _FORCE_INLINE_ void *get_class_ptr_static() {                         \
    static int ptr;                                                            \
    return &ptr;                                                               \
  }

#define RBCLASS(m_class, m_inherit)                                            \
  RBSOFTCLASS(m_class, m_inherits)                                             \
private:                                                                       \
  void operator=(const m_class &p_rval) {}                                     \
  friend class ::ClassDB

class ClassDB;

class Object {
public:
  using self_type = Object;

public:
};

} // namespace rbc
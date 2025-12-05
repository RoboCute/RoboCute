from string import Template

DEFAULT_INDENT = " " * 4

CPP_INTERFACE_TEMPLATE = Template("""
//! This File is Generated From Python Def
//! Modifying This File will not affect final result, checkout src/rbc_meta/ for real defs
//! ================== GENERATED CODE BEGIN ==================

#pragma once

// GENERAL_INCLUDE BEGIN
// ========================================
#include <luisa/core/basic_types.h>
#include <luisa/core/basic_traits.h>
#include <luisa/core/stl.h>
#include <luisa/vstl/meta_lib.h>
#include <luisa/vstl/v_guid.h>
#include <rbc_core/enum_serializer.h>
#include <rbc_core/func_serializer.h>
#include <rbc_core/serde.h>

// ========================================
// GENRAL_INCLUDE CONTENT END


// EXTRA_INCLUDE BEGIN
// ========================================
${EXTRA_INCLUDE}
// ========================================
// EXTRA_INCLUDE CONTENT END


// MAIN GENERATED CONTENT BEGIN
// ========================================
${ENUMS_EXPR}

${STRUCTS_EXPR}
// ========================================
// MAIN GENERATED CONTENT END


//! ================== GENERATED CODE END ==================
""")

CPP_IMPL_TEMPLATE = """


"""

PYBIND_CPP_INTERFACE_TEMPLATE = """
//! This File is Generated From Python Def
//! Modifying This File will not affect final result, checkout src/rbc_meta/ for real defs
//! ================== GENERATED CODE BEGIN ==================
#include <pybind11/pybind11.h>
#include <luisa/core/basic_types.h>
#include <luisa/core/basic_traits.h>
#include <luisa/core/logging.h>
#include <rbc_core/serde.h>
#include "module_register.h"

namespace py = pybind11;

void $EXPORT_FUNC_NAME (py::module& m)

//! ================== GENERATED CODE END ==================
"""

PYBIND_CPP_IMPL_TEMPLATE = """

"""

PYBIND_PY_INTERFACE_TEMPLATE = """

"""

CPP_ENUM_TEMPLATE = Template("""
namespace ${NAMESPACE_NAME} {
                             
enum struct ${ENUM_NAME}: uint32_t {
${ENUM_KVPAIRS}
};
}// namespace ${NAMESPACE_NAME}

namespace rbc_rtti_detail {
template<>
struct is_rtti_type<${NAMESPACE_NAME}::${ENUM_NAME}> {
${INDENT}static constexpr bool value = true;
${INDENT}static constexpr const char *name{"${NAMESPACE_NAME}::${ENUM_NAME}"};
};

}// namespace rbc_rtti_detail
""")


CPP_ENUM_KVPAIR_TEMPLATE = Template("${INDENT}${KEY}${VALUE_EXPR}")

CPP_STRUCT_TEMPLATE = Template("""
namespace ${NAMESPACE_NAME} {
                               
struct ${STRUCT_NAME} : vstd::IOperatorNewBase {
// MEMBERS
${MEMBERS_EXPR}
                    
// BUILT-IN METHODS
${INDENT}${FUNC_API} static ${STRUCT_NAME}* _create_();
virtual void dispose() = 0;
virtual ~${STRUCT_NAME}() = default;  
                                
${INDENT}${SER_DECL}
${INDENT}${DESER_DECL}  
${INDENT}${RPC_FUNC_DECL}                                  
// USER-DEFINED METHODS
${USER_DEFINED_METHODS_DECL}  
                         
};
                               
}// namespace ${NAMESPACE_NAME}
                               
namespace rbc_rtti_detail {
template<>
struct is_rtti_type<${NAMESPACE_NAME}::${STRUCT_NAME}> {
${INDENT}static constexpr bool value = true;
${INDENT}static constexpr const char* name{"${NAMESPACE_NAME}::${STRUCT_NAME}"};
${INDENT}static constexpr uint8_t md5[16]{${MD5_DIGEST}};
};
                               
}} // rbc_rtti_detail
""")

CPP_STRUCT_CTOR_DECL_TEMPLATE = Template("${STRUCT_NAME}();")
CPP_STRUCT_DTOR_DECL_TEMPLATE = Template("~${STRUCT_NAME}();")
CPP_STRUCT_MEMBER_EXPR_TEMPLATE = Template(
    "${INDENT}${VAR_TYPE_NAME} ${MEMBER_NAME} {${INIT_EXPR}};"
)
CPP_STRUCT_SER_DECL_TEMPLATE = Template(
    "${FUNC_API} void rbc_objser(rbc::JsonSerializer& obj) const;"
)
CPP_STRUCT_DESER_DECL_TEMPLATE = Template(
    "${FUNC_API} void rbc_objdeser(rbc::JsonDeSerializer& obj);"
)
CPP_STRUCT_METHOD_DECL_TEMPLATE = Template(
    "${INDENT}virtual ${RET_TYPE} ${FUNC_NAME}(${ARGS_EXPR}) = 0;"
)

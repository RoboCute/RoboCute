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
${INDENT}virtual void dispose() = 0;
${INDENT}virtual ~${STRUCT_NAME}() = default;  
                                
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
                               
} // rbc_rtti_detail
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

# Python interface templates
PY_INTERFACE_CLASS_TEMPLATE = Template("""
class ${CLASS_NAME}:
${INIT_METHOD}
${DISPOSE_METHOD}
${METHODS_EXPR}
""")

PY_INIT_METHOD_TEMPLATE = Template("""${INDENT}def __init__(self):
${INDENT}${INDENT}self._handle = create__${STRUCT_NAME}__()
""")

PY_DISPOSE_METHOD_TEMPLATE = Template("""${INDENT}def __del__(self):
${INDENT}${INDENT}dispose__${STRUCT_NAME}__(self._handle)
""")

PY_METHOD_TEMPLATE = Template("""${INDENT}def ${METHOD_NAME}(self${ARGS_DECL}):
${INDENT}${INDENT}${RETURN_EXPR}${STRUCT_NAME}__${METHOD_NAME}__(self._handle${ARGS_CALL})
""")

# C++ implementation templates
CPP_IMPL_TEMPLATE = Template("""
//! This File is Generated From Python Def
//! Modifying This File will not affect final result, checkout src/rbc_meta/ for real defs
//! ================== GENERATED CODE BEGIN ==================

#include <rbc_core/serde.h>
${EXTRA_INCLUDES}

${ENUM_INITERS_EXPR}

${STRUCT_IMPLS_EXPR}

//! ================== GENERATED CODE END ==================
""")

CPP_ENUM_INITER_TEMPLATE = Template(
    "static rbc::EnumSerIniter emm_${DIGEST}{${INITER}};"
)

CPP_STRUCT_SER_IMPL_TEMPLATE = Template("""
${NAMESPACE_OPEN}
void ${CLASS_NAME}::rbc_objser(rbc::JsonSerializer& obj) const {
${STORE_STMTS}
}
""")

CPP_STRUCT_DESER_IMPL_TEMPLATE = Template("""
void ${CLASS_NAME}::rbc_objdeser(rbc::JsonDeSerializer& obj) {
${LOAD_STMTS}
}
${NAMESPACE_CLOSE}
""")

CPP_RPC_ARG_STRUCT_TEMPLATE = Template("""
struct ${ARG_STRUCT_NAME} {
${ARG_MEMBERS}
${INDENT}void rbc_objser(rbc::JsonSerializer &obj) const {
${SER_STMTS}
${INDENT}}
${INDENT}void rbc_objdeser(rbc::JsonDeSerializer &obj) {
${DESER_STMTS}
${INDENT}}
};
""")

CPP_RPC_ARG_MEMBER_TEMPLATE = Template("${INDENT}${ARG_TYPE} ${ARG_NAME};")

CPP_RPC_SER_STMT_TEMPLATE = Template("${INDENT}${INDENT}obj._store(${ARG_NAME});")

CPP_RPC_DESER_STMT_TEMPLATE = Template("${INDENT}${INDENT}obj._load(${ARG_NAME});")

CPP_RPC_CALL_LAMBDA_TEMPLATE = Template("""
(rbc::FuncSerializer::AnyFuncPtr)+[](${SELF_PARAM}void *args, void *ret_value) {
${ARGS_CAST}
${RET_CONSTRUCT}
${FUNC_CALL}
${RET_CLOSE}
}
""")

CPP_FUNC_SERIALIZER_TEMPLATE = Template("""
static rbc::FuncSerializer func_ser${HASH_NAME}{
    std::initializer_list<const char *>{{${FUNC_NAMES}}},
    std::initializer_list<rbc::FuncSerializer::AnyFuncPtr>{{${CALL_EXPRS}}},
    std::initializer_list<rbc::HeapObjectMeta>{{${ARG_METAS}}},
    std::initializer_list<rbc::HeapObjectMeta>{{${RET_METAS}}},
    std::initializer_list<bool>{{${IS_STATICS}}}
};
""")

# C++ client interface templates
CPP_CLIENT_INTERFACE_TEMPLATE = Template("""
//! This File is Generated From Python Def
//! Modifying This File will not affect final result, checkout src/rbc_meta/ for real defs
//! ================== GENERATED CODE BEGIN ==================

#pragma once
#include <luisa/core/basic_types.h>
#include <luisa/core/basic_traits.h>
#include <luisa/core/stl.h>
#include <luisa/vstl/meta_lib.h>
#include <luisa/vstl/v_guid.h>
#include <rbc_core/enum_serializer.h>
#include <rbc_core/func_serializer.h>
#include <rbc_core/serde.h>
${RPC_INCLUDE}

${EXTRA_INCLUDES}

${CLIENT_CLASSES_EXPR}

//! ================== GENERATED CODE END ==================
""")

CPP_CLIENT_CLASS_TEMPLATE = Template("""
${NAMESPACE_OPEN}
struct ${CLASS_NAME}Client {
${METHOD_DECLS}
};
${NAMESPACE_CLOSE}
""")

CPP_CLIENT_METHOD_DECL_TEMPLATE = Template(
    "${INDENT}static ${RET_TYPE} ${METHOD_NAME}(rbc::RPCCommandList &${SELF_PARAM}${ARGS_DECL});"
)

# C++ client implementation templates
CPP_CLIENT_IMPL_TEMPLATE = Template("""
//! This File is Generated From Python Def
//! Modifying This File will not affect final result, checkout src/rbc_meta/ for real defs
//! ================== GENERATED CODE BEGIN ==================

${EXTRA_INCLUDES}

${CLIENT_IMPLS_EXPR}

//! ================== GENERATED CODE END ==================
""")

CPP_CLIENT_METHOD_IMPL_TEMPLATE = Template("""
${NAMESPACE_OPEN}
${RET_TYPE} ${CLASS_NAME}Client::${METHOD_NAME}(rbc::RPCCommandList &${JSON_SER_NAME}${SELF_PARAM}${ARGS_DECL}) {
${INDENT}${JSON_SER_NAME}.add_functioon("${FUNC_HASH}"${SELF_ARG});
${ADD_ARGS_STMTS}
${RETURN_STMT}
}
${NAMESPACE_CLOSE}
""")

CPP_CLIENT_ADD_ARG_STMT_TEMPLATE = Template(
    "${INDENT}${JSON_SER_NAME}.add_arg(${ARG_NAME});"
)

CPP_CLIENT_RETURN_STMT_TEMPLATE = Template(
    "${INDENT}return ${JSON_SER_NAME}.return_value<${RET_TYPE}>();"
)

# Pybind templates
PYBIND_CODE_TEMPLATE = Template("""
//! This File is Generated From Python Def
//! Modifying This File will not affect final result, checkout src/rbc_meta/ for real defs
//! ================== GENERATED CODE BEGIN ==================

#include <pybind11/pybind11.h>
#include <luisa/core/basic_types.h>
#include <luisa/core/basic_traits.h>
#include <luisa/core/logging.h>
#include <rbc_core/serde.h>
#include "module_register.h"
${EXTRA_INCLUDES}

namespace py = pybind11;
void ${EXPORT_FUNC_NAME}(py::module& m) {
${ENUM_BINDINGS}
${STRUCT_BINDINGS}
}

static ModuleRegister ${EXPORT_FUNC_NAME}_(${EXPORT_FUNC_NAME});

${IMPL_CODE}

//! ================== GENERATED CODE END ==================
""")

PYBIND_ENUM_BINDING_TEMPLATE = Template("""
${INDENT}py::enum_<${ENUM_NAME}>(m, "${CLASS_NAME}")
${ENUM_VALUES}
${INDENT};
""")

PYBIND_ENUM_VALUE_TEMPLATE = Template(
    '${INDENT}${INDENT}.value("${VALUE_NAME}", ${ENUM_NAME}::${VALUE_NAME})'
)

PYBIND_CREATE_FUNC_TEMPLATE = Template("""
${INDENT}m.def("${CREATE_NAME}", []() -> void* {
${INDENT}${INDENT}return ${STRUCT_NAME}::_create_();
${INDENT}});
""")

PYBIND_DISPOSE_FUNC_TEMPLATE = Template("""
${INDENT}m.def("${DISPOSE_NAME}", [](void* ${PTR_NAME}) {
${INDENT}${INDENT}static_cast<${STRUCT_NAME}*>(${PTR_NAME})->dispose();
${INDENT}});
""")

PYBIND_METHOD_FUNC_TEMPLATE = Template("""
${INDENT}m.def("${METHOD_NAME}", [](void* ${PTR_NAME}${ARGS_DECL})${RET_TYPE} {
${INDENT}${INDENT}${RETURN_EXPR}static_cast<${STRUCT_NAME}*>(${PTR_NAME})->${METHOD_NAME_CALL}(${ARGS_CALL})${RETURN_CLOSE};
${INDENT}});
""")

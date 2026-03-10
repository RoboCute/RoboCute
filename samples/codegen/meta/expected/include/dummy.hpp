
//! This File is Generated From Python Def
//! Modifying This File will not affect final result, checkout src/rbc_meta/ for real defs
//! ================== GENERATED CODE BEGIN ==================

#pragma once

// GENERAL_INCLUDE BEGIN
// ========================================           
#include <rbc_core/base.h>
#include <rbc_core/enum_serializer.h>
#include <rbc_core/func_serializer.h>
#include <rbc_core/serde.h>

// ========================================
// GENRAL_INCLUDE CONTENT END


// EXTRA_INCLUDE BEGIN
// ========================================

// ========================================
// EXTRA_INCLUDE CONTENT END


// MAIN GENERATED CONTENT BEGIN
// ========================================



namespace rbc {
                               
struct RBC_RUNTIME_API DummyMeta : ::rbc::RBCStruct {
// MEMBERS
    uint32_t vertex_count {};
    bool normal {};
    bool tangent {};
    uint32_t uv_count {};
    luisa::vector<uint32_t> submesh_offset {};
                    
// BUILT-IN METHODS

                                
    void rbc_objser(rbc::JsonSerializer& obj) const;
    void rbc_objdeser(rbc::JsonDeSerializer& obj);  
                               
// RPC METHODS
                                  
// USER-DEFINED METHODS
  
                         
};
                               
}// namespace rbc
                               
namespace rbc_rtti_detail {
template<>
struct is_rtti_type<rbc::DummyMeta> {
    static constexpr bool value = true;
    static constexpr const char* name{"rbc::DummyMeta"};
    static constexpr uint8_t md5[16]{202, 77, 248, 30, 17, 223, 24, 180, 155, 135, 101, 103, 200, 69, 135, 238};
};
                               
} // rbc_rtti_detail

// ========================================
// MAIN GENERATED CONTENT END


//! ================== GENERATED CODE END ==================

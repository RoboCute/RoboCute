
//! This File is Generated From Python Def
//! Modifying This File will not affect final result, checkout src/rbc_meta/ for real defs
//! ================== GENERATED CODE BEGIN ==================

#include <rbc_core/serde.h>
#include <dummy.hpp>




namespace rbc {
void DummyMeta::rbc_objser(rbc::JsonSerializer& obj) const {
        obj._store(this->vertex_count, "vertex_count");
        obj._store(this->normal, "normal");
        obj._store(this->tangent, "tangent");
        obj._store(this->uv_count, "uv_count");
        obj._store(this->submesh_offset, "submesh_offset");
}
}// rbc


namespace rbc {
void DummyMeta::rbc_objdeser(rbc::JsonDeSerializer& obj) {
        obj._load(this->vertex_count, "vertex_count");
        obj._load(this->normal, "normal");
        obj._load(this->tangent, "tangent");
        obj._load(this->uv_count, "uv_count");
        obj._load(this->submesh_offset, "submesh_offset");
}
}// rbc


//! ================== GENERATED CODE END ==================

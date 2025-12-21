#pragma once
/**
 * @file rbc_core/rttr.h
 * @author sailing-innocent
 * @brief 当前的RTTR主要为了Resource系统服务，并不完整，只保留了基础的ctor/dtor和cast功能
 */
#include <luisa/vstl/meta_lib.h>
#include <rbc_core/type_info.h>

namespace rbc {

struct RTTRRecordData;

struct RTTRBaseData {
    using CastFunc = void *(*)(void *);
    vstd::Guid type_id;
    CastFunc cast_to_base;

    template<typename T, typename Base>
    inline static RTTRBaseData *New() {
        auto result = new RTTRBaseData();
        result->type_id = rbc_rtti_detail::is_rtti_type<Base>::get_md5();
        result->cast_to_base = +[](void *p) -> void * {
            return static_cast<Base *>(reinterpret_cast<T *>(p));
        };
        return result;
    }
};

enum struct ERTTRAccessLevel {
    Public,
    Private,
    Protected
};

struct RTTRParamData {
    uint64_t index = 0;
    luisa::string name = {};
};

struct RTTRCtorData {
    RTTRRecordData *owner = nullptr;
    void *native_invoke = nullptr;

    inline ~RTTRCtorData() {
    }
};

using DtorInvoker = void *(*)(void *);

struct RTTRDtorData {
    RTTRRecordData *owner = nullptr;
    DtorInvoker native_invoker;
};

struct RTTRRecordData {
    luisa::string name;
    luisa::vector<luisa::string> name_spaces;
    vstd::Guid type_id;
    size_t size;
    size_t alignment;

    // bases
    luisa::vector<RTTRBaseData *> bases_data = {};
    luisa::unordered_map<vstd::Guid, uint64_t> flatten_base_offset = {};

    // ctor & dtor
    luisa::vector<RTTRCtorData *> ctor_data = {};
    RTTRDtorData dtor_data = {};

    // methods & field
    // static methods & field
    // extern method
    // flag & attributes
    inline ~RTTRRecordData() {
        for (auto base : bases_data) {
            delete[] base;
        }
        for (auto ctor : ctor_data) {
            delete[] ctor;
        }
    }
};

template<typename T>
struct RTTRRecordBuilder {
    inline RTTRRecordBuilder(RTTRRecordData *data);
};

struct RTTRType final {
};

struct IRTTRBasic : vstd::IOperatorNewBase {
    virtual ~IRTTRBasic();
};

struct RTTRType;
using RTTRTypeLoaderFunc = void (*)(RTTRType *type);
// register
void rttr_register_type_loader(const vstd::Guid &guid, RTTRTypeLoaderFunc load_func);
// get
RTTRType *rttr_get_type_from_guid(const vstd::Guid &guid);
void rttr_load_all_types(bool silent = false);
void rttr_unload_all_types(bool silent = false);

template<typename T>
inline RTTRType *type_of() {
    return rttr_get_type_from_guid(type_id_of<T>());
}

}// namespace rbc
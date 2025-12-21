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
    TypeInfo type_info = TypeInfo::invalid();
    CastFunc cast_to_base = nullptr;

    template<typename T, typename Base>
    inline static RTTRBaseData *New() {
        auto result = new RTTRBaseData();
        result->type_info = TypeInfo::get<Base>();
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
    TypeInfo type_info;
    size_t size;
    size_t alignment;

    // bases
    luisa::vector<RTTRBaseData *> bases_data = {};
    luisa::unordered_map<TypeInfo, uint64_t> flatten_base_offset = {};

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

// Helper to Wrap and Export Class Methods into lambda
struct RTTRExportHelper {
    template<typename T, typename... Args>
    inline static void *export_ctor() {
        auto result = +[](void *p, Args... args) {
            new (p) T(std::forward<Args>(args)...);
        };
        return reinterpret_cast<void *>(result);
    }

    template<typename T>
    inline static DtorInvoker export_dtor() {
        auto result = +[](void *p) {
            reinterpret_cast<T *>(p)->~T();
        };
        return result;
    }
    // TODO: export methods
};

struct RTTRCtorBuilder {
private:
    RTTRCtorData *_data;
public:
    explicit inline RTTRCtorBuilder(RTTRCtorData *data) : _data(data) {}
    RTTRCtorBuilder(const RTTRCtorBuilder &) = delete;
    RTTRCtorBuilder &operator=(const RTTRCtorBuilder &) = delete;
};

template<typename T>
struct RTTRRecordBuilder {
private:
    RTTRRecordData *_data;
public:
    explicit inline RTTRRecordBuilder(RTTRRecordData *data);

    template<typename... Args>
    inline RTTRCtorBuilder ctor() {
        {
            // if found, return
        }

        auto ctor_data = new RTTRCtorData();
        ctor_data->owner = _data;
        ctor_data->native_invoke = RTTRExportHelper::export_ctor<T, Args...>();

        return RTTRCtorBuilder{ctor_data};
    }
};

struct RTTRType final {
    // caster
    [[nodiscard]] bool is_based_on(TypeInfo type_info) const;
    [[nodiscard]] void *cast_to_base(TypeInfo type_info) const;

    // ctor & dtor
    [[nodiscard]] vstd::optional<RTTRDtorData> dtor_data() const;
    [[nodiscard]] DtorInvoker dtor_invoker() const;
    void invoke_dtor(void *p) const;
    [[nodiscard]] RTTRCtorData *find_default_ctor() const;

    // alloc
    [[nodiscard]] void *alloc(uint64_t count = 1) const;
    void free(void *p) const;
};

struct IRTTRBasic : vstd::IOperatorNewBase {
    virtual ~IRTTRBasic();

    // RTTR Caster
    // template<typename TO>
    // TO *rttr_cast();
    // template<typename TO>
    // const TO *rttr_cast() const;
    // template<typename TO>
    // [[nodiscard]] bool rttr_is() const noexcept;
    // [[nodiscard]] bool rttr_is(const TypeInfo &info) const;

    // RTTR SerDe
    // template<std::derived_from<IRTTRBasic> T>
    // static T *SerdeReadWithTypeAs();
};

struct RTTRType;
using RTTRTypeLoaderFunc = void (*)(RTTRType *type);
// register
void rttr_register_type_loader(const TypeInfo &info, RTTRTypeLoaderFunc load_func);
// get
RTTRType *rttr_get_type_from_info(const TypeInfo &info);
void rttr_load_all_types(bool silent = false);
void rttr_unload_all_types(bool silent = false);

template<typename T>
inline RTTRType *type_of() {
    return rttr_get_type_from_info(TypeInfo::get<T>());
}

}// namespace rbc
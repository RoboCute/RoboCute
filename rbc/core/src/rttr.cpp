#include "rbc_core/rttr.h"

namespace rbc {

// singleton to register RTTRType for reflection
struct RTTRTypeRegistry {
    luisa::unordered_map<TypeInfo, RTTRType *> _loaded_types;

public:
    static RTTRTypeRegistry &get() {
        static RTTRTypeRegistry g_instance;
        return g_instance;
    }
    inline ~RTTRTypeRegistry() {
        unload_all_types_no_lock(true);
    }
    inline void unload_all_types_no_lock(bool silent) {
        uint64_t sum_unload_types_count = 0;
        for (auto &type : _loaded_types) {
            delete[] type.second;
            ++sum_unload_types_count;
        }
        _loaded_types.clear();
    }

    inline RTTRType *get_type_from_info_no_lock(const TypeInfo &info) {
        auto loaded_result = _loaded_types.find(info);
        if (loaded_result != _loaded_types.end()) {
            return loaded_result->second;
        } else {
            return load_type_from_info_no_lock(info);
        }
        return nullptr;
    }

    inline RTTRType *load_type_from_info_no_lock(const TypeInfo &info) {
    }

    inline RTTRType *get_type_from_guid(const TypeInfo &info) {
    }

    inline void load_all_types_no_lock(bool silent) {
    }

    inline void load_all_types(bool silent) {
    }

    inline void unload_all_types(bool silent) {
        // TODO: unload all types with mutex
    }
};

}// namespace rbc
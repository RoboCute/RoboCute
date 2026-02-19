#pragma once
#include <luisa/ast/type.h>
#include <rbc_core/base.h>
#include <module_register.h>
using namespace luisa;
using namespace luisa::compute;
struct ArgTypes : rbc::RBCStruct {
    luisa::vector<Type const *> types;
    void check_same(ArgTypes const &other_);
    void append(Type const *ptr);
    Type const *get(uint64_t index) {
        return types[index];
    }
    auto size() const {
        return types.size();
    }
};
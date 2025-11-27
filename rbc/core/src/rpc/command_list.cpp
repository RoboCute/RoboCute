#include <rbc_core/rpc/future.h>
#include <rbc_core/rpc/command_list.h>
#include <rbc_core/func_serializer.h>
#include <luisa/vstl/stack_allocator.h>
namespace rbc {
void RPCCommandList::_add_future(RPCRetValueBase *future) {
    future->next = std::move(return_values);
    return_values = future;
}

void RPCCommandList::add_functioon(char const *name, void *handle) {
    _func_count += 1;
    arg_ser.add(name);
    arg_ser.add((uint64_t)handle);
}
luisa::BinaryBlob RPCCommandList::commit_commands() {
    return arg_ser.write_to();
}

void RPCCommandList::readback(
    vstd::function<RC<RPCRetValueBase>(uint64_t)> const &get_cmdlist,
    luisa::BinaryBlob &&serialized_return_values) {
    JsonDeSerializer ret_deser(luisa::string_view{
        reinterpret_cast<char const *>(serialized_return_values.data()),
        serialized_return_values.size()});
    uint64_t handle;
    LUISA_DEBUG_ASSERT(ret_deser.read(handle));

    // auto ret_value_sizes = ret_deser.last_array_size();
    // LUISA_ASSERT(ret_values.size() == ret_value_sizes);
    // for (auto &i : ret_values) {
    //     i.deser_func(i.storage, &ret_deser);
    // }
}

luisa::BinaryBlob RPCCommandList::server_execute(
    uint64_t call_count,
    luisa::BinaryBlob &&serialized_args) {
    JsonDeSerializer arg_deser(luisa::string_view{
        reinterpret_cast<char const *>(serialized_args.data()),
        serialized_args.size()});
    vstd::VEngineMallocVisitor alloc_callback;
    vstd::StackAllocator alloc(256, &alloc_callback, 2);
    JsonSerializer ret_ser{true};
    vstd::vector<std::pair<void *, vstd::func_ptr_t<void(void *)>>> ret_deleters;
    for (uint64_t i = 0; i < call_count; ++i) {
        luisa::string func_hash;
        uint64_t self;
        if (!(arg_deser.read(func_hash))) {
            break;
        }
        LUISA_DEBUG_ASSERT(arg_deser.read(self));
        auto call_meta = FuncSerializer::get_call_meta(func_hash);
        LUISA_DEBUG_ASSERT(call_meta);
        void *arg = nullptr;
        // allocate
        auto const &args_meta = call_meta->args_meta;
        if (args_meta) {
            auto chunk = alloc.allocate(args_meta.size, args_meta.alignment);
            arg = reinterpret_cast<void *>(chunk.handle + chunk.offset);
            if (args_meta.default_ctor) {
                args_meta.default_ctor(arg);
            } else if (args_meta.is_trivial_constructible) {
                std::memset(arg, 0, args_meta.size);
            }
            LUISA_DEBUG_ASSERT(args_meta.json_reader);
            args_meta.json_reader(arg, &arg_deser);
        }
        void *ret_ptr{};
        auto const &ret_value_meta = call_meta->ret_value_meta;
        if (ret_value_meta) {
            LUISA_DEBUG_ASSERT(ret_value_meta.json_writer);
            auto chunk = alloc.allocate(ret_value_meta.size, ret_value_meta.alignment);
            ret_ptr = reinterpret_cast<void *>(chunk.handle + chunk.offset);
            if (ret_value_meta.deleter) {
                ret_deleters.emplace_back(ret_ptr, ret_value_meta.deleter);
            }
        }
        // call
        call_meta->func(reinterpret_cast<void *>(self), arg, ret_ptr);
        if (ret_ptr)
            ret_value_meta.json_writer(ret_ptr, &ret_ser);

        // destructor
        if (arg && args_meta.deleter) {
            args_meta.deleter(arg);
        }
    }
    auto blob = ret_ser.write_to();
    for (auto &i : ret_deleters) {
        i.second(i.first);
    }
    return blob;
}
RPCCommandList::RPCCommandList(uint64_t custom_handle) : arg_ser(true) {
    arg_ser.add(custom_handle);
}
RPCCommandList::~RPCCommandList() {}
}// namespace rbc
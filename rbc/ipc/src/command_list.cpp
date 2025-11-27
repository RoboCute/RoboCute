#include <rbc_ipc/future.h>
#include <rbc_ipc/command_list.h>
#include <rbc_core/func_serializer.h>
#include <luisa/vstl/stack_allocator.h>
#include <rbc_ipc/rpc_server.h>
namespace rbc {
void RPCCommandList::_add_future(RPCRetValueBase *future) {
    future->next = std::move(return_values);
    return_values = future;
}

void RPCCommandList::add_functioon(char const *name, void *handle) {
    _func_count += 1;
    auto guid = vstd::Guid::TryParseGuid(name);
    LUISA_DEBUG_ASSERT(guid);
    arg_ser._store(*guid);
    arg_ser.add((uint64_t)handle);
}
auto RPCCommandList::commit_commands() -> Commit {
    auto d = vstd::scope_exit([&] {
        vstd::reset(arg_ser);
        _func_count = 0;
        _arg_count = 0;
    });
    return {
        arg_ser.write_to(),
        std::move(return_values)};
}

void RPCCommandList::readback(
    vstd::function<RC<RPCRetValueBase>(uint64_t)> const &get_retvalue_handle,
    luisa::span<std::byte const> data) {
    JsonDeSerializer ret_deser(luisa::string_view{
        reinterpret_cast<char const *>(data.data()),
        data.size()});
    uint64_t handle;
    LUISA_DEBUG_ASSERT(ret_deser.read(handle));
    auto ret_values = get_retvalue_handle(handle);

    // auto ret_value_sizes = ret_deser.last_array_size();
    // LUISA_ASSERT(ret_values.size() == ret_value_sizes);
    while (ret_values) {
        ret_values->deser_func(ret_values, &ret_deser);
        ret_values = ret_values->next;
    }
}

luisa::BinaryBlob RPCCommandList::server_execute(
    uint64_t call_count,
    luisa::span<std::byte const> data) {
    JsonDeSerializer arg_deser(luisa::string_view{
        reinterpret_cast<char const *>(data.data()),
        data.size()});
    vstd::VEngineMallocVisitor alloc_callback;
    vstd::StackAllocator alloc(256, &alloc_callback, 2);
    JsonSerializer ret_ser{true};
    vstd::vector<std::pair<void *, vstd::func_ptr_t<void(void *)>>> ret_deleters;
    bool has_ret_value{false};
    uint64_t handle;
    LUISA_DEBUG_ASSERT(arg_deser.read(handle));
    ret_ser.add(handle);

    for (uint64_t i = 0; i < call_count; ++i) {
        vstd::Guid func_hash;
        uint64_t self;
        if (!(arg_deser._load(func_hash))) {
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
        if (ret_ptr) {
            ret_value_meta.json_writer(ret_ptr, &ret_ser);
            has_ret_value = true;
        }

        // destructor
        if (arg && args_meta.deleter) {
            args_meta.deleter(arg);
        }
    }
    if (!has_ret_value) return {};
    auto blob = ret_ser.write_to();
    for (auto &i : ret_deleters) {
        i.second(i.first);
    }
    return blob;
}
RPCCommandList::RPCCommandList(RPCServer *server, uint thread_id, uint64_t custom_handle)
    : _server(server),
      arg_ser(true),
      _thread_id{thread_id},
      _custom_id{custom_handle} {
    arg_ser.add(custom_handle);
}
RPCCommandList::~RPCCommandList() {
    if (_func_count > 0) [[unlikely]] {
        LUISA_ERROR("cmdlist disposed without commit.");
    }
}
}// namespace rbc
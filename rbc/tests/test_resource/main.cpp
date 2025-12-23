#include "dummy_json_resource.h"
#include <rbc_core/runtime_static.h>
#include <rbc_world_v2/resource_loader.h>
#include <luisa/core/fiber.h>
using namespace rbc;
int main(int argc, char *argv[]) {
    luisa::fiber::scheduler scheduler;
    RuntimeStaticBase::init_all();
    world::init_world();
    auto meta_path = luisa::filesystem::path{argv[0]}.parent_path() / "dummy_scene";
    luisa::filesystem::remove_all(meta_path);
    world::init_resource_loader(meta_path);
    auto dispose_runtime_static = vstd::scope_exit([] {
        world::destroy_world();
        RuntimeStaticBase::dispose_all();
    });

    vstd::Guid root_guid;
    // write
    {
        // // create dummy
        RC<DummyJsonResource> a{world::create_object<DummyJsonResource>()};
        RC<DummyJsonResource> b{world::create_object<DummyJsonResource>()};
        RC<DummyJsonResource> c{world::create_object<DummyJsonResource>()};
        RC<DummyJsonResource> d{world::create_object<DummyJsonResource>()};

        d->create_empty({}, "D");
        c->create_empty({d}, "C");
        b->create_empty({d}, "B");
        a->create_empty({b, c}, "A");
        root_guid = a->guid();

        auto write_to = [&](RC<DummyJsonResource> &res) {
            // write binary
            auto bin = meta_path / (luisa::string{res->value()} + ".dummy");
            res->set_path(bin, 0);
            res->save_to_path();
            // write serialize_data
            world::register_resource(res.get());
        };
        write_to(a);
        write_to(b);
        write_to(c);
        write_to(d);
    }

    // read
    {
        auto dummy_ref = world::load_resource(root_guid);
        LUISA_ASSERT(dummy_ref->is_type_of(TypeInfo::get<DummyJsonResource>()));
        auto dummy_ptr = static_cast<DummyJsonResource *>(dummy_ref.get());
        dummy_ptr->wait_load_finished();
    }
}
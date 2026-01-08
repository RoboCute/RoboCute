#include "dummy_resource.h"
#include <rbc_core/runtime_static.h>
#include <luisa/core/fiber.h>
using namespace rbc;
int main(int argc, char *argv[]) {
    luisa::fiber::scheduler scheduler;
    RuntimeStaticBase::init_all();
    auto meta_path = luisa::filesystem::path{argv[0]}.parent_path() / "dummy_scene";
    luisa::filesystem::remove_all(meta_path);
    world::init_world(meta_path);
    auto dispose_runtime_static = vstd::scope_exit([] {
        world::destroy_world();
        RuntimeStaticBase::dispose_all();
    });

    vstd::Guid root_guid;
    // write
    {
        // // create dummy
        RC<DummyResource> a{world::create_object<DummyResource>()};
        RC<DummyResource> b{world::create_object<DummyResource>()};
        RC<DummyResource> c{world::create_object<DummyResource>()};
        RC<DummyResource> d{world::create_object<DummyResource>()};

        d->create_empty({}, "D");
        c->create_empty({d}, "C");
        b->create_empty({d}, "B");
        a->create_empty({b, c}, "A");
        root_guid = a->guid();

        auto write_to = [&](RC<DummyResource> &res) {
            // write binary
            auto bin = meta_path / (luisa::string{res->value()} + ".dummy");
            res->save_to_path();
            // write serialize_data
            world::register_resource_meta(res.get());
        };
        write_to(a);
        write_to(b);
        write_to(c);
        write_to(d);
    }

    // read
    {
        auto dummy_ref = world::load_resource(root_guid);
        LUISA_ASSERT(dummy_ref->is_type_of(TypeInfo::get<DummyResource>()));
        auto dummy_ptr = static_cast<DummyResource *>(dummy_ref.get());

        // wait dummy_ptr
        {
            auto wait_skybox = [&]() -> rbc::coroutine {
                co_await dummy_ptr->await_loading();
            }();
            while (!wait_skybox.done()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                wait_skybox.resume();
            }
        }
    }
}
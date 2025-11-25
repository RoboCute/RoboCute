#include <iostream>
#include "rbc_world/async_resource_loader.h"
#include "rbc_world/mesh_loader.h"

int main() {
    std::cout << "Main" << std::endl;
    rbc::AsyncResourceLoader loader;
    loader.initialize(4);
    loader.register_loader(rbc::ResourceType::Mesh, rbc::create_mesh_loader);
    int i = 0;
    rbc::ResourceID rid = 1;
    auto res = loader.load_resource(rid, static_cast<uint32_t>(rbc::ResourceType::Mesh), "D:/ws/data/assets/models/cube.obj", "");

    while (!loader.is_loaded(rid)) {

    }

    std::cout << "Loaded Mesh" << std::endl;
    auto state = loader.get_state(rid);
    auto size = loader.get_resource_size(rid);
    auto *mesh = reinterpret_cast<rbc::Mesh *>(loader.get_resource_data(rid));
    std::cout << "Loaded Mesh with " << mesh->vertices.size() << " verts and " << mesh->indices.size() << " indices" << std::endl;

    loader.shutdown();
    return 0;
}
#include <iostream>
#include "rbc_world/async_resource_loader.h"

int main() {
    std::cout << "Main" << std::endl;
    rbc::AsyncResourceLoader loader;
    loader.initialize(4);

    loader.shutdown();
    return 0;
}
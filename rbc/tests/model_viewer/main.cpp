#include <argparse/argparse.hpp>
#include <luisa/core/fiber.h>

int main(int argc, char *argv[]) {
    argparse::ArgumentParser program("rbc_model_viewer", "0.1.0");
    program.add_argument("project_dir").help("Path to project directory (where rbc_project.json exists)");
    try {
        program.parse_args(argc, argv);
    } catch (const std::exception &err) {
        std::cerr << err.what() << std::endl;
        std::cerr << program;
        return 1;
    }
    auto project_dir = program.get<std::string>("project_dir");
    LUISA_INFO("Launching Model Viewer for project: ", project_dir);

    // luisa::fiber::scheduler scheduler;
    return 0;
}
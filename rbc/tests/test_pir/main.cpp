#include <luisa/ast/function_builder.h>
#include <luisa/ast/function.h>

#include <nlohmann/json.hpp>
#include <fstream>
#include "ParticleSystem/ir/parser.h"
#include "ParticleSystem/ir/module.h"
#include "ParticleSystem/ir/func_builder.h"

using namespace luisa;
using namespace luisa::compute;
int main(int argc, char *argv[]) {
    using namespace d6::ps;
    IRParser parser;
    parser.load_file("D:/repos/sail_engine/assets/source/pir/simple.pir");
    eastl::unique_ptr<IRModule>
        ir{parser.parse()};

    luisa::string dbg_ir_str = ir->dump();
    LUISA_INFO("IR:\n{}", dbg_ir_str);

    // BUILD FUNCTION ACCORDING TO THE IR
    IRFuncBuilder builder{};
    ;
    luisa::string f_json = to_json(builder.build_func(*ir)->function());
    LUISA_INFO("Function JSON:\n{}", f_json);

    std::ofstream file("D:/repos/sail_engine/assets/source/pir/simple_ir.json");
    file << f_json;
    file.close();
    return 0;
}
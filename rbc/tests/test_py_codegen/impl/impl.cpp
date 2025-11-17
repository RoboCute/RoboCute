#include "example_module.h"
#include <luisa/core/dll_export.h>
#include <luisa/core/logging.h>
struct MyStructImpl : MyStruct {
    MyStructImpl(){
        LUISA_INFO("created");
    }
    void dispose() override
    {
        LUISA_INFO("Dispose");
        delete this;
    }
    void set_pos(luisa::double3 pos, int32_t idx) override
    {
        LUISA_INFO("set_pos {}", pos, idx);
    }
    luisa::string get_pos(luisa::string_view pos) override
    {
        LUISA_INFO("get_pos {}", pos);
        return luisa::string{ pos } + "_return";
    }
};
LUISA_EXPORT_API MyStructImpl* create_MyStruct()
{
    return new MyStructImpl();
}
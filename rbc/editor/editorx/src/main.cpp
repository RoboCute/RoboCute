#include <iostream>
#include <rbc_config.h>

#ifdef RBC_EDITOR_MODULE
LUISA_EXPORT_API int dll_main(int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
    std::cout << "Hello" << std::endl;
    return 0;
}
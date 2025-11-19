#pragma once
#ifndef __SHADER_LANG__
#define HOST_CODE(...) __VA_ARGS__
#define SHADER_CODE(...)
#else
#define HOST_CODE(...)
#define SHADER_CODE(...) __VA_ARGS__
#endif
#define TINYGLTF_IMPLEMENTATION
// Don't define STB_IMAGE_IMPLEMENTATION here - let the project's stb-image library provide it
// This avoids duplicate symbol definitions while still allowing tinygltf to use stb_image
#include "tiny_gltf.h"

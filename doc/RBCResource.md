# 自定义资源类型

#### .rbm (Mesh)
```
Header (20 bytes):
  uint32 magic ('RBM\0')
  uint32 version
  uint32 vertex_count
  uint32 index_count
  uint32 flags
Data:
  Vertex[] vertices
  uint32[] indices
```

#### .rbt (Texture)
```
Header (24 bytes):
  uint32 magic ('RBT\0')
  uint32 version
  uint32 width
  uint32 height
  uint32 depth
  uint32 mip_levels
  uint8 format
  uint8[3] reserved
Data:
  uint64 data_size
  uint8[] pixel_data
```

#### .rbm (Material)
```
Header (8 bytes):
  uint32 magic ('RBMT')
  uint32 version
Data:
  uint32 name_length
  char[] name
  float[4] base_color
  float metallic
  float roughness
  float[3] emissive
  uint32 base_color_texture
  uint32 metallic_roughness_texture
  uint32 normal_texture
  uint32 emissive_texture
  uint32 occlusion_texture
```
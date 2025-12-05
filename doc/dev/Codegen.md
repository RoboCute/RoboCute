# py-first代码生成架构

对于代码生成来说，至关重要的是类的信息

ClassInfo
- name
- cls
- module
- is_enum
- methods
- fields
- base_classes
- doc

反射注册表

ReflectionRegistry
- instance
- registered_classes
- register(cls: Type, module_name: str)
- _extract_class_info


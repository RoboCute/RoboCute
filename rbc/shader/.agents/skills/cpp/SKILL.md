---
name: cpp
---

# C++ Skill

This skill provides comprehensive C++ coding guidelines for the RoboCute project.

## Naming Conventions

- **Class names**: Use `CamelCase` (e.g., `MyClass`, `RenderPipeline`)
- **Functions and public variables**: Use `snake_case` (e.g., `get_value`, `process_data`)
- **Private/protected member variables and functions**: Use `_snake_case` (prefix with underscore)
  - Example: `_private_var`, `_internal_helper()`
- **Constants**: Use `kCamelCase` or `UPPER_SNAKE_CASE` for macros
- **Template parameters**: Use `CamelCase`
- **Syntax Check**: Run gen_json.cmd, and use tool:CppSyntaxCheck to check file syntax. Give up if file not in compile_commands.json.
- **Kernel Size**: [[kernel_1d(128)]] [[kernel_2d(16, 8)]] [[kernel_3d(8, 4, 4)]] kernel size should be used.

## Best Practices

1. **Prefer composition over inheritance**
2. **Keep functions small and focused** (single responsibility)
3. **Minimize public interface** - make members private by default
4. **Use const correctness** - mark methods and parameters const when possible
5. **Avoid raw loops** - use algorithms or range-based for
6. **Document public APIs** with clear comments
7. **Use strong types** - avoid primitive obsession
8. **Handle errors gracefully** - don't ignore error cases
9. **Reference** - DO NOT use const-lvalue reference
10. **Pointer** - DO NOT use pointer
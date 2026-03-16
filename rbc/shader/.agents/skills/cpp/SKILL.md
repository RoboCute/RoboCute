---
name: cpp
---

# C++ Master Skill for RoboCute Shader

- **Syntax Check** run tool:CppSyntaxCheck after write c++ file.
- **Class names**: Use `CamelCase` (e.g., `MyClass`, `RenderPipeline`)
- **Functions and public variables**: Use `snake_case` (e.g., `get_value`, `process_data`)
- **Private/protected member variables and functions**: Use `_snake_case` (prefix with underscore)
  - Example: `_private_var`, `_internal_helper()`
- **Constants**: Use `kCamelCase` or `UPPER_SNAKE_CASE` for macros
- **Template parameters**: Use `CamelCase`

## Code Style

- Use **4 spaces** for indentation (no tabs)
- Maximum line length: **100 characters**
- Always use braces `{}` for control structures, even for single-line blocks
- Place opening braces on the same line (K&R style)

```cpp
// Good
if (condition) {
    do_something();
}

// Bad
if (condition)
    do_something();
```
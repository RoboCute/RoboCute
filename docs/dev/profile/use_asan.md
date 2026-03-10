
### 使用 AddressSanitizer (ASAN) 测试内存泄漏

AddressSanitizer (ASAN) 是一个快速的内存错误检测工具，可以检测：
- 内存泄漏 (memory leaks)
- 使用已释放内存 (use-after-free)
- 缓冲区溢出 (buffer overflow)
- 使用未初始化内存 (use of uninitialized memory)
- 内存对齐错误 (misaligned memory access)

#### 配置 ASAN

在 xmake 中启用 ASAN：

**Linux/macOS:**
```bash
# 配置 debug 模式并启用 ASAN
xmake f -m debug --sanitize=address

# 编译测试
xmake build test_core

# 运行测试（ASAN 会自动检测内存问题）
xmake run test_core
```

**Windows (使用 clang-cl):**
```bash
# 配置 debug 模式并启用 ASAN
xmake f -m debug --sanitize=address --toolchain=clang

# 编译测试
xmake build test_core

# 运行测试
xmake run test_core
```

#### 在代码中启用 ASAN

如果需要在特定 target 中启用 ASAN，可以在 `xmake.lua` 中添加：

```lua
target("test_core")
    -- 启用 AddressSanitizer
    if is_plat("linux", "macosx") then
        add_cxflags("-fsanitize=address")
        add_mxflags("-fsanitize=address")
        add_ldflags("-fsanitize=address")
    elseif is_plat("windows") and is_toolchain("clang") then
        add_cxflags("-fsanitize=address")
        add_mxflags("-fsanitize=address")
        add_ldflags("-fsanitize=address")
    end
```

#### 测试内存泄漏示例

编写测试用例来验证内存分配/释放是否正确：

```cpp
#include "test_util.h"
#include <rbc_core/memory.h>

TEST_SUITE("core") {
    TEST_CASE("memory_leak_detection") {
        // 正确使用：分配后释放
        void *ptr = rbc_malloc(100);
        CHECK(ptr != nullptr);
        rbc_free(ptr);  // ASAN 不会报告泄漏
        
        // 错误示例：分配后未释放（会被 ASAN 检测到）
        // void *leaked = rbc_malloc(100);
        // 如果忘记释放，ASAN 会报告：
        // "ERROR: LeakSanitizer: detected memory leaks"
    }
    
    TEST_CASE("rbc_new_delete_leak_detection") {
        // 测试 RBCNew/RBCDelete 是否正确配对
        int *ptr = RBCNew<int>(42);
        CHECK(ptr != nullptr);
        CHECK(*ptr == 42);
        RBCDelete(ptr);  // 正确释放，ASAN 不会报告泄漏
        
        // 错误示例：忘记调用 RBCDelete
        // int *leaked = RBCNew<int>(42);
        // ASAN 会检测到泄漏
    }
    
    TEST_CASE("use_after_free_detection") {
        int *ptr = RBCNew<int>(100);
        RBCDelete(ptr);
        
        // 错误示例：使用已释放的内存（会被 ASAN 检测到）
        // *ptr = 200;  // ASAN 会报告：
        // "ERROR: AddressSanitizer: use-after-free"
    }
    
    TEST_CASE("buffer_overflow_detection") {
        int *arr = (int *)rbc_malloc(10 * sizeof(int));
        
        // 正确使用
        arr[9] = 100;
        
        // 错误示例：缓冲区溢出（会被 ASAN 检测到）
        // arr[10] = 200;  // ASAN 会报告：
        // "ERROR: AddressSanitizer: heap-buffer-overflow"
        
        rbc_free(arr);
    }
}
```

#### ASAN 输出解读

当 ASAN 检测到内存问题时，会输出类似以下的信息：

```
=================================================================
==12345==ERROR: LeakSanitizer: detected memory leaks

Direct leak of 100 byte(s) in 1 object(s) allocated from:
    #0 0x7f8b2c0d4b40 in malloc
    #1 0x55a1b2c3d4e0 in rbc_malloc rbc/core/src/memory.cpp:112
    #2 0x55a1b2c3e1a0 in test_memory_leak test_memory.cpp:15

SUMMARY: AddressSanitizer: 100 byte(s) leaked in 1 allocation(s).
=================================================================
```

关键信息：
- **Direct leak**: 直接泄漏（未释放的内存）
- **Indirect leak**: 间接泄漏（通过指针间接持有的内存）
- **Stack trace**: 显示分配内存的调用栈，帮助定位问题

#### 环境变量配置

可以通过环境变量控制 ASAN 的行为：

```bash
# 设置 ASAN 选项
export ASAN_OPTIONS="detect_leaks=1:halt_on_error=1:abort_on_error=1"

# Windows (PowerShell)
$env:ASAN_OPTIONS="detect_leaks=1:halt_on_error=1:abort_on_error=1"

# 运行测试
xmake run test_core
```

常用 ASAN 选项：
- `detect_leaks=1` - 启用内存泄漏检测（默认开启）
- `halt_on_error=1` - 检测到错误时停止程序
- `abort_on_error=1` - 检测到错误时中止程序
- `print_stats=1` - 打印统计信息
- `check_initialization_order=1` - 检查初始化顺序问题
- `detect_stack_use_after_return=1` - 检测栈返回后使用

#### 注意事项

1. **性能影响**: ASAN 会显著降低程序运行速度（约 2-3 倍），仅用于调试
2. **内存占用**: ASAN 会增加内存使用（约 2-3 倍）
3. **Windows 限制**: Windows 上需要 clang-cl 工具链，MSVC 不支持 ASAN
4. **与 mimalloc 兼容性**: 如果使用 mimalloc，可能需要特殊配置
5. **测试环境**: 建议在 CI/CD 中定期运行 ASAN 测试

#### 最佳实践

1. **定期运行**: 在每次代码提交前运行 ASAN 测试
2. **完整覆盖**: 确保所有内存分配函数都有对应的测试用例
3. **边界测试**: 测试边界情况（零大小、大内存、对齐等）
4. **配对检查**: 确保每个 `malloc` 都有对应的 `free`，每个 `RBCNew` 都有对应的 `RBCDelete`
5. **文档记录**: 在代码注释中说明内存所有权和生命周期

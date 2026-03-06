#pragma once
#include <rbc_display/to_str.h>
#include <rbc_display/transparent_window.h>
#include <rbc_core/base.h>
#include <luisa/core/logging.h>

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
// GWLP_EXSTYLE may not be defined in older Windows SDKs or certain compiler settings
#ifndef GWLP_EXSTYLE
#define GWLP_EXSTYLE (-20)
#endif
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

namespace rbc {

/**
 * @brief Windows implementation of transparent window
 */
class TransparentWindowWin32 : public TransparentWindow, public RBCStruct {
public:
    explicit TransparentWindowWin32(const TransparentWindowConfig &config);
    ~TransparentWindowWin32() override;

    void show() override;
    void hide() override;
    void close() override;
    void update_content(
        const uint8_t *pixel_data,
        uint32_t width,
        uint32_t height) override;
    void set_position(int32_t x, int32_t y) override;
    void set_size(uint32_t width, uint32_t height) override;
    void set_opacity(float opacity) override;
    void set_click_through(bool enable) override;
    bool is_visible() const override;
    WindowRect get_rect() const override;
    bool process_messages() override;

private:
    static LRESULT CALLBACK window_proc(
        HWND hwnd,
        UINT msg,
        WPARAM w_param,
        LPARAM l_param);

    bool create_window();
    void apply_transparency();
    void update_layered_window();

    HWND hwnd_{nullptr};
    HDC mem_dc_{nullptr};
    HBITMAP mem_bitmap_{nullptr};
    HBITMAP old_bitmap_{nullptr};
    uint32_t buffer_width_{0};
    uint32_t buffer_height_{0};

    TransparentWindowConfig config_;
    bool visible_{false};
    bool closed_{false};

    // Static map to associate HWND with instance
    static vstd::HashMap<HWND, TransparentWindowWin32 *> window_map_;
    static std::mutex map_mutex_;
};

vstd::HashMap<HWND, TransparentWindowWin32 *> TransparentWindowWin32::window_map_;
std::mutex TransparentWindowWin32::map_mutex_;

TransparentWindowWin32::TransparentWindowWin32(const TransparentWindowConfig &config)
    : config_(config) {
    if (!create_window()) {
        LUISA_ERROR("Failed to create transparent window");
        return;
    }
    apply_transparency();
}

TransparentWindowWin32::~TransparentWindowWin32() {
    close();
}

bool TransparentWindowWin32::create_window() {
    // Register window class
    static const wchar_t *class_name = L"RoboCuteTransparentWindow";
    static bool registered = false;

    if (!registered) {
        WNDCLASSEXW wcex{};
        wcex.cbSize = sizeof(WNDCLASSEXW);
        wcex.lpfnWndProc = window_proc;
        wcex.hInstance = GetModuleHandle(nullptr);
        wcex.lpszClassName = class_name;
        wcex.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
        wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);

        if (!RegisterClassExW(&wcex)) {
            LUISA_ERROR("Failed to register window class");
            return false;
        }
        registered = true;
    }

    // Create window
    // WS_EX_LAYERED: Required for UpdateLayeredWindow and transparency
    // WS_EX_APPWINDOW: Forces window to appear on taskbar
    DWORD ex_style = WS_EX_LAYERED | WS_EX_APPWINDOW;
    if (config_.topmost) {
        ex_style |= WS_EX_TOPMOST;
    }

    hwnd_ = CreateWindowExW(
        ex_style,
        class_name,
        reinterpret_cast<const wchar_t *>(
            luisa::string_to_wstring(config_.title).c_str()),
        WS_POPUP,
        config_.rect.x,
        config_.rect.y,
        static_cast<int>(config_.rect.width),
        static_cast<int>(config_.rect.height),
        nullptr,
        nullptr,
        GetModuleHandle(nullptr),
        nullptr);

    if (!hwnd_) {
        LUISA_ERROR("Failed to create window");
        return false;
    }

    // Store this instance in map
    {
        std::lock_guard<std::mutex> lock(map_mutex_);
        window_map_.emplace(hwnd_, this);
    }

    // Set window user data to this instance
    SetWindowLongPtr(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    return true;
}

void TransparentWindowWin32::apply_transparency() {
    if (!hwnd_) return;

    // Enable DWM blur behind (Windows Vista+)
    DWM_BLURBEHIND bb{};
    bb.dwFlags = DWM_BB_ENABLE;
    bb.fEnable = TRUE;
    DwmEnableBlurBehindWindow(hwnd_, &bb);

    // Set extended window attributes for transparency
    MARGINS margins{-1};
    DwmExtendFrameIntoClientArea(hwnd_, &margins);

    // Set initial opacity
    set_opacity(config_.opacity);

    // Apply click-through if enabled
    if (config_.click_through) {
        set_click_through(true);
    }
}

void TransparentWindowWin32::show() {
    if (hwnd_ && !closed_) {
        ShowWindow(hwnd_, SW_SHOW);
        visible_ = true;
    }
}

void TransparentWindowWin32::hide() {
    if (hwnd_) {
        ShowWindow(hwnd_, SW_HIDE);
        visible_ = false;
    }
}

void TransparentWindowWin32::close() {
    if (closed_) return;

    // Clean up GDI resources
    if (mem_dc_) {
        if (old_bitmap_) {
            SelectObject(mem_dc_, old_bitmap_);
        }
        if (mem_bitmap_) {
            DeleteObject(mem_bitmap_);
        }
        DeleteDC(mem_dc_);
        mem_dc_ = nullptr;
        mem_bitmap_ = nullptr;
        old_bitmap_ = nullptr;
    }

    // Remove from map
    if (hwnd_) {
        {
            std::lock_guard<std::mutex> lock(map_mutex_);
            window_map_.remove(hwnd_);
        }
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }

    closed_ = true;
    visible_ = false;
}

void TransparentWindowWin32::update_content(
    const uint8_t *pixel_data,
    uint32_t width,
    uint32_t height) {
    if (!hwnd_ || !pixel_data) return;

    // Create or resize memory DC if needed
    if (!mem_dc_ || width != buffer_width_ || height != buffer_height_) {
        if (mem_dc_) {
            if (old_bitmap_) {
                SelectObject(mem_dc_, old_bitmap_);
            }
            if (mem_bitmap_) {
                DeleteObject(mem_bitmap_);
            }
            DeleteDC(mem_dc_);
        }

        HDC screen_dc = GetDC(nullptr);
        mem_dc_ = CreateCompatibleDC(screen_dc);

        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = static_cast<LONG>(width);
        bmi.bmiHeader.biHeight = -static_cast<LONG>(height);// Top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        mem_bitmap_ = CreateDIBSection(
            mem_dc_, &bmi, DIB_RGB_COLORS, nullptr, nullptr, 0);
        old_bitmap_ = static_cast<HBITMAP>(SelectObject(mem_dc_, mem_bitmap_));

        buffer_width_ = width;
        buffer_height_ = height;

        ReleaseDC(nullptr, screen_dc);
    }

    // Copy pixel data to bitmap
    BITMAP bmp;
    GetObject(mem_bitmap_, sizeof(BITMAP), &bmp);
    memcpy(bmp.bmBits, pixel_data, width * height * 4);

    // Update layered window
    update_layered_window();
}

void TransparentWindowWin32::update_layered_window() {
    if (!hwnd_ || !mem_dc_) return;

    HDC screen_dc = GetDC(nullptr);

    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = static_cast<BYTE>(config_.opacity * 255);
    blend.AlphaFormat = AC_SRC_ALPHA;

    POINT src_pos{0, 0};
    SIZE size{
        static_cast<LONG>(buffer_width_),
        static_cast<LONG>(buffer_height_)};
    POINT dst_pos{0, 0};

    UpdateLayeredWindow(
        hwnd_,
        screen_dc,
        &dst_pos,
        &size,
        mem_dc_,
        &src_pos,
        0,
        &blend,
        ULW_ALPHA);

    ReleaseDC(nullptr, screen_dc);
}

void TransparentWindowWin32::set_position(int32_t x, int32_t y) {
    if (!hwnd_) return;
    config_.rect.x = x;
    config_.rect.y = y;
    SetWindowPos(
        hwnd_,
        nullptr,
        x,
        y,
        0,
        0,
        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void TransparentWindowWin32::set_size(uint32_t width, uint32_t height) {
    if (!hwnd_) return;
    config_.rect.width = width;
    config_.rect.height = height;
    SetWindowPos(
        hwnd_,
        nullptr,
        0,
        0,
        static_cast<int>(width),
        static_cast<int>(height),
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void TransparentWindowWin32::set_opacity(float opacity) {
    config_.opacity = std::clamp(opacity, 0.0f, 1.0f);
    update_layered_window();
}

void TransparentWindowWin32::set_click_through(bool enable) {
    if (!hwnd_) return;
    config_.click_through = enable;

    LONG_PTR ex_style = GetWindowLongPtr(hwnd_, GWLP_EXSTYLE);
    if (enable) {
        ex_style |= WS_EX_TRANSPARENT;
    } else {
        ex_style &= ~WS_EX_TRANSPARENT;
    }
    SetWindowLongPtr(hwnd_, GWLP_EXSTYLE, ex_style);
    // Force window to update its style
    SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

bool TransparentWindowWin32::is_visible() const {
    return visible_;
}

WindowRect TransparentWindowWin32::get_rect() const {
    return config_.rect;
}

bool TransparentWindowWin32::process_messages() {
    if (closed_) return false;

    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            close();
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return !closed_;
}

LRESULT CALLBACK TransparentWindowWin32::window_proc(
    HWND hwnd,
    UINT msg,
    WPARAM w_param,
    LPARAM l_param) {
    // Get instance from window map or user data
    TransparentWindowWin32 *window = nullptr;
    {
        std::lock_guard<std::mutex> lock(map_mutex_);
        auto iter = window_map_.find(hwnd);
        if (iter) {
            window = iter.value();
        }
    }

    if (!window) {
        return DefWindowProcW(hwnd, msg, w_param, l_param);
    }

    switch (msg) {
        case WM_DESTROY:
            window->closed_ = true;
            window->visible_ = false;
            PostQuitMessage(0);
            return 0;

        case WM_NCHITTEST: {
            if (window->config_.click_through) {
                return HTTRANSPARENT;
            }

            // Get mouse position in window coordinates
            POINT pt;
            pt.x = GET_X_LPARAM(l_param);
            pt.y = GET_Y_LPARAM(l_param);

            RECT rect;
            GetWindowRect(hwnd, &rect);

            int x = pt.x - rect.left;
            int y = pt.y - rect.top;
            int width = rect.right - rect.left;
            int height = rect.bottom - rect.top;
            const int border = 8;// Resize border thickness

            // Check corners first
            if (x < border && y < border) return HTTOPLEFT;
            if (x >= width - border && y < border) return HTTOPRIGHT;
            if (x < border && y >= height - border) return HTBOTTOMLEFT;
            if (x >= width - border && y >= height - border) return HTBOTTOMRIGHT;

            // Check edges
            if (x < border) return HTLEFT;
            if (x >= width - border) return HTRIGHT;
            if (y < border) return HTTOP;
            if (y >= height - border) return HTBOTTOM;

            // Default: allow dragging from caption area
            return HTCAPTION;
        }

        case WM_KEYDOWN: {
            if (w_param == VK_ESCAPE) {
                window->close();
                return 0;
            }
            return 1;
        }

        default:
            return DefWindowProcW(hwnd, msg, w_param, l_param);
    }
}

// Factory function implementation
TransparentWindow *TransparentWindow::create(
    const TransparentWindowConfig &config) {
#ifdef _WIN32
    auto window = new TransparentWindowWin32(config);
    if (!window->get_rect().width) {
        delete window;
        return nullptr;
    }
    return window;
#else
    LUISA_ERROR("TransparentWindow not implemented for this platform");
    return nullptr;
#endif
}

}// namespace rbc

#else
// Non-Windows platforms
namespace rbc {

RC<TransparentWindow> TransparentWindow::create(
    const TransparentWindowConfig &config) {
    LUISA_ERROR("TransparentWindow not implemented for this platform");
    return RC<TransparentWindow>();
}

}// namespace rbc

#endif

namespace rbc {
TransparentWindow *create_transparent_window(const TransparentWindowConfig &config) {
    return TransparentWindow::create(config);
}
}// namespace rbc
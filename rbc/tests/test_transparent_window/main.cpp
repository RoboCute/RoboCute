#include <rbc_graphics/scene_manager.h>
#include <rbc_graphics/shader_manager.h>
#include <rbc_graphics/render_device.h>
#include <luisa/core/clock.h>
#include <luisa/gui/window.h>
#include <luisa/runtime/swapchain.h>
#include <luisa/core/binary_io.h>
#include <rbc_render/render_plugin.h>
#include <rbc_graphics/device_assets/assets_manager.h>
#include <luisa/core/dynamic_module.h>
#include <rbc_render/generated/pipeline_settings.hpp>
#include <rbc_graphics/device_assets/device_sparse_image.h>
#include <rbc_graphics/device_assets/device_mesh.h>
#include <rbc_graphics/device_assets/device_image.h>
#include <rbc_graphics/graphics_utils.h>
#include <rbc_graphics/mat_manager.h>
#include <rbc_graphics/materials.h>
#include <rbc_render/click_manager.h>
#include <rbc_render/grid_drawer.h>
#include <luisa/gui/window.h>
#include <rbc_app/camera_controller.h>
#include <rbc_core/runtime_static.h>
#include <rbc_plugin/plugin_manager.h>
#include <tracy_wrapper.h>
#include <rbc_core/state_map.h>
#include <rbc_world/components/transform_component.h>
#include <rbc_world/components/render_component.h>
#include <rbc_world/components/light_component.h>
#include <rbc_world/entity.h>
#include <rbc_world/resources/mesh.h>
#include <rbc_world/resources/texture.h>
#include <rbc_world/resources/material.h>
#include <rbc_graphics/mesh_builder.h>
#include <rbc_world/importers/texture_loader.h>
#include <rbc_world/importers/texture_importer_stb.h>
#include <rbc_world/importers/texture_importer_exr.h>
#include <rbc_display/transparent_window.h>
#include <rbc_plugin/plugin_manager.h>

using namespace rbc;
using namespace luisa;
using namespace luisa::compute;
#include <material/mats.inl>

/**
 * @brief Draw a filled rectangle in pixel buffer
 * @param pixel_data The pixel buffer
 * @param width Buffer width
 * @param height Buffer height
 * @param x0 Left coordinate
 * @param y0 Top coordinate
 * @param x1 Right coordinate
 * @param y1 Bottom coordinate
 * @param r Red component
 * @param g Green component
 * @param b Blue component
 * @param a Alpha component
 */
void draw_rect(
    luisa::vector<uint8_t> &pixel_data,
    uint32_t width,
    uint32_t height,
    int32_t x0,
    int32_t y0,
    int32_t x1,
    int32_t y1,
    uint8_t r,
    uint8_t g,
    uint8_t b,
    uint8_t a) {
    // Clamp coordinates
    x0 = std::max(0, std::min(static_cast<int32_t>(width), x0));
    y0 = std::max(0, std::min(static_cast<int32_t>(height), y0));
    x1 = std::max(0, std::min(static_cast<int32_t>(width), x1));
    y1 = std::max(0, std::min(static_cast<int32_t>(height), y1));
    
    for (int32_t y = y0; y < y1; ++y) {
        for (int32_t x = x0; x < x1; ++x) {
            size_t idx = (y * width + x) * 4;
            // Alpha blend
            float alpha = a / 255.0f;
            pixel_data[idx + 0] = static_cast<uint8_t>(r * alpha + pixel_data[idx + 0] * (1 - alpha));
            pixel_data[idx + 1] = static_cast<uint8_t>(g * alpha + pixel_data[idx + 1] * (1 - alpha));
            pixel_data[idx + 2] = static_cast<uint8_t>(b * alpha + pixel_data[idx + 2] * (1 - alpha));
            pixel_data[idx + 3] = static_cast<uint8_t>(255);
        }
    }
}

/**
 * @brief Draw a line in pixel buffer
 * @param pixel_data The pixel buffer
 * @param width Buffer width
 * @param height Buffer height
 * @param x0 Start X
 * @param y0 Start Y
 * @param x1 End X
 * @param y1 End Y
 * @param r Red component
 * @param g Green component
 * @param b Blue component
 * @param thickness Line thickness
 */
void draw_line(
    luisa::vector<uint8_t> &pixel_data,
    uint32_t width,
    uint32_t height,
    int32_t x0,
    int32_t y0,
    int32_t x1,
    int32_t y1,
    uint8_t r,
    uint8_t g,
    uint8_t b,
    int32_t thickness = 1) {
    // Simple Bresenham line algorithm
    int32_t dx = std::abs(x1 - x0);
    int32_t dy = std::abs(y1 - y0);
    int32_t sx = (x0 < x1) ? 1 : -1;
    int32_t sy = (y0 < y1) ? 1 : -1;
    int32_t err = dx - dy;
    
    while (true) {
        // Draw pixel with thickness
        for (int32_t ty = -thickness / 2; ty <= thickness / 2; ++ty) {
            for (int32_t tx = -thickness / 2; tx <= thickness / 2; ++tx) {
                int32_t px = x0 + tx;
                int32_t py = y0 + ty;
                if (px >= 0 && px < static_cast<int32_t>(width) && py >= 0 && py < static_cast<int32_t>(height)) {
                    size_t idx = (py * width + px) * 4;
                    pixel_data[idx + 0] = r;
                    pixel_data[idx + 1] = g;
                    pixel_data[idx + 2] = b;
                    pixel_data[idx + 3] = 255;
                }
            }
        }
        
        if (x0 == x1 && y0 == y1) break;
        int32_t e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

/**
 * @brief Draw close button (X) in top-right corner
 * @param pixel_data The pixel buffer
 * @param width Buffer width
 * @param height Buffer height
 * @param btn_x Button X position
 * @param btn_y Button Y position
 * @param btn_size Button size
 * @param hover Whether button is hovered
 */
void draw_close_button(
    luisa::vector<uint8_t> &pixel_data,
    uint32_t width,
    uint32_t height,
    int32_t btn_x,
    int32_t btn_y,
    int32_t btn_size,
    bool hover) {
    uint8_t bg_r = hover ? 255 : 200;
    uint8_t bg_g = hover ? 80 : 50;
    uint8_t bg_b = hover ? 80 : 50;
    
    // Draw button background
    draw_rect(pixel_data, width, height, btn_x, btn_y, btn_x + btn_size, btn_y + btn_size, 
              bg_r, bg_g, bg_b, 230);
    
    // Draw X
    uint8_t x_color = hover ? 255 : 220;
    int32_t padding = btn_size / 4;
    draw_line(pixel_data, width, height, 
              btn_x + padding, btn_y + padding, 
              btn_x + btn_size - padding, btn_y + btn_size - padding,
              x_color, x_color, x_color, 2);
    draw_line(pixel_data, width, height, 
              btn_x + btn_size - padding, btn_y + padding,
              btn_x + padding, btn_y + btn_size - padding,
              x_color, x_color, x_color, 2);
}

/**
 * @brief Draw resize handle indicator in bottom-right corner
 * @param pixel_data The pixel buffer
 * @param width Buffer width
 * @param height Buffer height
 */
void draw_resize_handle(
    luisa::vector<uint8_t> &pixel_data,
    uint32_t width,
    uint32_t height) {
    const int32_t handle_size = 20;
    uint8_t r = 150, g = 150, b = 150;
    
    // Draw diagonal lines to indicate resize handle
    for (int i = 0; i < 3; ++i) {
        int offset = i * 5 + 5;
        draw_line(pixel_data, width, height,
                  static_cast<int32_t>(width) - handle_size + offset, static_cast<int32_t>(height) - 5,
                  static_cast<int32_t>(width) - 5, static_cast<int32_t>(height) - handle_size + offset,
                  r, g, b, 2);
    }
}

/**
 * @brief Draw title bar with window title
 * @param pixel_data The pixel buffer
 * @param width Buffer width
 * @param height Buffer height
 */
void draw_title_bar(
    luisa::vector<uint8_t> &pixel_data,
    uint32_t width,
    uint32_t height) {
    const int32_t title_height = 30;
    // Dark gradient title bar
    for (int32_t y = 0; y < title_height; ++y) {
        uint8_t brightness = static_cast<uint8_t>(60 + (y * 40 / title_height));
        for (int32_t x = 0; x < static_cast<int32_t>(width); ++x) {
            size_t idx = (y * width + x) * 4;
            pixel_data[idx + 0] = brightness;
            pixel_data[idx + 1] = brightness;
            pixel_data[idx + 2] = brightness + 20;
            pixel_data[idx + 3] = 240;
        }
    }
}

int main() {
    PluginManager::init();
    auto &&display_module = PluginManager::instance().load_module("rbc_display_plugin");

    // Create a TransparentWindow instance and run a simple render loop
    TransparentWindowConfig config;
    config.title = "Test Transparent Window";
    config.rect = {100, 100, 400, 300};
    config.opacity = 0.9f;
    config.topmost = true;
    config.click_through = false;

    auto window =
        luisa::unique_ptr<TransparentWindow>(display_module->invoke<TransparentWindow *(const TransparentWindowConfig &config)>(
            "create_transparent_window",
            config));
    if (!window) {
        LUISA_ERROR("Failed to create transparent window");
        return 1;
    }

    window->show();
    LUISA_INFO("Transparent window created and shown");
    LUISA_INFO("Controls: Drag anywhere to move, drag edges/corners to resize,");
    LUISA_INFO("         Click X button or press ESC to close");

    // Window content dimensions
    const uint32_t width = 400;
    const uint32_t height = 300;
    luisa::vector<uint8_t> pixel_data(width * height * 4);

    // Close button properties
    const int32_t btn_size = 24;
    const int32_t btn_margin = 4;
    const int32_t btn_x = static_cast<int32_t>(width) - btn_size - btn_margin;
    const int32_t btn_y = btn_margin + 3; // Below title bar offset

    // Mouse state
    bool is_close_hovered = false;
    bool is_close_pressed = false;

    Clock clk;
    while (window->process_messages()) {
        RBCFrameMark;

        // Get current window rect for resize feedback
        WindowRect rect = window->get_rect();

        // Clear with semi-transparent background
        std::fill(pixel_data.begin(), pixel_data.end(), 0);

        // Draw title bar (draggable area)
        draw_title_bar(pixel_data, width, height);

        // Draw main content area with gradient
        float time = clk.toc() * 0.001f;
        for (uint32_t y = 30; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                size_t idx = (y * width + x) * 4;
                float u = static_cast<float>(x) / width;
                float v = static_cast<float>(y - 30) / (height - 30);

                // Animated color gradient
                pixel_data[idx + 0] = static_cast<uint8_t>(255 * (0.3f + 0.3f * std::sin(u * 3.14159f * 2 + time)));
                pixel_data[idx + 1] = static_cast<uint8_t>(255 * (0.3f + 0.3f * std::sin(v * 3.14159f * 2 + time * 1.3f)));
                pixel_data[idx + 2] = static_cast<uint8_t>(255 * (0.5f + 0.3f * std::sin((u + v) * 3.14159f + time * 0.7f)));
                pixel_data[idx + 3] = 200;// Semi-transparent alpha
            }
        }

        // Draw instructions text area
        draw_rect(pixel_data, width, height, 20, 50, 380, 140, 0, 0, 0, 180);
        
        // Draw border around content
        const int32_t border = 2;
        uint8_t border_r = 100, border_g = 150, border_b = 200;
        // Top (below title)
        draw_rect(pixel_data, width, height, 0, 30, static_cast<int32_t>(width), 30 + border, border_r, border_g, border_b, 255);
        // Bottom
        draw_rect(pixel_data, width, height, 0, static_cast<int32_t>(height) - border, static_cast<int32_t>(width), static_cast<int32_t>(height), border_r, border_g, border_b, 255);
        // Left
        draw_rect(pixel_data, width, height, 0, 30, border, static_cast<int32_t>(height), border_r, border_g, border_b, 255);
        // Right
        draw_rect(pixel_data, width, height, static_cast<int32_t>(width) - border, 30, static_cast<int32_t>(width), static_cast<int32_t>(height), border_r, border_g, border_b, 255);

        // Draw resize handle indicator
        draw_resize_handle(pixel_data, width, height);

        // Draw close button
        draw_close_button(pixel_data, width, height, btn_x, btn_y, btn_size, is_close_hovered);

        // Update window content
        window->update_content(pixel_data.data(), width, height);

        // Small delay to avoid maxing out CPU (~60 FPS)
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    window->close();
    LUISA_INFO("Transparent window closed");

    return 0;
}

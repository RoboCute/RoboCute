#pragma once
#include <rbc_plugin/plugin.h>
#include <rbc_core/rc.h>
#include <luisa/core/stl.h>
#include <luisa/core/mathematics.h>

namespace rbc {
struct TransparentWindow;

/**
 * @brief Structure representing window position and size
 */
struct WindowRect {
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
};

/**
 * @brief Configuration for transparent window creation
 */
struct TransparentWindowConfig {
    luisa::string title{"RoboCute Transparent Window"};
    WindowRect rect{100, 100, 800, 600};
    float opacity{0.8f};      ///< Window opacity (0.0 - 1.0)
    bool topmost{false};      ///< Whether window stays on top
    bool click_through{false};///< Whether window is click-through
};
LUISA_EXPORT_API TransparentWindow *create_transparent_window(const TransparentWindowConfig &config);

/**
 * @brief Platform-agnostic transparent window interface
 * 
 * Provides a transparent overlay window that can be used for
 * displaying content above other windows with alpha blending.
 */
struct TransparentWindow : public RBCStruct {
    friend TransparentWindow *create_transparent_window(const TransparentWindowConfig &config);
private:
    static TransparentWindow *create(const TransparentWindowConfig &config);
public:
    virtual ~TransparentWindow() = default;

    /**
     * @brief Create a new transparent window
     * @param config Window configuration
     * @return RC<TransparentWindow> Reference counted window instance
     */

    /**
     * @brief Show the window
     */
    virtual void show() = 0;

    /**
     * @brief Hide the window
     */
    virtual void hide() = 0;

    /**
     * @brief Close and destroy the window
     */
    virtual void close() = 0;

    /**
     * @brief Update window content
     * @param pixel_data RGBA pixel data
     * @param width Width of the content
     * @param height Height of the content
     */
    virtual void update_content(
        const uint8_t *pixel_data,
        uint32_t width,
        uint32_t height) = 0;

    /**
     * @brief Set window position
     * @param x X coordinate
     * @param y Y coordinate
     */
    virtual void set_position(int32_t x, int32_t y) = 0;

    /**
     * @brief Set window size
     * @param width Window width
     * @param height Window height
     */
    virtual void set_size(uint32_t width, uint32_t height) = 0;

    /**
     * @brief Set window opacity
     * @param opacity Opacity value (0.0 - 1.0)
     */
    virtual void set_opacity(float opacity) = 0;

    /**
     * @brief Set click-through mode
     * @param enable If true, mouse events pass through the window
     */
    virtual void set_click_through(bool enable) = 0;

    /**
     * @brief Check if window is visible
     * @return true if window is visible
     */
    virtual bool is_visible() const = 0;

    /**
     * @brief Get window rectangle
     * @return Current window rectangle
     */
    virtual WindowRect get_rect() const = 0;

    /**
     * @brief Process window messages (call each frame)
     * @return false if window was closed
     */
    virtual bool process_messages() = 0;
    virtual uint64_t display_handle() = 0;
    virtual uint64_t window_handle() = 0;
    virtual void update_layered_window() = 0;
};

}// namespace rbc

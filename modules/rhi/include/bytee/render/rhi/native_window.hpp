#pragma once

#include <cstdint>

namespace bytee::render::rhi {

// A window (or none) to present to, described without any window-system types: Bytee never owns or creates windows.
// Fill it from your windowing library (samples/common does it for GLFW).
struct NativeWindow {
    enum class Platform : uint8_t {
        Headless, // no window: presentation is a no-op (tests, CI, servers)
        Metal,    // macOS / iOS: `handle` = CAMetalLayer*
        Win32,    // Windows: `display` = HINSTANCE, `handle` = HWND
        Xlib,     // Linux X11: `display` = Display*, `id` = Window
        Xcb,      // Linux X11 (XCB): `display` = xcb_connection_t*, `id` = xcb_window_t
        Wayland,  // Linux Wayland: `display` = wl_display*, `handle` = wl_surface*
    };

    Platform platform = Platform::Headless;
    void* display = nullptr;
    void* handle = nullptr;
    uint64_t id = 0;
};

} // namespace bytee::render::rhi

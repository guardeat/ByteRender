// macOS: Bytee presents through a CAMetalLayer attached to the window's content view (VK_EXT_metal_surface).

#include "sample_window.hpp"

#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>

namespace bytee::samples {

render::rhi::NativeWindow make_native_window(GLFWwindow* window) {
    NSWindow* ns_window = glfwGetCocoaWindow(window);
    NSView* view = [ns_window contentView];
    CAMetalLayer* layer = [CAMetalLayer layer];
    layer.contentsScale = [ns_window backingScaleFactor]; // render at full pixel resolution on Retina screens
    [view setWantsLayer:YES];
    [view setLayer:layer]; // the view retains the layer
    return render::rhi::NativeWindow{
        .platform = render::rhi::NativeWindow::Platform::Metal,
        .handle = layer, // non-ARC Objective-C++: object pointers convert to void* implicitly
    };
}

} // namespace bytee::samples

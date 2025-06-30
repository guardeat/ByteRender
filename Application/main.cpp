#include "render/renderer.h"
#include "core/window.h"

using namespace Byte;

int main() {
	Window window{100,100};

	Renderer renderer;

	renderer.initialize(window);

	return 0;
}